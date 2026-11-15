# Arquitetura de hardware do PlayStation 1 — referência técnica do RPCS1

> **Fase 1 — documentação técnica.** Este documento resume, com palavras próprias,
> o comportamento do hardware que o RPCS1 precisa reproduzir. Ele **não** descreve
> código de nenhum emulador. Os valores foram conferidos em novembro de 2026 contra as
> páginas do psx-spx listadas na seção [Fontes](#24-fontes-e-procedência).
> Itens marcados com ⚠️ têm divergência entre fontes ou ainda não foram verificados;
> eles estão consolidados na seção [Lacunas](#23-lacunas-e-pontos-a-verificar).

Convenções: endereços e valores em hexadecimal com sufixo `h` (ex.: `1F801070h`);
bits numerados a partir de 0 (bit menos significativo).

---

## Sumário

1. [Visão geral](#1-visão-geral)
2. [CPU (MIPS R3000A compatível)](#2-cpu-mips-r3000a-compatível)
3. [COP0 — controle do sistema e exceções](#3-cop0--controle-do-sistema-e-exceções)
4. [Mapa de memória](#4-mapa-de-memória)
5. [Memory Control](#5-memory-control)
6. [Mapa de E/S](#6-mapa-de-es)
7. [Interrupções](#7-interrupções)
8. [DMA](#8-dma)
9. [Timers](#9-timers)
10. [GPU](#10-gpu)
11. [GTE (COP2)](#11-gte-cop2)
12. [MDEC](#12-mdec)
13. [SPU](#13-spu)
14. [Controlador de CD-ROM](#14-controlador-de-cd-rom)
15. [Proteção SCEx, licença e modchips](#15-proteção-scex-licença-e-modchips)
16. [BIOS e kernel](#16-bios-e-kernel)
17. [PS-EXE e SYSTEM.CNF](#17-ps-exe-e-systemcnf)
18. [Controles (SIO0)](#18-controles-sio0)
19. [Memory Cards](#19-memory-cards)
20. [Regiões, modelos e versões de BIOS](#20-regiões-modelos-e-versões-de-bios)
21. [Temporização de vídeo por região](#21-temporização-de-vídeo-por-região)
22. [Fearful Harmony — estado da pesquisa](#22-fearful-harmony--estado-da-pesquisa)
23. [Lacunas e pontos a verificar](#23-lacunas-e-pontos-a-verificar)
24. [Fontes e procedência](#24-fontes-e-procedência)

---

## 1. Visão geral

```
                         ┌──────────────────────────────────────────┐
                         │ CPU R3000A compatível — 33,8688 MHz      │
                         │  COP0 (sistema)   COP2 (GTE)             │
                         │  I-cache 4 KiB    Scratchpad 1 KiB       │
                         └───────────────────┬──────────────────────┘
                                             │ barramento principal (32 bits)
  ┌───────────┬───────────┬──────────┬───────┴────┬───────────┬────────────┬───────────┐
  │ RAM 2 MiB │ BIOS ROM  │ DMA      │ Controle   │ Timers    │ SIO0/SIO1  │ Expansão  │
  │           │ 512 KiB   │ 7 canais │ de IRQ     │ 0, 1, 2   │ pads/cards │ 1, 2, 3   │
  └───────────┴───────────┴────┬─────┴────────────┴───────────┴────────────┴───────────┘
                               │
        ┌───────────────┬──────┴────────┬──────────────────┬───────────────────┐
        │ GPU           │ MDEC          │ SPU              │ CD-ROM            │
        │ VRAM 1 MiB    │ decodificador │ RAM de som       │ controlador +     │
        │ 1024×512×16   │ de macroblocos│ 512 KiB, 24 vozes│ decodificador     │
        └───────────────┴───────────────┴──────────────────┴───────────────────┘
```

| Componente | Resumo |
|---|---|
| CPU | MIPS R3000A compatível, 33,8688 MHz (= 44 100 × 768), sem FPU e sem TLB |
| RAM principal | 2 MiB, espelhada nos primeiros 8 MiB |
| BIOS | ROM de 512 KiB em `1FC00000h` |
| GPU | 2D com rasterização de triângulos/quads/retângulos/linhas; VRAM própria de 1 MiB |
| GTE | Coprocessador 2 de ponto fixo: transformação, projeção, iluminação |
| MDEC | Descompressão de imagens/vídeo baseada em DCT |
| SPU | 24 vozes ADPCM, 512 KiB de RAM, reverb, saída estéreo 44,1 kHz |
| CD-ROM | Leitura 1×/2× (75/150 setores/s), CD-DA, XA-ADPCM |
| DMA | 7 canais entre RAM e periféricos |
| SIO0 | Controles e Memory Cards (serial síncrona) |

O relógio da CPU (33,868800 MHz) aparece na seção de temporização de vídeo do psx-spx;
a página de especificações da CPU não o cita explicitamente.

---

## 2. CPU (MIPS R3000A compatível)

- 32 registradores gerais de 32 bits (`r0` sempre vale 0), mais `HI`, `LO` e `PC`.
- Ordem de bytes *little-endian*.
- Sem FPU (COP1 inexistente) e sem MMU/TLB: os segmentos KUSEG/KSEG0/KSEG1 só
  removem os 3 bits superiores do endereço (ver [§4](#4-mapa-de-memória)).
- **Branch delay slot:** a instrução seguinte a um desvio/salto sempre é executada.
- **Load delay slot:** o valor carregado por um `LW`/`LH`/`LB`... só fica visível para a
  instrução **após** a seguinte. O RPCS1 deve modelar isso explicitamente (inclusive o
  caso de duas cargas consecutivas no mesmo registrador e a interação com `MFC0`).
- Acessos de 16/32 bits desalinhados geram exceção de endereço (AdEL/AdES); `LWL/LWR/SWL/SWR`
  existem para acessos desalinhados intencionais.
- `ADD`, `ADDI` e `SUB` geram exceção de *overflow*; `ADDU/ADDIU/SUBU` não.
- Multiplicação e divisão escrevem em `HI/LO` com latência; ler `HI/LO` (MFHI/MFLO) antes
  do fim bloqueia a CPU até o resultado ficar pronto. Iniciar a operação custa 1 ciclo.

| Operação | Latência (ciclos) | Depende de |
|---|---|---|
| MULTU | 6 se `rs` < 800h; 9 se < 100000h; 13 nos demais | Apenas `rs` |
| MULT | 6 se `rs` em −800h..7FFh; 9 se em −100000h..FFFFFh; 13 nos demais | Apenas `rs` |
| DIV / DIVU | 36 | — |

- Divisão não gera exceção. DIVU por zero: HI = rs, LO = FFFFFFFFh. DIV por zero: HI = rs,
  LO = −1 se rs ≥ 0, +1 se rs < 0. DIV `80000000h / −1`: HI = 0, LO = 80000000h.
- **Load delay:** o valor carregado não é visto pela instrução seguinte; se uma exceção
  ocorre logo depois, a carga se completa mesmo assim. `LWL`/`LWR` podem ser encadeadas no
  mesmo registrador sem instrução intermediária (a segunda vê o resultado da primeira).
  Leituras de COP0/COP2 (`MFC0`, `MFC2`, `CFC2`) também têm 1 instrução de atraso.
- **BcondZ (opcode 01h):** `rt` = 00h BLTZ, 01h BGEZ, 10h BLTZAL, 11h BGEZAL. As variantes com
  link gravam `r31 = PC + 8` mesmo sem desvio; se `rs = r31`, a comparação usa o valor anterior.
- Alvo de desvio = endereço do desvio + 4 + offset×4; `J/JAL` = (PC AND F0000000h) + imm26×4.
  Salto para alvo desalinhado: o salto executa e a busca seguinte gera AdEL, com EPC = BadVaddr = alvo.
- Opcodes reservados (exceção RI): primários 14h–1Fh, 27h, 2Ch, 2Dh, 2Fh, 34h–37h, 3Ch–3Fh;
  SPECIAL 01h, 05h, 0Ah, 0Bh, 0Eh, 0Fh, 14h–17h, 1Ch–1Fh, 28h, 29h, 2Ch–3Fh. Bits não usados
  dos campos não geram exceção.
- `ADD`, `ADDI`, `SUB` com overflow não alteram o destino. Em `SB`/`SH` os 32 bits do
  registrador vão para o barramento.
- `LWL`/`LWR`/`SWL`/`SWR` (deslocamento 0..3 na palavra): LWR/SWR em +0 transferem os 32 bits;
  em +1, +2, +3 os 24, 16, 8 bits baixos do registrador ↔ bytes 1..3, 2..3, 3. LWL/SWL em +0,
  +1, +2 transferem os 8, 16, 24 bits altos ↔ bytes 0, 0..1, 0..2; em +3, os 32 bits.
- Fila de escrita de 4 palavras: stores custam 1 ciclo, salvo fila cheia; qualquer acesso a
  KSEG1 espera a fila esvaziar. A RAM principal tem tempo de acesso de ~6 ciclos.
- ⚠️ A tabela geral de ciclos por instrução e os custos de acerto/falha da I-cache não são
  documentados pelo psx-spx.
- **I-cache:** 4096 bytes, mapeamento direto, 256 linhas de 16 bytes; ativa apenas
  para execução em KUSEG e KSEG0.
- **Sem D-cache:** a memória de cache de dados é exposta como *scratchpad* de 1 KiB em
  `1F800000h–1F8003FFh` (não executável, não acessível via KSEG1).
- Identificação: `PRID` (COP0 r15) = `00000002h` no chip CXD8606CQ (bits 0–7 revisão,
  8–15 implementação).

---

## 3. COP0 — controle do sistema e exceções

### 3.1 Registradores

| Nº | Nome | Função | Acesso |
|---|---|---|---|
| r3 | BPC | Breakpoint no PC | L/E |
| r5 | BDA | Breakpoint de endereço de dados | L/E |
| r6 | TAR | Endereço-alvo (registrado no delay slot de desvio; chamado JUMPDEST em textos antigos) | L |
| r7 | DCIC | Controle de depuração e invalidação de cache | L/E |
| r8 | BadA / BadVaddr | Endereço que causou erro | L |
| r9 | BDAM | Máscara do breakpoint de dados | L/E |
| r11 | BPCM | Máscara do breakpoint de PC | L/E |
| r12 | SR | Status | L/E |
| r13 | CAUSE | Causa da última exceção | L (bits 8–9 L/E) |
| r14 | EPC | PC de retorno da exceção | L |
| r15 | PRID | Identificação do processador | L |

r0–r2, r4 e r10 não existem; r16–r31 retornam lixo.

### 3.2 SR (r12)

| Bits | Campo | Significado |
|---|---|---|
| 0 | IEc | Interrupções habilitadas (atual) |
| 1 | KUc | Modo atual (0 = kernel, 1 = usuário) |
| 2–3 | IEp/KUp | Valores anteriores |
| 4–5 | IEo/KUo | Valores "antigos" |
| 8–15 | Im | Máscara de interrupções (por bit de CAUSE.IP/Sw) |
| 16 | IsC | Isola a cache (escritas vão para a cache, não para a memória) |
| 17 | SwC | Troca caches (sem efeito observável no PS1) |
| 18–21 | PZ, CM, PE, TS | Bits de diagnóstico de cache/paridade |
| 22 | BEV | Vetores de exceção em ROM (1) ou RAM (0) |
| 25 | RE | Inversão de endianness (provavelmente ausente no PS1) |
| 28 | CU0 | Habilita COP0 em modo usuário |
| 30 | CU2 | Habilita COP2 (GTE) |

Na exceção, os pares IE/KU formam uma pilha de 3 níveis que é empurrada; `RFE` a desempilha
(bits 2–3 → 0–1 e 4–5 → 2–3).

### 3.3 CAUSE (r13)

| Bits | Campo |
|---|---|
| 2–6 | ExcCode (código da exceção) |
| 8–9 | Interrupções de software (L/E) |
| 10–15 | Interrupções pendentes (no PS1 só o bit 10 é usado — ver [§7](#7-interrupções)) |
| 28–29 | CE: número do coprocessador (para CpU) |
| 30 | BT: desvio tomado |
| 31 | BD: exceção ocorreu em delay slot (EPC aponta para o desvio) |

### 3.4 Códigos de exceção

| Código | Nome | Causa |
|---|---|---|
| 00h | INT | Interrupção externa |
| 01h–03h | MOD/TLBL/TLBS | TLB (não existe no PS1) |
| 04h | AdEL | Erro de endereço em leitura ou busca de instrução |
| 05h | AdES | Erro de endereço em escrita |
| 06h | IBE | Erro de barramento na busca de instrução |
| 07h | DBE | Erro de barramento em dados |
| 08h | Sys | `SYSCALL` |
| 09h | Bp | `BREAK` |
| 0Ah | RI | Instrução reservada |
| 0Bh | CpU | Coprocessador indisponível |
| 0Ch | Ov | Overflow aritmético |

### 3.5 Vetores

| Evento | BEV = 0 | BEV = 1 |
|---|---|---|
| Reset | `BFC00000h` | `BFC00000h` |
| UTLB miss | `80000000h` | `BFC00100h` |
| Break de depuração (COP0) | `80000040h` | `BFC00140h` |
| Geral | `80000080h` | `BFC00180h` |

Segundo o psx-spx, o PS1 usa na prática apenas os vetores BEV = 0 (além do reset).

### 3.5.1 Regras de acesso e de exceção (verificadas na Fase 4)

- **CAUSE** é somente leitura, exceto os bits 8–9 (interrupções de software). O bit 10 reflete
  ao vivo `(I_STAT AND I_MASK) != 0` e não é um latch; os bits 11–15 são sempre zero no PS1.
- **Interrupção** ocorre quando SR.IEc = 1 e algum bit de CAUSE 8–15 está habilitado em SR.Im.
  Interrupções sempre retornam a EPC + 0 (um desvio em delay slot é reexecutado).
- **TAR** só é atualizado em exceção dentro de delay slot de desvio tomado (BD = BT = 1);
  **BadVaddr** só por AdEL/AdES (inclusive acesso fora do KUSEG em modo usuário).
- **CE** (CAUSE bits 28–29) recebe o número do coprocessador em CpU.
- **COP0:** ler r0–r2, r4 ou r10 gera RI; r16–r31 leem valores imprevisíveis (podem ser lidos
  mesmo em modo usuário sem CU0). Só o comando RFE existe; TLBR/TLBWI/TLBWR/TLBP geram RI.
  LWC0/SWC0 geram CpU. Em modo usuário, o COP0 exige CU0. COP2 com CU2 = 0 gera CpU (CE = 2).
- **DCIC** (r7): bits 0–5 status, 12–13 redirecionamento, 23–31 habilitações (PC, dados, leitura,
  escrita, kernel/usuário, salto ao vetor de depuração 80000040h). BPC/BPCM e BDA/BDAM disparam
  quando `((PC XOR BPC) AND BPCM) = 0` / `((addr XOR BDA) AND BDAM) = 0`.
- **Peculiaridade da GTE:** se uma interrupção chega sobre um comando COP2, o comando é executado
  e EPC ainda aponta para ele; BIOS mais novas somam 4 ao EPC nesse caso (relevante na Fase 9).

### 3.6 Controle de cache (KSEG2)

`FFFE0130h` é o registrador BIU/Cache Control (BCC), com bits LOCK (0), INV (1), TAG (2),
RAM (3, habilita scratchpad), DBLKSZ (4–5), DS (7), IBLKSZ (8–9), IS1 (11, habilita I-cache),
INTP (12), RDPRI (13), NOPAD (14), BGNT (15), LDSCH (16), NOSTR (17). A BIOS usa SR.IsC
junto com esse registrador para invalidar a I-cache — o RPCS1 precisa reproduzir isso para
que o boot funcione. A BIOS grava `0001E988h` (RAM, DS, IBLKSZ = 4, IS1, RDPRI, NOPAD, BGNT, LDSCH).

### 3.7 I-cache e isolamento

- 4 KiB, mapeamento direto, 256 linhas de 4 palavras; índice = bits 11–4; tag = endereço
  **físico** bits 31–12 + 4 bits de validade (um por palavra). Aliases KUSEG/KSEG0 dividem a tag.
- Falha: preenche da palavra acessada até o fim da linha (palavra 0 → 0Fh, 1 → 0Eh, 2 → 0Ch,
  3 → 08h). IBLKSZ = 0 limita a falha na palavra 0 a duas palavras.
- Escritas na RAM não atualizam a cache: código automodificado roda obsoleto até a limpeza.
- Com SR.IsC = 1, loads/stores vão para a cache. Com BCC.TAG + IS1, um store grava a tag (do
  endereço) e a validade (bits 3–0 do dado), e um load devolve a validade e, no bit 4, se a tag
  coincide. Só com IS1, stores gravam palavras de código.
- Rotina de limpeza da BIOS: com TAG + IS1 e IsC, zera a base de cada linha (0000h–0FF0h); depois,
  só com IS1, zera cada palavra; por fim restaura BCC e SR.

⚠️ O psx-spx se contradiz sobre o caso "tag coincide mas a palavra é inválida" (refill completo ×
a partir da palavra acessada) e não descreve LOCK/INV, IS1 = 0 nem custos de acerto/falha.

---

## 4. Mapa de memória

| Região | KUSEG | KSEG0 (com cache) | KSEG1 (sem cache) | Tamanho |
|---|---|---|---|---|
| RAM principal | `00000000h` | `80000000h` | `A0000000h` | 2048 KiB (primeiros 64 KiB reservados à BIOS) |
| Expansão 1 (DEV0) | `1F000000h` | `9F000000h` | `BF000000h` | 8192 KiB |
| Scratchpad | `1F800000h` | `9F800000h` | — | 1 KiB |
| Portas de E/S | `1F801000h` | `9F801000h` | `BF801000h` | 4 KiB |
| Expansão 2 (DEV8) | `1F802000h` | `9F802000h` | `BF802000h` | 8 KiB |
| Expansão 3 (DEV1) | `1FA00000h` | `9FA00000h` | `BFA00000h` | 2048 KiB |
| BIOS ROM | `1FC00000h` | `9FC00000h` | `BFC00000h` | 512 KiB (máx. 4096 KiB) |
| Controle de cache | — | — | KSEG2: `FFFE0000h` | 0,5 KiB |

- Endereço físico = endereço virtual com os 3 bits superiores removidos
  (`addr & 1FFFFFFFh`) para KUSEG/KSEG0/KSEG1. KUSEG além de 512 MiB gera exceção.
- A RAM de 2 MiB é espelhada nos primeiros 8 MiB (comportamento padrão, controlado por
  `1F801060h`). A BIOS ROM pode ser espelhada nos últimos 4 MiB da região (desativado por padrão).
- `1F801060h` — chamado **DRAM_CTRL** no psx-spx atual (em textos antigos, *RAM_SIZE*):
  valor usual `00000B88h` (alguns modelos `00000888h`; placas PU-7/EARLY-PU-8 `00000B80h`).
- A BIOS guarda o tamanho da RAM em MiB no endereço de RAM `00000060h`.

### 4.1 Janela de RAM (DRAM_CTRL)

O espaço de RAM decodificado tem 16 MiB a partir de `00000000h`, dividido em dois bancos:

- banco /RAS0 = `1 MiB << ((bit9 << 1) | bit11)` → 1, 2, 4 ou 8 MiB;
- banco /RAS1 = `1 MiB << ((bit8 << 1) | bit11)`, somente se o bit 10 = 1; fica logo após /RAS0;
- o restante dos 16 MiB não é mapeado: o acesso gera exceção.

Com o valor usual `0B88h` o banco 0 é configurado como 8 MiB; como os chips têm 2 MiB,
surgem três espelhos acidentais (em 2, 4 e 6 MiB). `0888h` seria o valor "correto" para 2 MiB.
Outros bits: 0–2 retenção de refresh durante DMA, 4–5 período de refresh, 7 estado de espera
entre acessos consecutivos.

### 4.2 Exceções de memória

| Situação | Resultado |
|---|---|
| `LH/LHU/SH` em endereço ímpar, `LW/SW` fora de múltiplo de 4 | AdEL / AdES (BadVaddr atualizado) |
| `LWL/LWR/SWL/SWR` | Nunca geram erro de alinhamento |
| Regiões sem dispositivo, inclusive lacunas da área de E/S | Erro de barramento (IBE na busca, DBE em dados; BadVaddr **não** muda) |
| KUSEG acima de 512 MiB (`20000000h–7FFFFFFFh`) | Exceção |
| Acesso além da janela configurada em um Delay/Size | Erro de barramento |
| Busca de instrução no scratchpad | Erro de barramento na primeira busca |
| Scratchpad via KSEG1 (`BF800000h`) | Não mapeado |

Em uma carga que gera exceção, o registrador de destino não é alterado.

---

## 5. Memory Control

| Endereço | Registrador | Valor usual escrito pela BIOS |
|---|---|---|
| `1F801000h` | Base da Expansão 1 (DEV0) | `1F000000h` |
| `1F801004h` | Base da Expansão 2 (DEV8) | `1F802000h` |
| `1F801008h` | Atraso/tamanho DEV0 | `0013243Fh` |
| `1F80100Ch` | Atraso/tamanho DEV1 (Expansão 3) | `00003022h` |
| `1F801010h` | Atraso/tamanho DEV2 (BIOS ROM) | `0013243Fh` |
| `1F801014h` | Atraso/tamanho DEV4 (SPU) | `200931E1h` (`220931E1h` para leituras de SPU-RAM) |
| `1F801018h` | Atraso/tamanho DEV5 (CD-ROM) | `00020843h` ou `00020943h` |
| `1F80101Ch` | Atraso/tamanho DEV8 (Expansão 2) | `00070777h` |
| `1F801020h` | COM_DELAY | `00031125h`, `0000132Ch` ou `00001325h` |
| `1F801060h` | DRAM_CTRL | `00000B88h` |

Esses registradores definem tempos de acesso de cada dispositivo — relevantes para a
contagem de ciclos (Fase 5) e para jogos sensíveis a temporização.

Formato dos registradores Delay/Size (`1F801008h–1F80101Ch`):

| Bits | Campo |
|---|---|
| 0–3 / 4–7 | Atraso de escrita / leitura (1 a 16 ciclos) |
| 8–11 | Uso dos tempos COM0–COM3 (recuperação, retenção, flutuação, pré-strobe) |
| 12 | Largura do barramento de dados (0 = 8 bits, 1 = 16 bits) |
| 13 | Autoincremento de endereço |
| 16–20 | N = bits de endereço → **janela de 1 << N bytes** |
| 21–23 | Sempre zero |
| 24–27, 29, 30 | Ajustes de temporização de DMA / DMA "largo" de 32 bits |
| 28 | Flag de erro de endereço (escrever 1 limpa) |
| 31 | Espera por dispositivo externo |

Janelas resultantes dos valores usuais: Expansão 1 = 512 KiB (máx. 8 MiB), Expansão 3 = 1 byte
(máx. 2 MiB), BIOS = 512 KiB (máx. 4 MiB), Expansão 2 = 128 bytes (máx. 8 KiB). Os campos de
tamanho parecem ser ignorados para SPU e CD-ROM. A Expansão 2 parece aceitar apenas acessos de
8 bits, e qualquer base diferente de `1F802000h` em `1F801004h` parece desabilitá-la.

COM_DELAY (`1F801020h`): 4 bits para cada tempo COM0–COM3; bits 16–31 lidos como zero.

Uma observação importante sobre escritas estreitas: em `SB`/`SH` a CPU coloca os **32 bits** do
registrador no barramento; alguns registradores de E/S (ex.: DMA) aproveitam o valor inteiro.

---

## 6. Mapa de E/S

| Faixa | Dispositivo |
|---|---|
| `1F801000h–1F801023h` | Memory Control 1 |
| `1F801040h–1F80104Fh` | SIO0 (controles/Memory Cards): DR `+0`, SR `+4`, MR `+8`, CR `+Ah`, BR `+Eh` |
| `1F801050h–1F80105Fh` | SIO1 (porta serial): mesmos registradores + `+Ch` interno |
| `1F801060h` | DRAM_CTRL |
| `1F801070h` / `1F801074h` | I_STAT / I_MASK |
| `1F801080h–1F8010EFh` | DMA canais 0–6 (`1F801080h + n·10h`) |
| `1F8010F0h` / `1F8010F4h` | DPCR / DICR (`1F8010F8h`/`FCh` não documentados) |
| `1F801100h–1F80112Fh` | Timers 0, 1, 2 (`1F801100h + n·10h`) |
| `1F801800h–1F801803h` | CD-ROM (registradores indexados) |
| `1F801810h` | GPU: escrita GP0 / leitura GPUREAD |
| `1F801814h` | GPU: escrita GP1 / leitura GPUSTAT |
| `1F801820h` / `1F801824h` | MDEC0 (comando/dados) / MDEC1 (controle/status) |
| `1F801C00h–1F801D7Fh` | SPU: registradores das 24 vozes (`+10h` por voz) |
| `1F801D80h–1F801DBFh` | SPU: controle global |
| `1F801DC0h–1F801DFFh` | SPU: configuração do reverb |
| `1F801E00h–` | SPU: registradores internos (volumes atuais) |
| `1F802041h` | Expansão 2: display POST de 7 segmentos (progresso do boot) |

O byte POST em `1F802041h` é útil para depuração: a BIOS escreve códigos de progresso nele
durante a inicialização. O RPCS1 deve registrá-los em log (Fase 15).

---

## 7. Interrupções

`I_STAT` (`1F801070h`): leitura = interrupções pendentes; escrita = reconhecimento
(**0 limpa o bit, 1 mantém**). `I_MASK` (`1F801074h`): habilitação, leitura e escrita.

| Bit | IRQ | Origem |
|---|---|---|
| 0 | IRQ0 | VBLANK |
| 1 | IRQ1 | GPU (comando GP0(1Fh)) |
| 2 | IRQ2 | CD-ROM |
| 3 | IRQ3 | DMA |
| 4 | IRQ4 | Timer 0 |
| 5 | IRQ5 | Timer 1 |
| 6 | IRQ6 | Timer 2 |
| 7 | IRQ7 | Controle/Memory Card (byte recebido) |
| 8 | IRQ8 | SIO1 |
| 9 | IRQ9 | SPU |
| 10 | IRQ10 | Lightpen / PIO / DTL (pino compartilhado) |
| 11–15 | — | Sempre 0 |

- Os bits de I_STAT são **disparados por borda**.
- IRQ0, 4, 5 e 6 não exigem reconhecimento na origem; os demais também precisam ser
  reconhecidos no próprio dispositivo (ex.: SIO0 CR bit 4), na ordem I_STAT → dispositivo.
- Ligação com a CPU: se `(I_STAT & I_MASK) != 0`, **CAUSE bit 10** fica ativo. Esse bit não
  é um *latch*: ele cai sozinho quando a condição deixa de valer. A exceção ocorre quando
  SR bit 10 (Im) e SR.IEc estão ligados.

---

## 8. DMA

| Canal | Dispositivo | Uso típico |
|---|---|---|
| 0 | MDECin | RAM → MDEC |
| 1 | MDECout | MDEC → RAM |
| 2 | GPU | Listas de comandos (linked list) e imagens |
| 3 | CD-ROM | CD-ROM → RAM |
| 4 | SPU | RAM ↔ SPU |
| 5 | PIO | Porta de expansão |
| 6 | OTC | Limpeza reversa da *ordering table* |

Registradores por canal (`1F801080h + n·10h`):

- **MADR** (`+0`): endereço base, bits 0–23.
- **BCR** (`+4`): sync 0 → bits 0–15 = quantidade de palavras (0 = 10000h);
  sync 1 → bits 0–15 = tamanho do bloco, 16–31 = número de blocos; sync 2 → não usado.
- **CHCR** (`+8`): bit 0 direção (0 = dispositivo→RAM, 1 = RAM→dispositivo); bit 1 passo
  (0 = +4, 1 = −4); bit 8 *chopping*; bits 9–10 modo de sincronização; 16–18 janela DMA do
  chopping; 20–22 janela da CPU; bit 24 início/ocupado; bit 28 início forçado; bit 29 pausa;
  bit 30 *bus snooping*.

Modos de sincronização: **0 = burst** (tudo de uma vez; OTC e CD-ROM), **1 = slice**
(um bloco por requisição do dispositivo; MDEC, SPU, VRAM da GPU), **2 = linked list**
(listas de comandos da GPU), 3 = reservado.

- **DPCR** (`1F8010F0h`, reset `07654321h`): para cada canal, 3 bits de prioridade + 1 bit de
  habilitação (canal n nos bits 4n..4n+3); bits 28–30 prioridade da CPU.
- **DICR** (`1F8010F4h`): bits 0–6 "IRQ a cada bloco"; bit 15 erro de barramento (força bit 31);
  bits 16–22 habilitação de IRQ por canal; bit 23 habilitação mestre; bits 24–30 flags por canal
  (escrever 1 limpa); bit 31 flag mestre (somente leitura) = bit15 OR (bit23 AND algum bit 24–30).
  A transição 0→1 do bit 31 dispara IRQ3.

### 8.1 Comportamento medido (SCPH-5501, psx.wiki — "dma2", "dma-otc", "dmatime")

- **Início:** CHCR bit 24 + habilitação no DPCR + (DREQ do dispositivo ou bit 28). O bit 28 é limpo
  ao começar. O OTC só começa com o bit 28 (valor usual 11000002h). Habilitar no DPCR um canal já
  armado o inicia. Zerar o bit 24 durante a transferência a interrompe.
- **Fim:** bit 24 limpo. Burst sem chopping: MADR e BCR inalterados; com chopping, MADR avança e
  BCR zera. Slice: MADR = início + BS × BA × 4, BA = 0, BS mantido. Lista: MADR termina no marcador
  00FFFFFFh, BCR ignorado; fim em FFFFFEh ou 800000h liga o bit 15 (erro); lista que aponta para si
  mesma nunca termina; nós vazios são permitidos (~10 ciclos por nó, ~1 por palavra).
- **OTC:** ignora direção, passo e modo; cada entrada contém o endereço da anterior, a última
  00FFFFFFh; BCR 0 = 65 536 entradas; passar de 000000h não grava fora da RAM.
- **CHCR:** bits graváveis 71770703h (canais 0–5) e 51000002h no canal 6 (bit 1 sempre 1); espelho em +Ch.
  SB/SH em BCR e CHCR alteram só os próprios bytes.
- **DICR:** bits 0–5 R/W (bit 6 lê 0); 15 R/W; 16–23 R/W; 24–30 escrever 1 limpa; 31 somente leitura.
  ⚠️ **Diverge do psx-spx:** o flag do canal liga ao fim com a habilitação do canal (bit 16+n),
  **mesmo sem o bit 23**; só o bit 31/IRQ3 exige o bit 23. Ligar o bit 23 com um flag pendente dispara
  o IRQ3; um segundo flag com o bit 31 já ligado não gera nova borda.
- **DPCR:** reset 07654321h; empate de prioridade favorece o canal de número maior; os bits 28–30
  (prioridade da CPU) não tiveram efeito mensurável.
- **1F8010FCh** lê 00FFFFFFh (o psx-spx diz 00FFFFF7h); **1F8010F8h** varia entre execuções.
- **Duração:** OTC ≈1 ciclo/palavra; GPU slice 16×16 ≈ 511 ciclos; SPU burst ≈ 8 ciclos/palavra
  (a tabela do psx-spx diz 4). Durante o DMA a CPU continua rodando código em cache, mas acessos à
  RAM esperam o fim da transferência.

---

## 9. Timers

Três contadores de 16 bits; cada um tem `COUNT` (`+0`), `MODE` (`+4`) e `TARGET` (`+8`) em
`1F801100h + n·10h`.

| Bit de MODE | Nome | Significado |
|---|---|---|
| 0 | GATF | Sincronização habilitada |
| 1–2 | GATM | Modo de sincronização (depende do timer) |
| 3 | ZRET | Zera ao atingir 0 = FFFFh, 1 = TARGET |
| 4 | CMP | IRQ ao atingir TARGET |
| 5 | OVFL | IRQ ao atingir FFFFh |
| 6 | REPT | 0 = uma vez, 1 = repetir |
| 7 | LEVL | 0 = pulso, 1 = alterna |
| 8 | EXTC | Timers 0/1: fonte de clock (0 = sistema, 1 = dotclock/hblank) |
| 9 | PSCL | Timer 2: prescaler (0 = /1, 1 = /8) |
| 10 | INTF | Requisição de IRQ (**0 = sim**, 1 = não); vai a 1 após escrita em MODE |
| 11 | EQUF | Atingiu TARGET (zera na leitura) |
| 12 | OVFF | Atingiu FFFFh (zera na leitura) |

| Timer | Fontes de clock | Sincronização |
|---|---|---|
| 0 | Sistema ou dotclock da GPU | Com hblank (pausar / zerar / zerar e pausar fora / esperar e liberar) |
| 1 | Sistema ou hblank | Mesmos quatro modos, com vblank |
| 2 | Sistema ou sistema/8 | Modos 0/3 param o contador; 1/2 contam livremente |

### 9.1 Comportamento medido (SCPH-5501, psx.wiki — "Root Counter Behavior/Mode Combinations")

- **Reset no alvo** com clock do sistema: período = alvo + 2 ciclos (o 0 aparece por dois ciclos).
  Alvo 0 com reset: contador preso em 0, flag 11 ligado, sem IRQ. Escrever MODE mantém 0 por 2 ciclos.
- **Escrita em MODE:** preserva os bits 0–9, liga o bit 10, zera bits 11–15 e o contador, rearma o
  modo "uma vez". Escrever COUNT também rearma; escrever no contador o valor do alvo liga o flag 11.
- **Flags 11/12:** ligados com ou sem reset; limpos por leitura de MODE de 16/32 bits ou do byte
  alto; leituras do byte baixo de MODE, de COUNT ou de TARGET não os limpam.
- **IRQ:** pulso-repetido = todo evento; pulso-uma vez = só o primeiro; alternância-repetido = um a
  cada dois eventos; alternância-uma vez = só o primeiro (bit 10 fica em 0). Alvo + FFFFh em modo
  repetido: ambos geram IRQ. Escrever 0 no bit 10 não tem efeito.
- **Contagem de 140 000 ciclos** (timer 2, alvo 0100h): modo 0010h → 1 IRQ; 0050h → 3; 0058h → 542;
  0020h → 1 (em FFFFh).
- **Dotclock** reinicia a cada linha: 341, 426, 487, 682, 853 pontos por linha (256…640).
- **Sincronização** (inferência a partir das medições, D-032): reset (modos 1/2) e liberação
  (modo 3) acontecem no **fim** do blank.
- Leituras de COUNT com dotclock/hblank podem ser instáveis (relógios não sincronizados).

---

## 10. GPU

### 10.1 Memória e FIFO

- VRAM de 1 MiB organizada como **1024 × 512 pixels de 16 bits** (15 bits de cor + bit de máscara).
- Cache de textura de 2 KiB (áreas efetivas: 64×64 em 4 bits, 32×64 em 8 bits, 32×32 em 15 bits)
  e cache separada de CLUT; GP0(01h) limpa a cache de textura.
- FIFO de comandos de 64 bytes (16 palavras). E3h–E5h e 00h não ocupam espaço no FIFO.

### 10.2 GPUSTAT (`1F801814h`, leitura)

| Bits | Campo | Origem |
|---|---|---|
| 0–3 | Base X da página de textura (×64) | E1h |
| 4 | Base Y da página de textura (×256) | E1h |
| 5–6 | Semitransparência | E1h |
| 7–8 | Cores da textura (0 = 4 bits, 1 = 8 bits, 2 = 15 bits) | E1h |
| 9 | Dithering 24→15 bits | E1h |
| 10 | Desenho na área exibida permitido | E1h |
| 11 | Força bit de máscara ao desenhar | E6h |
| 12 | Não desenha sobre pixels mascarados | E6h |
| 13 | Campo de entrelaçamento | — |
| 14 | Espelhamento horizontal (apenas GPU v1) | GP1(08h) |
| 15 | Base Y 2 da textura (VRAM de 2 MiB) | E1h |
| 16 | Resolução horizontal 2 (368) | GP1(08h) |
| 17–18 | Resolução horizontal 1 (256/320/512/640) | GP1(08h) |
| 19 | Resolução vertical (240/480) | GP1(08h) |
| 20 | Modo de vídeo (0 = NTSC, 1 = PAL) | GP1(08h) |
| 21 | Profundidade da área exibida (0 = 15 bits, 1 = 24 bits) | GP1(08h) |
| 22 | Entrelaçamento vertical | GP1(08h) |
| 23 | Exibição desligada | GP1(03h) |
| 24 | IRQ1 | GP0(1Fh) / GP1(02h) |
| 25 | Requisição de DMA (depende de GP1(04h)) | — |
| 26 | Pronto para receber comando | — |
| 27 | Pronto para enviar VRAM→CPU | — |
| 28 | Pronto para receber bloco DMA | — |
| 29–30 | Direção do DMA | GP1(04h) |
| 31 | Linha par/ímpar sendo desenhada (entrelaçado) | — |

Após GP1(00h), GPUSTAT lê `14802000h`.

### 10.3 Comandos GP0

Os 3 bits superiores do comando definem o grupo:

| Grupo | Faixa | Função |
|---|---|---|
| 0 | `00h–1Fh` | Diversos: 00h NOP, 01h limpa cache, 02h preenchimento rápido de retângulo, 1Fh IRQ1 |
| 1 | `20h–3Fh` | Polígonos |
| 2 | `40h–5Fh` | Linhas |
| 3 | `60h–7Fh` | Retângulos/sprites |
| 4 | `80h–9Fh` | Cópia VRAM→VRAM |
| 5 | `A0h–BFh` | Cópia CPU→VRAM |
| 6 | `C0h–DFh` | Cópia VRAM→CPU |
| 7 | `E0h–FFh` | Ambiente (E1h–E6h) |

Bits de flag no byte de comando:

| Bit | Polígono | Linha | Retângulo |
|---|---|---|---|
| 24 | Textura "crua" (sem modulação) | — | Textura crua |
| 25 | Semitransparente | Semitransparente | Semitransparente |
| 26 | Texturizado | — | Texturizado |
| 27 | Quadrilátero (senão triângulo) | Polilinha | Tamanho (27–28): variável, 1×1, 8×8, 16×16 |
| 28 | Gouraud | Gouraud | (tamanho) |

- Quadriláteros são desenhados como dois triângulos (vértices 1-2-3 e 2-3-4).
- Polilinhas terminam numa palavra em que `(w & F000F000h) == 50005000h`.
- Linhas incluem o pixel final; retângulos de tamanho variável vão até 1023×511.
- Preenchimento (02h): coordenada X em passos de 16, ignora máscara e área de desenho.
- Cópias e preenchimentos dão a volta nas bordas de 1024×512.

Comandos de ambiente:

| Cmd | Função |
|---|---|
| E1h | Modo de desenho: página de textura, semitransparência, profundidade, dithering, desenho na área exibida |
| E2h | Janela de textura: `tex = (tex AND NOT(máscara·8)) OR ((offset AND máscara)·8)` |
| E3h / E4h | Cantos superior-esquerdo / inferior-direito da área de desenho |
| E5h | Offset de desenho (X e Y com sinal, 11 bits cada) |
| E6h | Máscara: bit 0 força bit 15 = 1; bit 1 não desenha sobre pixels com bit 15 = 1 |

### 10.4 Comandos GP1

| Cmd | Função |
|---|---|
| 00h | Reset da GPU |
| 01h | Reset do buffer de comandos (e da cache de CLUT) |
| 02h | Reconhece IRQ1 |
| 03h | Liga/desliga exibição |
| 04h | Direção do DMA (0 desligado, 1 FIFO, 2 CPU→GP0, 3 GPUREAD→CPU) |
| 05h | Início da área exibida na VRAM |
| 06h | Faixa horizontal de exibição (em ciclos do clock de vídeo) |
| 07h | Faixa vertical de exibição (em linhas) |
| 08h | Modo de exibição (resolução, NTSC/PAL, 24 bits, entrelaçamento) |
| 09h | Endereços Y 512–1023 / VRAM de 2 MiB (GPU v2; não alterado por GP1(00h)) |
| 10h–1Fh | Leitura de registradores internos via GPUREAD (janela de textura, área e offset de desenho; versão na v2) |

### 10.5 Atributos de renderização

- **Semitransparência** (B = fundo, F = primeiro plano): `0,5B + 0,5F`, `B + F`, `B − F`,
  `B + 0,25F`, com saturação em 0 e 255 por canal.
- **Texturas:** CLUT de 4 bits (16 cores), CLUT de 8 bits (256 cores) ou 15 bits direto.
  Páginas de 256×256 texels em X múltiplo de 64, Y múltiplo de 256. Cor `0000h` = transparente;
  o bit 15 da cor da textura controla a semitransparência por texel.
- **Modulação:** `cor = texel × cor_do_vértice / 128` (80h = neutro).
- **Dithering:** matriz 4×4 de deslocamentos entre −4 e +3 aplicados à cor de 8 bits antes de
  reduzir a 5 bits. Polígonos com Gouraud ou textura modulada e todas as linhas são
  "ditherizados"; retângulos nunca.
- **Precisão:** a rasterização usa coordenadas inteiras de 11 bits com sinal e regras próprias
  de preenchimento de borda — a Fase 8 deve validá-las com programas de teste, sem aproximar
  por ponto flutuante.

### 10.6 Temporização e resoluções

| Parâmetro | NTSC | PAL |
|---|---|---|
| Clock de vídeo nominal | 53,693175 MHz | 53,203425 MHz ⚠️ |
| Ciclos de vídeo por linha | 3413 | 3406 |
| Linhas por campo (progressivo / entrelaçado) | 263 / 262,5 | 314 / 312,5 |
| Taxa de quadros resultante (progressivo / entrelaçado) | ≈ 59,826 Hz / 59,940 Hz | ≈ 49,761 Hz / 50,000 Hz |

⚠️ O psx-spx também descreve o clock de vídeo como CPU × 11/7 = 53,222400 MHz (valor usado
em modelos PAL/PSone), o que diverge do valor nominal PAL acima.

**Resolvido na Fase 5 (NTSC):** medições em um SCPH-5501 (psx.wiki, "Display Timing") dão IRQ0 a
cada ≈566 140 ciclos da CPU (±200) e ≈2152,8 ciclos por linha, o que confere com 263 × 3413 ciclos
no cristal de 53,693175 MHz (566 204) e não com CPU × 11/7. Cada console usa o cristal da sua
região, independentemente do modo NTSC/PAL programado. Para PAL não há medição em console PAL.

**Definição de HBLANK/VBLANK (medida):** o HBLANK é a parte da linha fora do intervalo X1..X2 de
GP1(06h); o VBLANK começa na linha Y2 e termina na linha Y1 de GP1(07h) (23 linhas com 16/256). O
período do IRQ0 não muda com Y1/Y2; Y2 fora do quadro, Y1 = Y2 ou 0/263 suprimem o IRQ0. Entrelaçado:
campos de 262/263 linhas (NTSC) e 312/313 (modo PAL). Após GP1(00h): X = 200h..C00h, Y = 10h..100h,
256 pontos, NTSC, sem entrelaçamento.

| Largura | Divisor do dotclock |
|---|---|
| 256 | 10 |
| 320 | 8 |
| 368 | 7 |
| 512 | 5 |
| 640 | 4 |

Altura: 240 ou 480 (480 apenas com entrelaçamento) em NTSC; a faixa vertical de GP1(07h)
centraliza em 88h (NTSC, ±120 linhas) ou A3h (PAL, ±144 linhas).

### 10.7 Regras implementadas e verificadas na Fase 7

- **Tamanho dos comandos GP0:** polígono = 1 + n·(1 + textura) + (n − 1)·gouraud (n = 3 ou 4);
  linha = 3 (plana) ou 4 (gouraud); polilinha termina na primeira palavra de vértice com
  `(w AND F000F000h) = 50005000h`, só a partir do 3º vértice; retângulo = 2 + textura + (tamanho
  variável ? 1 : 0); 80h = 4; A0h e C0h = 3 (+ dados); demais = 1.
- **02h:** X AND 3F0h, Y AND 1FFh, largura `((L AND 3FFh) + 0Fh) AND NOT 0Fh` (3F1h–3FFh → 400h;
  400h → 0), altura AND 1FFh; cor 24 → 15 bits pelos 5 bits altos; ignora máscara, área e offset.
- **80h/A0h/C0h:** X AND 3FFh, Y AND 1FFh; tamanho `((v − 1) AND máscara) + 1` (0 = 1024 × 512);
  halfword baixo primeiro; E6h: não grava sobre bit 15 (bit 1) e grava com bit 15 (bit 0).
- **GPUSTAT** após GP1(00h) = 14802000h. Bit 25 (DREQ) por GP1(04h): 0 → 0, 1 → 1 (FIFO não
  cheio), 2 → bit 28, 3 → bit 27 (medido, "gpustat-dma-bits"). Bit 19 = 1 só com bit 22 = 1.
- **Divisor do dotclock** por GP1(08h): bit 6 → 7; senão bits 0–1 → 10, 8, 5, 4. Largura exibida
  = `(((X2 − X1) / divisor) + 2) AND NOT 3`.
- **Exibição de 24 bits:** cada pixel usa 3 bytes consecutivos da linha (R, G, B), a partir de X × 2.
- Escolhas não documentadas: `docs/DECISIONS.md`, D-040 a D-043.

### 10.8 Regras de renderização implementadas e verificadas na Fase 8

Medições do psx.wiki em SCPH-5501 ("gpu-rasterization", "gpu-primitives") usadas como testes:

| Caso | Medido | RPCS1 |
|---|---|---|
| Triângulo 0,0 / 10,0 / 0,10 | pixels com x + y ≤ 9; mesmo resultado nos dois sentidos | idem ✅ |
| Quadrado dividido em qualquer diagonal ou em leque | cada pixel desenhado uma vez | idem ✅ |
| Triângulos 0,0/200,0/200,1 · 0,0/0,200/1,200 · 0,0/200,1/200,2 | 200 · 199 · 100 pixels | idem ✅ |
| Largura 1023 / 1024; altura 511 / 512 (triângulos e linhas) | desenha / nada | idem ✅ |
| Linha gouraud 00h→FFh em 16 pixels, sem / com dithering | `02468acehjlnprtv` / `02368acfgjknprtv` | idem ✅ |
| Quadrilátero de 16 pixels, U 0→255 | `00 10 20 30 40 50 60 70 80 8F 9F AF BF CF DF EF` | idem ✅ |
| Retângulo 62h cor 808080h sobre 0000h/2108h/5EF7h/7FFFh | 2108h/318Ch/4E73h/5EF7h | idem ✅ |
| Retângulo 67h, texel C210h, mesmos fundos | A108h/B18Ch/CE73h/DEF7h | idem ✅ |
| Janela E2h máscara 1, deslocamento 1 | `08 … 0F 08 …` | idem ✅ |
| Polilinha fechada | cantos desenhados 2 vezes | idem ✅ |

- **Matriz de dithering** (linha = y AND 3, coluna = x AND 3), somada à cor de 8 bits antes de
  saturar e descartar 3 bits: `−4 0 −3 1 / 2 −2 3 −1 / −3 1 −4 0 / 3 −1 2 −2`.
- **Textura:** 4 bits = 4 índices por halfword (o primeiro nos bits 0–3), 8 bits = 2 (o primeiro
  nos bits 0–7); CLUT em X = campo × 16, Y = bits 6–14; texel 0000h transparente, 8000h preto
  opaco; bit 15 do texel controla a semitransparência e é gravado como bit de máscara.
- Escolhas não documentadas: `docs/DECISIONS.md`, D-044 a D-047.

---

## 11. GTE (COP2)

Coprocessador de ponto fixo, acessado por `MFC2/MTC2` (dados), `CFC2/CTC2` (controle),
`LWC2/SWC2` e por comandos `COP2 imm25`.

### 11.1 Registradores

| Dados (cop2r0–31) | | Controle (cop2r32–63) | |
|---|---|---|---|
| 0–5 | VXY0, VZ0, VXY1, VZ1, VXY2, VZ2 | 32–36 | Matriz de rotação RT |
| 6 | RGBC | 37–39 | Translação TRX, TRY, TRZ |
| 7 | OTZ | 40–44 | Matriz de luz L |
| 8–11 | IR0–IR3 | 45–47 | Cor de fundo RBK, GBK, BBK |
| 12–15 | SXY0–SXY2, SXYP (FIFO de tela) | 48–52 | Matriz de cor da luz LR..LB |
| 16–19 | SZ0–SZ3 (FIFO de Z) | 53–55 | Cor distante RFC, GFC, BFC |
| 20–22 | RGB0–RGB2 (FIFO de cor) | 56–57 | OFX, OFY (offset de tela) |
| 23 | RES1 (proibido) | 58 | H (distância de projeção; sem sinal, mas lido com extensão de sinal) |
| 24–27 | MAC0–MAC3 | 59–60 | DQA, DQB (profundidade) |
| 28–29 | IRGB, ORGB | 61–62 | ZSF3, ZSF4 |
| 30–31 | LZCS, LZCR (contagem de zeros/uns à esquerda) | 63 | FLAG |

### 11.2 Comandos

| Op | Nome | Ciclos | | Op | Nome | Ciclos |
|---|---|---|---|---|---|---|
| 01h | RTPS | 15 | | 1Ch | CC | 11 |
| 06h | NCLIP | 8 | | 1Eh | NCS | 14 |
| 0Ch | OP | 6 | | 20h | NCT | 30 |
| 10h | DPCS | 8 | | 28h | SQR | 5 |
| 11h | INTPL | 8 | | 29h | DCPL | 8 |
| 12h | MVMVA | 8 | | 2Ah | DPCT | 17 |
| 13h | NCDS | 19 | | 2Dh | AVSZ3 | 5 |
| 14h | CDP | 13 | | 2Eh | AVSZ4 | 6 |
| 16h | NCDT | 44 | | 30h | RTPT | 23 |
| 1Bh | NCCS | 17 | | 3Dh | GPF | 5 |
| | | | | 3Eh | GPL | 5 |
| | | | | 3Fh | NCCT | 39 |

Campos da palavra de comando: bits 0–5 = opcode real; bit 10 = `lm` (satura IR1–3 em 0..7FFFh
em vez de −8000h..7FFFh); bit 19 = `sf` (desloca o resultado 12 bits); para MVMVA,
bits 17–18 = matriz (rotação, luz, cor, reservada), 15–16 = vetor (V0, V1, V2, IR),
13–14 = translação (TR, BK, FC — com defeito no hardware —, nenhuma). Bits 20–24 contêm um
"opcode falso" ignorado pelo hardware.

### 11.3 FLAG (cop2r63)

| Bit | Condição |
|---|---|
| 31 | Resumo de erro (OR dos bits 30–23 e 18–13) |
| 30–28 | MAC1–3 > 43 bits positivo |
| 27–25 | MAC1–3 > 43 bits negativo |
| 24–22 | IR1–3 saturado |
| 21–19 | FIFO de cor R/G/B saturado (0..FFh) |
| 18 | SZ3 ou OTZ saturado (0..FFFFh) |
| 17 | Overflow de divisão (RTPS/RTPT) |
| 16 / 15 | MAC0 > 31 bits positivo / negativo |
| 14 / 13 | SX2 / SY2 saturado (−400h..3FFh) |
| 12 | IR0 saturado (0..1000h) |

Bits 12–30 são zerados no início de cada comando. MAC0–3 não são saturados (apenas sinalizam).

### 11.4 Divisão de perspectiva

A projeção calcula `H / SZ3` em ponto fixo. O hardware não faz uma divisão exata: normaliza
o divisor pela contagem de zeros à esquerda, obtém uma aproximação inicial do recíproco numa
tabela de 257 entradas e a refina com iterações de Newton-Raphson sem sinal; o resultado é
limitado a `1FFFFh`. Se `SZ3 ≤ H/2` (ou divisão por zero), o resultado é `1FFFFh` e o FLAG
bit 17 é ligado. O RPCS1 deve reproduzir esse algoritmo inteiro bit a bit (Fase 9), pois jogos
dependem dos erros de arredondamento resultantes.

---

## 12. MDEC

| Porta | Escrita | Leitura |
|---|---|---|
| `1F801820h` | Comando/parâmetros (normalmente via DMA0) | Dados decodificados (normalmente via DMA1) |
| `1F801824h` | Controle: bit 31 reset; bit 30 habilita DMA0; bit 29 habilita DMA1 | Status: 31 saída vazia, 30 entrada cheia, 29 ocupado, 28/27 requisições DMA0/1, 26–25 profundidade, 24 com sinal, 23 bit 15, 18–16 bloco atual, 15–0 palavras restantes − 1 |

| Comando (bits 31–29) | Função |
|---|---|
| 1 | Decodificar macrobloco(s): bits 28–27 profundidade (0 = 4 bits, 1 = 8 bits, 2 = 24 bits, 3 = 15 bits), bit 26 com sinal, bit 25 bit 15, bits 15–0 quantidade de palavras |
| 2 | Tabela(s) de quantização: 64 bytes de luminância (+64 de crominância se bit 0 = 1) |
| 3 | Tabela de escala da IDCT: 64 valores de 16 bits com sinal |
| 0, 4–7 | Sem função |

- 4/8 bits: blocos monocromáticos 8×8 (somente Y). 15/24 bits: macroblocos coloridos 16×16
  na ordem Cr, Cb, Y1, Y2, Y3, Y4 (Cr/Cb em meia resolução).
- Dados de entrada por bloco: um valor DC (bits 15–10 = fator de quantização, 9–0 = DC com
  sinal), seguido de pares RLE (bits 15–10 = quantidade de zeros, 9–0 = AC com sinal) em ordem
  *zig-zag*, terminados por `FE00h` (fim de bloco; também usado como preenchimento).
- Conversão YUV→RGB com saturação; saída sem sinal aplica XOR 808080h.
- Fluxo típico de vídeo (STR): CD-ROM → RAM (DMA3) → MDEC (DMA0) → RAM (DMA1) → GPU (DMA2).
  Blocos de DMA0/DMA1 de 20h palavras.

---

## 13. SPU

- RAM de som de **512 KiB** (`00000h–7FFFFh`), 24 vozes, saída estéreo a **44 100 Hz**.
- Cada voz em `1F801C00h + n·10h`:

| Offset | Registrador |
|---|---|
| +0h / +2h | Volume esquerdo / direito (bit 15: 0 = fixo, 1 = varredura) |
| +4h | Pitch (taxa de amostragem; `1000h` = 44 100 Hz; máx. `4000h`) |
| +6h | Endereço inicial (unidades de 8 bytes) |
| +8h / +Ah | ADSR (ataque, decaimento, sustentação, liberação) |
| +Ch | Volume atual do envelope ADSR (0..7FFFh) |
| +Eh | Endereço de repetição (loop) |

- Registradores globais: volume principal `1F801D80h`, volume do reverb `1F801D84h`,
  Key On `1F801D88h`, Key Off `1F801D8Ch`, modulação de pitch `1F801D90h` (voz n modulada
  pela voz n−1), ruído `1F801D94h`, reverb por voz `1F801D98h`, ENDX `1F801D9Ch` (somente leitura),
  início da área de trabalho do reverb `1F801DA2h`, endereço de IRQ `1F801DA4h`,
  endereço de transferência `1F801DA6h`, FIFO de dados `1F801DA8h`, controle (ATTR/SPUCNT)
  `1F801DAAh`, RAM_CTRL `1F801DACh` (deve ser `0004h`), status (STATX/SPUSTAT) `1F801DAEh`,
  volume do CD `1F801DB0h`, volume externo `1F801DB4h`, configuração do reverb `1F801DC0h–1F801DFFh`.
- ATTR: bit 15 liga a SPU, 14 mudo, 13–8 parâmetros do ruído, 7 reverb mestre, 6 IRQ9,
  5–4 modo de transferência (parado, manual, DMA escrita, DMA leitura), 3–0 entradas CD/externa
  e seu envio ao reverb.
- **ADPCM da SPU:** blocos de 16 bytes com 28 amostras de 4 bits. Byte 0 = deslocamento e filtro;
  byte 1 = flags (bit 0 fim de loop → liga ENDX e salta para o endereço de repetição; bit 1 repetir
  — se 0 com bit 0 = 1, força liberação e envelope 0; bit 2 início de loop → grava o endereço de
  repetição). A SPU tem 5 filtros (o XA do CD tem 4). ⚠️ A tabela de coeficientes dos filtros
  da SPU ainda não foi conferida.
- Interpolação gaussiana de 4 pontos; reverb processado a 22 050 Hz.
- Buffers de captura na RAM de som (400h bytes cada): CD esquerdo `00000h`, CD direito `00400h`,
  voz 1 `00800h`, voz 3 `00C00h`.
- IRQ9 dispara quando a SPU acessa o endereço configurado em `1F801DA4h`.

---

## 14. Controlador de CD-ROM

### 14.1 Registradores

O índice (bits 0–1 de `1F801800h`) seleciona o banco:

| Endereço | Leitura (todos os índices) | Escrita idx 0 | Escrita idx 1 | Escrita idx 2 | Escrita idx 3 |
|---|---|---|---|---|---|
| `1800h` | Status (HSTS) | Índice | Índice | Índice | Índice |
| `1801h` | FIFO de resposta | Comando | Dado de som (WRDATA) | Coding info (CI) | Volume R→R (ATV2) |
| `1802h` | FIFO de dados | Parâmetro | Máscara de IRQ | Volume L→L (ATV0) | Volume R→L (ATV3) |
| `1803h` | idx 0/2: máscara de IRQ; idx 1/3: flags de IRQ | Requisição (BFRD/BFWR/SMEN) | Limpa IRQ/flags | Volume L→R (ATV1) | Aplica volumes (ADPCTL) |

HSTS: bits 0–1 índice, 2 ADPCM ocupado, 3 FIFO de parâmetros vazio, 4 FIFO de parâmetros
não cheio, 5 FIFO de resposta não vazio, 6 FIFO de dados não vazio, 7 ocupado.
FIFOs de parâmetro e resposta de 16 bytes.

### 14.2 Comandos

| Cmd | Nome | | Cmd | Nome |
|---|---|---|---|---|
| 01h | Nop / GetStat | | 10h | GetlocL |
| 02h | Setloc | | 11h | GetlocP |
| 03h | Play | | 12h | SetSession |
| 04h | Forward | | 13h | GetTN |
| 05h | Backward | | 14h | GetTD |
| 06h | ReadN | | 15h | SeekL |
| 07h | MotorOn (Standby) | | 16h | SeekP |
| 08h | Stop | | 19h | Test (subfunções) |
| 09h | Pause | | 1Ah | GetID |
| 0Ah | Init | | 1Bh | ReadS |
| 0Bh | Mute | | 1Ch | Reset |
| 0Ch | Demute | | 1Dh | GetQ (não documentado pela Sony) |
| 0Dh | Setfilter | | 1Eh | ReadTOC (não documentado pela Sony) |
| 0Eh | Setmode | | 1Fh | VideoCD (somente SCPH-5903) |
| 0Fh | Getparam | | 50h–57h | Comandos secretos de desbloqueio/bloqueio |

Subfunções relevantes de `19h`: `04h/05h` leitura e contadores SCEx, `20h` data/versão do
firmware do controlador, `22h` string de região do controlador.

### 14.3 Respostas e status

- **INT1** dado pronto (novo setor; ou relatório durante Play); **INT2** conclusão (segunda
  resposta); **INT3** reconhecimento (primeira resposta de todo comando); **INT4** fim de dados;
  **INT5** erro (comando, leitura, licença ou tampa aberta).
- Byte de status: bit 7 tocando, 6 buscando, 5 lendo, 4 tampa aberta, 3 erro de ID, 2 erro de
  busca, 1 motor girando, 0 erro. Códigos de erro: `10h` subfunção/parâmetro inválido, `20h`
  número de parâmetros errado, `40h` comando inválido, `80h` não pode responder ainda / sem disco,
  `04h` falha de busca, `08h` tampa aberta.
- **Setmode:** bit 7 velocidade dupla, 6 envia XA-ADPCM à SPU, 5 tamanho do setor
  (0 = 800h, 1 = 924h), 4 bit "ignorar", 3 filtro XA, 2 relatório, 1 pausa automática,
  0 permite ler setores CD-DA.

### 14.4 GetID (`1Ah`)

| Situação | Respostas |
|---|---|
| Tampa aberta | INT5(11h, 80h) |
| Sem disco | INT3(stat), INT5(08h, 40h, 00h ×6) |
| CD de áudio | INT3, INT5(0Ah, 90h, 00h ×6) |
| Dados sem licença, Mode 1 | INT3, INT5(0Ah, 80h, 00h ×6) |
| Dados sem licença, Mode 2 | INT3, INT5(0Ah, 80h, 20h, 00h ×5) |
| Licenciado, Mode 2 | INT3, INT2(02h, 00h, 20h, 00h, 'S', 'C', 'E', 'I'/'A'/'E') |
| Com modchip, áudio/Mode 1 | INT3, INT2(02h, 00h, 00h, 00h, 'S', 'C', 'E', x) |

Byte 2: bit 7 = negado/sem licença, bit 6 = sem disco, bit 4 = CD de áudio. Byte 3 = tipo
(00h áudio/Mode 1, 20h Mode 2). Bytes 5–8 = string SCEx.

### 14.5 Setores e taxas

- Leitura contínua de **75 setores/s** (1×) ou **150 setores/s** (2×).
- Setor bruto de **2352 bytes** (930h). Dados: sincronismo (12 bytes), cabeçalho
  (minuto, segundo, setor, modo).
  - Mode 1: 2048 bytes de dados + EDC/ECC.
  - Mode 2 Form 1: subcabeçalho (4 bytes, duplicado) + 2048 bytes + EDC/ECC.
  - Mode 2 Form 2: subcabeçalho + 2324 bytes + EDC opcional.
- Subcabeçalho XA: arquivo, canal, submodo (bits: 0 EOR, 1 vídeo, 2 áudio, 3 dados, 4 trigger,
  5 Form 2, 6 tempo real, 7 EOF) e coding info.

### 14.6 XA-ADPCM

- Coding info: bits 0–1 mono/estéreo, bit 2 taxa (0 = 37 800 Hz, 1 = 18 900 Hz), bits 4–5
  4 ou 8 bits por amostra, bit 6 ênfase.
- Setores Form 2 com 18 grupos de 128 bytes (16 bytes de cabeçalho + 112 de dados).
  Cabeçalho: deslocamento (0–12) e filtro (0–3).
- Seleção por `Setfilter` (arquivo, canal) + Setmode bit 3. Reamostragem para 44,1 kHz antes
  da mistura na SPU. Volumes de mistura CD→SPU pelos registradores ATV0–ATV3.

---

## 15. Proteção SCEx, licença e modchips

- Discos originais trazem a string de região (**SCEI** Japão, **SCEA** América do Norte,
  **SCEE** Europa) codificada no sinal *wobble* do sulco, numa área no início do disco.
  Gravadores comuns não reproduzem esse sinal.
- A verificação é feita pelo **firmware do controlador do CD-ROM**, não pela BIOS; a BIOS
  apenas consulta o resultado (GetID) e recusa discos sem licença ou de outra região.
- Já a string de licença (setor 4) e o logotipo da PlayStation (setores 5–11) ficam na área
  de dados e **são** copiados por gravadores e por dumps comuns.
- **Modchips** são microcontroladores que injetam as letras SCEx no circuito da placa,
  imitando o que o drive leria de um disco original. Modchips "stealth" só transmitem em
  momentos específicos (ligar/inserir disco).

**Consequências para o RPCS1 (Fase 14):**

1. Imagens BIN/CUE/ISO **não contêm o wobble**. Portanto, ao carregar uma imagem, o RPCS1
   **não tem como comprovar** se ela veio de um disco original.
2. O "modo original" precisará de um metadado explícito (ex.: região declarada pelo usuário
   ou lida de banco de dados de dumps verificados) e deve ser identificado na interface como
   **simulado**.
3. O "modo modchip" deve atuar no nível do controlador (injeção do SCEx na resposta), sem
   tornar dados corrompidos ou inválidos em um jogo funcional.
4. Nenhuma das situações acima implica que uma cópia sempre produz erro: o resultado depende
   do modelo, da BIOS, da região e da mídia.

---

## 16. BIOS e kernel

- ROM de 512 KiB em `1FC00000h`; execução começa no reset em `BFC00000h` (KSEG1, sem cache).
- Organização descrita pelo psx-spx: parte 1 do kernel executa da ROM a partir de `BFC00000h`;
  parte 2 (em `BFC10000h`) é copiada para a RAM com cache; a "intro/bootmenu" (logotipo, gerenciador
  de Memory Card, CD Player) fica comprimida a partir de `BFC18000h` e é descomprimida para a RAM.
- String de versão em `BFC7FF32h` (exceto na v1.0).
- Mapa da RAM baixa usado pelo kernel:

| Endereço | Conteúdo |
|---|---|
| `00000060h` | Tamanho da RAM em MiB |
| `00000080h` | Vetor de exceção |
| `000000A0h` / `B0h` / `C0h` | Pontos de entrada das funções A(nn), B(nn), C(nn) |
| `00000100h` | Tabela de tabelas do kernel |
| `00000180h` | Argumento de linha de comando do SYSTEM.CNF (80h bytes) |
| `00000200h` | Tabela de saltos das funções A(nn) |
| `00000500h` | Código/dados do kernel realocados |
| `0000E000h` | Memória do kernel (2000h bytes) |
| `00010000h` | Início da memória do usuário |
| `001FFF00h` | Topo de pilha padrão |

- **Chamadas ao kernel:** salto para `A0h`, `B0h` ou `C0h` com o número da função em `r9` (t1);
  argumentos em `r4–r7` e na pilha (`[sp+10h]`...), retorno em `r2`. Funções "SYS" usam a instrução
  `SYSCALL` com o número em `r4`.
- **Fluxo de boot (alto nível):** inicialização do hardware → logotipo/som da Sony → verificação
  de licença e região do disco (via controlador de CD) → leitura do `SYSTEM.CNF` → carregamento
  do PS-EXE indicado → salto para o ponto de entrada. Sem disco, a BIOS exibe o menu
  (Memory Card / CD Player).

O RPCS1 **não** simula a BIOS com uma sequência própria: a apresentação deve surgir da execução
da BIOS real fornecida pelo usuário sobre o hardware emulado (Fases 15 e 16).

---

## 17. PS-EXE e SYSTEM.CNF

**SYSTEM.CNF** (na raiz do disco):

```
BOOT = cdrom:\ABCD_123.45;1 [argumento opcional]
TCB = 4
EVENT = 10
STACK = 801FFF00
```

`TCB` e `EVENT` em hexadecimal (padrões 4 e 10h); `STACK` é o topo da pilha.

**Cabeçalho PS-EXE** (800h bytes; o código começa no offset 800h):

| Offset | Tamanho | Campo |
|---|---|---|
| 000h | 8 | `"PS-X EXE"` |
| 010h | 4 | PC inicial |
| 014h | 4 | GP inicial (r28) |
| 018h | 4 | Endereço de destino na RAM |
| 01Ch | 4 | Tamanho do arquivo sem o cabeçalho (múltiplo de 800h) |
| 020h / 024h | 4 / 4 | Seção de dados (normalmente 0) |
| 028h / 02Ch | 4 / 4 | BSS: início e tamanho (preenchidos com zero, múltiplos de 4) |
| 030h / 034h | 4 / 4 | Base e offset da pilha (SP/FP); 0 = não alterar |
| 04Ch | variável | Marcador de região em ASCII ("Sony Computer Entertainment Inc. for ... area"); a BIOS não o verifica |

O carregamento direto de PS-EXE (sem BIOS completa) será usado nos testes das Fases 3–21.

---

## 18. Controles (SIO0)

- Registradores SIO0 em `1F801040h`: DR (dados), SR (status), MR (modo), CR (controle), BR (baud).
  Taxa padrão ≈ 250 kHz com BR = `0088h` (`33 868 800 / (reload × fator)`).
- CR: bit 0 TX, 1 DTR (seleciona o dispositivo; bit 13 escolhe a porta 1/2), 2 RX, 4 reset de
  erro, 6 reconhece IRQ, 10–12 habilitações de IRQ. SR: 0 TX pronto, 1 RX pronto, 2 TX vazio,
  7 /ACK, 9 IRQ.
- Protocolo: comunicação síncrona, LSB primeiro; o dispositivo puxa **/ACK** por ao menos ~2 µs
  após cada byte que espera continuação, o que gera IRQ7 (⚠️ regras exatas de /ACK: Fase 18).
- Endereço do dispositivo: `01h` controle, `81h` Memory Card.
- Leitura (`42h`): resposta = ID baixo, `5Ah`, botões (2 bytes, **0 = pressionado**) e, em modo
  analógico, 4 bytes de eixos (direito X/Y, esquerdo X/Y; 80h = centro).

| ID | Dispositivo |
|---|---|
| 5A41h | Controle digital (ou analógico em modo digital) |
| 5A73h | Analógico (DualShock) em modo analógico, LED vermelho |
| 5A53h | Analog Stick, LED verde |
| 5AF3h | Modo de configuração |
| 5A12h | Mouse |
| 5A23h | neGcon |

Bits dos botões: 0 Select, 1 L3, 2 R3, 3 Start, 4 ↑, 5 →, 6 ↓, 7 ←, 8 L2, 9 R2, 10 L1,
11 R1, 12 △, 13 ○, 14 ✕, 15 □.

Comandos de configuração do DualShock: `43h` entra/sai do modo de configuração, `44h` define
modo analógico/digital e trava do botão Analog, `45h` consulta o estado, `46h/47h/4Ch` respostas
de identificação, `4Dh` mapeia os bytes de vibração para os motores. ⚠️ O formato exato de `42h`
em modo normal veio de um resumo; deve ser conferido na Fase 18.

---

## 19. Memory Cards

- **128 KiB** = 1024 setores ("frames") de 128 bytes; 16 blocos de 8 KiB (64 frames cada).
- Bloco 0 = diretório; blocos 1–15 = saves.
- Comandos após o endereço `81h`: `52h` ('R') leitura, `57h` ('W') escrita, `53h` ('S') ID
  (somente cartões Sony).
- Leitura: FLAG, `5Ah`, `5Dh`, número do setor (MSB, LSB), `5Ch`, `5Dh`, eco do setor, 128 bytes,
  checksum (XOR de MSB, LSB e dados) e `47h` ('G', sucesso).
- Escrita termina com `47h` (sucesso), `4Eh` (checksum inválido) ou `FFh` (setor inválido).
- FLAG inicial `08h`: bit 3 = "diretório ainda não lido" (zera após uma escrita), bit 2 = erro de escrita.
- Frame 0 do bloco 0: `"MC"` + zeros + checksum (XOR). Frames 1–15: uma entrada por bloco
  (estado `51h` primeiro/único em uso, `52h` meio, `53h` último, `A0h` livre, `A1h–A3h` apagado;
  tamanho; próximo bloco; nome do arquivo; checksum XOR). Frames 16–35: lista de setores defeituosos;
  36–55: setores de reposição; 63: frame de teste de escrita.
- Primeiro frame de cada save: `"SC"`, flag do ícone (11h/12h/13h = 1/2/3 quadros), título
  em Shift-JIS; frames 1–3: ícones 16×16 de 4 bits.
- Temporização aproximada: IRQ7 cerca de 1500 ciclos após cada byte. ⚠️ Atraso adicional na
  leitura de cartões Sony deve ser confirmado na Fase 19.

---

## 20. Regiões, modelos e versões de BIOS

Convenção dos modelos SCPH-xxx**0** (Japão, NTSC-J), xxx**1** (América do Norte, NTSC-U/C) e
xxx**2** (Europa/PAL). Versões de BIOS associadas pelo psx-spx (a versão efetiva será sempre
identificada a partir do arquivo fornecido pelo usuário):

| BIOS | Data na ROM | Modelos |
|---|---|---|
| 1.0J | (sem string) | SCPH-1000 |
| 1.1J | 22/01/1995 | SCPH-3000 |
| 2.0A | 07/05/1995 | DTL-H1001 |
| 2.0E | 10/05/1995 | SCPH-1002 |
| 2.1J / 2.1A / 2.1E | 17/07/1995 | SCPH-3500 / SCPH-1001 / SCPH-1002 |
| 2.2J / 2.2A / 2.2E | 04/12/1995 | SCPH-5000 / SCPH-1001 / SCPH-1002 |
| 3.0J | 09/09/1996 | SCPH-5500 |
| **3.0A** | **18/11/1996** | **SCPH-5501**, SCPH-7003 |
| 3.0E | 06/01/1997 | SCPH-5502, SCPH-5552 |
| 4.0J | 18/08/1997 | SCPH-7000, SCPH-9000 |
| 4.1A | 16/12/1997 | SCPH-7001, 7501, 7503, 9001 |
| 4.1E | 16/12/1997 | SCPH-7002, 7502, 9002 |
| 4.3J / 4.4x / 4.5x | 2000 | PSone (SCPH-100/101/102) |

O perfil inicial do RPCS1 é o **SCPH-5501 com BIOS 3.0A**, condicionado à BIOS fornecida
pelo usuário. Hashes de identificação de BIOS serão compilados na Fase 15 a partir de fonte
verificável (não foram incluídos aqui para evitar erros de transcrição).

---

## 21. Temporização de vídeo por região

| Modo de vídeo / clock | Entrelaçado | Progressivo |
|---|---|---|
| NTSC com clock NTSC | 59,940 Hz | 59,826 Hz |
| PAL com clock PAL | 50,000 Hz | 49,761 Hz |
| NTSC com clock PAL | 59,393 Hz | 59,280 Hz |
| PAL com clock NTSC | 50,460 Hz | 50,219 Hz |

Um console PAL rodando um jogo em modo NTSC (ou vice-versa) apresenta taxas diferentes das
nominais. O RPCS1 deve derivar VBLANK e HBLANK do clock da GPU de acordo com o modelo e o modo
de vídeo programado (Fases 5, 7 e 20).

---

## 22. Fearful Harmony — estado da pesquisa

"Fearful Harmony" é um **nome comunitário** para variantes anormais da sequência de
inicialização do PlayStation (som distorcido/sombrio e tela alterada) relatadas em alguns
consoles. **Nada nesta seção foi verificado tecnicamente pelo RPCS1.**

O que se sabe com segurança, a partir do hardware:

- O som e as imagens de inicialização são produzidos pela própria BIOS (intro/bootmenu),
  executando na CPU, enviando amostras e comandos à SPU e à GPU.
- Durante esse processo a BIOS conversa com o controlador de CD-ROM (GetID, leitura de
  setores, licença). Erros e IRQs do CD-ROM ocorrem em paralelo à animação.

Hipóteses a investigar (Fase 17), **sem pressupor nenhuma**:

1. Falhas de leitura ou respostas de erro do CD-ROM em momentos específicos alteram o fluxo
   da BIOS enquanto a SPU continua reproduzindo/ou repetindo amostras.
2. Corrupção de dados em RAM ou na RAM de som causada por condições de leitura inválida.
3. Dependência de versão de BIOS e de modelo (inclusive diferenças do firmware do controlador).
4. Temporização: o fenômeno pode depender de quando um IRQ chega em relação ao laço da intro.

Requisitos para considerar o recurso implementado:

- O comportamento deve emergir da emulação (CPU + RAM + BIOS + CD-ROM + SPU), **nunca** de
  vídeo, imagem, WAV ou animação pré-gravados.
- Deve existir uma condição de entrada reprodutível (modelo, BIOS, mídia, momento do erro)
  com log de instruções e eventos.
- Idealmente, validação contra capturas de hardware real. Sem isso, o recurso fica marcado
  como **experimental** ou **não implementado**.

---

## 23. Lacunas e pontos a verificar

| # | Item | Fase |
|---|---|---|
| 1 | Clock de vídeo PAL: 53,203425 MHz (nominal) × 53,222400 MHz (CPU × 11/7) — NTSC resolvido por medição; PAL sem medição | 5 ✓ NTSC / ⚠️ PAL |
| 2 | Latências de MULT/DIV por tamanho de operando e penalidades de acesso por região | 3, 5 |
| 3 | Ciclos de LZCS/LZCR e dependências de leitura da GTE durante comandos | 9 |
| 4 | Tabela de coeficientes dos 5 filtros ADPCM da SPU | 11 |
| 5 | Temporização de comandos do CD-ROM (busca, INT1 por setor, Init, GetID) | 12 |
| 6 | Formato de `42h` em modo normal do controle; tempos de /ACK | 18 |
| 7 | Atrasos de leitura de Memory Cards Sony | 19 |
| 8 | Temporização detalhada de DMA (chopping, prioridades, roubo de ciclos da CPU) | 6 |
| 9 | Regras exatas de rasterização (bordas, precisão de UV, dithering) | 8 ✓ bordas, UV, dithering medidos; ⚠️ precisão da cor gouraud |
| 10 | Hashes de BIOS para identificação de versão | 15 |
| 11 | Fearful Harmony: nenhum fato técnico confirmado | 17 |
| 12 | Valor lido de regiões de expansão sem dispositivo (barramento aberto) | 2 ⚠️ adotado FFh |
| 13 | Valores de power-on dos registradores de Memory Control e do BCC | 2 ⚠️ adotados os valores da BIOS / 0 |
| 14 | Efeito de escrita na BIOS ROM | 2 ⚠️ adotado: ignorada |
| 15 | Leituras estreitas de registradores de 32 bits; Memory Control respeita byte enables? | 2, 6 ⚠️ |
| 16 | Banco /RAS1 habilitado sem chips; endereços não documentados de FFFE0000h–FFFE01FFh | 2 ⚠️ adotado: lê 0 |
| 17 | Conflito: `1F801E80h` (SPU) "lê FFh" × regra geral de lacunas com erro de barramento | 11 |
| 18 | Como habilitar o espelho da BIOS nos últimos 4 MiB (adotado: janela > 512 KiB espelha) | 15 |
| 19 | Duas cargas seguidas no mesmo registrador; escrita no load delay slot | 3 ⚠️ derivado do pipeline |
| 20 | Valores de `rt` em BcondZ além de 00h/01h/10h/11h | 3 ⚠️ adotado: bit 0 decide, sem link |
| 21 | Desvio dentro de delay slot | 3 ⚠️ resultado do modelo de pipeline |
| 22 | MTHI/MTLO durante MULT/DIV em andamento; "não tocar HI/LO por ~2 ciclos" | 3 ⚠️ |
| 23 | Valores de GPR, HI e LO após o reset | 3 ⚠️ adotado: zero |
| 24 | Comportamento de COP1/COP3, LWC0/1/3 e SWC0/1/3 | 4 |
| 25 | Ciclos por instrução, custo da I-cache e de buscas sem cache | 5 |
| 26 | Valor de SR após o reset; empilhamento de SR na entrada de exceção | 4 ⚠️ adotado: arquitetura R3000 |
| 27 | MTC0 em registradores somente leitura/inexistentes; CE fora de CpU | 4 ⚠️ adotado: ignorado / zero |
| 28 | COP1/COP3, LWC1/3, SWC1/3 com CU1/CU3 ligados; CFC0/CTC0/BC0x | 4 ⚠️ adotado: sem efeito / RI |
| 29 | Modos LOCK/INV do BCC; I-cache com IS1 = 0; stores de 8/16 bits na cache isolada | 4 ⚠️ |
| 30 | Disparo de breakpoints de hardware (conflito TR × "sempre salta") | 23 |
| 31 | Bits 16–31 de I_STAT/I_MASK ("lixo") | 4 ⚠️ adotado: zero |
| 32 | Posição exata dos eventos dentro da linha (ordem HSYNC × bordas, fase do dotclock e do /8) | 5 ⚠️ |
| 33 | Timer 1 em HBLANK pausado no VBLANK: medido 239/quadro, modelo 240; sysclock reset no VBLANK lido no IRQ0: medido 55 425–57 886, modelo 57 937 | 5 ⚠️ calibração pendente |
| 34 | Tempos de acesso à memória, I-cache e fila de escrita (fórmulas incompletas no psx-spx) | 20 |
| 35 | Taxa PAL citada pelo psx-spx (49,761 Hz) × 49,747 Hz calculados com os mesmos valores | 5 ⚠️ |
| 36 | Paralisação da CPU durante o DMA, chopping e tempo exato por dispositivo | 20 |
| 37 | SH em registradores do DMA: "só os próprios bytes" (medido) × "escrita de 32 bits" (psx-spx) | 6 ⚠️ adotado: medido |
| 38 | Bits 0–5 do DICR (IRQ por bloco/nó?), bit 29 do CHCR, 1F8010F8h, modo 3 reservado | 6 ⚠️ |
| 39 | Fim de lista com outros endereços de bit 23 ligado; cabeçalho enviado ou não ao dispositivo | 6, 7 ⚠️ |
| 40 | Tempo de execução dos comandos da GPU e ocupação real do FIFO (bits 25/26/28) | 8, 20 ⚠️ adotado: instantâneo |
| 41 | Cópia 80h com origem e destino sobrepostos (cópia em blocos) | 8 ⚠️ adotado: pixel a pixel |
| 42 | Metade alta da última palavra de C0h com total ímpar; GPUREAD com LB/LH | 7 ⚠️ adotado: 0 / avança |
| 43 | E1h bit 11 armazenado com GP1(09h) = 0? GP1(00h)/GP1(01h) durante C0h | 7 ⚠️ |
| 44 | Algoritmo exato das linhas (casos de meio passo) e ponto fixo da cor gouraud | 8 ⚠️ D-044/D-045 |
| 45 | Cache de textura: artefato com U ímpar e espelho que começa em U + 1 | 8 ⚠️ não emulado |
| 46 | Ordem dithering × semitransparência; fórmula exata da modulação (v0 × v2) | 8 ⚠️ |
| 47 | E1h bit 10 (desenho na área exibida) em modo entrelaçado; tempo de desenho | 8, 20 |

As escolhas marcadas como "adotado" estão justificadas em `docs/DECISIONS.md` (D-012 a D-047)
e cobertas por testes, para que qualquer correção futura fique visível como mudança de teste.

Itens de 1 a 9 serão resolvidos com programas de teste próprios e/ou suítes de teste públicas
com licença compatível (a verificar caso a caso).

---

## 24. Fontes e procedência

Páginas consultadas (novembro de 2026):

- psx-spx (índice): <https://psx-spx.consoledev.net/>
- Mapa de memória: <https://psx-spx.consoledev.net/memorymap/>
- Memory Control: <https://psx-spx.consoledev.net/memorycontrol/>
- Porta de expansão (PIO): <https://psx-spx.consoledev.net/expansionportpio/>
- Mapa de E/S: <https://psx-spx.consoledev.net/iomap/>
- Interrupções: <https://psx-spx.consoledev.net/interrupts/>
- DMA: <https://psx-spx.consoledev.net/dmachannels/>
- Timers: <https://psx-spx.consoledev.net/timers/>
- CPU: <https://psx-spx.consoledev.net/cpuspecifications/>
- GPU: <https://psx-spx.consoledev.net/graphicsprocessingunitgpu/>
- GTE: <https://psx-spx.consoledev.net/geometrytransformationenginegte/>
- MDEC: <https://psx-spx.consoledev.net/macroblockdecodermdec/>
- SPU: <https://psx-spx.consoledev.net/soundprocessingunitspu/>
- CD-ROM: <https://psx-spx.consoledev.net/cdromdrive/> e <https://psx-spx.consoledev.net/cdromformat/>
- Formatos de arquivo: <https://psx-spx.consoledev.net/cdromfileformats/>
- Controles e Memory Cards: <https://psx-spx.consoledev.net/controllersandmemorycards/>
- SIO: <https://psx-spx.consoledev.net/serialinterfacessio/>
- BIOS/kernel: <https://psx-spx.consoledev.net/kernelbios/>

Procedência: o próprio psx-spx informa que é derivado do documento "Nocash PSX Specifications"
e que não é uma documentação *clean-room*. O RPCS1 o usa **apenas** como referência de fatos de
hardware (endereços, bits, comportamento). Nenhum pseudocódigo das páginas é copiado para o
código-fonte; as implementações são escritas a partir da compreensão do comportamento e
validadas por testes próprios. A licença de reprodução do texto do psx-spx ainda deve ser
verificada antes de citar trechos literais (ver `docs/REFERENCES.md`).

psx.wiki (<https://psx.wiki/>), consultado na Fase 5: páginas de hardware
(<https://psx.wiki/hardware/timers/>, <https://psx.wiki/hardware/gpu/gpustat/>,
<https://psx.wiki/hardware/gpu/video-timing/>) e resultados de testes executados em um SCPH-5501
(<https://psx.wiki/tests/display-timing/>, <https://psx.wiki/tests/timers2/>,
<https://psx.wiki/tests/timers3/>, <https://psx.wiki/tests/timers-ps1-tests/>). Fase 6:
<https://psx.wiki/hardware/dma/>, <https://psx.wiki/tests/dma2/>, <https://psx.wiki/tests/dma-otc/>,
<https://psx.wiki/tests/dmatime/>, <https://psx.wiki/tests/dma-chain-looping/>,
<https://psx.wiki/tests/dma-chopping/>. Só os números medidos e as descrições de comportamento foram
usados; nenhum código de teste foi copiado.
