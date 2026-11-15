// Configurações do RPCS1 com validação.
//
// As configurações são lidas de um arquivo INI (rpcs1.ini). Valores inválidos
// geram erro com a seção/chave exatas; chaves desconhecidas geram avisos.
//
// Estado na Fase 0: apenas [Log] e [Video] têm efeito. [System] e [BIOS] são
// lidas e validadas, mas ainda não são usadas — não existe emulação.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/console_model.hpp"
#include "core/error.hpp"
#include "core/ini.hpp"
#include "core/log.hpp"
#include "core/types.hpp"

namespace rpcs1 {

struct Settings {
    // [System]
    ConsoleModel model = default_console_model;
    std::optional<Region> region_override; // vazio = região do modelo ("auto")

    // [BIOS]
    std::filesystem::path bios_path; // vazio = não configurada

    // [Video]
    u32 window_scale = 2; // multiplicador da saída de 320x240

    // [Log]
    log::Level log_level = log::Level::Info;
    std::filesystem::path log_file = "rpcs1.log"; // vazio = sem arquivo

    static constexpr u32 min_window_scale = 1;
    static constexpr u32 max_window_scale = 8;

    [[nodiscard]] Region effective_region() const noexcept;

    [[nodiscard]] IniDocument to_ini() const;

    bool operator==(const Settings&) const = default;
};

struct SettingsLoadResult {
    Settings settings;
    std::vector<std::string> warnings; // chaves/seções desconhecidas etc.
};

[[nodiscard]] Result<SettingsLoadResult> settings_from_ini(const IniDocument& doc);

// Resolve um caminho relativo em relação à pasta do arquivo de configuração.
[[nodiscard]] std::filesystem::path resolve_path(const std::filesystem::path& base_directory,
                                                 const std::filesystem::path& path);

} // namespace rpcs1
