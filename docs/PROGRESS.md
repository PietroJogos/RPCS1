# Progresso do RPCS1

Última atualização: 15/11/2026.

Estados possíveis: **não iniciada**, **em andamento**, **concluída**, **bloqueada**.
Uma fase só é "concluída" com implementação real, testes passando e documentação atualizada.

| Fase | Nome | Estado | Observações |
|---|---|---|---|
| 0 | Fundação do projeto | **concluída** | Build MSVC Debug/Release sem avisos; testes unitários e smoke tests passando |
| 1 | Documentação técnica | **concluída (1ª versão)** | `HARDWARE_ARCHITECTURE.md`; 11 lacunas registradas para fases futuras |
| 2 | Memória e barramento | **concluída** | 38 testes novos; 7 comportamentos não documentados adotados e marcados ⚠️ (D-012 a D-017) |
| 3 | CPU R3000A | **concluída** | Interpretador sem coprocessadores; 56 testes novos; exceções detectadas (despacho na Fase 4) |
| 4 | COP0, exceções, cache | **concluída** | COP0, despacho de exceções, interrupções, I-cache, isolamento e controlador I_STAT/I_MASK; 36 testes novos |
| 5 | Scheduler e timers | **concluída** | Scheduler, temporização de vídeo (IRQ0) e timers validados contra medições do SCPH-5501; 31 testes novos; 2 divergências de 1 linha registradas (D-033); tempos de memória movidos para a Fase 20 |
| 6 | DMA | **concluída** | 7 canais, burst/slice/lista/OTC, DPCR/DICR/IRQ3 validados contra medições do SCPH-5501; 19 testes novos; CPU não paralisada durante o DMA (D-036) |
| 7 | GPU: registradores e VRAM | **concluída** | GP0/GP1, GPUSTAT, VRAM, preenchimento, cópias e transferências (CPU e DMA), exibição 15/24 bits; programa de teste gera imagem conferida pixel a pixel; 26 testes novos; desenho de primitivas descartado com aviso (Fase 8) |
| 8 | GPU: renderização | **concluída** | Rasterizador por software com regras medidas no SCPH-5501 (preenchimento, limites, dithering, UV, semitransparência); 21 testes novos; imagem de teste conferida pixel a pixel e por SHA-256 |
| 9 | GTE | não iniciada | Próxima fase |
| 10 | MDEC | não iniciada | |
| 11 | SPU | não iniciada | |
| 12 | CD-ROM | não iniciada | |
| 13 | Formatos de mídia | não iniciada | |
| 14 | Autenticação e modchip | não iniciada | |
| 15 | Execução da BIOS | não iniciada | Exigirá BIOS fornecida pelo usuário |
| 16 | Boot autêntico (SCPH-5501) | não iniciada | Exigirá BIOS 3.0A do usuário |
| 17 | Fearful Harmony | não iniciada | Nenhum fato técnico confirmado |
| 18 | Controles e SIO | não iniciada | |
| 19 | Memory Cards | não iniciada | |
| 20 | Sincronização | não iniciada | |
| 21 | Compatibilidade | não iniciada | |
| 22 | Interface Windows | não iniciada | |
| 23 | Save states e debug | não iniciada | |
| 24 | Desempenho | não iniciada | |
| 25 | Lançamento 0.1.0 | não iniciada | |

## Fase 0 — o que foi realmente implementado

- **Build:** CMake ≥ 3.24, C++20, presets para Visual Studio 2022 x64, avisos `/W4 /permissive-`
  tratados como erro, manifesto (Windows 10/11, DPI PerMonitorV2, UTF-8, caminhos longos) e
  recurso de versão do executável.
- **Erros:** `Result<T>`/`Status` com códigos de erro e mensagens; asserções sempre ativas;
  tratador de exceções não tratadas (SEH e `std::terminate`) que registra em log.
- **Logging:** níveis Trace→Fatal/Off, 23 categorias por componente com nível individual,
  destinos console/arquivo/memória, seguro entre threads, formatação adiada (mensagens filtradas
  não são formatadas — verificado por teste).
- **Configuração:** parser INI próprio (comentários preservados, aspas, BOM, CRLF, erros com
  número de linha), `Settings` validado (modelo, região, BIOS, escala, log), gravação atômica,
  criação do `rpcs1.ini` padrão na primeira execução.
- **Linha de comando:** `--help`, `--version`, `--config`, `--log-level`, `--headless`,
  `--exit-after`, argumentos em UTF-8 via `GetCommandLineW`.
- **E/S:** leitura com limite de tamanho, escrita atômica (temporário + renomear), nomes Unicode.
- **Janela:** Win32 nativa, DPI-aware, apresentação de framebuffer via GDI com proporção 4:3.
  Exibe uma tela **preta** porque não há GPU emulada — não há imagem falsa.
- **Catálogo de modelos:** 17 modelos SCPH com região e BIOS associada; nenhum perfil de
  hardware implementado (`profile_implemented = false`).

## Fase 2 — o que foi realmente implementado

- **Endereçamento:** KUSEG/KSEG0/KSEG1/KSEG2, tradução para físico, verificação de alinhamento
  (`BusFault::AddressError` → AdEL/AdES) e erro de barramento (`BusFault::BusError` → IBE/DBE)
  para tudo que não existe, inclusive KUSEG acima de 512 MiB e lacunas da área de E/S.
- **RAM principal de 2 MiB** com janela definida pelo DRAM_CTRL (bancos /RAS0 e /RAS1):
  com o padrão `0B88h` surgem os três espelhos documentados; além da janela, erro de barramento.
- **Scratchpad de 1 KiB** apenas em KUSEG/KSEG0, não executável.
- **BIOS ROM de 512 KiB** carregada de arquivo do usuário (tamanho validado), somente leitura,
  janela pelo registrador DEV2 (janela maior espelha a ROM).
- **Memory Control:** 9 registradores de `1F801000h`, DRAM_CTRL e BCC, com bits fixos e janelas
  `1 << N` das Expansões 1/2/3 e da BIOS.
- **Expansões:** sem dispositivo (barramento aberto FFh ⚠️); Expansão 2 só 8 bits, desabilitada
  se a base mudar; registro do display **POST** (`1F802041h`) com log.
- **E/S mapeada:** interface `IoDevice`; as 9 faixas de periféricos usam marcadores explícitos
  `UnimplementedDevice` (leem 0 e avisam no log, citando a fase prevista).
- **Frontend:** `[BIOS] path` agora é carregado e mapeado (sem execução) e o resultado aparece no log.

## Fase 3 — o que foi realmente implementado

- **Interpretador R3000A** (`src/cpu`) com todas as instruções sem coprocessador: ALU e
  imediatos (sinal/zero conforme o psx-spx), deslocamentos (5 bits baixos), LB/LBU/LH/LHU/LW,
  SB/SH/SW, LWL/LWR/SWL/SWR, BEQ/BNE/BLEZ/BGTZ, BcondZ (com link incondicional), J/JAL/JR/JALR,
  MULT/MULTU/DIV/DIVU com HI/LO, MFHI/MFLO/MTHI/MTLO, SYSCALL/BREAK.
- **Delay slot de desvio** e **load delay slot** (inclusive LWL/LWR encadeadas, escrita no slot
  e conclusão da carga em exceção).
- **Latências documentadas** de MULT (6/9/13 por `rs`) e DIV (36), com bloqueio de MFHI/MFLO.
- **Detecção de exceções** com EPC, BD, BT, TAR e BadVaddr: Ov, AdEL (dados e busca), AdES,
  IBE, DBE, Sys, Bp e RI (tabela completa de opcodes reservados).
- **Desmontador** para logs.
- **Codificador MIPS** em `tests/` para escrever programas de teste sem toolchain externa, e um
  gerador de **BIOS sintética** para os testes de fumaça.
- **Frontend:** a BIOS configurada é executada até a primeira instrução que dependa de um
  componente inexistente; o log informa quantas instruções rodaram e onde parou.

## Fase 4 — o que foi realmente implementado

- **COP0** (`src/cpu/cop0.*`): SR, CAUSE (bits 8–9 graváveis, bit 10 ao vivo), EPC, BadVaddr, TAR,
  PRID = 2, BPC/BDA/BDAM/BPCM/DCIC (armazenados), máscaras de bits documentadas.
- **MFC0** (com atraso de uma instrução), **MTC0**, **RFE**; RI para registradores inexistentes e
  comandos TLB; CpU para LWC0/SWC0, COP0 em modo usuário sem CU0 e COP1/2/3 sem CU.
- **Despacho de exceções** ao vetor (80000080h ou BFC00180h conforme BEV) com EPC, CAUSE (ExcCode,
  BD, BT, CE), BadVaddr, TAR e empilhamento de SR. Exceções não param mais a CPU.
- **Interrupções:** hardware (CAUSE bit 10) e software (bits 8–9), aceitas entre instruções quando
  IEc e SR.Im permitem; EPC correto em delay slot; carga pendente concluída.
- **Modo usuário:** AdEL/AdES para acessos e buscas fora do KUSEG.
- **I-cache** (`src/cpu/icache.*`): 256 linhas, tag física, preenchimento até o fim da linha,
  IBLKSZ, código obsoleto após escrita na RAM, KSEG1 sem cache.
- **Isolamento de cache** (SR.IsC) com os modos TAG e IS1 do BCC; a rotina de limpeza no estilo
  da BIOS invalida a cache sem tocar na RAM (testado).
- **Controlador de interrupções** (`src/interrupts`): I_STAT/I_MASK em 1F801070h/74h.
- **`system::Console`** interliga barramento, controlador e CPU; o frontend o utiliza.
- **CLI:** `--cpu-steps <n>` limita a execução da BIOS.

## Fase 5 — o que foi realmente implementado

- **Base de tempo única** (`src/system/clock.hpp`): ciclos da CPU a 33,8688 MHz; conversão inteira
  exata para o cristal de vídeo da região (53,693175 / 53,203425 MHz), sem deriva.
- **Scheduler** (`src/system/scheduler.*`): um evento por componente, despachado em ordem de tempo
  antes de cada instrução pelo `Console::run`.
- **Temporização de vídeo** (`src/gpu/video_timing.*`): linhas de 3413/3406 ciclos, quadros de
  263/314 linhas (entrelaçado 262/263 e 312/313), HBLANK pelo intervalo X de GP1(06h), VBLANK de Y2
  a Y1 de GP1(07h) com **IRQ0**, dotclock por linha, valores de GP1(00h) no reset.
- **Timers 0–2** (`src/timers/timers.*`): fontes sistema/dotclock/HBLANK/÷8, alvo e FFFFh, reset no
  alvo com período alvo + 2, IRQ em pulso/alternância, uma vez/repetido, flags 11/12 com as regras
  de leitura medidas, sincronização com HBLANK/VBLANK, IRQ4/5/6.
- **Validação contra medições** do SCPH-5501 (psx.wiki): período do quadro, linhas, HBLANKs por
  quadro, pontos por linha e por quadro, VBLANK de 23 linhas, casos de Y1/Y2 sem IRQ0, número de
  IRQs por modo, parte visível da linha.
- **Frontend:** a BIOS roda pelo `Console::run` (eventos de vídeo e timers); o log informa tempo
  emulado, quadros e VBLANKs.

## Fase 6 — o que foi realmente implementado

- **Controlador de DMA** (`src/dma`): 7 canais com MADR/BCR/CHCR (espelho em +Ch), DPCR e DICR,
  máscaras de bits e escritas estreitas como medidas no SCPH-5501.
- **Modos:** burst (com e sem chopping), slice (um bloco por DREQ, pausa e retomada), lista
  encadeada (nós vazios, fim em FFFFFFh, erro em endereços de bit 23, lista em laço que nunca termina)
  e OTC (tabela de ordenação para baixo, 65 536 entradas com BCR 0).
- **Início e prioridade:** bit 24 + DPCR + (DREQ ou bit 28); habilitar no DPCR inicia canais armados
  em ordem de prioridade (empate → canal maior).
- **DICR/IRQ3:** flag com a habilitação do canal (medido, independe do bit 23), bit 31 recalculado,
  IRQ3 na borda; bit 15 em erro de barramento.
- **Interface `DmaPort`** para os dispositivos, com marcadores explícitos para GPU, MDEC, CD-ROM,
  SPU e porta de expansão.
- **Build:** opção `RPCS1_MSVC_MP_JOBS` para compilar com pouca memória livre.

## Fase 7 — o que foi realmente implementado

- **VRAM** (`src/gpu/vram.hpp`): 1024 × 512 pixels de 16 bits, volta nas bordas, conversões de cor.
- **GPU** (`src/gpu/gpu.hpp`): GP0/GPUREAD em 1F801810h e GP1/GPUSTAT em 1F801814h.
  - Todos os comandos GP0 são montados com o número exato de palavras (inclusive polilinhas e o
    marcador 5000_5000h, nunca antes do 2º vértice), então o FIFO não se desalinha mesmo com os
    comandos de desenho, que nesta fase são **descartados com aviso** (Fase 8).
  - Preenchimento 02h (passo de 16 em X, largura arredondada, sem máscara), cópia 80h, CPU→VRAM
    A0h e VRAM→CPU C0h, com as regras de máscara de E6h e tamanho 0 = máximo.
  - Atributos E1h–E6h (bit 11 de E1h = base Y 2 da textura, GPUSTAT 15); GP1(09h) armazenado (sem
    efeito com 1 MiB); IRQ1 por GP0(1Fh), reconhecido por GP1(02h).
  - Todos os comandos GP1 (00h–09h, 10h–1Fh da GPU v2, espelhos 40h–FFh); GPUSTAT completo (vale
    14802000h após o reset).
  - Canal 2 de DMA: DREQ conforme GP1(04h) (medido no SCPH-5501); a CPU avisa o DMA quando o DREQ
    muda (ex.: C0h com o canal já armado).
  - GP1(06h/07h/08h) alimentam a temporização de vídeo da Fase 5 (modo, entrelaçamento, divisor).
  - Imagem da área de exibição em 15 e 24 bits (`render_display`), preta com a exibição desligada.
- **Console:** GPU ligada ao barramento e ao canal 2; o reset reinicia a GPU e preserva a VRAM.
- **Frontend:** a janela mostra a área de exibição da VRAM ao fim da execução; `--dump-frame <arq>`
  grava a imagem em BMP (`src/video/bmp.hpp`); o log resume o estado da GPU.
- **Programa de teste** (`rpcs1_make_test_bios --gpu`, código MIPS próprio): configura a exibição,
  preenche retângulos, envia um preenchimento por DMA, um triângulo (descartado), transfere e copia
  pixels; o teste `smoke.gpu_frame` confere 31 pixels do BMP gerado.

## Fase 8 — o que foi realmente implementado

- **Rasterizador por software** (`src/gpu/renderer.hpp`), alimentado pelos comandos GP0 20h–7Fh:
  - Triângulos e quadriláteros (1-2-3 e 2-3-4), planos ou gouraud, com amostragem nos pixels
    inteiros e regra esquerda/superior (medido: 0,0/10,0/0,10 → x + y ≤ 9; diagonais e leques
    cobrem cada pixel uma vez; triângulos finos de 200/199/100 pixels).
  - Vértices de 11 bits com sinal, deslocamento E5h com volta, limite de 1023 × 511 por triângulo
    e por linha (medido), área de desenho E3h/E4h inclusiva.
  - Linhas e polilinhas com os extremos incluídos, simétricas, gouraud; polilinhas desenham os
    cantos duas vezes (medido).
  - Retângulos 1×1, 8×8, 16×16 e variáveis (largura módulo 1024, altura módulo 512), com
    textura da página de E1h e espelhamento (E1h bits 12/13).
  - Texturas de 4, 8 e 15 bits, CLUT, janela de textura E2h, texel 0000h transparente, modulação
    (texel × cor)/128 saturada, texturas cruas; página de textura dos polígonos atualiza E1h bits
    0–8 e 11.
  - Semitransparência nos 4 modos (B/2 + F/2 medido), só em texels com bit 15 nas primitivas
    texturizadas; dithering 4×4 (medido) em polígonos gouraud/modulados e em linhas; bit de máscara.
- **Programa de teste** `rpcs1_make_test_bios --gpu-draw` e teste `smoke.gpu_draw` (17 pixels
  calculados à mão e SHA-256); o programa da Fase 7 agora desenha um triângulo amarelo.

## Recursos ausentes (conscientemente)

- Cache de textura (e os artefatos medidos de U ímpar em retângulos), desenho na área exibida em
  modo entrelaçado (E1h bit 10), tempo de execução dos comandos, limite de 16 palavras do FIFO e
  cópia em blocos com sobreposição (D-040, D-042, D-046, D-047).
- Laço de emulação em tempo real com a janela aberta (Fase 20): a janela mostra o estado final.
- GTE (COP2); SPU, CD-ROM, MDEC, SIO, controles, Memory Cards — seus canais de DMA já existem, mas
  os dispositivos ainda são marcadores.
- Paralisação da CPU durante o DMA, efeito do chopping no tempo e ritmo real de cada dispositivo
  (D-036); tempos de acesso à memória (Fase 20).
- Calibração fina de 1 linha nos timers sincronizados ao VBLANK (D-033).
- Disparo de breakpoints de hardware (Fase 23); modos LOCK/INV do BCC.

## Próxima dependência técnica

**Fase 9 — GTE (COP2):** registradores de dados e de controle, MFC2/MTC2/CFC2/CTC2/LWC2/SWC2 e os
comandos (RTPS/RTPT, NCLIP, MVMVA, NCDS/NCCS, AVSZ3/4, OP, DPCS, INTPL, SQR, GPF/GPL...) com FLAG,
saturações e a divisão de perspectiva por tabela. É a instrução da GTE que hoje para a execução da
BIOS e é ela que alimenta a GPU com vértices nos jogos.
