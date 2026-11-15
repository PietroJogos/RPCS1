# GPU (Fase 8): o rasterizador desenha as primitivas de um programa de teste e a imagem é
# conferida em BMP.
#
# BIOS SINTÉTICA gerada por rpcs1_make_test_bios --gpu-draw (programa MIPS próprio; nenhum código
# da Sony). Os pixels abaixo foram calculados à mão a partir das regras documentadas (cores de
# comando, CLUT, semitransparência, modulação e matriz de dithering); o SHA-256 foi registrado
# depois de conferir esses pixels e a imagem visualmente.
if(NOT DEFINED RPCS1_EXE OR NOT DEFINED MAKE_BIOS OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "RPCS1_EXE, MAKE_BIOS e WORK_DIR são obrigatórios")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/bmp_check.cmake")

execute_process(COMMAND "${MAKE_BIOS}" --gpu-draw "${WORK_DIR}/draw.bin" RESULT_VARIABLE code)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "falha ao gerar BIOS sintética (código ${code})")
endif()

set(frame "${WORK_DIR}/desenho.bmp")
run_rpcs1("[BIOS]\npath = draw.bin\n[Log]\nfile =\n" out code
    --cpu-steps 20000 --dump-frame "${frame}")
if(NOT code EQUAL 0)
    message(FATAL_ERROR "código de saída ${code}:\n${out}")
endif()
expect_contains("${out}" "POST (1F802041h) = 2h")
expect_contains("${out}" "GPU: exibição ligada | 320×240 (15 bits) a partir de VRAM (0, 0)")
expect_contains("${out}" "9 primitivas desenhadas")
expect_contains("${out}" "(320×240, BMP 24 bits)")

set(W 320)
set(H 240)
# Fundo: 302010h → RGB555 (2, 4, 6) → 8 bits (10h, 21h, 31h).
expect_pixel("${frame}" ${W} ${H} 0 0 "312110")
expect_pixel("${frame}" ${W} ${H} 300 230 "312110")
# Vértice vermelho do triângulo gouraud: 255 − 4 (matriz em (2,2)) → 31; verde/azul 0.
expect_pixel("${frame}" ${W} ${H} 10 10 "0000ff")
# Triângulo plano amarelo: sem dithering.
expect_pixel("${frame}" ${W} ${H} 165 145 "00ffff")
# Quadrilátero modulado por 808080h, texel branco (31 → 248), dithering −4 em (170,10): 244 → 30 → F7h.
expect_pixel("${frame}" ${W} ${H} 170 10 "f7f7f7")
# Sprite cru: U 0 → índice 1 (branco); U 4 → índice 2 (laranja 021Fh: R 31, G 16 → 84h).
expect_pixel("${frame}" ${W} ${H} 280 10 "ffffff")
expect_pixel("${frame}" ${W} ${H} 284 10 "0084ff")
expect_pixel("${frame}" ${W} ${H} 280 14 "0084ff")
# Semitransparência B/2 + F/2 com azul (0,0,31): sobre o amarelo → (15,15,15) → 7Bh;
# sobre o fundo → (1, 2, 18) → (08h, 10h, 94h).
expect_pixel("${frame}" ${W} ${H} 185 155 "7b7b7b")
expect_pixel("${frame}" ${W} ${H} 270 200 "941008")
# Polilinha branca: 255 com dithering continua 31.
expect_pixel("${frame}" ${W} ${H} 50 150 "ffffff")
expect_pixel("${frame}" ${W} ${H} 10 190 "ffffff")
expect_pixel("${frame}" ${W} ${H} 140 230 "ffffff")
# Quadrilátero aditivo B + F com (0, 8, 0) sobre o fundo: (2, 12, 6) → (10h, 63h, 31h).
expect_pixel("${frame}" ${W} ${H} 60 190 "316310")
# Triângulo ciano deslocado +60 por E5h: (250..280, 215..235).
expect_pixel("${frame}" ${W} ${H} 255 220 "ffff00")
expect_pixel("${frame}" ${W} ${H} 195 220 "312110")

expect_sha256("${frame}" "B05BC41D877C9525132F2313955F265A0D810A8AD97D7A3038E32B936FA227AD")
message(STATUS "imagem desenhada pela GPU emulada verificada")
