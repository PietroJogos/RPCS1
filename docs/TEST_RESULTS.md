# Resultados de testes

Registro dos testes **realmente executados**. Cada entrada indica ambiente, comando e resultado.
A entrada mais recente fica no topo.

## 15/11/2026 — Fase 6 (versão 0.0.6)

**Ambiente:** o mesmo das fases anteriores, com **pouca memória livre na máquina** (≈0,9–1,2 GB de
32 GB). Build limpo a partir de pasta vazia: Debug com `RPCS1_MSVC_MP_JOBS=2`, Release com
`RPCS1_MSVC_MP_JOBS=1`, ambos com `/m:1`.

### Compilação: Debug e Release sem avisos. CTest: 8/8 em Debug e 8/8 em Release.

### Testes unitários: 241/241 (222 anteriores + 19 novos)
| Grupo | Casos | O que verifica |
|---|---|---|
| `dma/` | 18 | Reset; máscaras de MADR/CHCR e espelho; SB/SH por bytes; DICR (bits graváveis, bit 15, bit 31); OTC (tabela, bit 28, DPCR, BCR 0, passagem por 0); flag sem bit 23 (3 casos medidos); borda do IRQ3; burst nos dois sentidos, passo −4, chopping; espera de DREQ; slice com pausa; lista com nós vazios e duração; fim com erro; lista em laço; prioridade; aviso de canal sem dispositivo |
| `console/` | 1 | Programa da CPU configura o OTC, espera o bit 24, recebe IRQ3 e lê a tabela |

### Comparação com medições em SCPH-5501 (psx.wiki)
| Grandeza | Medido | RPCS1 |
|---|---|---|
| Máscara do CHCR (0–5 / 6) | 71770703h / 51000002h | idem ✅ |
| DICR após OTC: só habilitação / + bit 23 / só bit 23 | 40400000h / C0C00000h+IRQ3 / 00800000h | idem ✅ |
| Escrever 8000h / 80000000h no DICR | 80008000h + IRQ3 / 0 | idem ✅ |
| OTC a partir de 4 com 3 entradas | [4]=0, [0]=00FFFFFCh, 1FFFFCh intacto | idem ✅ |
| Slice 16×16: MADR, BCR finais | início + 400h, 00000010h | regra idêntica ✅ |
| Lista: MADR final / BCR | 00FFFFFFh / inalterado | idem ✅ |
| Lista em laço | ocupada para sempre, sem IRQ | idem ✅ |
| Prioridade: dados que ficam na RAM | do canal de menor prioridade | idem ✅ |
| Duração do OTC de 65 536 entradas | 69 586 (com custo de leitura) | 65 536 ⚠️ sem sobrecarga |

### Teste de mutação
Exigir o bit 23 para ligar o flag (a regra do psx-spx) e tirar o marcador de fim do OTC:
**7 testes falharam** como esperado; código restaurado e 241/241 novamente.

### Problemas encontrados e corrigidos
1. **Compilação sem memória (C1060)** na máquina de desenvolvimento: criada a opção
   `RPCS1_MSVC_MP_JOBS` e o build passou a usar menos processos paralelos.
2. **Objeto antigo após o build interrompido:** a primeira execução mostrou 789 VBLANKs em vez de 3 e
   uma violação de acesso em testes que usam o `Console`. Causa: `test_timers.obj` de antes da mudança
   do tamanho do `Console` (o build tinha falhado por falta de memória). Um build limpo resolveu; o
   código estava correto. Por isso os resultados acima vêm de builds a partir de pasta vazia.
3. Revisão antes da compilação: escrita estreita no DICR podia limpar flags de bytes não escritos;
   corrigido antes de qualquer teste.

## 15/11/2026 — Fase 5 (versão 0.0.5)

**Ambiente:** o mesmo das fases anteriores; build limpo a partir de pasta vazia, Debug e Release.

### Compilação: Debug e Release sem avisos. CTest: 8/8 em Debug e 8/8 em Release.

### Testes unitários: 222/222 (191 anteriores + 31 novos)
| Grupo | Casos | O que verifica |
|---|---|---|
| `scheduler/`, `clock/` | 3 | Ordem, empates, cancelamento, fonte de tempo; conversão CPU↔vídeo exata por 1 h |
| `video/` | 8 | Períodos de quadro, linhas, HBLANKs, VBLANK de 23 linhas, PAL, entrelaçado, 9 casos de Y1/Y2, 6 de X1/X2, pontos por linha (5 larguras) e por quadro |
| `timers/`, `console/` | 20 | Reset, sistema e ÷8, IRQs por modo, sequência do reset no alvo, alvo 0, FFFFh, flags, MODE, COUNT = alvo, uma vez/rearme, pulso, alternância, dotclock, HBLANK, sincronizações (T0, T1, T2, modo 3), CPU lendo timers no Console, laço que conta VBLANKs |

### Comparação com medições em SCPH-5501 (psx.wiki)
| Grandeza | Medido | RPCS1 | Situação |
|---|---|---|---|
| Período do IRQ0 (NTSC progressivo) | ≈566 140 (±200) | 566 203–566 204 | ✅ dentro da tolerância |
| Campos entrelaçados | ≈566 150 / ≈563 930 | 566 204 / 564 051 | ✅ |
| Bit PAL num console NTSC | ≈674 450 | 674 613–674 614 | ✅ |
| HBLANKs por quadro | 263 | 263 | ✅ |
| VBLANK com Y = 16..256 | 23 linhas | 23 | ✅ |
| Y2 = 300/1023, Y1 = Y2, 0/263 | sem IRQ0 | sem IRQ0 | ✅ |
| Pontos por linha (256/320/368/512/640) | 341/426/487/682/853 | idem | ✅ |
| Pontos por quadro (256 / 320) | 89 687 / 112 043 | 89 683 / 112 038 | ✅ (±10) |
| IRQs em 140 000 ciclos (0010h/0050h/0058h/0020h) | 1 / 3 / 542 / 1 | 1 / 3 / 542 / 1 | ✅ |
| Parte visível da linha (X 600..3160) | 1613–1614 | 1614,8 | ✅ |
| Timer 1 HBLANK pausado no VBLANK | 239 / quadro | 240 | ⚠️ diverge 1 linha (D-033) |
| Timer 1 sysclock reset no VBLANK, no IRQ0 | 55 425–57 886 | 57 937 | ⚠️ (D-033) |

### Testes de mutação
- 1ª tentativa **inválida**: a mutação não compilou (aviso C4127 tratado como erro) e os testes
  rodaram o binário anterior. Refeita com mutação compilável.
- Reset no alvo sem o "+2" e quadro NTSC com 262 linhas: **8 testes falharam** como esperado;
  código restaurado, recompilado e 222/222 novamente.

### Problemas encontrados e corrigidos
1. Dois erros de cálculo nos próprios testes (janela de 11 linhas em vez de 10; espera longa demais
   no teste de sincronização 3), corrigidos antes da execução.
2. Expectativa errada no teste do modo PAL (674 617 calculado à mão; o correto é 674 613,4) e
   contagem de HBLANKs iniciada no instante errado. O código estava certo; os testes foram ajustados.

### Desempenho observado (não é meta)
Release: 2 milhões de passos (todos com exceção) em 89 ms (~22 milhões/s). O console real executa
33,9 milhões de ciclos/s; otimização é a Fase 24.

## 15/11/2026 — Fase 4 (versão 0.0.4)

**Ambiente:** o mesmo das fases anteriores; build limpo a partir de pasta vazia, Debug e Release.

### Compilação
| Configuração | Resultado | Avisos (`/W4 /permissive- /WX`) |
|---|---|---|
| Debug | ✅ sucesso | 0 |
| Release | ✅ sucesso | 0 |

### CTest: 8/8 em Debug e 8/8 em Release
`smoke.bios_load` usa uma nova BIOS **sintética** (programa próprio): POST 7 → `SYSCALL` atendido
pelo tratador em BFC00180h (POST 8, retorno com `RFE`) → POST 9 → liga CU2 e para no comando da
GTE indicando a Fase 9. O log confere **16 instruções, 17 ciclos, 1 exceção**. Com 512 KiB de
bytes `41h` e `--cpu-steps 1000`, o log confere 1000 exceções RI em BFC00180h e o limite atingido.

### Testes unitários: 191/191 (155 anteriores + 36 novos)
| Grupo | Casos | Cobertura |
|---|---|---|
| `cop0/` | 17 | Reset, MTC0/MFC0 com atraso, DCIC, bits fixos do SR, somente leitura, RI (5 regs), r16–r31, TLB/CFC0, RFE (4), empilhamento (3), BEV, BD/BT/TAR, BadVaddr, CE (8), COP1/COP3, CU0 em modo usuário, acessos fora do KUSEG (5 + busca), tratador completo com retorno |
| `irq/`, `irq_controller/`, `console/` | 10 | IRQ de hardware antes da instrução, máscaras (3), software, delay slot, carga concluída, bit 10 ao vivo, I_STAT/I_MASK, ligação ao barramento, IRQ ponta a ponta com reconhecimento, power_on/reset |
| `icache/` | 7 | Desligada, código obsoleto e KSEG1, preenchimento (4 + IBLKSZ 0), aliases KUSEG/KSEG0, isolamento IS1 e TAG, rotina de limpeza estilo BIOS sem apagar a RAM |
| Ajustados da Fase 3 | 3 | Overflow agora desvia ao vetor; GTE com CU2 para (Fase 9); exceções não param a execução |
| `cli/` | 1 | `--cpu-steps` |

### Teste de mutação
Isolamento de cache desligado + empilhamento de SR removido: **8 testes falharam** como esperado;
código restaurado e conferido.

### Não testado
- Nenhuma BIOS real (nenhuma fornecida).
- Disparo de breakpoints de hardware (não implementado, D-030).
- Os casos ⚠️ D-025 a D-030 seguem a arquitetura R3000 ou escolhas documentadas.

## 15/11/2026 — Fase 3 (versão 0.0.3)

**Ambiente:** o mesmo das fases anteriores; build limpo a partir de pasta vazia, Debug e Release.

### Compilação
| Configuração | Resultado | Avisos (`/W4 /permissive- /WX`) |
|---|---|---|
| Debug | ✅ sucesso | 0 |
| Release | ✅ sucesso | 0 |

### CTest: 8/8 em Debug e 8/8 em Release
`smoke.bios_load` agora gera uma **BIOS sintética** com um programa MIPS próprio
(`tests/tools/make_test_bios.cpp`, nenhum código da Sony) e verifica no log que a CPU executou
**305 instruções em 305 ciclos**, escreveu **POST = 7h** e parou no `mfc0 t3, $12` indicando a
Fase 4. Também verifica que 512 KiB de bytes `41h` param na primeira instrução (opcode COP0).

### Testes unitários: 155/155 (99 anteriores + 56 novos)
| Grupo | Casos | Cobertura |
|---|---|---|
| ALU (`test_cpu_alu`) | 11 | Reset, execução a partir de BFC00000h, LUI/ORI, ADDIU/ADDU, ADD/ADDI/SUB com e sem overflow (6 casos), lógicas, comparações, deslocamentos, r0, ciclos |
| Memória (`test_cpu_memory`) | 14 | Extensão de sinal, stores, SH em E/S com registrador inteiro, 3 cenários de load delay, store no slot, LWL/LWR (8 casos), LWR+LWL encadeadas, SWL/SWR (8 casos), SWR+SWL, AdEL/AdES (5 casos), DBE (4 casos), carga concluída em exceção |
| Desvios (`test_cpu_branch`) | 12 | BEQ/BNE, laço para trás, BLEZ/BGTZ/BLTZ/BGEZ (14 casos), link sem desvio, `rs = ra`, BcondZ não documentado, J/JAL, JR/JALR (inclusive `rd = rs`), desvio no load delay slot, alvo desalinhado, desvio em delay slot |
| Exceções (`test_cpu_exceptions`) | 7 | SYSCALL/BREAK, BD/EPC/BT/TAR, todos os opcodes reservados primários e SPECIAL, bits não usados, coprocessadores não implementados (7 casos), CPU parada, IBE (4 endereços) |
| MULT/DIV (`test_cpu_muldiv`) | 7 | MULT/MULTU (8 casos), DIV/DIVU (5), divisão por zero e 80000000h/−1 (6), MTHI/MTLO, tabela de latência (13 pontos), bloqueio de MFLO (6 casos), latência escondida |
| Programas e desmontador | 5 | Soma 1..100 (402 instruções), cópia de bytes, sub-rotina JAL/JR, fatorial com MULTU, 20 formatos do desmontador |

### Teste de mutação
Para confirmar que os testes não passam por acaso, o load delay foi desativado de propósito
(carga gravando o registrador imediatamente): **5 testes falharam** como esperado. O código
original foi restaurado e conferido.

### Problemas encontrados e corrigidos
1. Erros de contagem em dois testes recém-escritos (número de passos e deslocamento de desvio),
   corrigidos antes da primeira execução.
2. Uma parada por instrução não implementada consumia 1 ciclo (306 ciclos para 305 instruções);
   agora não consome.

### Não testado
- Nenhuma BIOS real foi executada (nenhuma foi fornecida). Com uma BIOS real, espera-se parada no
  primeiro acesso ao COP0, que ocorre logo no início da inicialização.
- Suítes públicas de teste de CPU não foram usadas (licenças ainda não avaliadas).
- Os casos ⚠️ (D-019 a D-024) seguem o modelo de pipeline ou escolhas documentadas, não medições.

## 15/11/2026 — Fase 2 (versão 0.0.2)

**Ambiente:** o mesmo da Fase 0 (Windows 11 Pro x64, MSVC 19.44.35229, CMake 4.4.4), build
limpo a partir de pasta vazia, Debug e Release.

### Compilação
| Configuração | Resultado | Avisos (`/W4 /permissive- /WX`) |
|---|---|---|
| Debug | ✅ sucesso | 0 |
| Release | ✅ sucesso | 0 |

### CTest: 8/8 em Debug e 8/8 em Release
Novo: `smoke.bios_load` — com uma BIOS **sintética** de 512 KiB (bytes `41h`, não é uma BIOS
real) o programa registra "BIOS carregada" e "primeira instrução: 41414141h"; com um arquivo de
3 bytes registra a exigência de 524 288 bytes; com arquivo ausente, "não encontrado". Em todos os
casos encerra normalmente e informa que nada é executado.

### Testes unitários: 99/99 (61 anteriores + 38 novos)
| Grupo | Casos | Cobertura |
|---|---|---|
| `address/` | 4 | Fronteiras de segmento, tradução física (inclui varredura), regiões com cache, alinhamento |
| `memory_block/` | 5 | Tamanhos, little-endian, escritas estreitas, espelho por máscara, últimos bytes |
| `bios_rom/` | 3 | 6 tamanhos inválidos rejeitados, leitura, arquivo ausente/grande/pequeno/válido |
| `memory_control/` | 6 | Reset e janelas padrão, 8 valores de DRAM_CTRL, leituras estreitas, bits fixos, escrita estreita, reset |
| `bus/` | 20 | RAM nos 3 segmentos, SB/SH, espelhos 0B88h, DRAM_CTRL 0888h/0000h, /RAS1, desalinhamento, scratchpad, BIOS (aliases, somente leitura, janela, espelho), Expansões 1/2/3 e POST, 12 endereços inexistentes, 14 lacunas de E/S, marcadores não implementados, dispositivo conectado, faixas de E/S, KSEG2, power_on/reset, varredura de 2 MiB |

### Problemas encontrados e corrigidos
1. **Queda do executável de testes (0xC0000005)** em `bios_rom/load_file`. Causa: o macro
   `CHECK_EQ` do framework guardava `const auto&` para `synthetic_image(...)[12345]`, ou seja,
   uma referência para dentro de um vetor temporário já destruído. O código do barramento
   estava correto (valor 0xBF conferido). Correção: o macro copia os operandos (D-018).
2. A cópia tornou constantes algumas comparações e o MSVC emitiu C4127 (tratado como erro);
   a comparação passou a ser feita por função auxiliar.

### Não testado
- Comportamento com uma BIOS real (nenhuma foi fornecida; e ela ainda não seria executada).
- Os casos ⚠️ (barramento aberto, escrita na ROM, reset do BCC etc.) seguem escolhas documentadas,
  não medições em hardware.

## 15/11/2026 — Fase 0 (versão 0.0.1)

**Ambiente:** Windows 11 Pro (10.0.26300) x64 · Visual Studio 2022 · MSVC 19.44.35229 ·
CMake 4.4.4 · Windows SDK 10.0.26100.0 · build limpo a partir de pasta vazia.

**Comandos:**

```bash
cmake --preset windows-msvc
cmake --build --preset debug
cmake --build --preset release
ctest --preset debug
ctest --preset release
```

### Compilação

| Configuração | Resultado | Avisos (`/W4 /permissive- /WX`) |
|---|---|---|
| Debug | ✅ sucesso | 0 |
| Release | ✅ sucesso | 0 |

`RPCS1.exe` (Release, 564 224 bytes) depende apenas de `KERNEL32`, `USER32`, `GDI32` e `SHELL32`
(runtime do MSVC vinculado estaticamente).

### CTest

| Teste | Debug | Release | O que verifica |
|---|---|---|---|
| `unit.all` | ✅ | ✅ | 61 testes unitários (abaixo) |
| `smoke.version` | ✅ | ✅ | `--version` imprime "RPCS1 0.0.1" |
| `smoke.help` | ✅ | ✅ | `--help` lista as opções |
| `smoke.headless_first_run` | ✅ | ✅ | Sem `rpcs1.ini`: cria configuração e log, sai com 0; 2ª execução lê o arquivo sem avisos |
| `smoke.unknown_option` | ✅ | ✅ | Opção desconhecida → código de saída 2 |
| `smoke.invalid_config` | ✅ | ✅ | `[Video] scale = 99` → código de saída 3 e mensagem com seção/chave |
| `smoke.window_open_close` | ✅ | ✅ | Abre a janela Win32 real e a fecha após 500 ms com encerramento normal |

**Total: 7/7 em Debug e 7/7 em Release.**

### Testes unitários (61/61 passaram)

| Grupo | Casos | Cobertura |
|---|---|---|
| `cli/` | 6 | Padrões, opções simples, `--opt valor` e `--opt=valor`, caminhos com acentos, 10 entradas inválidas, texto de ajuda |
| `console_model/` | 3 | Catálogo consistente (nomes únicos, região por sufixo, nenhum perfil marcado como implementado), parse, letras SCEx |
| `error/` | 6 | `Result` com valor/erro, `BadResultAccess`, tipos conversíveis e só-movíveis, `Status`, `describe` |
| `file_io/` | 7 | Arquivo ausente, pasta, limite de tamanho, 256 bytes binários, arquivo vazio, escrita atômica, nomes Unicode |
| `framebuffer/`, `window/` | 3 | Acesso a pixels, limites, cálculo 4:3 (pillarbox/letterbox) |
| `ini/` | 10 | Leitura, maiúsculas, comentários/BOM/CRLF, aspas, valores vazios, duplicatas, 6 erros com linha, ida e volta, arquivo |
| `log/` | 9 | Níveis, categorias, filtros global e por categoria, `Off`, formatação adiada, formato, remoção de destino, 8 threads × 500 mensagens |
| `settings/` | 8 | Padrões, valores válidos, região automática por modelo, 6 valores inválidos, avisos, ida e volta, caminhos relativos |
| `strings/` | 6 | `trim`, `iequals`, inteiros decimais/hex e limites, booleanos |
| `unicode/` | 3 | Caminhos UTF-8 (inclui japonês), vazio, UTF-16 ↔ UTF-8 |

### Problemas encontrados e corrigidos

1. **Referência pendente em `Result<T>::value() &&`** — o teste
   `error/Result aceita tipos conversíveis e só-movíveis` falhou na primeira execução
   (`greeting(true).value()` produzia `""`). Causa: o método retornava `T&&` para um objeto
   temporário. Correção: retornar `T` por valor. Teste passou após a correção.
2. **Erro de compilação nos testes** — `test_ini.cpp` usava `usize` sem incluir `core/types.hpp`.

### Não testado

- Outros compiladores (GCC/Clang) e Windows 10 físico (apenas Windows 11 foi usado).
- Comportamento com DPI alto em múltiplos monitores (código presente, não verificado manualmente).
- Nenhum teste de hardware do PlayStation existe ainda — não há emulação.
