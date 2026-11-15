# Módulo `timers`

**Estado: implementado (Fase 5).**

`timers.hpp` — os três root counters (1F801100h–1F80112Fh): COUNT, MODE e TARGET; fontes sistema,
dotclock (timer 0), HBLANK (timer 1) e sistema ÷ 8 (timer 2); reset no alvo (período alvo + 2 no
clock do sistema), IRQ em FFFFh/alvo, pulso/alternância, uma vez/repetido, flags 11/12 com as regras
de leitura medidas e os quatro modos de sincronização com HBLANK/VBLANK. Gera IRQ4/5/6.

Implementação "preguiçosa": o contador é atualizado só quando lido, escrito, numa borda de
blank ou no evento agendado em que atinge alvo/FFFFh.

Divergências conhecidas em relação a medições: ver `docs/DECISIONS.md`, D-033.
