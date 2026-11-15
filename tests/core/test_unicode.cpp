#include <string>

#include "core/unicode.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;

TEST_CASE("unicode/caminho UTF-8 faz ida e volta") {
    const std::string text = "C:/Usuários/João/Memória ção — 日本.ini";
    const auto path = unicode::path_from_utf8(text);
    CHECK_EQ(unicode::path_to_utf8(path), text);
}

TEST_CASE("unicode/caminho vazio") {
    CHECK(unicode::path_from_utf8("").empty());
    CHECK_EQ(unicode::path_to_utf8({}), std::string());
}

#ifdef _WIN32
TEST_CASE("unicode/conversão UTF-16 <-> UTF-8 no Windows") {
    const std::wstring wide = L"A\u00E7\u00E3o \u65E5\u672C";
    const std::string utf8 = unicode::wide_to_utf8(wide);
    CHECK_EQ(utf8, std::string("Ação 日本"));
    CHECK(unicode::utf8_to_wide(utf8) == wide);
    CHECK(unicode::wide_to_utf8(L"").empty());
    CHECK(unicode::utf8_to_wide("").empty());
}
#endif
