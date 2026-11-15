# Resultados de testes

Registro dos testes **realmente executados**. Cada entrada indica ambiente, comando e resultado.

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
