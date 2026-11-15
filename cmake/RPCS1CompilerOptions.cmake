# Opcoes de compilacao comuns a todos os alvos do RPCS1.

# Compilacao paralela do MSVC (/MP). Vazio = todos os nucleos; 1 = sem paralelismo.
# Util em maquinas com pouca memoria livre (erro C1060, "heap do compilador insuficiente").
set(RPCS1_MSVC_MP_JOBS "" CACHE STRING "Processos paralelos do /MP do MSVC (vazio = todos os nucleos)")

function(rpcs1_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4             # avisos de alto nivel
            /permissive-    # conformidade estrita com o padrao
            /utf-8          # codigo-fonte e strings em UTF-8
            /Zc:__cplusplus
            /Zc:preprocessor
            /EHsc)
        if(RPCS1_MSVC_MP_JOBS STREQUAL "")
            target_compile_options(${target} PRIVATE /MP)
        elseif(RPCS1_MSVC_MP_JOBS GREATER 1)
            target_compile_options(${target} PRIVATE /MP${RPCS1_MSVC_MP_JOBS})
        endif()
        if(RPCS1_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
            -Woverloaded-virtual -Wcast-align)
        if(RPCS1_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(WIN32)
        target_compile_definitions(${target} PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    endif()
endfunction()
