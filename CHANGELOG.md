# Changelog

Formato baseado em "Keep a Changelog". Versões seguem SemVer; antes da 0.1.0 todas são pré-lançamentos.

## [0.0.3] — 15/11/2026 — pré-alfa (Fase 3)

### Adicionado
- Interpretador da CPU R3000A: ALU, deslocamentos, loads/stores (inclusive LWL/LWR/SWL/SWR),
  desvios e saltos com delay slot, load delay slot, MULT/DIV com latência documentada.
- Detecção de exceções (Ov, AdEL, AdES, IBE, DBE, Sys, Bp, RI) com EPC/BD/BT/TAR/BadVaddr.
- Desmontador MIPS para logs.
- A BIOS configurada é executada até a primeira instrução de coprocessador (limite atual).
- Codificador MIPS e gerador de BIOS sintética para testes; 56 testes de CPU.

### Não incluído
- COP0, despacho de exceções e interrupções (Fase 4); GTE; vídeo; áudio; periféricos.

## [0.0.2] — 15/11/2026 — pré-alfa (Fase 2)

### Adicionado
- Barramento de memória: segmentos KUSEG/KSEG0/KSEG1/KSEG2, acessos de 8/16/32 bits,
  erros de alinhamento e de barramento.
- RAM de 2 MiB com janela e espelhos pelo DRAM_CTRL; scratchpad de 1 KiB; BIOS ROM de 512 KiB.
- Memory Control (`1F801000h–1F801020h`, `1F801060h`, `FFFE0130h`) com janelas das regiões.
- Regiões de expansão e registro do display POST (`1F802041h`).
- Interface de periféricos de E/S com marcadores explícitos para os ainda não implementados.
- `[BIOS] path` é carregado, validado e mapeado (não executado).
- 38 testes unitários e 1 smoke test novos.

### Corrigido
- `CHECK_EQ` do framework de testes podia ler memória liberada.

### Não incluído
- CPU e demais periféricos: nada é executado.

## [0.0.1] — 15/11/2026 — pré-alfa (Fase 0)

### Adicionado
- Estrutura modular do projeto e build CMake/C++20 para Windows x64.
- Logging com níveis, categorias, destinos console/arquivo/memória.
- `Result<T>`/`Status`, asserções e registro de falhas fatais.
- Parser INI e configurações validadas (`rpcs1.ini`).
- Linha de comando (`--help`, `--version`, `--config`, `--log-level`, `--headless`, `--exit-after`).
- Janela Win32 com apresentação de framebuffer 4:3 (tela preta: nada é emulado ainda).
- Framework de testes próprio, 61 testes unitários e 6 testes de fumaça.
- Documentação técnica do hardware do PS1 (Fase 1), roadmap, progresso e decisões.

### Não incluído
- Qualquer emulação de hardware do PlayStation.
