# Resultados de testes

Registro dos testes **realmente executados**. Cada entrada indica ambiente, comando e resultado.
A entrada mais recente fica no topo.

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
