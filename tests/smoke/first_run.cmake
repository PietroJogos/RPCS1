# Simula a primeira execução: sem rpcs1.ini, o programa deve criar a
# configuração padrão, gravar o log e encerrar com código 0 em modo headless.
# Em seguida, uma segunda execução deve ler o arquivo criado sem avisos.
if(NOT DEFINED RPCS1_EXE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "RPCS1_EXE e WORK_DIR são obrigatórios")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(config "${WORK_DIR}/rpcs1.ini")

execute_process(
    COMMAND "${RPCS1_EXE}" --headless --config "${config}"
    RESULT_VARIABLE exit_code
    ERROR_VARIABLE err)
if(NOT exit_code EQUAL 0)
    message(FATAL_ERROR "primeira execução falhou (código ${exit_code}):\n${err}")
endif()
if(NOT EXISTS "${config}")
    message(FATAL_ERROR "rpcs1.ini não foi criado")
endif()
if(NOT EXISTS "${WORK_DIR}/rpcs1.log")
    message(FATAL_ERROR "rpcs1.log não foi criado ao lado da configuração")
endif()
file(READ "${config}" content)
foreach(expected "[System]" "model = SCPH-5501" "[BIOS]" "[Video]" "scale = 2" "[Log]")
    string(FIND "${content}" "${expected}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "rpcs1.ini não contém '${expected}':\n${content}")
    endif()
endforeach()

execute_process(
    COMMAND "${RPCS1_EXE}" --headless --config "${config}"
    RESULT_VARIABLE exit_code
    ERROR_VARIABLE err)
if(NOT exit_code EQUAL 0)
    message(FATAL_ERROR "segunda execução falhou (código ${exit_code}):\n${err}")
endif()
string(FIND "${err}" "desconhecida" pos)
if(NOT pos EQUAL -1)
    message(FATAL_ERROR "a configuração gerada produziu avisos:\n${err}")
endif()
message(STATUS "primeira e segunda execução corretas")
