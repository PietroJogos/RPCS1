// Interpretação da linha de comando do RPCS1.
#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include "core/error.hpp"
#include "core/log.hpp"
#include "core/types.hpp"

namespace rpcs1 {

struct CommandLine {
    bool show_help = false;
    bool show_version = false;
    bool headless = false;                          // não cria janela
    std::optional<std::filesystem::path> config_path;
    std::optional<log::Level> log_level;            // sobrepõe [Log] level
    std::optional<u32> exit_after_ms;               // encerra sozinho (testes de fumaça)
    std::optional<u64> cpu_steps;                   // limite de passos da CPU ao rodar a BIOS

    static constexpr u32 max_exit_after_ms = 24u * 60u * 60u * 1000u;
    static constexpr u64 max_cpu_steps = 10'000'000'000ull;
};

// `args` não inclui o nome do programa.
[[nodiscard]] Result<CommandLine> parse_command_line(std::span<const std::string> args);

[[nodiscard]] std::string usage_text();

} // namespace rpcs1
