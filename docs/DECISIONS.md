# Decisões técnicas do RPCS1

Registro das decisões de arquitetura, com motivo e consequências. Novas decisões são
acrescentadas no fim; decisões revistas são marcadas como substituídas, não apagadas.

## D-001 — Nenhuma dependência externa na Fase 0
- **Contexto:** o plano sugere SDL3 e Catch2/GoogleTest. Nenhum dos dois estava instalado e
  baixá-los exigiria buscar código de terceiros na rede durante o build.
- **Decisão:** usar apenas a biblioteca padrão C++20 e a API do Windows.
- **Consequência:** builds reprodutíveis e offline; `third_party/` vazio. Bibliotecas genéricas
  podem ser adicionadas depois, com licença registrada em `docs/REFERENCES.md`.

## D-002 — Framework de testes próprio
- **Decisão:** `tests/framework/rpcs1_test.hpp` (TEST_CASE, CHECK, REQUIRE, CHECK_EQ,
  CHECK_THROWS, TempDir), ~200 linhas.
- **Motivo:** D-001; necessidades atuais são simples.
- **Revisão:** migrar para Catch2 (licença BSL-1.0) se surgirem necessidades como testes
  parametrizados extensos ou benchmarks.

## D-003 — Janela Win32 nativa em vez de SDL3 (por enquanto)
- **Decisão:** `src/video/win32_window.cpp` com `user32`/`gdi32`; apresentação por
  `StretchDIBits` (renderização em CPU, como pede o plano).
- **Consequência:** a interface `video::Window` isola a plataforma; um backend SDL3 pode ser
  adicionado sem alterar o restante. Áudio (WASAPI) e entrada (XInput/Raw Input) seguirão o
  mesmo padrão, ou SDL3 se for adotado.

## D-004 — Executável com subsistema de console
- **Decisão:** o `RPCS1.exe` usa o subsistema de console para exibir logs e permitir testes
  automatizados que leem a saída.
- **Revisão:** na Fase 22 (interface), avaliar subsistema GUI com console opcional.

## D-005 — Erros como valores (`Result<T>`/`Status`)
- **Decisão:** erros esperados (arquivo ausente, configuração inválida, BIOS de tamanho errado)
  são valores; exceções ficam para falhas de programação.
- **Observação:** `Result<T>::value() &&` retorna **por valor** — retornar `T&&` gerava
  referência pendente em `const auto& x = f().value();` (bug detectado pelos testes na
  primeira execução).

## D-006 — Asserções sempre ativas
- **Motivo:** continuar emulando com estado interno inconsistente produz resultados
  enganosos, o que contraria a regra de não falsificar resultados.

## D-007 — Texto em UTF-8 internamente
- **Decisão:** `std::string` UTF-8 em todo o código; conversão para UTF-16 apenas na fronteira
  com a API do Windows. Argumentos de linha de comando via `GetCommandLineW`; manifesto com
  `activeCodePage = UTF-8`; console configurado para UTF-8.

## D-008 — Configuração em INI com gravação atômica
- **Decisão:** `rpcs1.ini` legível, comentado e com caminhos relativos à pasta do arquivo.
  Toda gravação usa arquivo temporário + renomear, padrão que será reutilizado pelos Memory Cards.

## D-009 — Interpretador antes de qualquer recompilador
- **Decisão:** a CPU será um interpretador fiel (Fase 3). Um recompilador só será estudado na
  Fase 24 e terá de ser validado contra o interpretador.

## D-011 — Runtime do MSVC vinculado estaticamente
- **Decisão:** `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded[Debug]` (`/MT`, `/MTd`).
- **Motivo:** o executável distribuído funciona em Windows 10/11 sem instalar o Visual C++
  Redistributable.

## D-010 — MSVC como compilador de referência
- **Decisão:** Visual Studio 2022 (MSVC 19.44) é o compilador verificado. O código evita
  extensões específicas, mas nenhum outro compilador foi testado ainda (o MinGW disponível na
  máquina não possui `make`/`ninja`).
