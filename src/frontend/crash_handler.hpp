// Registro de falhas fatais não tratadas (exceções SEH no Windows, std::terminate).
#pragma once

namespace rpcs1::frontend {

void install_crash_handlers();

} // namespace rpcs1::frontend
