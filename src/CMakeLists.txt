# --------------------------------------------------------------------------
# Biblioteca de nucleo: tipos, erros, logging, configuracao, linha de comando,
# E/S de arquivos e utilitarios de plataforma. Nao contem emulacao ainda.
# --------------------------------------------------------------------------
configure_file(core/version.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/core/version.hpp" @ONLY)

add_library(rpcs1_core STATIC
    core/assert.cpp
    core/build_info.cpp
    core/cli.cpp
    core/console_model.cpp
    core/error.cpp
    core/file_io.cpp
    core/ini.cpp
    core/log.cpp
    core/platform.cpp
    core/settings.cpp
    core/strings.cpp
    core/unicode.cpp)

target_include_directories(rpcs1_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(WIN32)
    target_link_libraries(rpcs1_core PRIVATE shell32) # CommandLineToArgvW
endif()
rpcs1_configure_target(rpcs1_core)

# --------------------------------------------------------------------------
# Hardware emulado: memória e barramento (Fase 2), CPU (Fase 3), COP0, exceções,
# I-cache e controlador de interrupções (Fase 4) e o Console que os interliga.
# --------------------------------------------------------------------------
add_library(rpcs1_hw STATIC
    bus/address.cpp
    bus/bus.cpp
    bus/io_device.cpp
    cpu/cop0.cpp
    cpu/cpu.cpp
    cpu/disassembler.cpp
    cpu/icache.cpp
    gpu/video_timing.cpp
    interrupts/interrupt_controller.cpp
    memory/bios_rom.cpp
    memory/memory_control.cpp
    system/console.cpp
    system/scheduler.cpp
    timers/timers.cpp)
target_link_libraries(rpcs1_hw PUBLIC rpcs1_core)
rpcs1_configure_target(rpcs1_hw)

# --------------------------------------------------------------------------
# Video do hospedeiro: framebuffer e janela nativa (Win32).
# --------------------------------------------------------------------------
add_library(rpcs1_video STATIC
    video/framebuffer.cpp
    video/window.cpp)
if(WIN32)
    target_sources(rpcs1_video PRIVATE video/win32_window.cpp)
    target_link_libraries(rpcs1_video PRIVATE user32 gdi32)
endif()
target_link_libraries(rpcs1_video PUBLIC rpcs1_core)
rpcs1_configure_target(rpcs1_video)

# --------------------------------------------------------------------------
# Executavel principal.
# --------------------------------------------------------------------------
add_executable(rpcs1
    frontend/main.cpp
    frontend/application.cpp
    frontend/crash_handler.cpp)
if(WIN32)
    configure_file(frontend/rpcs1.rc.in "${CMAKE_CURRENT_BINARY_DIR}/generated/rpcs1.rc" @ONLY)
    target_sources(rpcs1 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/rpcs1.rc")
    if(MSVC)
        configure_file(frontend/rpcs1.manifest.in
            "${CMAKE_CURRENT_BINARY_DIR}/generated/rpcs1.manifest" @ONLY)
        target_sources(rpcs1 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/rpcs1.manifest")
    endif()
endif()
target_link_libraries(rpcs1 PRIVATE rpcs1_core rpcs1_hw rpcs1_video)
rpcs1_configure_target(rpcs1)
set_target_properties(rpcs1 PROPERTIES OUTPUT_NAME "RPCS1")
