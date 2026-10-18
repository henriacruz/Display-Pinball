# Medicoes na placa

ESP32 em COM8, display ST7796, SD a 20 MHz, speedPercent=100.
video2 no cartao: versao 15 fps/256 cores; demais GIFs originais.

| Configuracao | GIF | FPS | Duracao da volta | Atrasados |
| --- | --- | ---: | ---: | --- |
| LCD 40 MHz, sem instrumentacao | video1 | 5,2 | 29.229 ms | 152/153 |
| LCD 40 MHz, sem instrumentacao | video2 | 5,1 | 14.838 ms | 75/75 |
| LCD 80 MHz, com instrumentacao | video1 | 5,7 | 26.960 ms | 152/153 |
| LCD 80 MHz, com instrumentacao | video2 | 5,2 | 14.549 ms | 75/75 |
| LCD 80 MHz, cache experimental de 8 KiB | video1 | 5,1 | 29.869 ms | 152/153 |
| LCD 80 MHz, cache experimental de 8 KiB | video2 | 4,3 | 17.435 ms | 75/75 |

Tempos medios por frame a 80 MHz, sem cache:

| GIF | Callbacks SD | Desenho e espera DMA | Decoder/outros | Envios LCD por volta |
| --- | ---: | ---: | ---: | ---: |
| video1 | 56,3 ms | 78,5 ms | 40,2 ms | 2.409.768 |
| video2 | 112,5 ms | 12,9 ms | 67,4 ms | 2.250 |

Tempos nao representam ocupacao independente dos barramentos: DMA se sobrepoe
ao processamento. Decoder/outros e o tempo de trabalho restante, excluindo os
callbacks medidos. Abertura e limpeza inicial nao entram nas medias.

O cache de 8 KiB foi removido por piorar o tempo total nesta medicao.
80 MHz deu ganho pequeno; estabilidade visual exige observar a tela fisica.
Nenhum erro de GIF/SD foi registrado nas voltas completas medidas.
Os numeros sao amostras, nao um benchmark controlado com repeticoes extensas.

Proximos candidatos: reduzir fragmentacao de transparencia nos GIFs originais;
reduzir bytes por frame do video2 e medir custo de leitura/decodificacao;
evitar limpeza inicial desnecessaria na troca (questao visual separada do FPS).

## Segundo lote: 64 cores, 15 fps, sem transparencia fragmentada

Mesma configuracao do firmware: LCD 80 MHz, SD 20 MHz, sem cache adicional.
Arquivos de `gifs/otimizados/lote-v2/64cores` no cartao. Voltas completas medidas:

| GIF | Frames | Duracao real | FPS real | Atrasados | SD/frame | Desenho/frame | Decoder/outros/frame | Envios/volta |
| --- | ---: | ---: | ---: | --- | ---: | ---: | ---: | ---: |
| video1 | 78 | 9.757 ms | 8,0 | 77/78 | 67,1 ms | 11,6 ms | 44,2 ms | 2.231 |
| video2 | 75 | 10.194 ms | 7,4 | 75/75 | 73,7 ms | 12,6 ms | 48,3 ms | 2.250 |
| video3 | 82 | 9.779 ms | 8,4 | 82/82 | 63,3 ms | 11,9 ms | 42,7 ms | 2.316 |
| video4 | 93 | 12.163 ms | 7,6 | 93/93 | 69,9 ms | 12,6 ms | 47,1 ms | 2.781 |

video1 repetiu em 9.766 ms (8,0 fps); video4 em 12.179 ms (7,6 fps).
Contra a medicao anterior a 80 MHz, video1 subiu de 5,7 para 8,0 fps
(aproximadamente 40%), e video2 de 5,2 para 7,4 fps (aproximadamente 42%).
Nao ha baseline anterior registrado para video3/video4.
Nenhum erro de SD ou GIF apareceu nessas amostras.
A leitura e a decodificacao ainda impedem atingir 15 fps; as animacoes duram
aproximadamente o dobro do planejado. Melhorar a troca visual entre arquivos
nao corrige esse limite de processamento.

## Suavizacao leve: somente video2

Mesmo firmware e parametros do segundo lote; video2 substituido pelo candidato
`lote-v3/leve/video2.gif` (5.618.376 bytes, sigma 0,6).

| Volta | Frames | Duracao real | FPS real | Atrasados | SD/frame | Desenho/frame | Decoder/outros/frame |
| --- | ---: | ---: | ---: | --- | ---: | ---: | ---: |
| 1 | 75 | 9.665 ms | 7,8 | 75/75 | 68,9 ms | 12,4 ms | 46,3 ms |
| 2 | 75 | 9.679 ms | 7,7 | 75/75 | 68,9 ms | 12,5 ms | 46,3 ms |

44.029 leituras e 2.250 envios por volta, sem erros registrados nessas voltas.
Contra 10.194 ms no lote v2, a media de 9.672 ms reduz o tempo em cerca de 5,1%
e aumenta a taxa efetiva em cerca de 5,4%. O ganho e pequeno, com perda de nitidez.
video1, sem alteracao, permaneceu em 8,0 fps (9.773 e 9.785 ms).
Nao justifica prometer 15 fps nem estender automaticamente a suavizacao aos demais.

## RAM disponivel para Turbo

Diagnostico em execucao, com GIFs e SD ativos, Turbo desativado:

- RAM interna livre: 188.060 bytes em video1; 187.784 bytes ao abrir video2.
- Maior bloco interno de 8 bits: 110.580 bytes em ambas as consultas.
- Minimo livre: 182.496 / 182.152 bytes.
- PSRAM detectada pelo firmware: 0 bytes.
- `allocTurboBuf` do AnimatedGIF 2.2.3 exige bloco unico de
  `320 * 480 + 0x6100 = 178.432` bytes.

Conclusao: o Turbo nao cabe por alocacao dinamica na organizacao atual.
Apesar de o total livre superar a alocacao, o maior bloco e menor em 67.852 bytes.
Isso nao comprova ausencia fisica de PSRAM, somente que nao foi detectada/habilitada
por este firmware. Reorganizar buffers pode ser investigado, sem garantia de caber.
Comando serial `m` permite repetir a consulta; o firmware tambem mostra a memoria
ao iniciar e abrir cada GIF.

## Callbacks sem consulta de posicao apos leitura/seek

GIFs com suavizacao leve em todos os quatro arquivos, LCD 80 MHz, SD 20 MHz,
playOnce e fade ativos em ambas as medicoes. Unica alteracao: iPos avanca pelos
bytes realmente lidos e recebe a posicao solicitada apos seek confirmado, sem
chamar File.position(). Sem novo buffer ou alocacao.

| GIF | Antes: volta / FPS | Depois: volta / FPS | SD antes/frame | SD depois/frame |
| --- | --- | --- | ---: | ---: |
| video1 | 9.804 ms / 8,0 | 9.429 ms / 8,3 | 66,9 ms | 61,7 ms |
| video2 | 9.636 ms / 7,8 | 9.270 ms / 8,1 | 68,6 ms | 63,5 ms |
| video3 | 9.707 ms / 8,4 | 9.344 ms / 8,8 | 62,7 ms | 58,0 ms |
| video4 | 12.156 ms / 7,7 | 11.691 ms / 8,0 | 69,7 ms | 64,4 ms |

A taxa efetiva aumentou aproximadamente 4% nos quatro arquivos. A leitura caiu
4,7-5,3 ms/frame. Frames, bytes lidos e envios por volta se mantiveram iguais.
Todos os GIFs completaram uma volta e avancaram sem erros de SD/decoder no log.
Maior bloco livre e RAM livre se mantiveram iguais; nao resolve o limite do Turbo.
Frames atrasados continuam em 77/78, 75/75, 82/82 e 93/93 respectivamente.
Logs brutos: `tools/position-before.log` e `tools/position-after.log`.
Resultados sao amostras de placa real, nao garantia de mesmo ganho com outro cartao.

## Compilacao para velocidade: -O2 em lugar de -Os

Aplicativo e bibliotecas compiladas de fontes, incluindo AnimatedGIF, LovyanGFX
e partes Arduino, usam -O2. Componentes ESP-IDF precompilados permanecem iguais.
Confirmado no comando de compilacao de AnimatedGIF.cpp: -O2 presente, -Os ausente.
GIFs, SD 20 MHz, LCD 80 MHz, fade e reproducao unica mantidos.

| GIF | Antes: volta / FPS | Depois: volta / FPS | SD antes/depois por frame | Desenho antes/depois por frame |
| --- | --- | --- | --- | --- |
| video1 | 9.436 ms / 8,3 | 8.739 ms / 8,9 | 61,8 / 57,8 ms | 11,7 / 7,3 ms |
| video2 | 9.274 ms / 8,1 | 8.595 ms / 8,7 | 63,5 / 59,7 ms | 12,6 / 7,5 ms |
| video3 | 9.345 ms / 8,8 | 8.668 ms / 9,5 | 58,0 / 54,4 ms | 11,9 / 7,7 ms |
| video4 | 11.691 ms / 8,0 | 10.870 ms / 8,6 | 64,4 / 60,5 ms | 12,6 / 8,0 ms |

Ganho na taxa efetiva de aproximadamente 7,6-8,0%. O tempo residual do decoder
ficou praticamente igual; nao atribuir o ganho apenas ao decoder.
Frames, bytes lidos e envios se mantiveram iguais, sem erros nas voltas medidas.
RAM estatica: 79.004 bytes, sem aumento. Flash do programa: 430.113 bytes,
contra 418.177 bytes antes (+11.936 bytes). RAM livre e maior bloco inalterados.
Todos os arquivos ainda apresentam atrasos; nao atingiram os 15 fps previstos.
Logs: `optimization-before.log`, `optimization-after.log`; comandos do compilador
em `optimization-build.log`. O modo --verbose falhou na geracao final por erro
interno de impressao do SCons; compilacao/geracao/gravaçao normal passou.

## Frequencia solicitada ao SD: 20 para 25 MHz

Mesmos GIFs, -O2, LCD 80 MHz, fades e callbacks otimizados.
O valor e o solicitado ao driver, nao uma medicao eletrica do clock SPI.

| GIF | Antes: volta / FPS | Depois: volta / FPS | SD antes/depois por frame |
| --- | --- | --- | --- |
| video1 | 8.741 ms / 8,9 | 8.686 ms / 9,0 | 57,8 / 57,3 ms |
| video2 | 8.596 ms / 8,7 | 8.533 ms / 8,8 | 59,7 / 59,2 ms |
| video3 | 8.668 ms / 9,5 | 8.613 ms / 9,5 | 54,4 / 53,9 ms |
| video4 | 10.871 ms / 8,6 | 10.788 ms / 8,6 | 60,5 / 59,9 ms |

Diferenca pequena: taxa efetiva cerca de 0,6-0,8% maior nesta amostra.
Nao e ganho perceptivel garantido nem benchmark de estabilidade prolongada.
Todos os quatro arquivos completaram uma volta sem erros registrados; frames,
bytes e envios por volta permaneceram iguais. Configuracao de 25 MHz mantida.
Logs: `sd-frequency-before.log` e `sd-frequency-after.log`.
Continuam os mesmos atrasos por arquivo; proximo candidato seria investigar
o custo dos acessos pequenos e alinhamento por setor, separadamente.

## Cache de um setor de 512 bytes alinhado

Cache em RAM estatica; seeks do decoder atualizam a posicao logica e o arquivo
so e reposicionado ao recarregar outro setor, quando necessario. Cache invalidado
por arquivo, leitura do ultimo setor limitada ao EOF. Bytes entregues e erros
de leitura continuam contabilizados. SD 25 MHz, LCD 80 MHz, -O2, mesmos GIFs.

| GIF | Antes: volta / FPS | Depois: volta / FPS | SD antes/depois por frame | Callbacks / leituras de setor |
| --- | --- | --- | --- | --- |
| video1 | 8.685 ms / 9,0 | 8.558 ms / 9,1 | 57,3 / 55,2 ms | 45.542 / 11.352 |
| video2 | 8.533 ms / 8,8 | 8.415 ms / 8,9 | 59,2 / 57,2 ms | 44.029 / 10.974 |
| video3 | 8.612 ms / 9,5 | 8.486 ms / 9,7 | 53,9 / 52,1 ms | 43.848 / 10.927 |
| video4 | 10.787 ms / 8,6 | 10.626 ms / 8,8 | 59,9 / 57,8 ms | 56.225 / 14.016 |

Ganho efetivo pequeno, cerca de 1,4-1,5%. Leituras de setor sao chamadas ao
arquivo, nao transacoes eletricas medidas no cartao. Cerca de 75% menos chamadas
File.read(), mas o restante do trabalho limita o ganho total.
RAM estatica passou de 79.004 para 79.548 bytes (+544); programa passou de
430.109 para 430.449 bytes. RAM livre em execucao caiu pelos mesmos 544 bytes;
maior bloco continua 110.580 bytes. Frames, bytes entregues e envios por volta
iguais ao baseline. Todos os quatro GIFs completaram uma volta sem erros;
sequencia retornou ao video1. Atrasos por arquivo permanecem iguais.
Logs: `sector-before.log` e `sector-after.log`. Cache mantido pelo pequeno ganho.
