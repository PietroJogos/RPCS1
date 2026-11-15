# Carregamento de BIOS pela configuração, usando arquivos SINTÉTICOS
# (512 KiB de 'A'; não é uma BIOS real). Verifica que:
#  1. um arquivo de 512 KiB é carregado e mapeado em BFC00000h;
#  2. um arquivo de tamanho errado é rejeitado com mensagem clara;
#  3. em nenhum caso algo é executado (o programa encerra normalmente).
if(NOT DEFINED RPCS1_EXE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "RPCS1_EXE e WORK_DIR são obrigatórios")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

string(REPEAT "A" 524288 rom)
file(WRITE "${WORK_DIR}/sintetica.bin" "${rom}")
file(WRITE "${WORK_DIR}/pequena.bin" "ABC")

function(run_with_bios bios_file out_var code_var)
    file(WRITE "${WORK_DIR}/rpcs1.ini" "[BIOS]\npath = ${bios_file}\n[Log]\nfile =\n")
    execute_process(
        COMMAND "${RPCS1_EXE}" --headless --config "${WORK_DIR}/rpcs1.ini"
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

run_with_bios("sintetica.bin" out code)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "BIOS válida: código ${code}\n${out}")
endif()
expect_contains("${out}" "BIOS carregada")
expect_contains("${out}" "primeira instrução: 41414141h")
expect_contains("${out}" "Nada é executado")

run_with_bios("pequena.bin" out code)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "BIOS inválida: código ${code}\n${out}")
endif()
expect_contains("${out}" "BIOS não carregada")
expect_contains("${out}" "524288 bytes")

run_with_bios("nao_existe.bin" out code)
expect_contains("${out}" "BIOS não carregada")
expect_contains("${out}" "não encontrado")

message(STATUS "carregamento de BIOS sintética verificado")
