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

## D-012 — Estado de reset do Memory Control (Fase 2) ⚠️
- **Contexto:** o psx-spx documenta os valores que a BIOS grava, não os de power-on
  (diz apenas que os atrasos começam no máximo).
- **Decisão:** o reset usa os valores usuais da BIOS (`DRAM_CTRL = 0B88h`, janelas de 512 KiB,
  1 byte, 512 KiB e 128 bytes...) e `BCC = 0`.
- **Motivo:** permite executar PS-EXE de teste sem BIOS (Fases 3+) com o mapa usual. Como a
  temporização ainda não é emulada, os campos de atraso não têm efeito.

## D-013 — Barramento aberto das regiões de expansão lê FFh ⚠️
- **Contexto:** sem cartucho, o valor lido das Expansões 1/2/3 não é documentado.
- **Decisão:** dentro da janela configurada, leituras retornam FFh por byte; escritas são descartadas.
- **Consequência:** a BIOS não encontrará a string de licença em `1F000004h`/`1F000084h`, como
  num console sem cartucho. Rever se houver medição em hardware.

## D-014 — Escritas estreitas em E/S entregam o registrador inteiro
- **Fato (psx-spx):** em SB/SH o R3000A coloca os 32 bits do registrador no barramento.
- **Decisão:** `IoDevice::io_write` recebe o valor completo + largura + deslocamento; cada
  dispositivo decide. RAM/scratchpad gravam só os bytes endereçados. O Memory Control trata
  escritas estreitas como escrita de 32 bits (⚠️ não documentado).

## D-015 — Periféricos ainda não implementados são marcadores explícitos
- **Decisão:** `UnimplementedDevice` ocupa a faixa de cada periférico: lê 0, descarta escritas,
  conta acessos e avisa no log uma vez por registrador, citando a fase prevista.
- **Motivo:** o barramento fica completo e os acessos ficam visíveis, sem fingir comportamento.
  Lacunas entre periféricos geram erro de barramento, conforme o psx-spx.

## D-016 — Escrita na BIOS ROM é descartada ⚠️
- Não documentado. A escrita é ignorada sem exceção e gera um aviso único no log.

## D-017 — Casos não documentados com leitura zero ⚠️
- Banco /RAS1 habilitado (consoles de varejo não têm chips nele) e endereços do bloco
  `FFFE0000h–FFFE01FFh` diferentes de `FFFE0130h`: leitura 0, escrita descartada, aviso único.
- RAM e scratchpad começam zerados no `power_on()` (no hardware o conteúdo é indefinido),
  para que testes e futuros save states sejam determinísticos.

## D-018 — `CHECK_EQ` copia os operandos
- **Contexto:** na Fase 2 um teste derrubou o processo (0xC0000005): o macro guardava
  `const auto&` para `synthetic_image(...)[i]`, uma referência para dentro de um vetor temporário.
- **Decisão:** os operandos são copiados e comparados por função auxiliar; stdout dos testes
  sem buffer para que uma queda não esconda a saída anterior.

## D-019 — Modelo do load delay slot (Fase 3) ⚠️ em parte
- **Documentado:** a instrução seguinte vê o valor antigo; a carga se completa mesmo se houver
  exceção logo depois; LWL/LWR encadeadas no mesmo registrador veem o valor pendente.
- **Derivado do pipeline de 5 estágios (não documentado):** uma escrita imediata (ALU, link)
  pela instrução do delay slot no mesmo registrador prevalece e descarta a carga; duas cargas
  seguidas no mesmo registrador NÃO se cancelam — a primeira se completa ao fim da segunda.
- **Implementação:** carga "entrante" (da instrução anterior) aplicada ao fim do passo atual,
  e carga "pendente" (desta instrução) aplicada ao fim do próximo.

## D-020 — BcondZ com `rt` não documentado ⚠️
- Bit 0 de `rt` decide BLTZ/BGEZ; link só quando `rt AND 1Eh = 10h` (10h/11h).

## D-021 — Desvio em delay slot ⚠️
- Sem tratamento especial: o resultado emerge do modelo (o alvo do primeiro desvio executa
  como delay slot do segundo). Coberto por teste para tornar visível qualquer mudança futura.

## D-022 — MTHI/MTLO durante MULT/DIV ⚠️
- O resultado de MULT/DIV é calculado na emissão; o atraso só afeta MFHI/MFLO (bloqueio).
  MTHI/MTLO gravam imediatamente. A observação do psx-spx de "não tocar HI/LO por ~2 ciclos"
  não é modelada.

## D-023 — Exceções na Fase 3: detecção sem despacho
- Todas as exceções das instruções sem coprocessador são detectadas com EPC, BD, BT, TAR e
  BadVaddr corretos (`ExceptionRecord`), mas a CPU **para** na instrução causadora em vez de
  desviar ao vetor: SR/CAUSE/EPC no COP0, vetores e RFE são a Fase 4.
- Instruções de coprocessador (COP0–3, LWCn, SWCn) param como "não implementadas", citando a
  fase (4 ou 9), e não consomem ciclo. GPRs, HI e LO começam zerados no reset (⚠️ não documentado).

## D-024 — Modelo de ciclos provisório e SWL/SWR em E/S
- Ciclos = 1 por instrução + bloqueio de MFHI/MFLO conforme a latência documentada.
  Tempos de acesso à memória, I-cache e fila de escrita entram na Fase 5 (e 4, para a cache).
- SWL/SWR parciais viram escritas de byte (correto para RAM; ⚠️ em E/S o resultado é
  desconhecido segundo o psx-spx).

## D-010 — MSVC como compilador de referência
- **Decisão:** Visual Studio 2022 (MSVC 19.44) é o compilador verificado. O código evita
  extensões específicas, mas nenhum outro compilador foi testado ainda (o MinGW disponível na
  máquina não possui `make`/`ninja`).
