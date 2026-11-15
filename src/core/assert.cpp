#include "core/assert.hpp"

#include <cstdio>
#include <cstdlib>

#include "core/log.hpp"

namespace rpcs1::detail {

void assertion_failed(const char* expression, const char* message, const char* file, int line) {
    // Tenta registrar no logger; se isso falhar, ao menos stderr recebe a mensagem.
    try {
        log::fatal(log::Category::Core, "Asserção falhou: {} — {} ({}:{})", expression,
                   message ? message : "", file, line);
        log::Logger::instance().flush();
    } catch (...) {
        std::fprintf(stderr, "Assercao falhou: %s (%s:%d)\n", expression, file, line);
    }
    std::fflush(stderr);
    std::abort();
}

} // namespace rpcs1::detail
