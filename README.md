# RPCS1

**Emulador original de PlayStation 1 para Windows 10 e 11 — desenvolvido pelo PiStudio BR.**

> ⚠️ **Estado atual: pré-alfa (Fase 0 — fundação).** Nenhum componente do PlayStation é
> emulado ainda. O executável abre uma janela com tela preta (sem sinal de vídeo), lê e valida
> a configuração e encerra corretamente. **Nenhum jogo, BIOS ou programa de PS1 roda nesta versão.**

## Objetivo

Reproduzir fielmente o comportamento interno do PlayStation 1 "Fat" — CPU, memória, GPU, GTE,
MDEC, SPU, CD-ROM, DMA, timers, controles e Memory Cards — inicializando a partir de uma BIOS
legítima fornecida pelo próprio usuário. O núcleo é escrito do zero, sem código de outros
emuladores. Veja o [roadmap das 26 fases](docs/ROADMAP.md) e o [progresso](docs/PROGRESS.md).

## O que existe hoje (Fase 0)

- Build CMake/C++20 para Windows x64 (Visual Studio 2022), sem dependências externas.
- Sistema de logging com níveis e categorias por componente, em console e arquivo.
- Tratamento de erros por valor, asserções e registro de falhas fatais.
- Configuração `rpcs1.ini` validada e comentada, criada na primeira execução.
- Janela Win32 DPI-aware com apresentação de framebuffer em proporção 4:3.
- 61 testes unitários e 6 testes de fumaça do executável.
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
