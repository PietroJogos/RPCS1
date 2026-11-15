#include "core/cli.hpp"

#include <format>
#include <string_view>

#include "core/strings.hpp"
#include "core/unicode.hpp"

namespace rpcs1 {

Result<CommandLine> parse_command_line(std::span<const std::string> args) {
    CommandLine cl;

    for (usize i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        std::string_view inline_value;
        bool has_inline_value = false;

        // Suporta "--opcao=valor" além de "--opcao valor".
        if (strings::starts_with(arg, "--")) {
            const auto eq = arg.find('=');
            if (eq != std::string_view::npos) {
                inline_value = arg.substr(eq + 1);
                arg = arg.substr(0, eq);
                has_inline_value = true;
            }
        }

        auto take_value = [&](std::string_view option) -> Result<std::string> {
            if (has_inline_value) {
                return std::string(inline_value);
            }
            if (i + 1 >= args.size()) {
                return make_error(ErrorCode::InvalidArgument,
                                  std::format("a opção {} exige um valor", option));
            }
            return args[++i];
        };

        auto reject_value = [&](std::string_view option) -> Status {
            if (has_inline_value) {
                return make_error(ErrorCode::InvalidArgument,
                                  std::format("a opção {} não aceita valor", option));
            }
            return Status::success();
        };

        if (arg == "-h" || arg == "--help") {
            if (auto st = reject_value(arg); !st) return st.error();
            cl.show_help = true;
        } else if (arg == "-v" || arg == "--version") {
            if (auto st = reject_value(arg); !st) return st.error();
            cl.show_version = true;
        } else if (arg == "--headless") {
            if (auto st = reject_value(arg); !st) return st.error();
            cl.headless = true;
        } else if (arg == "--config") {
            auto value = take_value(arg);
            if (!value) return value.error();
            if (value.value().empty()) {
                return make_error(ErrorCode::InvalidArgument, "--config recebeu um caminho vazio");
            }
            cl.config_path = unicode::path_from_utf8(value.value());
        } else if (arg == "--log-level") {
            auto value = take_value(arg);
            if (!value) return value.error();
            const auto level = log::parse_level(value.value());
            if (!level) {
                return make_error(ErrorCode::InvalidArgument,
                                  std::format("nível de log inválido: \"{}\"", value.value()));
            }
            cl.log_level = *level;
        } else if (arg == "--exit-after") {
            auto value = take_value(arg);
            if (!value) return value.error();
            const auto ms =
                strings::parse_unsigned(value.value(), 1, CommandLine::max_exit_after_ms);
            if (!ms) {
                return make_error(ErrorCode::InvalidArgument,
                                  std::format("--exit-after espera milissegundos (1 a {}): \"{}\"",
                                              CommandLine::max_exit_after_ms, value.value()));
            }
            cl.exit_after_ms = static_cast<u32>(*ms);
        } else if (arg == "--cpu-steps") {
            auto value = take_value(arg);
            if (!value) return value.error();
            const auto steps = strings::parse_unsigned(value.value(), 1, CommandLine::max_cpu_steps);
            if (!steps) {
                return make_error(ErrorCode::InvalidArgument,
                                  std::format("--cpu-steps espera um número de passos (1 a {}): \"{}\"",
                                              CommandLine::max_cpu_steps, value.value()));
            }
            cl.cpu_steps = *steps;
        } else if (strings::starts_with(arg, "-")) {
            return make_error(ErrorCode::InvalidArgument,
                              std::format("opção desconhecida: {}", args[i]));
        } else {
            return make_error(ErrorCode::Unsupported,
                              std::format("argumento não suportado nesta fase: \"{}\" "
                                          "(abertura de discos ainda não implementada)",
                                          args[i]));
        }
    }
    return cl;
}

std::string usage_text() {
    return "Uso: RPCS1 [opções]\n"
           "\n"
           "Opções:\n"
           "  -h, --help             Mostra esta ajuda e encerra.\n"
           "  -v, --version          Mostra a versão e encerra.\n"
           "      --config <arq>     Usa o arquivo de configuração indicado\n"
           "                         (padrão: rpcs1.ini na pasta do executável).\n"
           "      --log-level <n>    trace, debug, info, warn, error, fatal ou off.\n"
           "      --headless         Não abre janela (testes automatizados).\n"
           "      --exit-after <ms>  Encerra automaticamente após <ms> milissegundos.\n"
           "      --cpu-steps <n>    Limite de passos da CPU ao executar a BIOS\n"
           "                         (padrão: 50000000).\n"
           "\n"
           "Estado atual: Fase 5 (CPU, COP0, timers e temporização de vídeo). A BIOS\n"
           "configurada é executada até a primeira instrução que dependa de um\n"
           "componente inexistente (GTE) ou até o limite de passos. Não há vídeo nem\n"
           "áudio: a janela exibe somente uma tela preta sem sinal.\n";
}

} // namespace rpcs1
