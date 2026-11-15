#include "core/console_model.hpp"

#include <array>

#include "core/assert.hpp"
#include "core/strings.hpp"

namespace rpcs1 {

namespace {

// Associação modelo → versão de BIOS conforme a tabela de versões do psx-spx
// (https://psx-spx.consoledev.net/kernelbios/). A versão real em uso será
// sempre determinada pela BIOS fornecida pelo usuário, nunca por esta tabela.
constexpr std::array<ConsoleModelInfo, 17> models = {{
    {ConsoleModel::Scph1000, "SCPH-1000", Region::NtscJ, "1.0J", false},
    {ConsoleModel::Scph1001, "SCPH-1001", Region::NtscUC, "2.1A, 2.2A", false},
    {ConsoleModel::Scph1002, "SCPH-1002", Region::Pal, "2.0E, 2.1E, 2.2E", false},
    {ConsoleModel::Scph3000, "SCPH-3000", Region::NtscJ, "1.1J", false},
    {ConsoleModel::Scph3500, "SCPH-3500", Region::NtscJ, "2.1J", false},
    {ConsoleModel::Scph5000, "SCPH-5000", Region::NtscJ, "2.2J", false},
    {ConsoleModel::Scph5500, "SCPH-5500", Region::NtscJ, "3.0J", false},
    {ConsoleModel::Scph5501, "SCPH-5501", Region::NtscUC, "3.0A", false},
    {ConsoleModel::Scph5502, "SCPH-5502", Region::Pal, "3.0E", false},
    {ConsoleModel::Scph7000, "SCPH-7000", Region::NtscJ, "4.0J", false},
    {ConsoleModel::Scph7001, "SCPH-7001", Region::NtscUC, "4.1A", false},
    {ConsoleModel::Scph7002, "SCPH-7002", Region::Pal, "4.1E", false},
    {ConsoleModel::Scph7501, "SCPH-7501", Region::NtscUC, "4.1A", false},
    {ConsoleModel::Scph7502, "SCPH-7502", Region::Pal, "4.1E", false},
    {ConsoleModel::Scph9000, "SCPH-9000", Region::NtscJ, "4.0J", false},
    {ConsoleModel::Scph9001, "SCPH-9001", Region::NtscUC, "4.1A", false},
    {ConsoleModel::Scph9002, "SCPH-9002", Region::Pal, "4.1E", false},
}};

} // namespace

std::string_view to_string(Region region) noexcept {
    switch (region) {
    case Region::NtscJ: return "NTSC-J";
    case Region::NtscUC: return "NTSC-U/C";
    case Region::Pal: return "PAL";
    }
    return "?";
}

std::optional<Region> parse_region(std::string_view text) noexcept {
    text = strings::trim(text);
    if (strings::iequals(text, "NTSC-J") || strings::iequals(text, "J") ||
        strings::iequals(text, "JP")) {
        return Region::NtscJ;
    }
    if (strings::iequals(text, "NTSC-U/C") || strings::iequals(text, "NTSC-U") ||
        strings::iequals(text, "NTSC-UC") || strings::iequals(text, "U/C") ||
        strings::iequals(text, "US")) {
        return Region::NtscUC;
    }
    if (strings::iequals(text, "PAL") || strings::iequals(text, "E") ||
        strings::iequals(text, "EU")) {
        return Region::Pal;
    }
    return std::nullopt;
}

char scex_letter(Region region) noexcept {
    switch (region) {
    case Region::NtscJ: return 'I';  // SCEI
    case Region::NtscUC: return 'A'; // SCEA
    case Region::Pal: return 'E';    // SCEE
    }
    return '?';
}

std::span<const ConsoleModelInfo> all_console_models() noexcept {
    return models;
}

const ConsoleModelInfo& model_info(ConsoleModel model) noexcept {
    for (const auto& info : models) {
        if (info.model == model) {
            return info;
        }
    }
    RPCS1_UNREACHABLE("modelo de console sem entrada no catálogo");
}

std::optional<ConsoleModel> parse_console_model(std::string_view text) noexcept {
    text = strings::trim(text);
    for (const auto& info : models) {
        // Aceita "SCPH-5501" e "SCPH5501".
        if (strings::iequals(text, info.name)) {
            return info.model;
        }
        if (text.size() + 1 == info.name.size() &&
            strings::iequals(text.substr(0, 4), info.name.substr(0, 4)) &&
            strings::iequals(text.substr(4), info.name.substr(5))) {
            return info.model;
        }
    }
    return std::nullopt;
}

} // namespace rpcs1
