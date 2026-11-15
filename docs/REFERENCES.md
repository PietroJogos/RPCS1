# Referências, dependências e procedência

Regra do projeto: toda dependência e toda documentação utilizada tem sua origem e licença
registradas aqui **antes** de ser usada.

## Dependências de software

| Item | Uso | Licença | Situação |
|---|---|---|---|
| Biblioteca padrão C++20 e runtime do MSVC | Todo o código; vinculado estaticamente (`/MT`) | Termos de licença do Visual Studio para o runtime redistribuível | Em uso |
| API Win32 (`user32`, `gdi32`, `shell32`, `kernel32`) | Janela, linha de comando | Componente do sistema operacional | Em uso |
| CMake | Build | BSD-3-Clause | Ferramenta de build (não distribuída) |

Nenhuma biblioteca de terceiros é compilada ou distribuída com o RPCS1 neste momento.

## Documentação técnica

| Fonte | Uso | Procedência / licença |
|---|---|---|
| psx-spx — <https://psx-spx.consoledev.net/> | Fatos de hardware (endereços, bits, comportamento) | Derivado do "Nocash PSX Specifications" (Martin Korth); o próprio site declara não ser *clean-room*. Licença de reprodução do texto **a verificar** antes de citar trechos literais. O RPCS1 não copia pseudocódigo para o código-fonte. |
| psx.wiki — <https://psx.wiki/> | Conferência cruzada (planejada) | Ainda não consultado; licença a verificar |

## Proibido no repositório

- BIOS, jogos, imagens de disco ou qualquer arquivo protegido da Sony.
- Código de outros emuladores (DuckStation, ePSXe, Mednafen/Beetle PSX, PCSX, PCSX-Redux etc.).
- Chaves ou dados proprietários.

O `.gitignore` bloqueia extensões típicas (`*.bin`, `*.cue`, `*.iso`, `*.mcd`...) como proteção
adicional.
