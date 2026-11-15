# --------------------------------------------------------------------------
# Testes unitários (framework próprio, sem dependências externas).
# --------------------------------------------------------------------------
add_executable(rpcs1_tests
    framework/test_main.cpp
    core/test_cli.cpp
    core/test_console_model.cpp
    core/test_error.cpp
    core/test_file_io.cpp
    core/test_ini.cpp
    core/test_log.cpp
    core/test_settings.cpp
    core/test_strings.cpp
    core/test_unicode.cpp
    bus/test_address.cpp
    bus/test_bus.cpp
    cpu/test_cop0.cpp
    cpu/test_cpu_alu.cpp
    cpu/test_cpu_branch.cpp
    cpu/test_icache.cpp
    cpu/test_interrupts.cpp
    cpu/test_cpu_exceptions.cpp
    cpu/test_cpu_memory.cpp
    cpu/test_cpu_muldiv.cpp
    cpu/test_cpu_programs.cpp
    memory/test_bios_rom.cpp
    memory/test_memory_block.cpp
    memory/test_memory_control.cpp
    dma/test_dma.cpp
    gpu/test_video_timing.cpp
    system/test_scheduler.cpp
    timers/test_timers.cpp
    video/test_framebuffer.cpp)
target_include_directories(rpcs1_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rpcs1_tests PRIVATE rpcs1_core rpcs1_hw rpcs1_video)
rpcs1_configure_target(rpcs1_tests)

add_test(NAME unit.all COMMAND rpcs1_tests)
set_tests_properties(unit.all PROPERTIES LABELS "unit")

# --------------------------------------------------------------------------
# Testes de fumaça do executável: abre, encerra e trata erros corretamente.
# Usam uma pasta própria em <build>/smoke para não tocar em arquivos do usuário.
# --------------------------------------------------------------------------
set(RPCS1_SMOKE_DIR "${CMAKE_BINARY_DIR}/smoke")
file(MAKE_DIRECTORY "${RPCS1_SMOKE_DIR}")

add_test(NAME smoke.version COMMAND rpcs1 --version)
set_tests_properties(smoke.version PROPERTIES
    LABELS "smoke"
    PASS_REGULAR_EXPRESSION "RPCS1 ${PROJECT_VERSION}")

add_test(NAME smoke.help COMMAND rpcs1 --help)
set_tests_properties(smoke.help PROPERTIES
    LABELS "smoke"
    PASS_REGULAR_EXPRESSION "--headless")

# Primeira execução: cria a configuração padrão e encerra sem janela.
add_test(NAME smoke.headless_first_run
    COMMAND ${CMAKE_COMMAND}
        -DRPCS1_EXE=$<TARGET_FILE:rpcs1>
        -DWORK_DIR=${RPCS1_SMOKE_DIR}/first_run
        -P "${CMAKE_CURRENT_SOURCE_DIR}/smoke/first_run.cmake")
set_tests_properties(smoke.headless_first_run PROPERTIES LABELS "smoke")

# Código de saída 2 para opção desconhecida.
add_test(NAME smoke.unknown_option
    COMMAND ${CMAKE_COMMAND}
        -DRPCS1_EXE=$<TARGET_FILE:rpcs1>
        -DEXPECTED_EXIT=2
        "-DARGS=--opcao-inexistente"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/smoke/expect_exit.cmake")
set_tests_properties(smoke.unknown_option PROPERTIES LABELS "smoke")

# Código de saída 3 para configuração inválida (o arquivo não pode ser sobrescrito).
add_test(NAME smoke.invalid_config
    COMMAND ${CMAKE_COMMAND}
        -DRPCS1_EXE=$<TARGET_FILE:rpcs1>
        -DEXPECTED_EXIT=3
        "-DARGS=--headless|--config|${CMAKE_CURRENT_SOURCE_DIR}/data/invalid_scale.ini"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/smoke/expect_exit.cmake")
set_tests_properties(smoke.invalid_config PROPERTIES LABELS "smoke")

# Gerador de BIOS sintética (programa MIPS próprio; nenhum código da Sony).
add_executable(rpcs1_make_test_bios tools/make_test_bios.cpp)
target_include_directories(rpcs1_make_test_bios PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rpcs1_make_test_bios PRIVATE rpcs1_core)
rpcs1_configure_target(rpcs1_make_test_bios)

# BIOS sintética executada pela CPU; tamanho errado e arquivo ausente rejeitados.
add_test(NAME smoke.bios_load
    COMMAND ${CMAKE_COMMAND}
        -DRPCS1_EXE=$<TARGET_FILE:rpcs1>
        -DMAKE_BIOS=$<TARGET_FILE:rpcs1_make_test_bios>
        -DWORK_DIR=${RPCS1_SMOKE_DIR}/bios_load
        -P "${CMAKE_CURRENT_SOURCE_DIR}/smoke/bios_load.cmake")
set_tests_properties(smoke.bios_load PROPERTIES LABELS "smoke")

# Abre a janela Win32 de verdade e a fecha após 500 ms (requer área de trabalho interativa).
if(WIN32)
    add_test(NAME smoke.window_open_close
        COMMAND rpcs1 --exit-after 500 --config "${RPCS1_SMOKE_DIR}/window/rpcs1.ini")
    set_tests_properties(smoke.window_open_close PROPERTIES
        LABELS "smoke;gui"
        TIMEOUT 30
        PASS_REGULAR_EXPRESSION "Encerrado normalmente")
endif()
