#include "core/strings.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;

TEST_CASE("strings/trim remove espaços das bordas") {
    CHECK_EQ(strings::trim("  abc \t\r\n"), std::string_view("abc"));
    CHECK_EQ(strings::trim(""), std::string_view(""));
    CHECK_EQ(strings::trim("   "), std::string_view(""));
    CHECK_EQ(strings::trim("a b"), std::string_view("a b"));
}

TEST_CASE("strings/iequals ignora maiúsculas ASCII") {
    CHECK(strings::iequals("SCPH-5501", "scph-5501"));
    CHECK(!strings::iequals("abc", "abcd"));
    CHECK(!strings::iequals("abc", "abd"));
    CHECK(strings::iequals("", ""));
}

TEST_CASE("strings/parse_unsigned aceita decimal e hexadecimal") {
    CHECK_EQ(strings::parse_unsigned("0"), std::optional<u64>(0));
    CHECK_EQ(strings::parse_unsigned("1234"), std::optional<u64>(1234));
    CHECK_EQ(strings::parse_unsigned("0x1F801070"), std::optional<u64>(0x1F801070));
    CHECK_EQ(strings::parse_unsigned("0XbfC00000"), std::optional<u64>(0xBFC00000));
    CHECK_EQ(strings::parse_unsigned("18446744073709551615"),
             std::optional<u64>(UINT64_MAX));
}

TEST_CASE("strings/parse_unsigned rejeita entradas inválidas") {
    CHECK(!strings::parse_unsigned(""));
    CHECK(!strings::parse_unsigned(" 1"));
    CHECK(!strings::parse_unsigned("1 "));
    CHECK(!strings::parse_unsigned("-1"));
    CHECK(!strings::parse_unsigned("+1"));
    CHECK(!strings::parse_unsigned("12abc"));
    CHECK(!strings::parse_unsigned("0x"));
    CHECK(!strings::parse_unsigned("18446744073709551616")); // estoura 64 bits
}

TEST_CASE("strings/parse_unsigned respeita limites") {
    CHECK_EQ(strings::parse_unsigned("8", 1, 8), std::optional<u64>(8));
    CHECK(!strings::parse_unsigned("9", 1, 8));
    CHECK(!strings::parse_unsigned("0", 1, 8));
}

TEST_CASE("strings/parse_bool") {
    CHECK_EQ(strings::parse_bool("true"), std::optional<bool>(true));
    CHECK_EQ(strings::parse_bool("SIM"), std::optional<bool>(true));
    CHECK_EQ(strings::parse_bool("1"), std::optional<bool>(true));
    CHECK_EQ(strings::parse_bool("off"), std::optional<bool>(false));
    CHECK_EQ(strings::parse_bool("não"), std::optional<bool>(false));
    CHECK(!strings::parse_bool("talvez"));
    CHECK(!strings::parse_bool(""));
}
