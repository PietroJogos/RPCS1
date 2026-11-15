# Módulo `cpu`

**Estado: interpretador implementado (Fase 3); COP0/exceções na Fase 4; GTE na Fase 9.**

- `instruction.hpp` — campos das instruções MIPS I e tabelas de opcodes.
- `cpu.hpp` — interpretador do R3000A: ALU, deslocamentos, loads/stores (inclusive
  LWL/LWR/SWL/SWR), desvios e saltos com delay slot, load delay slot, MULT/DIV com HI/LO e
  latência, SYSCALL/BREAK, detecção de exceções (Ov, AdEL, AdES, IBE, DBE, Sys, Bp, RI).
- `disassembler.hpp` — texto legível das instruções (logs e futuro debugger).

Limites atuais: exceções são detectadas mas não desviam ao vetor (a CPU para); instruções de
coprocessador param como "não implementadas"; ciclos não incluem tempos de memória.
Testes em `tests/cpu/`, com o codificador `tests/cpu/mips_assembler.hpp`.
