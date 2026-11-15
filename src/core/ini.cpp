#include "core/ini.hpp"

#include <format>

#include "core/file_io.hpp"
#include "core/strings.hpp"

namespace rpcs1 {

namespace {

std::vector<std::string> comment_lines(std::string_view comment) {
    std::vector<std::string> lines;
    while (true) {
        const auto pos = comment.find('\n');
        const std::string_view line = comment.substr(0, pos);
        lines.push_back(line.empty() ? std::string(";") : "; " + std::string(line));
        if (pos == std::string_view::npos) {
            break;
        }
        comment.remove_prefix(pos + 1);
    }
    return lines;
}

bool needs_quotes(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    const auto first = value.front();
    const auto last = value.back();
    return first == ' ' || first == '\t' || last == ' ' || last == '\t' || first == '"' ||
           first == ';' || first == '#';
}

} // namespace

Result<IniDocument> IniDocument::parse(std::string_view text) {
    IniDocument doc;

    // Ignora BOM UTF-8.
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }

    std::vector<std::string> pending_comments;
    Section* current = nullptr;
    bool seen_content = false;
    std::size_t line_number = 0;

    while (!text.empty() || line_number == 0) {
        ++line_number;
        const auto newline = text.find('\n');
        std::string_view raw = text.substr(0, newline);
        text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
        const std::string_view line = strings::trim(raw);

        if (line.empty()) {
            // Linha em branco separa o bloco de comentários do topo do arquivo.
            if (!seen_content && !pending_comments.empty()) {
                doc.header_comments.insert(doc.header_comments.end(), pending_comments.begin(),
                                           pending_comments.end());
                pending_comments.clear();
            }
            if (text.empty()) {
                break;
            }
            continue;
        }

        if (line.front() == ';' || line.front() == '#') {
            pending_comments.emplace_back(line);
        } else if (line.front() == '[') {
            if (line.back() != ']') {
                return make_error(ErrorCode::ParseError,
                                  std::format("linha {}: cabeçalho de seção sem ']'", line_number));
            }
            const std::string_view name = strings::trim(line.substr(1, line.size() - 2));
            if (name.empty()) {
                return make_error(ErrorCode::ParseError,
                                  std::format("linha {}: nome de seção vazio", line_number));
            }
            current = &doc.ensure_section(name);
            current->comments.insert(current->comments.end(), pending_comments.begin(),
                                     pending_comments.end());
            pending_comments.clear();
            seen_content = true;
        } else {
            const auto equals = line.find('=');
            if (equals == std::string_view::npos) {
                return make_error(ErrorCode::ParseError,
                                  std::format("linha {}: esperado 'chave = valor'", line_number));
            }
            if (current == nullptr) {
                return make_error(ErrorCode::ParseError,
                                  std::format("linha {}: chave fora de qualquer seção", line_number));
            }
            const std::string_view key = strings::trim(line.substr(0, equals));
            std::string_view value = strings::trim(line.substr(equals + 1));
            if (key.empty()) {
                return make_error(ErrorCode::ParseError,
                                  std::format("linha {}: chave vazia", line_number));
            }
            if (!value.empty() && value.front() == '"') {
                if (value.size() < 2 || value.back() != '"') {
                    return make_error(ErrorCode::ParseError,
                                      std::format("linha {}: aspas não fechadas", line_number));
                }
                value = value.substr(1, value.size() - 2);
            }
            Entry& entry = doc.ensure_entry(current->name, key);
            entry.value = std::string(value);
            entry.comments.insert(entry.comments.end(), pending_comments.begin(),
                                  pending_comments.end());
            pending_comments.clear();
            seen_content = true;
            // ensure_entry pode realocar o vetor de seções.
            current = &doc.ensure_section(current->name);
        }

        if (text.empty()) {
            break;
        }
    }

    if (!seen_content) {
        doc.header_comments.insert(doc.header_comments.end(), pending_comments.begin(),
                                   pending_comments.end());
    } else {
        doc.trailing_comments = std::move(pending_comments);
    }
    return doc;
}

Result<IniDocument> IniDocument::load_file(const std::filesystem::path& path) {
    auto text = file_io::read_text(path, max_file_size);
    if (!text) {
        return text.error();
    }
    return parse(text.value());
}

std::string IniDocument::serialize() const {
    std::string out;
    for (const auto& line : header_comments) {
        out += line;
        out += '\n';
    }
    if (!header_comments.empty()) {
        out += '\n';
    }
    for (std::size_t i = 0; i < sections_.size(); ++i) {
        const Section& section = sections_[i];
        if (i > 0) {
            out += '\n';
        }
        for (const auto& line : section.comments) {
            out += line;
            out += '\n';
        }
        out += '[' + section.name + "]\n";
        for (const Entry& entry : section.entries) {
            for (const auto& line : entry.comments) {
                out += line;
                out += '\n';
            }
            out += entry.key;
            out += entry.value.empty() ? " =" : " = ";
            if (needs_quotes(entry.value)) {
                out += '"' + entry.value + '"';
            } else {
                out += entry.value;
            }
            out += '\n';
        }
    }
    if (!trailing_comments.empty()) {
        out += '\n';
        for (const auto& line : trailing_comments) {
            out += line;
            out += '\n';
        }
    }
    return out;
}

Status IniDocument::save_file(const std::filesystem::path& path) const {
    return file_io::write_atomic(path, std::string_view(serialize()));
}

const IniDocument::Section* IniDocument::find_section(std::string_view name) const {
    for (const auto& section : sections_) {
        if (strings::iequals(section.name, name)) {
            return &section;
        }
    }
    return nullptr;
}

std::optional<std::string> IniDocument::get(std::string_view section,
                                            std::string_view key) const {
    const Section* s = find_section(section);
    if (s == nullptr) {
        return std::nullopt;
    }
    for (const auto& entry : s->entries) {
        if (strings::iequals(entry.key, key)) {
            return entry.value;
        }
    }
    return std::nullopt;
}

void IniDocument::set(std::string_view section, std::string_view key, std::string value) {
    ensure_entry(section, key).value = std::move(value);
}

void IniDocument::set_comment(std::string_view section, std::string_view key,
                              std::string_view comment) {
    ensure_entry(section, key).comments = comment_lines(comment);
}

void IniDocument::set_section_comment(std::string_view section, std::string_view comment) {
    ensure_section(section).comments = comment_lines(comment);
}

IniDocument::Section& IniDocument::ensure_section(std::string_view name) {
    for (auto& section : sections_) {
        if (strings::iequals(section.name, name)) {
            return section;
        }
    }
    sections_.push_back(Section{std::string(name), {}, {}});
    return sections_.back();
}

IniDocument::Entry& IniDocument::ensure_entry(std::string_view section, std::string_view key) {
    Section& s = ensure_section(section);
    for (auto& entry : s.entries) {
        if (strings::iequals(entry.key, key)) {
            return entry;
        }
    }
    s.entries.push_back(Entry{std::string(key), {}, {}});
    return s.entries.back();
}

} // namespace rpcs1
