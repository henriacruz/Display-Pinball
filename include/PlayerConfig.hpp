#pragma once
#include <stdint.h>
namespace config {
constexpr unsigned rotation = 0; // 0/2 retrato; 1/3 paisagem.
// Avanca ao fim da primeira reproducao; ignora secondsPerGif quando true.
constexpr bool playOnce = true;
// Quando playOnce=false: tempo minimo; 0 = repete ate tocar.
// A troca espera terminar a repeticao atual para nao cortar a animacao.
constexpr uint32_t secondsPerGif = 15;
// Velocidade em relacao ao GIF original: 100 = normal, 200 = 2x mais rapido.
constexpr unsigned speedPercent = 100;
constexpr uint32_t minFrameDelayMs = 20; // Limite inferior por frame.
constexpr bool invertColors = false;
constexpr unsigned brightness = 180;
// Fade do backlight na troca; 0 desativa a respectiva etapa.
constexpr uint32_t fadeOutMs = 150;
constexpr uint32_t fadeInMs = 200;
// SPI do display. 80 MHz pode aumentar o FPS; volte a 40 MHz se houver ruido.
constexpr uint32_t displayFrequency = 80000000;
constexpr int sdCs = 5, sdSck = 18, sdMiso = 19, sdMosi = 23;
// Frequencia solicitada ao SD; driver Arduino limita a 25 MHz.
constexpr unsigned sdFrequency = 25000000;
constexpr int touchIrq = 36; // XPT2046 PENIRQ: LOW enquanto pressionado.
constexpr const char* files[] = {
    "/video1.gif", "/video2.gif", "/video3.gif", "/video4.gif"};
constexpr unsigned fileCount = sizeof(files) / sizeof(files[0]);
static_assert(speedPercent > 0, "speedPercent deve ser maior que zero");
}
