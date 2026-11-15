# Changelog

Formato baseado em "Keep a Changelog". Versões seguem SemVer; antes da 0.1.0 todas são pré-lançamentos.

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
