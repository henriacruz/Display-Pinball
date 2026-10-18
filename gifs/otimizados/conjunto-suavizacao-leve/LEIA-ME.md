# Conjunto com suavizacao leve

Quatro GIFs da lista ativa, todos com o mesmo tratamento do video2 testado:
320x480, 15 fps, 64 cores, suavizacao gaussiana sigma 0,6, sem dithering e
sem transparencia fragmentada. Disposal 1. Fontes originais preservadas em `gifs/`.
video5 continua fora da lista ativa e deste conjunto.

Copie somente `video1.gif`, `video2.gif`, `video3.gif` e `video4.gif` desta pasta
para a raiz do cartao, substituindo os de mesmo nome. Nao precisa alterar o
firmware. Ejetar pelo Windows e inserir o cartao no ESP32 desligado.

Todos os quadros foram decodificados e conferidos; hashes SHA-256, duracoes,
quantidades de quadros e tamanhos estao em `verificacao.json`.
As duracoes sao identicas as do lote v2; diferem no maximo 33 ms dos originais.

O conjunto foi aprovado visualmente na placa. No teste inicial isolado, video2
atingiu 7,7-7,8 fps, contra 7,4 fps sem suavizacao. video1, video3 e video4 ficam
ligeiramente maiores que no lote v2; nao foi isolado um ganho da suavizacao para
esses arquivos. Com as otimizacoes posteriores do firmware, o conjunto completo
atingiu 9,1 / 8,9 / 9,7 / 8,8 fps, respectivamente.
O tratamento reduz nitidez e nao muda os pontos de corte dos loops.
O firmware atual faz a transicao por fade do backlight. Medicoes e limites em
`tools/performance-notes.md`.

Para voltar ao conjunto anterior sem suavizacao, use os quatro GIFs de
`gifs/otimizados/lote-v2/64cores/`, regenerados por `tools/prepare_gifs.py`.

Candidatos produzidos por `tools/smooth_gifs.py`, reunidos aqui sem reconversao.
