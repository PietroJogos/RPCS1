// Documento INI simples com preservação de ordem e de comentários.
//
// Sintaxe aceita:
//   ; comentário           (também '#')
//   [Seção]
//   chave = valor
//   chave = "valor com espaços nas bordas"
// Seções e chaves não diferenciam maiúsculas/minúsculas na busca.
// Chaves repetidas: vale a última. Seções repetidas são mescladas.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"

namespace rpcs1 {

class IniDocument {
public:
    struct Entry {
        std::string key;
        std::string value;
        std::vector<std::string> comments; // linhas de comentário antes da chave
    };
    struct Section {
        std::string name;
        std::vector<Entry> entries;
        std::vector<std::string> comments; // linhas de comentário antes do cabeçalho
    };

    static constexpr std::size_t max_file_size = 1024 * 1024;

    [[nodiscard]] static Result<IniDocument> parse(std::string_view text);
    [[nodiscard]] static Result<IniDocument> load_file(const std::filesystem::path& path);

    [[nodiscard]] std::string serialize() const;
    [[nodiscard]] Status save_file(const std::filesystem::path& path) const;

    [[nodiscard]] std::optional<std::string> get(std::string_view section,
                                                 std::string_view key) const;
    void set(std::string_view section, std::string_view key, std::string value);

    // Comentário multilinha associado a uma chave ou seção (prefixo "; " automático).
    void set_comment(std::string_view section, std::string_view key, std::string_view comment);
    void set_section_comment(std::string_view section, std::string_view comment);

    [[nodiscard]] const std::vector<Section>& sections() const noexcept { return sections_; }
    [[nodiscard]] const Section* find_section(std::string_view name) const;

    std::vector<std::string> header_comments;  // comentários no topo do arquivo
    std::vector<std::string> trailing_comments; // comentários após a última chave

private:
    Section& ensure_section(std::string_view name);
    Entry& ensure_entry(std::string_view section, std::string_view key);

    std::vector<Section> sections_;
};

} // namespace rpcs1
