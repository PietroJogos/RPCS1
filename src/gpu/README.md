# Módulo `gpu`

**Estado: temporização de vídeo implementada (Fase 5). GP0/GP1, VRAM e desenho: Fases 7 e 8.**

- `video_timing.hpp` — gerador de HSYNC, HBLANK, VBLANK, IRQ0 e dotclock, com o cristal da região
  do console, os valores de GP1(00h) e os intervalos de GP1(06h)/GP1(07h). A GPU (Fase 7) passará a
  atualizar esses parâmetros com `set_display_timing`.
