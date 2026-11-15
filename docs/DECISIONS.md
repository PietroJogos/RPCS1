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

## D-025 — SR no reset e na entrada de exceção (Fase 4) ⚠️
- **Reset:** SR = BEV (400000h): kernel, interrupções desligadas, vetores em ROM — convenção da
  arquitetura R3000; o psx-spx não informa o valor. A BIOS grava SR logo no início.
- **Entrada em exceção:** (KUc, IEc) → (KUp, IEp) → (KUo, IEo), entrando em modo kernel sem
  interrupções — o inverso do RFE documentado. CE fica zero fora de CpU.

## D-026 — Regras de acesso ao COP0 não documentadas ⚠️
- MTC0 em TAR, BadVaddr, EPC, PRID e registradores inexistentes é ignorado; leitura de r16–r31
  devolve zero (no hardware é imprevisível). Comandos COP0 além de RFE e CFC0/CTC0/BC0x geram RI.

## D-027 — COP1/COP3 inexistentes ⚠️
- Com CU1/CU3 desligados geram CpU (CE = 1/3). Com eles ligados, o comportamento não é
  documentado: as instruções não têm efeito.

## D-028 — Modelo da I-cache e do isolamento ⚠️ em parte
- **Documentado e implementado:** geometria, tag física, preenchimento até o fim da linha,
  IBLKSZ, KSEG1 sem cache, código obsoleto após escrita na RAM, isolamento com TAG/IS1.
- **Escolhas:** tag coincidente com palavra inválida soma a nova validade à existente (o psx-spx
  se contradiz); IBLKSZ = 2–3 tratado como 4; com IS1 = 0 a cache não é usada e stores isolados são
  descartados; SB/SH isolados gravam o registrador inteiro; bits 31–5 do load de tag leem zero;
  LOCK/INV e os bits RAM/DS do scratchpad não têm efeito; o KSEG2 não é afetado pelo isolamento.
- **Ausente:** custos de acerto e falha (Fase 5).

## D-029 — Controlador de interrupções antecipado para a Fase 4
- A especificação da Fase 4 inclui "interrupções da CPU" e "máscaras de interrupção"; para testar
  o caminho completo, I_STAT/I_MASK foram implementados agora (antes previstos na Fase 5).
- ⚠️ Bits 16–31 ("lixo" no hardware) leem zero; escritas estreitas usam o registrador inteiro (D-014).
- A classe `system::Console` passa a possuir e interligar barramento, controlador e CPU.

## D-030 — Breakpoints de hardware adiados
- BPC/BDA/BDAM/BPCM/DCIC são armazenados com as máscaras documentadas, mas não disparam: o
  psx-spx é contraditório sobre o bit TR, e o uso principal é depuração (Fase 23).

## D-031 — Modelo da temporização de vídeo (Fase 5)
- **Medido e implementado:** cristal por região do console; 3413/3406 ciclos de vídeo por linha
  conforme o modo; 263/314 linhas (entrelaçado 263/262 e 313/312); HBLANK fora de X1..X2; VBLANK de
  Y2 a Y1 com IRQ0 no início; dotclock reiniciado a cada linha; conversão inteira exata CPU ↔ vídeo.
- **Escolhas ⚠️:** HSYNC processado antes das bordas na mesma posição; dotclock com ticks em
  x = k × divisor; campo longo primeiro no entrelaçado; novos parâmetros de GP1 aplicados no próximo
  HSYNC (o dotclock recomeça ali); com Y1 = Y2 o término prevalece; VBLANK/HBLANK continuam com a
  exibição desligada (desconhecido no psx-spx).

## D-032 — Sincronização dos timers no fim do blank (inferido)
- O psx-spx diz apenas "zerar no blank". As medições do psx.wiki (timer 0 em sincronização 1 lido
  no início do HBLANK ≈ duração da parte visível; timer 1 em sysclock sincronização 1 lido no IRQ0 ≈
  tempo visível mod 65 536; sincronização 2 contando a partir do VBLANK e lendo 0 depois dele)
  só são consistentes com reset/liberação no **fim** do blank. Adotado para os modos 1, 2 e 3.

## D-033 — Divergências conhecidas em relação às medições
- Timer 1 (HBLANK) pausado no VBLANK: medido 239 por quadro; o modelo dá 240 (Y2 − Y1).
- Timer 1 (sistema) com reset no VBLANK lido no IRQ0: medido 55 425–57 886; o modelo dá 57 937.
- Origem provável: posição do VBLANK dos timers dentro da linha, ou custo das instruções de leitura
  no hardware (o RPCS1 ainda conta 1 ciclo por instrução). Os testes registram o valor do modelo e
  citam o medido; a calibração exige analisar os registros brutos dos testes (não lidos ainda).

## D-034 — Tempos de acesso à memória adiados
- O psx-spx não dá as fórmulas completas (BIOS/expansões, I-cache, fila de escrita). Inventar números
  violaria a regra de não falsificar; a CPU continua com 1 ciclo por instrução + bloqueios de
  MULT/DIV. A calibração de temporização foi movida para a Fase 20, com testes próprios.

## D-035 — Outras escolhas dos timers ⚠️
- Bit 8 seleciona a fonte só nos timers 0/1 e bit 9 só no timer 2 (combinações não documentadas).
- Prescaler /8 em fase com o tempo absoluto; pulso do bit 10 com 2 ciclos; alvo 0 nunca gera IRQ;
  alvo = FFFFh gera um único IRQ; bits 16–31 lidos como zero; +Ch de cada timer lido como zero;
  reset no alvo com fontes lentas aplicado no mesmo tick (o hardware reseta 1 ciclo depois).

## D-036 — Modelo de tempo do DMA (Fase 6)
- **Burst, slice e OTC:** os dados são movidos no instante do início; o canal permanece ocupado
  (CHCR bit 24) pela duração estimada e o flag/IRQ3 ocorre ao fim, via scheduler.
- **Lista encadeada:** avança nó a nó no tempo emulado (10 ciclos por nó + 1 por palavra, medido),
  processada em fatias de até 2048 ciclos por evento; uma lista em laço fica ocupada para sempre
  sem travar o emulador, e a CPU roda entre as fatias.
- **Duração por palavra:** tabela do psx-spx (1 para MDEC/GPU/OTC, 24 CD-ROM, 4 SPU, 20 PIO).
  ⚠️ Medições divergem (SPU ≈8; GPU slice ≈2); o ritmo real virá do DREQ de cada dispositivo
  quando ele for implementado (Fases 7, 10–12).
- **CPU não paralisada** durante o DMA e chopping sem efeito no tempo: dependem dos tempos de
  acesso à memória (Fase 20). Ler o destino antes do fim já mostra os dados novos.

## D-037 — Escritas estreitas nos registradores do DMA
- Medido no SCPH-5501: SB/SH em BCR e CHCR alteram só os próprios bytes. O psx-spx menciona SH em
  registradores do DMA tratado como escrita de 32 bits. Adotada a medição, aplicada a todos os
  registradores do DMA (⚠️ MADR/DPCR/DICR não foram medidos). No DICR, flags de bytes não escritos
  não são limpos.

## D-038 — Outras escolhas do DMA ⚠️
- 1F8010F8h lê 0 (varia no hardware); modo de sincronização 3 tratado como burst; fim de lista com
  qualquer endereço de bit 23 ligado diferente de FFFFFFh = erro de barramento; o cabeçalho da lista
  não é enviado ao dispositivo; acesso fora da RAM liga o bit 15 do DICR (inclusive o OTC que passa de
  000000h); bits 0–5 do DICR e o bit 29 do CHCR são armazenados sem efeito.
- Canais sem dispositivo usam `PlaceholderPort`: DREQ ativo, escritas descartadas, leitura 0 (PIO lê
  FFFFFFFFh, medido) e aviso único no log citando a fase.

## D-039 — Limite de paralelismo na compilação
- Em máquinas com pouca memória livre o MSVC falha com C1060. A opção `RPCS1_MSVC_MP_JOBS`
  (vazio = todos os núcleos, 1 = desliga /MP) permite compilar nesses casos.

## D-040 — Execução instantânea dos comandos da GPU (Fase 7) ⚠️
- Cada comando GP0 é executado no instante em que sua última palavra chega; não há tempo de
  execução, o FIFO de 16 palavras nunca enche e GPUSTAT bit 25 com GP1(04h) = 1 ("FIFO não cheio")
  fica sempre ligado. Os bits 26 e 28 caem só enquanto um comando está incompleto (bit 28: só
  polígonos e linhas) ou durante uma transferência A0h (bit 26).
- Motivo: os tempos de desenho e de transferência dependem do rasterizador (Fase 8) e da
  sincronização fina (Fase 20); um tempo inventado agora seria um palpite.

## D-041 — GPU emulada: versão 2 (Fase 7)
- O SCPH-5501 usa a GPU "nova" (208 pinos). Adotado o comportamento v2: GP1(10h–1Fh) com índice nos
  bits 0–3, versão = 2 no índice 7, índice 8 = 0, demais índices mantêm o GPUREAD; GPUSTAT bit 14
  (espelhamento, só v1) = 0; GP1(20h) sem efeito; bit 13 = 1 sem entrelaçamento.
- GP1(09h) (bit 0 = endereços Y de 512 a 1023, VRAM de 2 MiB) é armazenado e não é alterado por
  GP1(00h); sem efeito com 1 MiB, em que Y já dá a volta em 512.
- GP0(E1h) bit 11 (base Y 2 da textura) aparece no GPUSTAT bit 15. ⚠️ Armazenado mesmo com
  GP1(09h) = 0 (as fontes não dizem se é mascarado).

## D-042 — Pontos não documentados das transferências e da exibição ⚠️
- Cópia 80h feita pixel a pixel, linha por linha: com origem e destino sobrepostos o hardware copia
  em blocos e o resultado pode diferir.
- C0h com total ímpar de pixels: a metade alta da última palavra lida é 0. A0h com total ímpar: a
  metade alta da última palavra é descartada (documentado).
- GP1(00h) abandona uma leitura C0h pendente; GP1(01h) a mantém.
- SB/SH em GP0/GP1: o registrador inteiro da CPU é tratado como a palavra escrita (mesma regra do
  barramento, D-015). Leituras de GPUREAD de 8/16 bits avançam a transferência a cada acesso.
- Largura exibida: `(((X2 − X1) / divisor) + 2) AND NOT 3` (psx-spx), com X2 limitado ao fim da linha
  (3413/3406 ciclos) e Y2 ao fim do quadro (263/314 linhas); 480 linhas só com entrelaçamento.
- Imagem de 24 bits: fluxo de bytes R, G, B a partir de X × 2 (X em halfwords).
- GPUSTAT bit 31 sem entrelaçamento = paridade da linha atual; com 480i = campo; 0 no VBLANK.
- O reset do console reinicia a GPU como GP1(00h) e preserva a VRAM; ao ligar, a VRAM é zerada
  (o conteúdo real é indefinido).

## D-043 — Saída de imagem da Fase 7
- A janela mostra a área de exibição da VRAM no estado em que a execução da CPU terminou; a emulação
  não continua com a janela aberta (laço em tempo real e frame pacing: Fase 20).
- `--dump-frame <arq>` grava essa imagem em BMP de 24 bits. O teste `smoke.gpu_frame` confere o BMP
  gerado por um programa MIPS próprio pixel a pixel e por SHA-256 (regressão).
- Comandos de desenho (20h–7Fh) são consumidos com o número exato de palavras e descartados com um
  aviso por opcode, para que nada pareça desenhado sem ter sido.

## D-044 — Rasterização de triângulos (Fase 8)
- Amostragem nas coordenadas inteiras dos pixels, com arestas esquerdas e superiores incluídas e
  direitas e inferiores excluídas (equivale a desenhar `[x_esq, x_dir)` em cada linha `[y_topo,
  y_base)`). Confere com todas as contagens medidas no psx.wiki (triângulo retângulo, diagonais,
  leque, triângulos finos de 200/199/100 pixels).
- Interpolação afim exata (inteiros de 64 bits) da cor gouraud e de U/V, arredondada para o mais
  próximo (medido para U: "00 10 … 80 8F …"). ⚠️ Para a cor o arredondamento foi adotado igual ao de
  U/V; o formato de ponto fixo do hardware não é documentado e pode divergir em casos raros.
- Triângulo de área zero não desenha nada (⚠️). Limite de 1023 × 511 avaliado por triângulo
  (medido: quadrilátero de 1060 com triângulos menores que 1024 é desenhado).

## D-045 — Linhas (Fase 8) ⚠️
- O algoritmo do hardware não é documentado. Adotado: DDA com `max(|dx|, |dy|)` passos, posição
  menor arredondada para o mais próximo, sempre a partir do extremo de cima (e da esquerda), o que
  garante o resultado igual nos dois sentidos (medido: simétrico). Extremos incluídos, extremos
  iguais = 1 pixel e cor gouraud conferem com as medições; a escolha de pixel nos casos de meio
  passo não foi comparada com a tabela de octantes publicada.

## D-046 — Texturas e retângulos (Fase 8) ⚠️
- A cache de textura não é emulada: o artefato medido em retângulos com U ímpar (um texel repetido
  a cada 4 pixels) não é reproduzido; a imagem usa sempre o texel exato.
- Espelhamento de retângulos (E1h bits 12/13): U/V decrescem a partir do valor do comando. A
  medição publicada (8×8, U 0, espelho em X) começa em 01h ("01 00 FF …"), um a mais: diferença
  registrada, ligada à cache, não emulada.
- Tamanho variável: largura AND 3FFh e altura AND 1FFh (medido: 1025 → 1, 1024 → 0, 513 → 1).

## D-047 — Cor, semitransparência e pontos não emulados (Fase 8) ⚠️
- Modulação: `((texel5 << 3) × cor8) >> 7` em 8 bits, depois dithering (se ligado), saturação em
  0..255 e `>> 3`. A página "rendering" do psx.wiki cita fórmulas da v0/v2 em escalas diferentes;
  adotada a forma "/128" (80h = neutro), que confere com o psx-spx.
- Semitransparência calculada em 5 bits sobre a cor já quantizada (medido no modo 0: (B + F)/2 com
  arredondamento para baixo); modos 1–3 com saturação em 0..31. A ordem dithering → mistura não foi
  medida.
- Dithering: polígonos gouraud ou modulados e todas as linhas; polígonos planos sem textura e
  retângulos nunca (psx-spx e psx.wiki "rendering"; um resumo de teste diz o contrário para
  retângulos, sem dados verbatim).
- Não emulados: E1h bit 10 (desenho na área exibida em modo entrelaçado), tempo de desenho (os
  comandos continuam instantâneos, D-040) e o terminador de polilinha 5555000Ch citado num teste
  (não explicado pela regra F000F000h).

## D-010 — MSVC como compilador de referência
- **Decisão:** Visual Studio 2022 (MSVC 19.44) é o compilador verificado. O código evita
  extensões específicas, mas nenhum outro compilador foi testado ainda (o MinGW disponível na
  máquina não possui `make`/`ninja`).
