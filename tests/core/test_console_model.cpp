#include <set>
#include <string>

#include "core/console_model.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;

TEST_CASE("console_model/catálogo consistente") {
    std::set<std::string_view> names;
    for (const auto& info : all_console_models()) {
        CHECK(names.insert(info.name).second); // nomes únicos
        CHECK(&model_info(info.model) == &info);
        CHECK(!info.profile_implemented);      // nenhum perfil existe na Fase 0
        // Convenção dos modelos SCPH-xxx0/1/2: Japão, América do Norte, Europa.
        const char last = info.name.back();
        if (last == '0') CHECK(info.region == Region::NtscJ);
        if (last == '1') CHECK(info.region == Region::NtscUC);
        if (last == '2') CHECK(info.region == Region::Pal);
    }
    CHECK(names.count("SCPH-5501") == 1);
}

TEST_CASE("console_model/parse aceita variações") {
    CHECK(parse_console_model("SCPH-5501") == ConsoleModel::Scph5501);
    CHECK(parse_console_model("scph-5501") == ConsoleModel::Scph5501);
    CHECK(parse_console_model("SCPH5501") == ConsoleModel::Scph5501);
    CHECK(parse_console_model(" SCPH-1001 ") == ConsoleModel::Scph1001);
    CHECK(!parse_console_model("SCPH-5503").has_value());
    CHECK(!parse_console_model("").has_value());
    CHECK(!parse_console_model("SCPH-").has_value());
}

TEST_CASE("console_model/regiões e letras SCEx") {
    CHECK(parse_region("NTSC-J") == Region::NtscJ);
    CHECK(parse_region("ntsc-u/c") == Region::NtscUC);
    CHECK(parse_region("NTSC-U") == Region::NtscUC);
    CHECK(parse_region("pal") == Region::Pal);
    CHECK(!parse_region("SECAM").has_value());
    CHECK_EQ(scex_letter(Region::NtscJ), 'I');
    CHECK_EQ(scex_letter(Region::NtscUC), 'A');
    CHECK_EQ(scex_letter(Region::Pal), 'E');
    for (Region r : {Region::NtscJ, Region::NtscUC, Region::Pal}) {
        CHECK(parse_region(to_string(r)) == r);
    }
}
