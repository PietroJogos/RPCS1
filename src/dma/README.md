# Módulo `dma`

**Estado: implementado (Fase 6).**

- `dma_controller.hpp` — os 7 canais (MADR, BCR, CHCR e espelho), DPCR, DICR e IRQ3; modos burst,
  slice (um bloco por DREQ, com pausa e retomada) e lista encadeada; OTC (canal 6); prioridade do
  DPCR; erro de barramento fora da RAM; regras de escrita medidas no SCPH-5501.
- `dma_port.hpp` — interface do lado do dispositivo (`DmaPort`: DREQ, leitura, escrita) e o
  `PlaceholderPort` usado enquanto GPU, MDEC, CD-ROM, SPU e a porta de expansão não existem.

Modelo de tempo e limitações: `docs/DECISIONS.md`, D-036 a D-038.
