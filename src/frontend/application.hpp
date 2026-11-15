// Ciclo de vida do aplicativo RPCS1 (Fase 0).
#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string>

#include "core/cli.hpp"
#include "core/error.hpp"
#include "core/log.hpp"
#include "core/settings.hpp"

namespace rpcs1::frontend {

enum class ExitCode : int {
    Success = 0,
    RuntimeError = 1,
    UsageError = 2,
    ConfigError = 3,
};

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] ExitCode run(std::span<const std::string> args);

private:
    [[nodiscard]] Status load_settings(const CommandLine& cl);
    void configure_logging(const CommandLine& cl);
    [[nodiscard]] ExitCode run_window_loop(const CommandLine& cl);

    std::shared_ptr<log::ConsoleSink> console_sink_;
    std::shared_ptr<log::FileSink> file_sink_;
    std::filesystem::path config_path_;
    Settings settings_;
};

} // namespace rpcs1::frontend
