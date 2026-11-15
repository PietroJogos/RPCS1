// Catálogo de modelos de console e regiões.
//
// IMPORTANTE: isto é apenas um catálogo de identificação. Nenhum perfil de
// hardware está implementado na Fase 0 — o campo `profile_implemented` é falso
// para todos os modelos e só será ativado quando o comportamento específico do
// modelo for de fato emulado e testado (Fase 16 em diante).
#pragma once

#include <optional>
#include <span>
#include <string_view>

namespace rpcs1 {

enum class Region { NtscJ, NtscUC, Pal };

[[nodiscard]] std::string_view to_string(Region region) noexcept;
[[nodiscard]] std::optional<Region> parse_region(std::string_view text) noexcept;

// Letra da string de licença SCEx que o drive espera para a região.
[[nodiscard]] char scex_letter(Region region) noexcept; // 'I', 'A' ou 'E'

enum class ConsoleModel {
    Scph1000,
    Scph1001,
    Scph1002,
    Scph3000,
    Scph3500,
    Scph5000,
    Scph5500,
    Scph5501,
    Scph5502,
    Scph7000,
    Scph7001,
    Scph7002,
    Scph7501,
    Scph7502,
    Scph9000,
    Scph9001,
    Scph9002,
};

struct ConsoleModelInfo {
    ConsoleModel model;
    std::string_view name;            // "SCPH-5501"
    Region region;
    std::string_view bios_versions;   // versões de BIOS associadas (documentação)
    bool profile_implemented;         // perfil de hardware emulado e testado?
};

[[nodiscard]] std::span<const ConsoleModelInfo> all_console_models() noexcept;
[[nodiscard]] const ConsoleModelInfo& model_info(ConsoleModel model) noexcept;
[[nodiscard]] std::optional<ConsoleModel> parse_console_model(std::string_view text) noexcept;

// Modelo-alvo inicial do projeto (ver docs/ROADMAP.md, Fase 16).
inline constexpr ConsoleModel default_console_model = ConsoleModel::Scph5501;

} // namespace rpcs1
