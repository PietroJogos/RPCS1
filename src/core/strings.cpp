#include "core/strings.hpp"

#include <charconv>
#include <system_error>

namespace rpcs1::strings {

namespace {

constexpr bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

constexpr char lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

} // namespace

std::string_view trim(std::string_view text) noexcept {
    usize begin = 0;
    usize end = text.size();
    while (begin < end && is_space(text[begin])) {
        ++begin;
    }
    while (end > begin && is_space(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string to_lower_ascii(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = lower(c);
    }
    return out;
}

bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (usize i = 0; i < a.size(); ++i) {
        if (lower(a[i]) != lower(b[i])) {
            return false;
        }
    }
    return true;
}

bool starts_with(std::string_view text, std::string_view prefix) noexcept {
    return text.substr(0, prefix.size()) == prefix;
}

std::optional<u64> parse_unsigned(std::string_view text, u64 min, u64 max) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    }
    u64 value = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value, base);
    if (ec != std::errc{} || ptr != last) {
        return std::nullopt;
    }
    if (value < min || value > max) {
        return std::nullopt;
    }
    return value;
}

std::optional<bool> parse_bool(std::string_view text) noexcept {
    static constexpr std::string_view truthy[] = {"true", "1", "yes", "on", "sim"};
    static constexpr std::string_view falsy[] = {"false", "0", "no", "off", "nao", "não"};
    for (auto word : truthy) {
        if (iequals(text, word)) {
            return true;
        }
    }
    for (auto word : falsy) {
        if (iequals(text, word)) {
            return false;
        }
    }
    return std::nullopt;
}

} // namespace rpcs1::strings
