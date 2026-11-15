# Roadmap do RPCS1 — 26 fases

Cada fase só é considerada concluída quando possui **implementação real**, **testes
automatizados passando**, **build verificável** e **documentação atualizada**. Classes vazias,
stubs ou funções que retornam valores fixos não contam como progresso.

Legenda de dependências: "→" indica a(s) fase(s) que precisam estar prontas antes.
O estado atual de cada fase fica em [PROGRESS.md](PROGRESS.md); os resultados de testes em
[TEST_RESULTS.md](TEST_RESULTS.md).

## Estratégia de testes (vale para todas as fases)

1. **Testes unitários em C++** (`tests/`), com o framework próprio do projeto.
2. **Montador de teste embutido** (a partir da Fase 3): um codificador de instruções MIPS em
   C++ dentro de `tests/`, para escrever programas de teste sem depender de toolchain externa.
3. **Programas PS-EXE próprios** (Fase 7 em diante), gerados pelo mesmo codificador ou por
   toolchain MIPS a ser escolhida (licença a verificar).
4. **Suítes públicas de teste de hardware** — candidatas a avaliar caso a caso quanto a licença
   e procedência antes de qualquer uso. Nenhuma foi integrada ainda.
5. **BIOS e jogos do usuário** — usados apenas localmente, nunca versionados.

---

## Fase 0 — Fundação do projeto
- **Objetivo:** estrutura modular, build CMake C++20, executável Windows, logging, erros,
  configuração e testes.
- **Entregáveis:** `CMakeLists.txt`, `CMakePresets.json`, `src/core` (tipos, `Result/Status`,
  logging, INI, configurações, CLI, E/S atômica, Unicode), `src/video` (framebuffer, janela Win32),
  `src/frontend` (aplicação), `tests/` (framework + testes unitários + smoke tests).
- **Aceitação:** compila sem avisos (`/W4 /WX`), abre e fecha a janela, testes passam.

## Fase 1 — Documentação técnica do PS1
- **Entregáveis:** `docs/HARDWARE_ARCHITECTURE.md` com CPU, COP0, memória, E/S, IRQ, DMA, timers,
  GPU, GTE, MDEC, SPU, CD-ROM, SCEx, BIOS, PS-EXE, controles, Memory Cards, regiões, modelos.
- **Aceitação:** fatos conferidos contra as fontes, divergências e lacunas listadas.

## Fase 2 — Memória e barramento → 0, 1
- **Objetivos:** `Bus` com RAM 2 MiB (espelhada em 8 MiB), scratchpad 1 KiB, BIOS 512 KiB,
  Memory Control, KUSEG/KSEG0/KSEG1/KSEG2, acessos de 8/16/32 bits, regiões de E/S mapeadas para
  dispositivos (inicialmente registradores que registram acessos não implementados em log),
  comportamento para endereços inválidos (erro de barramento sinalizado à CPU).
- **Testes:** leitura/escrita em cada região e espelho, extensão de tamanho, alinhamento, acesso
  ao scratchpad via KSEG1 (inválido), ROM somente leitura, desempenho do despacho.
- **Aceitação:** 100% dos testes de endereçamento; nenhum acesso fora dos limites (verificado com
  testes de fronteira e AddressSanitizer quando disponível).

## Fase 3 — CPU MIPS R3000A (interpretador) → 2
- **Objetivos:** registradores, decodificação, todas as instruções do conjunto do PS1, delay slot
  de desvio, load delay (incluindo cargas consecutivas no mesmo registrador), MULT/DIV com HI/LO
  e casos especiais (divisão por zero, `0x80000000 / -1`), LWL/LWR/SWL/SWR, contagem de ciclos.
- **Testes:** codificador de instruções em C++; um teste por instrução e por caso-limite;
  sequências com dependências de delay slot.
- **Aceitação:** todos os testes de instrução passam; nenhum recompilador nesta fase.

## Fase 4 — COP0, exceções e cache → 3
- **Objetivos:** registradores COP0, pilha IE/KU, `RFE`, exceções (AdEL/AdES/IBE/DBE/Sys/Bp/RI/CpU/Ov/INT),
  EPC/BD em delay slot, BEV, interrupções via CAUSE bit 10, isolamento de cache (SR.IsC) e registrador
  BCC, I-cache (comportamento observável).
- **Testes:** cada exceção, em delay slot e fora dele; `SYSCALL`/`BREAK`; mascaramento.
- **Aceitação:** sequências de teste reproduzem EPC/CAUSE/SR esperados.

## Fase 5 — Scheduler, timers e sincronização → 4
- **Objetivos:** agendador de eventos por ciclo (base: clock da CPU), timers 0–2 com todas as fontes
  e modos de sincronização, controlador de IRQ (I_STAT/I_MASK por borda), eventos de HBLANK/VBLANK
  derivados do clock de vídeo por região, estado de reset coerente.
- **Testes:** contagem/alvo/overflow/one-shot/toggle, sincronização com blanks, ordenação de eventos.
- **Decisão pendente:** modelo de clock de vídeo PAL (ver lacuna nº 1 da documentação).

## Fase 6 — DMA → 5
- **Objetivos:** 7 canais, modos burst/slice/linked list, direção, passo, DPCR/DICR, IRQ3,
  OTC, temporização de roubo de ciclos.
- **Testes:** transferências por canal com dispositivos de teste, encadeamento de listas, IRQs.

## Fase 7 — GPU: registradores e VRAM → 6
- **Objetivos:** GP0/GP1, FIFO, VRAM 1024×512×16, cópias CPU↔VRAM e VRAM↔VRAM, preenchimento,
  GPUSTAT, área/modo de exibição, NTSC/PAL, entrelaçamento, saída do framebuffer para a janela.
- **Aceitação:** programa de teste escreve na VRAM e a imagem aparece na janela, comparada por
  hash de imagem nos testes.

## Fase 8 — GPU: renderização por software → 7
- **Objetivos:** triângulos, quadriláteros, retângulos, linhas/polilinhas, Gouraud, texturas
  4/8/15 bits, CLUT, janela de textura, semitransparência, dithering, máscara, área de desenho,
  offset, regras de borda do hardware.
- **Testes:** imagens de referência geradas por programas de teste e conferidas manualmente;
  regressão por hash. Aceleração por GPU só depois, comparada contra este renderizador.

## Fase 9 — GTE → 4
- **Objetivos:** registradores COP2, todos os comandos, saturação e FLAG, divisão UNR bit a bit,
  temporização e bloqueio por dependência.
- **Testes:** vetores de teste por comando, incluindo casos de saturação e overflow de divisão.

## Fase 10 — MDEC → 6
- **Objetivos:** comandos 1–3, decodificação RLE/zig-zag, IDCT, YUV→RGB, profundidades 4/8/15/24,
  DMA0/DMA1.
- **Testes:** blocos codificados pelo próprio projeto com saída conhecida.

## Fase 11 — SPU e áudio → 6
- **Objetivos:** 24 vozes ADPCM, ADSR, pitch, loops, ruído, modulação, reverb, captura, IRQ9,
  DMA4, mistura do CD, saída 44,1 kHz para o dispositivo de áudio do Windows (WASAPI).
- **Testes:** decodificação ADPCM de blocos próprios, envelopes ADSR, mixagem determinística.

## Fase 12 — Controlador de CD-ROM → 6, 11
- **Objetivos:** registradores indexados, comandos, FIFOs, INT1–INT5, DMA3, busca e tempos de
  leitura, 1×/2×, tampa e ausência de disco, erros, CD-DA, XA-ADPCM.
- **Testes:** sequências de comando com respostas e tempos esperados usando imagens sintéticas.

## Fase 13 — Formatos de mídia → 12
- **Objetivos:** BIN/CUE (múltiplas faixas, áudio), ISO quando adequado, Mode 1/2, XA, subcanal
  quando disponível, múltiplos discos e troca de disco.
- **Testes:** imagens sintéticas geradas pelos testes (nunca jogos comerciais no repositório).

## Fase 14 — Autenticação e modchip → 12, 13
- **Objetivos:** modo original (região/licença com metadado explícito, marcado como simulado),
  modo modchip (injeção SCEx no nível do controlador), respostas de GetID por modelo.
- **Documentação:** SCEx, regiões, limitações de dumps (sem wobble).

## Fase 15 — Execução da BIOS → 4, 5, 6, 7, 12
- **Objetivos:** carregamento e validação (512 KiB), identificação da versão, mapeamento, reset,
  execução real, log do byte POST.
- **Bloqueio conhecido:** requer BIOS fornecida pelo usuário; sem ela, apenas testes sintéticos.

## Fase 16 — Inicialização autêntica do PS1 Fat → 8, 11, 15, 18, 19
- **Objetivos:** perfil SCPH-5501 (BIOS 3.0A); logotipo, som, gerenciador de Memory Card,
  CD Player e boot de disco produzidos pela BIOS emulada. Planejar perfis SCPH-100x/500x/700x.

## Fase 17 — Erros autênticos e Fearful Harmony → 16
- **Objetivos:** investigar relatos, diferenciar boatos de comportamento demonstrado, reproduzir
  apenas condições que emergem da emulação, logs de instruções/eventos, testes reprodutíveis.
- **Regra:** nunca usar mídia pré-gravada; caso não seja reproduzível, marcar como experimental
  ou não implementado.

## Fase 18 — Controles e SIO → 5
- **Objetivos:** SIO0, controle digital, DualShock (analógico, vibração), duas portas,
  temporização de /ACK, mapeamento de teclado e controles USB/Bluetooth (XInput/Raw Input).

## Fase 19 — Memory Cards → 18
- **Objetivos:** dois slots, protocolo, 128 KiB, persistência `.mcr/.mcd`, gravação atômica e
  cópia de segurança antes de operações destrutivas, detecção de corrupção.

## Fase 20 — Sincronização vídeo/áudio/CPU → 7, 11
- **Objetivos:** frame pacing, controle de velocidade, VBLANK, buffers de áudio com tratamento de
  underrun/overrun, temporização regional, pausa/reset/retomada.

## Fase 21 — Compatibilidade → 16
- **Objetivos:** suíte de compatibilidade com estados *não testado / falha de boot / menu /
  ingame / jogável*, registrando conteúdo, BIOS, região, modelo, gráficos, áudio, controle e saves.

## Fase 22 — Interface para Windows → 20
- **Objetivos:** menus Arquivo/Sistema/Configurações/Memory Cards/Logs; durante a emulação,
  apenas a saída de vídeo do console.

## Fase 23 — Save states e depuração → 21
- **Objetivos:** save/load state versionados, debugger MIPS, visualizadores de registradores,
  memória, IRQ, GPU, DMA, logs de CD-ROM, captura de estado para reproduzir erros.

## Fase 24 — Desempenho e estabilidade → 21
- **Objetivos:** profiling, redução de alocações, robustez de carregamento, testes prolongados,
  regressões de desempenho. Recompilador dinâmico apenas como estudo, nunca substituindo o
  interpretador.

## Fase 25 — Lançamento → todas
- **Objetivos:** build Windows x64, documentação de instalação/BIOS/controles/discos,
  compatibilidade conhecida, limitações, créditos, licenças, CHANGELOG, versão 0.1.0.
- **Pendência:** verificar se o nome "RPCS1" pode causar confusão com projetos existentes
  antes de qualquer distribuição pública ampla.
