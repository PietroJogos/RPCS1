// Serviços mínimos do sistema operacional hospedeiro.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/error.hpp"

namespace rpcs1::platform {

// Pasta onde está o executável em execução.
[[nodiscard]] Result<std::filesystem::path> executable_directory();

// Configura o console do Windows para exibir UTF-8. Sem efeito em outras plataformas.
void configure_console_utf8() noexcept;

// Argumentos da linha de comando em UTF-8, sem o nome do programa.
// No Windows usa GetCommandLineW (suporta caminhos com acentos);
// nas demais plataformas usa argv.
[[nodiscard]] std::vector<std::string> command_line_arguments(int argc, char** argv);

} // namespace rpcs1::platform
