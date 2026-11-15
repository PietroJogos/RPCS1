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
| psx-spx — <https://psx-spx.consoledev.net/> | Fatos de hardware (endereços, bits, comportamento) | Derivado do "Nocash PSX Specifications" (Martin Korth). ⚠️ A página inicial declara que o documento **não é clean-room** e que contém texto copiado ou reescrito de material confidencial da Sony. O RPCS1 usa apenas fatos de comportamento do hardware, sem reproduzir texto nem pseudocódigo; a avaliação jurídica dessa procedência cabe ao PiStudio BR antes da distribuição ampla. Licença de reprodução do texto **a verificar**. |
| psx.wiki — <https://psx.wiki/> | Fase 5: descrições de hardware e **resultados de testes medidos em um SCPH-5501** (timers, temporização de vídeo, GPUSTAT) | Nenhuma licença declarada nas páginas lidas (a licença geral do site não foi verificada). Usados apenas números medidos e descrições de comportamento; nenhum código de teste foi copiado. Os programas de teste citados (gpudisp, timers2, timers3, ps1-tests) não foram obtidos nem executados. |

## Proibido no repositório

- BIOS, jogos, imagens de disco ou qualquer arquivo protegido da Sony.
- Código de outros emuladores (DuckStation, ePSXe, Mednafen/Beetle PSX, PCSX, PCSX-Redux etc.).
- Chaves ou dados proprietários.

O `.gitignore` bloqueia extensões típicas (`*.bin`, `*.cue`, `*.iso`, `*.mcd`...) como proteção
adicional.
