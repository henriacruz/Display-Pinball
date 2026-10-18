# Display Pinball — player de GIFs

Projeto PlatformIO + Arduino para ESP32-32E com LCD ST7796 de 3,5 polegadas
(320x480), touch resistivo XPT2046 e leitor microSD.
O perfil de hardware e ESP32-3248S035R; a identificacao exata da placa ainda
nao foi confirmada. A pinagem coincide com o codigo fornecido.

## Uso

O conjunto aprovado esta em `gifs/otimizados/conjunto-suavizacao-leve/`:
320x480, 15 fps, 64 cores e suavizacao leve. Os GIFs originais ficam em `gifs/`.
`video5.gif` esta preservado como original, mas fora da lista de reproducao.

1. Prepare um microSD em FAT32 e copie `video1.gif` ate `video4.gif` para a raiz.
2. Use GIFs de ate 320x480 em modo retrato. 270x480 tambem funciona, centralizado.
3. Insira o cartao e ligue a placa. Cada GIF toca uma unica vez e passa ao proximo; o toque permite avancar antes do fim.
4. Solte o dedo antes do proximo toque. Depois do quarto arquivo, volta ao primeiro.

Arquivos menores sao centralizados, sem ampliar. Imagens maiores sao rejeitadas
com mensagem para redimensionar; nao ha redimensionamento em tempo real.
Arquivos ausentes, invalidos ou nao suportados sao pulados depois de 1,5 segundo.
Se todos estiverem ausentes, o player continua percorrendo a lista.
Sem SD, tenta montar o cartao novamente a cada 2 segundos.
Prefira desligar a placa antes de trocar/remover um cartao em uso.

## Configuracao

Edite `include/PlayerConfig.hpp`:

- `rotation`: 0 ou 2 para retrato; 1 ou 3 para paisagem (480x320).
- `playOnce`: `true` por padrao; avanca ao terminar a primeira reproducao,
  ignorando `secondsPerGif`. O toque sempre permite avancar.
- `secondsPerGif`: com `playOnce = false`, tempo minimo de cada GIF (15 s); ao fim da repeticao
  seguinte avanca sozinho. `0` repete ate tocar. O toque sempre avanca.
- `speedPercent`: velocidade da animacao; 100 = original, 150 = 1,5x, 200 = 2x.
- `minFrameDelayMs`: tempo minimo por frame (padrao 20 ms).
- `displayFrequency`: SPI do display, 80 MHz em teste de desempenho; use 40 MHz se houver
  ruido ou cores erradas.
- `invertColors`: inverte as cores do painel se necessario.
- `brightness`: intensidade do backlight, de 0 a 255.
- `fadeOutMs` / `fadeInMs`: transicao pelo backlight (150 / 200 ms).
  O ultimo quadro escurece; o proximo e carregado e desenhado com brilho zero,
  depois clareia. O carregamento acrescenta tempo ao intervalo escuro.
  `0` desativa a respectiva rampa. O fade nao entra na medicao de FPS.
  Toques capturados ao escurecer ficam pendentes; durante a entrada podem pular
  para o proximo GIF. Avisos de SD/arquivo restauram o brilho para serem visiveis.
- `files`: lista de nomes dos GIFs na raiz do cartao.
- `sdFrequency`: 25 MHz solicitados ao driver (limite da versao Arduino usada);
  pode reduzir para 20 ou 10 MHz se houver falhas no SD. O clock efetivo depende
  dos divisores do SPI. O ganho medido sobre 20 MHz foi inferior a 1%.

O monitor serial usa 115200 baud. Envie `n` para avancar sem tocar na tela e
`t` para consultar GPIO36 (1=solto, 0=pressionado) e o estado do temporizador.
Envie `m` para consultar RAM interna livre, maior bloco contiguo, minimo livre
e PSRAM detectada pelo firmware. O diagnostico tambem aparece na inicializacao
e ao abrir cada GIF. O Turbo permanece desativado; em 320x480, a biblioteca
instalada exige um bloco adicional de 178.432 bytes para esse modo.
Cada toque aceito registra `TOQUE: avancando GIF`.
O player detecta pressao e nao precisa de calibracao de coordenadas.
A demonstracao anterior, com desenho e calibracao pelo comando `c`, foi
preservada em `examples/touch_demo/main.cpp` (fora da compilacao).

## Compilar e gravar

O ambiente PlatformIO usa `-O2` para priorizar velocidade no aplicativo e nas
bibliotecas compiladas a partir das fontes, incluindo AnimatedGIF. Remove `-Os`.
Componentes precompilados do ESP-IDF nao sao alterados por essa configuracao.

No terminal do PlatformIO:

```sh
pio run
pio run --target upload --upload-port COM7
pio device monitor --port COM7 --baud 115200
```

Se `pio` nao estiver no PATH do PowerShell:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

## Hardware

LCD e touch compartilham HSPI; SD usa VSPI independente.
O player consulta PENIRQ (GPIO36) a cada 5 ms em uma tarefa de temporizador,
inclusive durante a decodificacao. Nao usa SPI para detectar o toque.

| Sinal | GPIO |
| --- | --- |
| LCD/touch SCLK | 14 |
| LCD/touch MOSI | 13 |
| LCD/touch MISO | 12 |
| LCD CS / DC | 15 / 2 |
| Backlight | 27 |
| Touch CS | 33 |
| SD CS / SCLK | 5 / 18 |
| SD MISO / MOSI | 19 / 23 |

## Adaptacao do codigo recebido

O trecho enviado continha copias repetidas. A implementacao usa uma unica
versao com LovyanGFX, conservando a configuracao do projeto.

- Leitura limitada ao tamanho real do arquivo, sem descartar o ultimo byte.
- Leitura com cache de um setor de 512 bytes alinhado ao offset do arquivo,
  limitado ao tamanho real no ultimo setor. Pedidos pequenos usam o cache;
  seeks logicos so reposicionam o arquivo fisico ao carregar outro setor,
  se necessario. O cache e invalidado ao abrir/fechar cada arquivo.
- Posicao logica atualizada pelos bytes realmente entregues ao decoder,
  sem consultar `File.position()` em cada callback; EOF e leituras incompletas
  continuam verificados.
- RGB565 em ordem nativa para a LovyanGFX.
- Um avanco por toque: 30 ms de pressao e 60 ms de liberacao para rearmar.
- Transacoes do LCD limitadas a um frame; toque capturado pelo temporizador.
- Temporizacao desconta o tempo de decodificacao; intervalo minimo de 20 ms.
- Repeticao reinicializa o decoder e limpa o canvas somente se o primeiro
  frame nao cobrir a imagem inteira de forma opaca.
- Transparencia preservada; disposal 2 restaura o retangulo anterior para a cor
  de fundo global depois do tempo de exibicao, antes do proximo frame.

Nao usa framebuffer completo nem exige PSRAM. GIFs com disposal 3
(restaurar imagem anterior) sao rejeitados explicitamente: exporte-os com frames
completos e disposal 1 ou 2. Paletas locais sao aceitas. O tempo de resposta ao
toque inclui a decodificacao de um frame; GIFs pesados podem responder mais devagar.
O loop embutido no GIF e ignorado. Com `playOnce = true`, cada arquivo toca uma
unica vez. Com `playOnce = false`, a repeticao usa `secondsPerGif`. Ao repetir, o GIF
recomeca sem reabrir o arquivo nem apagar a tela; o canvas so e limpo se o primeiro
frame nao cobrir a imagem inteira.

## Desempenho

As linhas sao enviadas em blocos por DMA com buffer duplo: o ESP32 decodifica o
proximo bloco enquanto o anterior vai para o display. Ao fim de cada repeticao o
monitor serial mostra, por exemplo, `/video1.gif: 48 frames em 4000 ms (12.0 fps), 3 atrasados`.
"Atrasados" sao frames que levaram mais que o tempo pedido pelo GIF; se forem muitos,
a placa esta no limite e o GIF parece lento. Para ficar mais fluido:

- compare GIFs opacos com versoes que atualizam so as areas alteradas;
  transparencia muito fragmentada pode gerar muitos envios pequenos e piorar
  o desempenho. O conjunto aprovado usa quadros opacos sem dithering;
- use 15-20 fps; mais que isso a placa nao acompanha em tela cheia;
- teste `displayFrequency = 80000000`.

Uma segunda linha `PERF` mostra medias por frame: tempo nos callbacks de leitura
e seek do SD, tempo no desenho/conversao e espera do DMA, e o restante no decoder
e controle. Tambem informa bytes lidos, numero de leituras e envios ao display.
O DMA pode trabalhar durante leitura/decodificacao; os valores representam tempo
observado no processador, nao tempos independentes de ocupacao dos barramentos.
A instrumentacao acrescenta uma pequena sobrecarga e exclui a abertura e limpeza
inicial do arquivo.
`leituras` conta callbacks do decoder; `setores` conta chamadas de leitura
de blocos ao arquivo SD (nao necessariamente transacoes fisicas do cartao).
`lidos` conta bytes entregues ao decoder, excluindo antecipacao pelo cache.

A compilacao nao verifica a exibicao fisica nem a compatibilidade dos seus GIFs:
essas verificacoes exigem a placa e o cartao com os arquivos.

Na ultima medicao com os quatro GIFs aprovados, o player atingiu 9,1 / 8,9 /
9,7 / 8,8 fps, respectivamente. Configuracao: compilacao `-O2`, LCD a 80 MHz,
SD solicitado a 25 MHz, cache alinhado de 512 bytes, reproducao unica e fades
de 150/200 ms. Ainda ha frames atrasados: 15 fps e a taxa solicitada pelos GIFs,
nao a taxa efetiva medida. Historico e limites em `tools/performance-notes.md`.

## Preparacao dos GIFs

Os scripts Python usam Pillow e o FFmpeg fornecido por imageio-ffmpeg, instalados
localmente em `.tools/gif-processing` (pasta fora do Git):

```sh
python -m pip install --target .tools/gif-processing pillow imageio-ffmpeg
python tools/prepare_gifs.py
python tools/smooth_gifs.py
```

O primeiro gera `lote-v2`; o segundo gera `lote-v3`. Os quatro GIFs aprovados
correspondem a `lote-v3/leve/`, reunidos em `conjunto-suavizacao-leve/`.
Versoes usadas na preparacao: Pillow 13.0.0 e imageio-ffmpeg 0.6.0.
O diagnostico serial pode ser capturado com
`python tools/serial_check.py --port COM8 --seconds 50` (requer pyserial).

## Referencias

- [AnimatedGIF — biblioteca e API](https://github.com/bitbank2/AnimatedGIF)
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [ESP32 Dev Module no PlatformIO](https://docs.platformio.org/en/stable/boards/espressif32/esp32dev.html)
