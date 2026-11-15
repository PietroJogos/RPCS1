# Funções comuns dos testes de fumaça que conferem imagens BMP geradas pelo RPCS1.

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

function(expect_sha256 file expected)
    file(SHA256 "${file}" hash)
    string(TOUPPER "${hash}" hash)
    if(NOT hash STREQUAL "${expected}")
        message(FATAL_ERROR "SHA-256 de ${file} mudou: ${hash}")
    endif()
endfunction()
