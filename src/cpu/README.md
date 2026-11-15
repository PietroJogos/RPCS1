# Módulo `cpu`

**Estado: interpretador (Fase 3) + COP0, exceções, interrupções e I-cache (Fase 4). GTE: Fase 9.**

- `instruction.hpp` — campos das instruções MIPS I e tabelas de opcodes.
- `cpu.hpp` — interpretador do R3000A: instruções sem coprocessador, delay slots, load delay,
  MULT/DIV com latência, despacho de exceções, interrupções, modo usuário, verificação de CU0–CU3,
  isolamento de cache nos acessos de dados.
- `cop0.hpp` — registradores do COP0 (SR, CAUSE, EPC, BadVaddr, TAR, PRID, depuração), RFE e
  entrada em exceção.
- `icache.hpp` — I-cache de 4 KiB (tag física, preenchimento até o fim da linha) e os acessos
  com a cache isolada.
- `interrupt_line.hpp` — interface da linha de interrupção (CAUSE bit 10).
- `disassembler.hpp` — texto legível das instruções.

Limites atuais: COP2/GTE para a CPU como "não implementado"; breakpoints de hardware não
disparam; ciclos não incluem tempos de memória nem da cache.
