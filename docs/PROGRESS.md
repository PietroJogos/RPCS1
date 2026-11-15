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
| 4 | COP0, exceções, cache | não iniciada | Próxima fase |
| 5 | Scheduler e timers | não iniciada | Decisão pendente sobre clock de vídeo PAL |
| 6 | DMA | não iniciada | |
| 7 | GPU: registradores e VRAM | não iniciada | |
| 8 | GPU: renderização | não iniciada | |
| 9 | GTE | não iniciada | |
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

## Recursos ausentes (conscientemente)

- COP0 (SR, CAUSE, EPC, vetores, RFE, interrupções) e o desvio ao tratador de exceção — Fase 4.
  Uma BIOS real para no primeiro `MTC0`/`MFC0`.
- GTE (COP2), GPU, SPU, CD-ROM, DMA, timers, controlador de IRQ, SIO, controles, Memory Cards.
- Temporização de memória, I-cache e fila de escrita (Fases 4 e 5).

## Próxima dependência técnica

**Fase 4 — COP0, exceções e cache:** registradores SR/CAUSE/EPC/BadVaddr/PRID, `MFC0`/`MTC0`/`RFE`,
despacho aos vetores usando os `ExceptionRecord` já produzidos pela CPU, interrupções via CAUSE
bit 10 e isolamento de cache (necessário para a BIOS limpar a I-cache).
