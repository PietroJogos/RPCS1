# RPCS1 — emulador original de PlayStation 1
# Desenvolvido pelo PiStudio BR.
cmake_minimum_required(VERSION 3.24)

project(RPCS1
    VERSION 0.0.5
    DESCRIPTION "RPCS1 - emulador original de PlayStation 1 (PiStudio BR)"
    LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "RPCS1 requer uma compilacao 64 bits (x64).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Runtime do MSVC vinculado estaticamente: o RPCS1.exe roda sem exigir a instalação
# do Visual C++ Redistributable (ver docs/DECISIONS.md, D-011).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(RPCS1_BUILD_TESTS "Compilar os testes automatizados" ON)
option(RPCS1_WARNINGS_AS_ERRORS "Tratar avisos do compilador como erros" ON)

# Todos os executaveis vao para <build>/bin (ou <build>/bin/<Config> em geradores multi-config).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(RPCS1CompilerOptions)

add_subdirectory(src)

if(RPCS1_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
