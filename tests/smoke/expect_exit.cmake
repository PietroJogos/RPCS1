# Executa RPCS1_EXE com ARGS (separados por '|') e verifica se o código de
# saída é EXPECTED_EXIT.
if(NOT DEFINED RPCS1_EXE OR NOT DEFINED EXPECTED_EXIT)
    message(FATAL_ERROR "RPCS1_EXE e EXPECTED_EXIT são obrigatórios")
endif()
string(REPLACE "|" ";" ARGS "${ARGS}")

execute_process(
    COMMAND "${RPCS1_EXE}" ${ARGS}
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

message(STATUS "saída: ${out}${err}")
if(NOT exit_code STREQUAL "${EXPECTED_EXIT}")
    message(FATAL_ERROR "código de saída ${exit_code}; esperado ${EXPECTED_EXIT}")
endif()
message(STATUS "código de saída ${exit_code} conforme esperado")
