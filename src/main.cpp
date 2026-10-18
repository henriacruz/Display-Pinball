#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <AnimatedGIF.h>
#include <algorithm>
#include <limits.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include "Display.hpp"
#include "PlayerConfig.hpp"
#include "TouchButton.hpp"

namespace {
Display display;
SPIClass sdSpi(VSPI);
AnimatedGIF gif;
File gifFile;
// Um setor alinhado; posicao fisica so muda ao recarregar o cache.
constexpr int32_t sectorSize = 512;
alignas(4) uint8_t sectorBuffer[sectorSize];
int32_t sectorStart = -1, physicalPosition = 0;
size_t sectorLength = 0;
unsigned current = 0;
bool sdReady = false, ioFailed = false, unsupportedDisposal = false;
int gifX = 0, gifY = 0;
uint16_t background = TFT_BLACK;
struct Frame {
  int x, y, w, h;
  uint8_t disposal;
} frame{};
bool frameTransparent = false;
struct Performance {
  uint64_t sdUs = 0, drawUs = 0, workUs = 0, bytes = 0;
  uint32_t reads = 0, diskReads = 0, sends = 0;
} performance;

struct MeasureTime {
  uint64_t& total;
  int64_t start = esp_timer_get_time();
  explicit MeasureTime(uint64_t& value) : total(value) {}
  ~MeasureTime() { total += esp_timer_get_time() - start; }
};

// Linhas agrupadas em blocos enviados por DMA: enquanto um buffer vai para o
// display, o decodificador preenche o outro. Pixels ja na ordem do painel.
constexpr int blockLines = 16, maxWidth = 480;
alignas(4) uint16_t blockBuffers[2][blockLines * maxWidth];
int fillBuffer = 0;
struct Block {
  int x, y, w, lines;
} block{};

TouchButton touchButton;
esp_timer_handle_t touchTimer = nullptr;
portMUX_TYPE touchMux = portMUX_INITIALIZER_UNLOCKED;
bool touchPending = false;
bool touchTimerReady = false;
unsigned currentBrightness = config::brightness;

void printMemory() {
  const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  Serial.printf("MEM: interna livre=%u, maior bloco=%u, minimo livre=%u; "
                "PSRAM total=%u, livre=%u; Turbo 320x480 requer=%u bytes\n",
                static_cast<unsigned>(heap_caps_get_free_size(caps)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(caps)),
                static_cast<unsigned>(heap_caps_get_minimum_free_size(caps)),
                static_cast<unsigned>(ESP.getPsramSize()),
                static_cast<unsigned>(ESP.getFreePsram()),
                static_cast<unsigned>(320U * 480U + TURBO_BUFFER_SIZE));
}

void sampleTouch(void*) {
  // Executado pela tarefa do esp_timer: sem SPI, desenho ou acesso ao SD.
  if (touchButton.update(digitalRead(config::touchIrq) == LOW, millis())) {
    portENTER_CRITICAL(&touchMux);
    touchPending = true;
    portEXIT_CRITICAL(&touchMux);
  }
}

bool nextRequested() {
  bool serialNext = false;
  while (Serial.available()) {
    const char command = Serial.read();
    if (command == 'n') serialNext = true;
    if (command == 'm') printMemory();
    if (command == 't') {
      Serial.printf("Touch IRQ GPIO%d=%d (0=pressionado); timer=%s\n",
                    config::touchIrq, digitalRead(config::touchIrq),
                    touchTimerReady ? "OK" : "FALHOU");
    }
  }
  portENTER_CRITICAL(&touchMux);
  const bool touched = touchPending;
  touchPending = false;
  portEXIT_CRITICAL(&touchMux);
  if (touched) Serial.println("TOQUE: avancando GIF");
  if (serialNext) Serial.println("SERIAL: avancando GIF");
  return serialNext || touched;
}

// Fade sobre o quadro parado, sem framebuffer adicional. No fade-out, os
// eventos ficam pendentes; no fade-in, um toque pode pular o GIF normalmente.
bool fadeBrightness(unsigned target, uint32_t duration, bool allowSkip = false) {
  const unsigned initial = currentBrightness;
  const uint32_t started = millis();
  while (duration && currentBrightness != target) {
    if (allowSkip && nextRequested()) return true;
    const uint32_t elapsed = millis() - started;
    if (elapsed >= duration) break;
    const float t = static_cast<float>(elapsed) / duration;
    const float eased = t * t * (3.0f - 2.0f * t);
    const unsigned value = static_cast<unsigned>(initial +
        (static_cast<int>(target) - static_cast<int>(initial)) * eased + 0.5f);
    if (value != currentBrightness) {
      currentBrightness = value;
      display.setBrightness(value);
    }
    delay(2);
  }
  currentBrightness = target;
  display.setBrightness(target);
  return false;
}

bool waitUntil(uint32_t started, uint32_t duration) {
  do {
    if (nextRequested()) return true;
    delay(1);
  } while (millis() - started < duration);
  return false;
}

void message(const char* title, const char* detail) {
  display.fillScreen(TFT_BLACK);
  display.setTextSize(2);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(12, 40);
  display.println(title);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(12, 90);
  display.println(detail);
  // Erros e avisos continuam visiveis mesmo se a troca apagou o backlight.
  currentBrightness = config::brightness;
  display.setBrightness(currentBrightness);
  Serial.printf("%s: %s\n", title, detail);
}

void* openFile(const char* name, int32_t* size) {
  sectorStart = -1;
  sectorLength = 0;
  gifFile = SD.open(name, FILE_READ);
  if (!gifFile || gifFile.isDirectory() || gifFile.size() > INT32_MAX) {
    gifFile.close();
    return nullptr;
  }
  *size = static_cast<int32_t>(gifFile.size());
  // Fundo da tabela global: a paleta de um frame pode ser diferente.
  background = TFT_BLACK;
  uint8_t header[13];
  if (gifFile.read(header, sizeof(header)) == sizeof(header) && (header[10] & 0x80)) {
    const unsigned paletteSize = 1U << ((header[10] & 7) + 1);
    uint8_t rgb[3];
    if (header[11] < paletteSize && gifFile.seek(13 + 3 * header[11]) &&
        gifFile.read(rgb, 3) == 3) {
      background = display.color565(rgb[0], rgb[1], rgb[2]);
    }
  }
  if (!gifFile.seek(0)) {
    gifFile.close();
    return nullptr;
  }
  physicalPosition = 0;
  return &gifFile;
}

void closeFile(void* handle) {
  sectorStart = -1;
  sectorLength = 0;
  if (handle) static_cast<File*>(handle)->close();
}

int32_t readFile(GIFFILE* file, uint8_t* buffer, int32_t requested) {
  MeasureTime timing(performance.sdUs);
  auto* handle = static_cast<File*>(file->fHandle);
  const int32_t remaining = file->iSize - file->iPos;
  if (!handle || !*handle || requested <= 0 || remaining <= 0) return 0;
  const int32_t count = std::min(requested, remaining);
  ++performance.reads;
  int32_t copied = 0;
  while (copied < count) {
    const int32_t position = file->iPos;
    const int32_t aligned = position - position % sectorSize;
    if (sectorStart != aligned) {
      sectorStart = -1;
      sectorLength = 0;
      if (physicalPosition != aligned && !handle->seek(aligned)) {
        ioFailed = true;
        break;
      }
      const size_t wanted = std::min(sectorSize, file->iSize - aligned);
      sectorLength = handle->read(sectorBuffer, wanted);
      ++performance.diskReads;
      physicalPosition = aligned + sectorLength;
      if (sectorLength != wanted) ioFailed = true;
      sectorStart = aligned;
    }
    const size_t offset = position - sectorStart;
    if (offset >= sectorLength) { ioFailed = true; break; }
    const size_t chunk = std::min(static_cast<size_t>(count - copied), sectorLength - offset);
    memcpy(buffer + copied, sectorBuffer + offset, chunk);
    file->iPos += chunk;
    copied += chunk;
  }
  performance.bytes += copied;
  if (copied != count) ioFailed = true;
  return copied;
}

int32_t seekFile(GIFFILE* file, int32_t position) {
  MeasureTime timing(performance.sdUs);
  auto* handle = static_cast<File*>(file->fHandle);
  if (!handle || !*handle || position < 0 || position > file->iSize) {
    ioFailed = true;
    return -1;
  }
  // Seek logico: o cache atende a posicao ou a proxima leitura reposiciona o SD.
  file->iPos = position;
  return file->iPos;
}

void pushDMA(int x, int y, int w, int h, const uint16_t* pixels) {
  ++performance.sends;
  // swap565_t = formato nativo do painel: DMA direto, sem conversao/copia.
  display.pushImageDMA(x, y, w, h, reinterpret_cast<const lgfx::swap565_t*>(pixels));
}

void flushBlock() {
  if (block.lines == 0) return;
  pushDMA(block.x, block.y, block.w, block.lines, blockBuffers[fillBuffer]);
  // O proximo envio espera este terminar; o outro buffer ja esta livre.
  fillBuffer ^= 1;
  block.lines = 0;
}

void drawGif(GIFDRAW* draw) {
  MeasureTime timing(performance.drawUs);
  frame = {draw->iX, draw->iY, draw->iWidth, draw->iHeight, draw->ucDisposalMethod};
  // Metodo 3 requer guardar a imagem anterior; nao suportado neste modo sem framebuffer.
  if (frame.disposal >= 3) {
    unsupportedDisposal = true;
    return;
  }
  const int y = gifY + draw->iY + draw->y;
  const int left = gifX + draw->iX;
  if (y < 0 || y >= display.height()) return;
  const int first = std::max(0, -left);
  const int end = std::min({draw->iWidth, static_cast<int>(display.width()) - left, maxWidth});
  if (first >= end) return;
  const uint8_t* pixels = draw->pPixels;
  const uint16_t* palette = draw->pPalette;
  const uint8_t key = draw->ucTransparent;
  const bool opaque = !draw->ucHasTransparency ||
                      std::find(pixels + first, pixels + end, key) == pixels + end;
  if (opaque) {
    const int x0 = left + first, width = end - first;
    if (block.lines && (block.x != x0 || block.w != width || block.y + block.lines != y)) {
      flushBlock();
    }
    if (block.lines == 0) block = {x0, y, width, 0};
    uint16_t* out = blockBuffers[fillBuffer] + block.lines * width;
    for (int x = first; x < end; ++x) *out++ = palette[pixels[x]];
    if (++block.lines == blockLines) flushBlock();
    return;
  }
  // Linha com transparencia: envia somente os trechos visiveis. Cada trecho
  // ocupa sua propria faixa do buffer, sem sobrescrever um DMA em andamento.
  frameTransparent = true;
  flushBlock();
  uint16_t* line = blockBuffers[fillBuffer];
  int x = first;
  while (x < end) {
    if (pixels[x] == key) {
      ++x;
      continue;
    }
    const int start = x;
    while (x < end && pixels[x] != key) {
      line[x] = palette[pixels[x]];
      ++x;
    }
    pushDMA(left + start, y, x - start, 1, line + start);
  }
  fillBuffer ^= 1;
}

uint32_t frameDuration(int gifDelay) {
  const uint32_t scaled = static_cast<uint32_t>(std::max(0, gifDelay)) * 100U / config::speedPercent;
  return std::max(config::minFrameDelayMs, scaled);
}

void resetCanvas() {
  display.fillScreen(TFT_BLACK);
  display.fillRect(gifX, gifY, gif.getCanvasWidth(), gif.getCanvasHeight(), background);
  frame = {};
}

void playCurrent() {
  fadeBrightness(0, config::fadeOutMs);
  const char* name = config::files[current];
  Serial.printf("Abrindo %s\n", name);
  ioFailed = false;
  unsupportedDisposal = false;
  gif.begin(BIG_ENDIAN_PIXELS);
  if (!gif.open(name, openFile, closeFile, readFile, seekFile, drawGif)) {
    Serial.printf("Erro GIF: %d\n", gif.getLastError());
    gifFile.close();
    message("GIF ausente/invalido", name);
    waitUntil(millis(), 1500);
    return;
  }
  const int width = gif.getCanvasWidth(), height = gif.getCanvasHeight();
  if (width <= 0 || height <= 0 || width > display.width() || height > display.height()) {
    message("GIF maior que tela", "Redimensione o arquivo");
    Serial.printf("GIF %dx%d; tela %dx%d\n", width, height, display.width(), display.height());
    gif.close();
    waitUntil(millis(), 1500);
    return;
  }
  gifX = (display.width() - width) / 2;
  gifY = (display.height() - height) / 2;
  resetCanvas();
  printMemory();
  performance = {};
  uint32_t shownAt = millis();
  uint32_t loopStarted = shownAt;
  unsigned frames = 0, lateFrames = 0;
  bool firstFrame = true, firstFrameCovers = false;
  while (true) {
    if (nextRequested()) break;
    const uint32_t started = millis();
    const int64_t workStarted = esp_timer_get_time();
    // Restaura o fundo DEPOIS do tempo de exibicao do frame anterior.
    if (frame.disposal == 2) {
      MeasureTime timing(performance.drawUs);
      display.fillRect(gifX + frame.x, gifY + frame.y, frame.w, frame.h, background);
    }
    int frameDelay = 0;
    frameTransparent = false;
    display.startWrite();
    const int result = gif.playFrame(false, &frameDelay);
    {
      MeasureTime timing(performance.drawUs);
      flushBlock();
      display.waitDMA();
      display.endWrite();
    }
    performance.workUs += esp_timer_get_time() - workStarted;
    if (result < 0 || ioFailed || unsupportedDisposal) {
      Serial.printf("Erro GIF: decoder=%d IO=%d disposal=%d\n",
                    gif.getLastError(), ioFailed, frame.disposal);
      message("GIF nao suportado", unsupportedDisposal ? "Exporte frames completos" : "Verifique arquivo / SD");
      waitUntil(millis(), 1500);
      break;
    }
    const bool revealFrame = firstFrame;
    if (firstFrame) {
      // Primeiro frame opaco e de tela cheia: o loop nao precisa limpar o canvas.
      firstFrameCovers = !frameTransparent && frame.x == 0 && frame.y == 0 &&
                         frame.w == width && frame.h == height;
      firstFrame = false;
    }
    ++frames;
    const uint32_t duration = frameDuration(frameDelay);
    if (millis() - started > duration) ++lateFrames;
    if (revealFrame) {
      // O primeiro quadro ja terminou o DMA: so agora revela a imagem inteira.
      const uint32_t fadeStarted = millis();
      const bool skipped = fadeBrightness(config::brightness, config::fadeInMs, true);
      const uint32_t fadeElapsed = millis() - fadeStarted;
      // O fade nao entra no tempo minimo nem no FPS medido da animacao.
      shownAt += fadeElapsed;
      loopStarted += fadeElapsed;
      if (skipped) break;
    }
    if (waitUntil(started, duration)) break;
    if (result == 0) {
      const uint32_t now = millis();
      const uint32_t loopMs = now - loopStarted;
      Serial.printf("%s: %u frames em %lu ms (%.1f fps), %u atrasados\n", name, frames,
                    static_cast<unsigned long>(loopMs),
                    loopMs ? frames * 1000.0f / loopMs : 0.0f, lateFrames);
      const uint64_t accounted = performance.sdUs + performance.drawUs;
      const uint64_t decoderUs = performance.workUs > accounted
          ? performance.workUs - accounted : 0;
      Serial.printf("PERF media/frame: SD=%.1f ms, desenho/espera DMA=%.1f ms, "
                    "decoder/outros=%.1f ms; lidos=%llu bytes, leituras=%lu, setores=%lu, envios=%lu; LCD=%lu MHz\n",
                    performance.sdUs / (1000.0 * frames),
                    performance.drawUs / (1000.0 * frames), decoderUs / (1000.0 * frames),
                    static_cast<unsigned long long>(performance.bytes),
                    static_cast<unsigned long>(performance.reads),
                    static_cast<unsigned long>(performance.diskReads),
                    static_cast<unsigned long>(performance.sends),
                    static_cast<unsigned long>(config::displayFrequency / 1000000));
      if (config::playOnce ||
          (config::secondsPerGif > 0 && now - shownAt >= config::secondsPerGif * 1000UL)) break;
      // Recomeca sem reabrir o arquivo nem apagar a tela: evita a piscada no loop.
      gif.reset();
      if (!firstFrameCovers) display.fillRect(gifX, gifY, width, height, background);
      frame = {};
      loopStarted = now;
      frames = lateFrames = 0;
      performance = {};
    }
  }
  gif.close();
}
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(config::sdCs, OUTPUT);
  digitalWrite(config::sdCs, HIGH);
  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH);
  display.init();
  display.setRotation(config::rotation);
  display.setBrightness(config::brightness);
  display.invertDisplay(config::invertColors);
  // GPIO36 nao tem pull-up interno; usa o circuito PENIRQ da placa.
  pinMode(config::touchIrq, INPUT);
  esp_timer_create_args_t timerArgs = {};
  timerArgs.callback = sampleTouch;
  timerArgs.name = "touch-button";
  const esp_err_t created = esp_timer_create(&timerArgs, &touchTimer);
  const esp_err_t started = created == ESP_OK
      ? esp_timer_start_periodic(touchTimer, 5000) : created;
  touchTimerReady = started == ESP_OK;
  Serial.printf("Touch IRQ GPIO%d=%d; timer=%s\n", config::touchIrq,
                digitalRead(config::touchIrq), esp_err_to_name(started));
  sdSpi.begin(config::sdSck, config::sdMiso, config::sdMosi, config::sdCs);
  message("DISPLAY PINBALL", "GIF player / SD");
  Serial.println("Toque para avancar; n=proximo GIF; t=diagnostico touch; m=memoria.");
  printMemory();
}

void loop() {
  if (!sdReady) {
    sdReady = SD.begin(config::sdCs, sdSpi, config::sdFrequency);
    if (!sdReady) {
      SD.end();
      message("Insira um SD FAT32", "Tentando novamente...");
      waitUntil(millis(), 2000);
      return;
    }
    Serial.println("SD inicializado.");
    for (const char* name : config::files) {
      Serial.printf("%s: %s\n", name, SD.exists(name) ? "OK" : "AUSENTE");
    }
  }
  playCurrent();
  current = (current + 1) % config::fileCount;
  if (ioFailed) {
    SD.end();
    sdReady = false;
  }
  delay(1);
}
