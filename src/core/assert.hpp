// Asserções de invariantes internas. Sempre ativas (inclusive em Release):
// num emulador, continuar executando com estado corrompido produz resultados
// enganosos, o que é pior do que interromper com uma mensagem clara.
#pragma once

namespace rpcs1::detail {

[[noreturn]] void assertion_failed(const char* expression, const char* message, const char* file,
                                   int line);

} // namespace rpcs1::detail

#define RPCS1_ASSERT(condition, message)                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            ::rpcs1::detail::assertion_failed(#condition, (message), __FILE__, __LINE__);      \
        }                                                                                      \
    } while (false)

#define RPCS1_UNREACHABLE(message)                                                             \
    ::rpcs1::detail::assertion_failed("inalcançável", (message), __FILE__, __LINE__)
