#include "frontend/application.hpp"

#include <chrono>
#include <cstdio>
#include <optional>
#include <thread>

#include "core/build_info.hpp"
#include "core/unicode.hpp"
#include "core/platform.hpp"
#include "core/version.hpp"
#include "video/framebuffer.hpp"
#include "video/window.hpp"

namespace rpcs1::frontend {

namespace {

using log::Category;

// Resolução de saída nominal usada para dimensionar a janela até a GPU existir.
constexpr u32 nominal_width = 320;
constexpr u32 nominal_height = 240;

void print_stdout(const std::string& text) {
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
}

} // namespace

Application::Application() : console_sink_(std::make_shared<log::ConsoleSink>()) {
    log::Logger::instance().add_sink(console_sink_);
}

Application::~Application() {
    auto& logger = log::Logger::instance();
    logger.flush();
    if (file_sink_) {
        logger.remove_sink(file_sink_);
    }
    logger.remove_sink(console_sink_);
}

ExitCode Application::run(std::span<const std::string> args) {
    auto parsed = parse_command_line(args);
    if (!parsed) {
        std::fprintf(stderr, "RPCS1: %s\n\n", parsed.error().message.c_str());
        std::fputs(usage_text().c_str(), stderr);
        return ExitCode::UsageError;
    }
    const CommandLine& cl = parsed.value();

    if (cl.show_help) {
        print_stdout(usage_text());
        return ExitCode::Success;
    }
    if (cl.show_version) {
        print_stdout(build_info::full_version_string() + "\nDesenvolvido pelo " +
                     version::developer + ".\n");
        return ExitCode::Success;
    }

    if (cl.log_level) {
        log::Logger::instance().set_level(*cl.log_level);
    }

    if (auto status = load_settings(cl); !status) {
        log::error(Category::Config, "{}", status.error().describe());
        return ExitCode::ConfigError;
    }
    configure_logging(cl);

    log::info(Category::Core, "{}", build_info::full_version_string());
    log::info(Category::Core, "Desenvolvido pelo {}", version::developer);
    log::info(Category::Config, "Configuração: {}", unicode::path_to_utf8(config_path_));
    log::info(Category::Config, "Modelo: {} | Região: {} | BIOS: {}",
              model_info(settings_.model).name, to_string(settings_.effective_region()),
              settings_.bios_path.empty() ? std::string("(não configurada)")
                                          : unicode::path_to_utf8(settings_.bios_path));
    initialize_hardware(cl);

    if (cl.headless) {
        log::info(Category::Frontend, "Modo headless: inicialização concluída, encerrando.");
        return ExitCode::Success;
    }
    return run_window_loop(cl);
}

Status Application::load_settings(const CommandLine& cl) {
    if (cl.config_path) {
        config_path_ = *cl.config_path;
    } else {
        auto exe_dir = platform::executable_directory();
        if (!exe_dir) {
            return exe_dir.error();
        }
        config_path_ = exe_dir.value() / "rpcs1.ini";
    }

    std::error_code ec;
    if (!std::filesystem::exists(config_path_, ec)) {
        settings_ = Settings{};
        // Primeira execução: grava os valores padrão documentados.
        if (auto saved = settings_.to_ini().save_file(config_path_); !saved) {
            log::warn(Category::Config, "Não foi possível criar {}: {}",
                      unicode::path_to_utf8(config_path_), saved.error().describe());
        } else {
            log::info(Category::Config, "Arquivo de configuração criado com valores padrão: {}",
                      unicode::path_to_utf8(config_path_));
        }
        return Status::success();
    }

    auto doc = IniDocument::load_file(config_path_);
    if (!doc) {
        return make_error(doc.error().code, unicode::path_to_utf8(config_path_) + ": " +
                                                doc.error().message);
    }
    auto loaded = settings_from_ini(doc.value());
    if (!loaded) {
        return make_error(loaded.error().code, unicode::path_to_utf8(config_path_) + ": " +
                                                   loaded.error().message);
    }
    for (const auto& warning : loaded.value().warnings) {
        log::warn(Category::Config, "{}", warning);
    }
    settings_ = loaded.value().settings;
    return Status::success();
}

void Application::configure_logging(const CommandLine& cl) {
    auto& logger = log::Logger::instance();
    logger.set_level(cl.log_level ? *cl.log_level : settings_.log_level);

    if (settings_.log_file.empty()) {
        return;
    }
    const auto path = resolve_path(config_path_.parent_path(), settings_.log_file);
    auto sink = log::FileSink::open(path);
    if (!sink) {
        log::warn(Category::Core, "Log em arquivo desativado ({}): {}",
                  unicode::path_to_utf8(path), sink.error().describe());
        return;
    }
    file_sink_ = sink.value();
    logger.add_sink(file_sink_);
    log::info(Category::Core, "Log em arquivo: {}", unicode::path_to_utf8(path));
}

void Application::initialize_hardware(const CommandLine& cl) {
    console_ = std::make_unique<system::Console>(model_info(settings_.model).region);
    log::info(Category::Bus,
              "Console pronto: RAM 2 MiB, scratchpad 1 KiB, BIOS ROM 512 KiB, Memory Control, "
              "CPU R3000A com COP0 e I-cache, interrupções, timers e temporização de vídeo ({})",
              to_string(console_->region()));

    // Honestidade sobre o estado: só a CPU e o núcleo do sistema existem.
    log::warn(Category::Core,
              "Fase 5: CPU, COP0, interrupções, timers e temporização de vídeo (VBLANK) emulados; "
              "sem GTE (Fase 9). GPU (desenho), SPU, CD-ROM, DMA e demais periféricos ainda não existem.");

    if (settings_.bios_path.empty()) {
        log::info(Category::Bios, "Nenhuma BIOS configurada ([BIOS] path); a CPU não será executada.");
        return;
    }
    const auto path = resolve_path(config_path_.parent_path(), settings_.bios_path);
    if (auto loaded = console_->load_bios(path); !loaded) {
        log::error(Category::Bios, "BIOS não carregada: {}", loaded.error().describe());
        return;
    }
    log::info(Category::Bios, "BIOS mapeada em BFC00000h; primeira instrução: {:08X}h",
              console_->bus().fetch_instruction(0xBFC00000).value);
    run_bios_until_stop(cl.cpu_steps.value_or(default_cpu_steps));
}

void Application::run_bios_until_stop(u64 limit) {
    // Executa a BIOS real do usuário até a primeira instrução que dependa de um componente
    // ainda inexistente. Nada é simulado: o resultado reflete exatamente o que foi emulado.
    cpu::Cpu& cpu = console_->cpu();
    const auto start = std::chrono::steady_clock::now();
    const u64 executed = console_->run(limit);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
    log::info(Category::Cpu,
              "CPU executou {} instruções da BIOS ({} ciclos, {} exceções/interrupções, {} ms).",
              executed, cpu.cycles(), cpu.exceptions_taken(), ms);
    log::info(Category::Video, "Tempo emulado: {:.3f} s ({} quadros de vídeo, {} VBLANKs).",
              static_cast<double>(cpu.cycles()) / static_cast<double>(system::cpu_clock_hz),
              console_->video().frames(), console_->video().vblank_starts());

    if (const auto& unimpl = cpu.last_unimplemented(); cpu.halted() && unimpl) {
        log::warn(Category::Cpu,
                  "Execução interrompida em {:08X}h: a instrução requer a Fase {}. "
                  "É o limite atual da emulação, não um erro da BIOS.",
                  unimpl->instruction_address, unimpl->phase);
    } else {
        log::info(Category::Cpu, "Limite de {} passos atingido; PC = {:08X}h.", limit, cpu.pc());
    }
    if (const auto& ex = cpu.last_exception()) {
        log::info(Category::Cpu, "Última exceção: {} em {:08X}h (EPC {:08X}h).",
                  cpu::to_string(ex->code), ex->instruction_address, ex->epc);
    }
    if (const auto post = console_->bus().post_code()) {
        log::info(Category::Bios, "Último código POST: {:X}h", *post);
    }
}
ExitCode Application::run_window_loop(const CommandLine& cl) {
    video::WindowConfig config;
    config.title = std::string(version::product) + " " + version::string +
                   " — pré-alfa (sem vídeo ainda)";
    config.client_width = nominal_width * settings_.window_scale;
    config.client_height = nominal_height * settings_.window_scale;

    auto created = video::create_window(config);
    if (!created) {
        log::error(Category::Video, "Falha ao criar a janela: {}", created.error().describe());
        return ExitCode::RuntimeError;
    }
    std::unique_ptr<video::Window> window = std::move(created).value();

    // Sem GPU emulada não há sinal de vídeo: a saída é preta, como uma TV sem sinal do console.
    video::Framebuffer framebuffer(nominal_width, nominal_height, 0x000000);

    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    std::optional<clock::time_point> deadline;
    if (cl.exit_after_ms) {
        deadline = start + std::chrono::milliseconds(*cl.exit_after_ms);
    }
    // Ritmo provisório do laço do hospedeiro; o frame pacing real é a Fase 20.
    constexpr auto host_frame = std::chrono::milliseconds(16);

    log::info(Category::Frontend, "Janela aberta. Feche-a para encerrar.");
    u64 frames = 0;
    while (window->pump_events()) {
        window->present(framebuffer);
        ++frames;
        if (deadline && clock::now() >= *deadline) {
            log::info(Category::Frontend, "--exit-after atingido; encerrando.");
            window->close();
            break;
        }
        std::this_thread::sleep_for(host_frame);
    }

    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
    log::info(Category::Frontend, "Encerrado normalmente ({} quadros do hospedeiro em {} ms).",
              frames, elapsed);
    return ExitCode::Success;
}

} // namespace rpcs1::frontend
