# Changelog

Formato baseado em "Keep a Changelog". Versões seguem SemVer; antes da 0.1.0 todas são pré-lançamentos.

## [0.0.8] — 15/11/2026 — pré-alfa (Fase 8)

### Adicionado
- Rasterizador por software da GPU: triângulos, quadriláteros, linhas, polilinhas e retângulos;
  gouraud; texturas de 4/8/15 bits com CLUT e janela de textura; modulação; semitransparência;
  dithering; bit de máscara; área e deslocamento de desenho.
- Regras conferidas contra medições de um SCPH-5501 (psx.wiki): preenchimento de bordas, limites
  de 1023 × 511, dithering, interpolação de UV, semitransparência, janela de textura.
- Programa de teste `rpcs1_make_test_bios --gpu-draw` e teste `smoke.gpu_draw`; 21 testes novos.

### Alterado
- Os comandos de desenho deixaram de ser descartados; o log mostra primitivas e pixels desenhados.
- O programa de teste da Fase 7 define a área de desenho e desenha um triângulo amarelo.

### Limitações
- Cache de textura, desenho na área exibida em modo entrelaçado e tempo de desenho não emulados
  (D-046, D-047).

## [0.0.7] — 15/11/2026 — pré-alfa (Fase 7)

### Adicionado
- GPU: GP0/GPUREAD e GP1/GPUSTAT, VRAM de 1 MiB, preenchimento (02h), cópias VRAM→VRAM (80h),
  CPU→VRAM (A0h) e VRAM→CPU (C0h) com máscara (E6h), atributos E1h–E6h, IRQ1, todos os comandos
  GP1 (GPU v2) e GPUSTAT (14802000h após o reset).
- Canal 2 de DMA ligado à GPU, com o DREQ por GP1(04h) medido no SCPH-5501.
- GP1(06h/07h/08h) controlam a temporização de vídeo (modo, entrelaçamento, divisor).
- Imagem da área de exibição (15 e 24 bits) na janela e em BMP com `--dump-frame <arq>`.
- Programa de teste MIPS próprio (`rpcs1_make_test_bios --gpu`) e teste `smoke.gpu_frame`, que
  confere a imagem gerada pixel a pixel e por SHA-256; 26 testes unitários novos.

### Limitações
- Polígonos, linhas e retângulos ainda não são desenhados (Fase 8): são consumidos e descartados
  com aviso no log. Comandos executam instantaneamente (D-040).
- A janela mostra o estado final da execução; não há laço em tempo real (Fase 20).

## [0.0.6] — 15/11/2026 — pré-alfa (Fase 6)

### Adicionado
- Controlador de DMA com os 7 canais: burst, slice, lista encadeada e OTC; DPCR, DICR e IRQ3.
- Regras medidas em um SCPH-5501: máscaras do CHCR, flag do DICR sem o bit 23, escritas por byte.
- Interface `DmaPort` para os dispositivos; marcadores para GPU, MDEC, CD-ROM, SPU e expansão.
- Opção de build `RPCS1_MSVC_MP_JOBS` para máquinas com pouca memória livre.
- 19 testes novos.

## [0.0.5] — 15/11/2026 — pré-alfa (Fase 5)

### Adicionado
- Base de tempo única (ciclos da CPU) com conversão exata para o cristal de vídeo da região.
- Scheduler de eventos despachados antes de cada instrução.
- Temporização de vídeo: HSYNC, HBLANK (GP1(06h)), VBLANK (GP1(07h)) e IRQ0; dotclock por linha.
- Timers 0, 1 e 2 com todas as fontes, alvo/FFFFh, IRQ pulso/alternância, uma vez/repetido,
  flags e sincronização com HBLANK/VBLANK.
- Validação contra medições publicadas de um SCPH-5501; 31 testes novos.

### Alterado
- A BIOS configurada agora roda pelo `Console::run`, com vídeo e timers ativos.
- Tempos de acesso à memória movidos para a Fase 20 (dados insuficientes).

## [0.0.4] — 15/11/2026 — pré-alfa (Fase 4)

### Adicionado
- COP0: SR, CAUSE, EPC, BadVaddr, TAR, PRID e registradores de depuração; MFC0/MTC0/RFE.
- Despacho de exceções aos vetores; interrupções de hardware e de software; modo usuário.
- I-cache (256 linhas, tag física) e isolamento de cache usado pela BIOS para limpá-la.
- Controlador de interrupções (I_STAT/I_MASK) e a classe `Console` que interliga os componentes.
- Opção `--cpu-steps`; 36 testes novos.

### Alterado
- Exceções agora desviam ao tratador em vez de parar a CPU.

### Não incluído
- GTE, timers, DMA, GPU, SPU, CD-ROM e demais periféricos; temporização de memória.

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
