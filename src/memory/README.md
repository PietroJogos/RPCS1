# Módulo `memory`

**Estado: implementado (Fase 2).**

- `memory_block.hpp` — bloco de memória little-endian (RAM principal de 2 MiB, scratchpad de 1 KiB).
- `bios_rom.hpp` — BIOS ROM de 512 KiB carregada de arquivo do usuário, com validação de tamanho.
- `memory_control.hpp` — registradores Memory Control 1 (`1F801000h–1F801020h`), DRAM_CTRL
  (`1F801060h`) e BCC (`FFFE0130h`), com decodificação das janelas de cada região.

Os campos de atraso são armazenados, mas só afetarão a temporização na Fase 5; os bits do
BCC terão efeito com a I-cache na Fase 4.
