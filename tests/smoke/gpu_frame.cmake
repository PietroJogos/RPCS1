# GPU (Fase 7): um programa de teste escreve na VRAM e a imagem resultante é gravada em BMP.
#
# Usa a BIOS SINTÉTICA gerada por rpcs1_make_test_bios --gpu (programa MIPS próprio; nenhum
# código da Sony). A imagem conferida aqui é a que o executável calcula a partir da VRAM
# emulada depois que a CPU executou o programa: nada é desenhado pelo frontend.
#  1. 320×240 com fundo, três preenchimentos coloridos (GP0(02h)), uma faixa branca enviada
#     por DMA (canal 2, lista encadeada), uma faixa cinza enviada depois de um triângulo
#     (descartado até a Fase 8: a faixa só sai certa se o FIFO continuar alinhado), 4×2
#     pixels enviados pela CPU (A0h) e a cópia deles na VRAM (80h);
#  2. sem BIOS, a exibição continua desligada após o reset: imagem preta 256×240;
#  3. caminho de saída inválido → código 1 com mensagem.
if(NOT DEFINED RPCS1_EXE OR NOT DEFINED MAKE_BIOS OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "RPCS1_EXE, MAKE_BIOS e WORK_DIR são obrigatórios")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(COMMAND "${MAKE_BIOS}" --gpu "${WORK_DIR}/gpu.bin" RESULT_VARIABLE code)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "falha ao gerar BIOS sintética (código ${code})")
endif()

function(run_rpcs1 ini_text out_var code_var)
    file(WRITE "${WORK_DIR}/rpcs1.ini" "${ini_text}")
    execute_process(
        COMMAND "${RPCS1_EXE}" --headless --config "${WORK_DIR}/rpcs1.ini" ${ARGN}
        RESULT_VARIABLE code
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)
    set(${out_var} "${out}${err}" PARENT_SCOPE)
    set(${code_var} "${code}" PARENT_SCOPE)
endfunction()

function(expect_contains text needle)
    string(FIND "${text}" "${needle}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "saída não contém '${needle}':\n${text}")
    endif()
endfunction()

function(expect_bytes file offset count expected what)
    file(READ "${file}" got OFFSET ${offset} LIMIT ${count} HEX)
    if(NOT got STREQUAL "${expected}")
        message(FATAL_ERROR "${what}: lido ${got}, esperado ${expected}")
    endif()
endfunction()

# Pixel (x, y) de um BMP de 24 bits de baixo para cima: bytes B, G, R.
function(expect_pixel file width height x y bgr)
    math(EXPR stride "(${width} * 3 + 3) / 4 * 4")
    math(EXPR offset "54 + (${height} - 1 - ${y}) * ${stride} + ${x} * 3")
    expect_bytes("${file}" ${offset} 3 "${bgr}" "pixel (${x}, ${y})")
endfunction()

# ---------------------------------------------------------------- 1. programa de teste
set(frame "${WORK_DIR}/quadro.bmp")
run_rpcs1("[BIOS]\npath = gpu.bin\n[Log]\nfile =\n" out code
    --cpu-steps 20000 --dump-frame "${frame}")
if(NOT code EQUAL 0)
    message(FATAL_ERROR "código de saída ${code}:\n${out}")
endif()
expect_contains("${out}" "POST (1F802041h) = 1h")
expect_contains("${out}" "POST (1F802041h) = 2h")
expect_contains("${out}" "GP0(20h): comando de desenho ainda não implementado (Fase 8)")
expect_contains("${out}" "GPU: exibição ligada | 320×240 (15 bits) a partir de VRAM (0, 0)")
expect_contains("${out}" "1 comandos de desenho descartados")
expect_contains("${out}" "(320×240, BMP 24 bits)")

file(SIZE "${frame}" size)
if(NOT size EQUAL 230454) # 54 + 320 × 3 × 240
    message(FATAL_ERROR "tamanho do BMP: ${size}")
endif()
expect_bytes("${frame}" 0 2 "424d" "assinatura BM")
expect_bytes("${frame}" 18 4 "40010000" "largura")
expect_bytes("${frame}" 22 4 "f0000000" "altura")
expect_bytes("${frame}" 28 2 "1800" "bits por pixel")

set(W 320)
set(H 240)
# Fundo: cor de comando 400000h → RGB555 B = 8 → 8 bits: 42h.
expect_pixel("${frame}" ${W} ${H} 0 0 "420000")
expect_pixel("${frame}" ${W} ${H} 319 239 "420000")
expect_pixel("${frame}" ${W} ${H} 15 16 "420000")
expect_pixel("${frame}" ${W} ${H} 16 16 "0000ff")      # vermelho
expect_pixel("${frame}" ${W} ${H} 111 79 "0000ff")
expect_pixel("${frame}" ${W} ${H} 112 80 "420000")
expect_pixel("${frame}" ${W} ${H} 120 20 "00ff00")     # verde
expect_pixel("${frame}" ${W} ${H} 220 20 "ff0000")     # azul
expect_pixel("${frame}" ${W} ${H} 20 100 "ffffff")     # branco, via DMA
expect_pixel("${frame}" ${W} ${H} 303 111 "ffffff")
expect_pixel("${frame}" ${W} ${H} 304 100 "420000")
expect_pixel("${frame}" ${W} ${H} 160 200 "420000")    # triângulo não desenhado
expect_pixel("${frame}" ${W} ${H} 20 130 "848484")     # cinza: 80h → 16 → 84h
# CPU → VRAM em (32,176)
expect_pixel("${frame}" ${W} ${H} 32 176 "0000ff")
expect_pixel("${frame}" ${W} ${H} 33 176 "00ff00")
expect_pixel("${frame}" ${W} ${H} 34 176 "ff0000")
expect_pixel("${frame}" ${W} ${H} 35 176 "ffffff")
expect_pixel("${frame}" ${W} ${H} 32 177 "00ffff")     # amarelo
expect_pixel("${frame}" ${W} ${H} 33 177 "ffff00")     # ciano
expect_pixel("${frame}" ${W} ${H} 34 177 "ff00ff")     # magenta
expect_pixel("${frame}" ${W} ${H} 35 177 "000000")
expect_pixel("${frame}" ${W} ${H} 36 176 "420000")
# VRAM → VRAM em (64,176)
expect_pixel("${frame}" ${W} ${H} 64 176 "0000ff")
expect_pixel("${frame}" ${W} ${H} 67 176 "ffffff")
expect_pixel("${frame}" ${W} ${H} 66 177 "ff00ff")
expect_pixel("${frame}" ${W} ${H} 67 177 "000000")
expect_pixel("${frame}" ${W} ${H} 68 176 "420000")
# Regressão: hash da imagem inteira, registrado depois de conferir os pixels acima e a imagem
# visualmente (Fase 7). Qualquer mudança na saída da GPU para este programa altera o hash.
file(SHA256 "${frame}" frame_hash)
string(TOUPPER "${frame_hash}" frame_hash)
if(NOT frame_hash STREQUAL "79C2774E547F9B3F33CA528C3B25EE79C01E965778312F82E122FBF59F9F07CD")
    message(FATAL_ERROR "SHA-256 do quadro mudou: ${frame_hash}")
endif()

# --------------------------------------------------- 2. sem BIOS: exibição desligada
set(black "${WORK_DIR}/preto.bmp")
run_rpcs1("[Log]\nfile =\n" out code --dump-frame "${black}")
if(NOT code EQUAL 0)
    message(FATAL_ERROR "código de saída ${code}:\n${out}")
endif()
expect_contains("${out}" "(256×240, BMP 24 bits)")
expect_bytes("${black}" 18 4 "00010000" "largura sem BIOS")
expect_pixel("${black}" 256 240 128 120 "000000")

# ------------------------------------------------------ 3. caminho de saída inválido
run_rpcs1("[Log]\nfile =\n" out code --dump-frame "${WORK_DIR}")
if(NOT code EQUAL 1)
    message(FATAL_ERROR "esperado código 1 para caminho inválido, obtido ${code}:\n${out}")
endif()
expect_contains("${out}" "Falha ao gravar o quadro")

message(STATUS "imagem gerada pela GPU emulada verificada")
