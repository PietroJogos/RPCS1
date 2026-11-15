# RPCS1

**Emulador original de PlayStation 1 para Windows 10 e 11 — desenvolvido pelo PiStudio BR.**

> ⚠️ **Estado atual: pré-alfa (Fase 4 — CPU com COP0).** Memória, barramento, CPU, COP0,
> exceções, interrupções e I-cache estão emulados e testados. A BIOS configurada é executada até a
> primeira instrução da GTE (Fase 9) ou até o limite de passos — sem timers, GPU e demais
> periféricos ela não chega ao fim da inicialização. Não há vídeo nem áudio: a janela exibe tela
> preta. **Nenhum jogo, BIOS ou programa de PS1 roda por completo.**

## Objetivo

Reproduzir fielmente o comportamento interno do PlayStation 1 "Fat" — CPU, memória, GPU, GTE,
MDEC, SPU, CD-ROM, DMA, timers, controles e Memory Cards — inicializando a partir de uma BIOS
legítima fornecida pelo próprio usuário. O núcleo é escrito do zero, sem código de outros
emuladores. Veja o [roadmap das 26 fases](docs/ROADMAP.md) e o [progresso](docs/PROGRESS.md).

## O que existe hoje

- **Fase 4:** COP0 (SR, CAUSE, EPC...), despacho de exceções, interrupções de hardware e
  software, modo usuário, I-cache, isolamento de cache e controlador de interrupções.
- **Fase 3:** interpretador da CPU R3000A com delay slots, load delay e MULT/DIV com latência
  documentada.
- **Fase 2:** barramento com segmentos KUSEG/KSEG0/KSEG1/KSEG2, RAM de 2 MiB (espelhos pelo
  DRAM_CTRL), scratchpad, BIOS ROM de 512 KiB, Memory Control, regiões de expansão, display
  POST, erros de alinhamento e de barramento. Periféricos aparecem como "não implementados" no log.
- Build CMake/C++20 para Windows x64 (Visual Studio 2022), sem dependências externas.
- Sistema de logging com níveis e categorias por componente, em console e arquivo.
- Tratamento de erros por valor, asserções e registro de falhas fatais.
- Configuração `rpcs1.ini` validada e comentada, criada na primeira execução.
- Janela Win32 DPI-aware com apresentação de framebuffer em proporção 4:3.
- 191 testes unitários e 7 testes de fumaça do executável.
- Documentação técnica do hardware: [docs/HARDWARE_ARCHITECTURE.md](docs/HARDWARE_ARCHITECTURE.md).

## Compilar

Requisitos: Windows 10/11 x64, Visual Studio 2022 (carga de trabalho "Desenvolvimento para
desktop com C++") e CMake ≥ 3.24.

```bash
cmake --preset windows-msvc
```

```bash
cmake --build --preset release
```

```bash
ctest --preset release
```

O executável fica em `build/msvc/bin/Release/RPCS1.exe`.

## Usar

```bash
RPCS1.exe --help
```

| Opção | Efeito |
|---|---|
| `--config <arq>` | Usa outro arquivo de configuração (padrão: `rpcs1.ini` ao lado do executável) |
| `--log-level <nível>` | `trace`, `debug`, `info`, `warn`, `error`, `fatal` ou `off` |
| `--headless` | Inicializa sem abrir janela (testes) |
| `--exit-after <ms>` | Fecha sozinho após o tempo indicado (testes) |
| `--version` | Mostra a versão |

Códigos de saída: `0` sucesso, `1` erro em tempo de execução, `2` uso incorreto, `3` configuração inválida.

## BIOS, jogos e aspectos legais

- O RPCS1 **não distribui** BIOS, jogos ou qualquer arquivo protegido da Sony.
- Use apenas BIOS extraída do seu próprio console e imagens de discos que você possui legalmente.
- "PlayStation" é marca registrada da Sony Interactive Entertainment. O RPCS1 é um projeto
  independente, sem afiliação com a Sony.

## Documentação

| Documento | Conteúdo |
|---|---|
| [HARDWARE_ARCHITECTURE.md](docs/HARDWARE_ARCHITECTURE.md) | Referência técnica do hardware do PS1 |
| [ROADMAP.md](docs/ROADMAP.md) | As 26 fases, entregáveis e critérios de aceitação |
| [PROGRESS.md](docs/PROGRESS.md) | Estado real de cada fase |
| [TEST_RESULTS.md](docs/TEST_RESULTS.md) | Resultados dos testes executados |
| [DECISIONS.md](docs/DECISIONS.md) | Decisões técnicas e seus motivos |
| [REFERENCES.md](docs/REFERENCES.md) | Fontes, dependências e licenças |

## Licença

Ver [LICENSE](LICENSE). A licença de distribuição do código ainda não foi definida pelo
PiStudio BR.
