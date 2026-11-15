# Módulo `gpu`

**Estado: registradores, VRAM, temporização de vídeo (Fases 5 e 7) e renderização por software
(Fase 8) implementados.**

- `vram.hpp` — VRAM de 1 MiB (1024 × 512 pixels de 16 bits) com volta nas bordas, conversão
  RGB555 → RGB888 e cor de comando (24 bits) → RGB555.
- `gpu.hpp` — portas GP0/GPUREAD (1F801810h) e GP1/GPUSTAT (1F801814h):
  - montagem dos comandos GP0 com o número exato de palavras de cada um (o FIFO continua
    alinhado mesmo com comandos ainda não implementados), polilinhas com o marcador 5000_5000h;
  - preenchimento (02h), cópia VRAM→VRAM (80h), CPU→VRAM (A0h), VRAM→CPU (C0h) com as regras de
    máscara de GP0(E6h); atributos E1h–E6h; IRQ1 por GP0(1Fh);
  - todos os comandos GP1, inclusive GP1(10h) da GPU v2; GPUSTAT completo;
  - porta do canal 2 de DMA com o DREQ conforme GP1(04h) (medido no SCPH-5501);
  - parâmetros de exibição enviados a `VideoTiming` e imagem da área de exibição (15 e 24 bits).
  - Comandos de desenho 20h–7Fh decodificados e enviados ao rasterizador.
  - **Não implementado:** tempo de execução dos comandos e limite real do FIFO (D-040).
- `renderer.hpp` — rasterizador por software: triângulos (regra de preenchimento medida),
  quadriláteros, linhas e polilinhas, retângulos 1×1/8×8/16×16/variáveis, gouraud, texturas de
  4/8/15 bits com CLUT, janela de textura, modulação, semitransparência, dithering, bit de máscara,
  área e deslocamento de desenho (D-044 a D-047).
- `video_timing.hpp` — gerador de HSYNC, HBLANK, VBLANK, IRQ0 e dotclock, com o cristal da região
  do console; recebe X1/X2/Y1/Y2, modo e divisor da GPU (aplicados no próximo HSYNC).
