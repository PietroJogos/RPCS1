#include "core/settings.hpp"

#include <format>
#include <string_view>

#include "core/strings.hpp"
#include "core/unicode.hpp"

namespace rpcs1 {

namespace {

struct KnownKey {
    std::string_view section;
    std::string_view key;
};

constexpr KnownKey known_keys[] = {
    {"System", "model"}, {"System", "region"}, {"BIOS", "path"},
    {"Video", "scale"},  {"Log", "level"},     {"Log", "file"},
};

bool is_known_section(std::string_view section) {
    for (const auto& known : known_keys) {
        if (strings::iequals(known.section, section)) {
            return true;
        }
    }
    return false;
}

bool is_known_key(std::string_view section, std::string_view key) {
    for (const auto& known : known_keys) {
        if (strings::iequals(known.section, section) && strings::iequals(known.key, key)) {
            return true;
        }
    }
    return false;
}

Error invalid_value(std::string_view section, std::string_view key, std::string_view value,
                    std::string_view expected) {
    return make_error(ErrorCode::InvalidData,
                      std::format("[{}] {} = \"{}\" é inválido; esperado: {}", section, key, value,
                                  expected));
}

std::string model_list() {
    std::string list;
    for (const auto& info : all_console_models()) {
        if (!list.empty()) {
            list += ", ";
        }
        list += info.name;
    }
    return list;
}

} // namespace

Region Settings::effective_region() const noexcept {
    return region_override ? *region_override : model_info(model).region;
}

IniDocument Settings::to_ini() const {
    IniDocument doc;
    doc.header_comments = {
        "; Configuração do RPCS1 — PiStudio BR",
        "; Caminhos relativos são resolvidos a partir da pasta deste arquivo.",
    };

    doc.set("System", "model", std::string(model_info(model).name));
    doc.set_comment("System", "model",
                    "Modelo do console. Modelo-alvo inicial: SCPH-5501.\n"
                    "(Ainda sem efeito: nenhum perfil de hardware implementado — Fase 16.)");
    doc.set("System", "region",
            region_override ? std::string(to_string(*region_override)) : std::string("auto"));
    doc.set_comment("System", "region",
                    "auto, NTSC-J, NTSC-U/C ou PAL. 'auto' usa a região do modelo.\n"
                    "(Ainda sem efeito — Fases 7 e 14.)");

    doc.set("BIOS", "path", unicode::path_to_utf8(bios_path));
    doc.set_comment("BIOS", "path",
                    "Arquivo de BIOS (512 KiB) extraído pelo próprio usuário de seu console.\n"
                    "O RPCS1 não distribui BIOS. (Ainda sem efeito — Fase 15.)");

    doc.set("Video", "scale", std::to_string(window_scale));
    doc.set_comment("Video", "scale",
                    std::format("Multiplicador da janela sobre 320x240 ({} a {}).",
                                min_window_scale, max_window_scale));

    doc.set("Log", "level", strings::to_lower_ascii(log::to_string(log_level)));
    doc.set_comment("Log", "level", "trace, debug, info, warn, error, fatal ou off.");
    doc.set("Log", "file", unicode::path_to_utf8(log_file));
    doc.set_comment("Log", "file", "Arquivo de log. Deixe vazio para registrar só no console.");
    return doc;
}

Result<SettingsLoadResult> settings_from_ini(const IniDocument& doc) {
    SettingsLoadResult result;
    Settings& s = result.settings;

    for (const auto& section : doc.sections()) {
        if (!is_known_section(section.name)) {
            result.warnings.push_back(std::format("seção desconhecida ignorada: [{}]", section.name));
            continue;
        }
        for (const auto& entry : section.entries) {
            if (!is_known_key(section.name, entry.key)) {
                result.warnings.push_back(
                    std::format("chave desconhecida ignorada: [{}] {}", section.name, entry.key));
            }
        }
    }

    if (auto value = doc.get("System", "model")) {
        const auto model = parse_console_model(*value);
        if (!model) {
            return invalid_value("System", "model", *value, model_list());
        }
        s.model = *model;
    }

    if (auto value = doc.get("System", "region")) {
        if (strings::iequals(strings::trim(*value), "auto") || strings::trim(*value).empty()) {
            s.region_override.reset();
        } else {
            const auto region = parse_region(*value);
            if (!region) {
                return invalid_value("System", "region", *value, "auto, NTSC-J, NTSC-U/C ou PAL");
            }
            s.region_override = *region;
        }
    }

    if (auto value = doc.get("BIOS", "path")) {
        s.bios_path = unicode::path_from_utf8(*value);
    }

    if (auto value = doc.get("Video", "scale")) {
        const auto scale =
            strings::parse_unsigned(*value, Settings::min_window_scale, Settings::max_window_scale);
        if (!scale) {
            return invalid_value("Video", "scale", *value,
                                 std::format("inteiro de {} a {}", Settings::min_window_scale,
                                             Settings::max_window_scale));
        }
        s.window_scale = static_cast<u32>(*scale);
    }

    if (auto value = doc.get("Log", "level")) {
        const auto level = log::parse_level(*value);
        if (!level) {
            return invalid_value("Log", "level", *value,
                                 "trace, debug, info, warn, error, fatal ou off");
        }
        s.log_level = *level;
    }

    if (auto value = doc.get("Log", "file")) {
        s.log_file = unicode::path_from_utf8(*value);
    }

    return result;
}

std::filesystem::path resolve_path(const std::filesystem::path& base_directory,
                                   const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) {
        return path;
    }
    return (base_directory / path).lexically_normal();
}

} // namespace rpcs1
