# Módulo `interrupts`

**Estado: implementado (Fase 4).**

`interrupt_controller.hpp` — I_STAT (1F801070h) e I_MASK (1F801074h): pedidos por borda via
`request(Irq)`, reconhecimento escrevendo 0, máscara de 11 bits. Implementa a linha de
interrupção da CPU: `(I_STAT AND I_MASK) != 0` → CAUSE bit 10.

As fontes (VBLANK, timers, DMA, CD-ROM, GPU, SPU, SIO) chamarão `request()` quando forem
implementadas nas fases seguintes.
