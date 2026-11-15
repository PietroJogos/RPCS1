# Carregamento e execução de BIOS pela configuração, usando arquivos SINTÉTICOS
# (nenhuma BIOS real). Verifica que:
#  1. uma BIOS sintética com programa MIPS é executada pela CPU emulada até o primeiro
#     MFC0 (limite da Fase 3), escrevendo o código POST 7;
#  2. 512 KiB de bytes 41h param na primeira instrução (41414141h é um opcode COP0);
#  3. arquivos de tamanho errado ou ausentes são rejeitados com mensagem clara;
#  4. em todos os casos o programa encerra normalmente.
if(NOT DEFINED RPCS1_EXE OR NOT DEFINED MAKE_BIOS OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "RPCS1_EXE, MAKE_BIOS e WORK_DIR são obrigatórios")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(COMMAND "${MAKE_BIOS}" "${WORK_DIR}/programa.bin" RESULT_VARIABLE code)
if(NOT code EQUAL 0)
    message(FATAL_ERROR "falha ao gerar BIOS sintética (código ${code})")
endif()
string(REPEAT "A" 524288 rom)
file(WRITE "${WORK_DIR}/letras.bin" "${rom}")
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

function(expect_exit_zero code text)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "código de saída ${code}:\n${text}")
    endif()
endfunction()

run_with_bios("programa.bin" out code)
expect_exit_zero("${code}" "${out}")
expect_contains("${out}" "BIOS carregada")
expect_contains("${out}" "CPU executou 305 instruções da BIOS (305 ciclos")
expect_contains("${out}" "POST (1F802041h) = 7h")
expect_contains("${out}" "mfc0")
expect_contains("${out}" "requer a Fase 4")

run_with_bios("letras.bin" out code)
expect_exit_zero("${code}" "${out}")
expect_contains("${out}" "primeira instrução: 41414141h")
expect_contains("${out}" "CPU executou 0 instruções")
expect_contains("${out}" "requer a Fase 4")

run_with_bios("pequena.bin" out code)
expect_exit_zero("${code}" "${out}")
expect_contains("${out}" "BIOS não carregada")
expect_contains("${out}" "524288 bytes")

run_with_bios("nao_existe.bin" out code)
expect_contains("${out}" "BIOS não carregada")
expect_contains("${out}" "não encontrado")

message(STATUS "carregamento e execução de BIOS sintética verificados")
