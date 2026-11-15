#include <string>

#include "core/settings.hpp"
#include "core/unicode.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;

namespace {

Result<SettingsLoadResult> load(std::string_view text) {
    auto doc = IniDocument::parse(text);
    if (!doc) {
        return doc.error();
    }
    return settings_from_ini(doc.value());
}

} // namespace

TEST_CASE("settings/valores padrão") {
    const Settings s;
    CHECK(s.model == ConsoleModel::Scph5501);
    CHECK(!s.region_override.has_value());
    CHECK(s.effective_region() == Region::NtscUC);
    CHECK(s.bios_path.empty());
    CHECK_EQ(s.window_scale, u32{2});
    CHECK(s.log_level == log::Level::Info);
}

TEST_CASE("settings/documento vazio produz padrões sem avisos") {
    auto r = load("");
    REQUIRE(r.ok());
    CHECK(r.value().settings == Settings{});
    CHECK(r.value().warnings.empty());
}

TEST_CASE("settings/leitura de valores válidos") {
    auto r = load("[System]\nmodel = scph5502\nregion = NTSC-J\n"
                  "[BIOS]\npath = bios/scph5502.bin\n"
                  "[Video]\nscale = 4\n"
                  "[Log]\nlevel = debug\nfile =\n");
    REQUIRE(r.ok());
    const Settings& s = r.value().settings;
    CHECK(s.model == ConsoleModel::Scph5502);
    REQUIRE(s.region_override.has_value());
    CHECK(*s.region_override == Region::NtscJ);
    CHECK(s.effective_region() == Region::NtscJ);
    CHECK_EQ(s.bios_path, std::filesystem::path("bios/scph5502.bin"));
    CHECK_EQ(s.window_scale, u32{4});
    CHECK(s.log_level == log::Level::Debug);
    CHECK(s.log_file.empty());
}

TEST_CASE("settings/região auto segue o modelo") {
    struct Case {
        const char* model;
        Region region;
    };
    const Case cases[] = {
        {"SCPH-5500", Region::NtscJ},
        {"SCPH-5501", Region::NtscUC},
        {"SCPH-5502", Region::Pal},
        {"SCPH-1002", Region::Pal},
    };
    for (const auto& c : cases) {
        auto r = load(std::string("[System]\nregion = auto\nmodel = ") + c.model + "\n");
        REQUIRE(r.ok());
        CHECK(r.value().settings.effective_region() == c.region);
    }
}

TEST_CASE("settings/valores inválidos geram erro com seção e chave") {
    struct Case {
        const char* text;
        const char* expected;
    };
    const Case cases[] = {
        {"[System]\nmodel = SCPH-9999\n", "[System] model"},
        {"[System]\nregion = Marte\n", "[System] region"},
        {"[Video]\nscale = 0\n", "[Video] scale"},
        {"[Video]\nscale = 9\n", "[Video] scale"},
        {"[Video]\nscale = dois\n", "[Video] scale"},
        {"[Log]\nlevel = barulhento\n", "[Log] level"},
    };
    for (const auto& c : cases) {
        auto r = load(c.text);
        CHECK(!r.ok());
        if (!r.ok()) {
            CHECK(r.error().code == ErrorCode::InvalidData);
            CHECK(r.error().message.find(c.expected) != std::string::npos);
        }
    }
}

TEST_CASE("settings/chaves e seções desconhecidas geram avisos") {
    auto r = load("[System]\nmodel = SCPH-5501\nturbo = 1\n[Extra]\nx = 1\n");
    REQUIRE(r.ok());
    REQUIRE_EQ(r.value().warnings.size(), usize{2});
    CHECK(r.value().warnings[0].find("turbo") != std::string::npos);
    CHECK(r.value().warnings[1].find("[Extra]") != std::string::npos);
}

TEST_CASE("settings/to_ini e leitura fazem ida e volta") {
    Settings s;
    s.model = ConsoleModel::Scph1001;
    s.region_override = Region::Pal;
    s.bios_path = unicode::path_from_utf8("C:/BIOS do Usuário/scph1001.bin");
    s.window_scale = 3;
    s.log_level = log::Level::Trace;
    s.log_file = "logs/saida.log";

    const std::string text = s.to_ini().serialize();
    auto r = load(text);
    REQUIRE(r.ok());
    CHECK(r.value().settings == s);
    CHECK(r.value().warnings.empty());

    auto defaults = load(Settings{}.to_ini().serialize());
    REQUIRE(defaults.ok());
    CHECK(defaults.value().settings == Settings{});
}

TEST_CASE("settings/resolve_path") {
    const std::filesystem::path base = "C:/RPCS1";
    CHECK_EQ(resolve_path(base, "rpcs1.log"), std::filesystem::path("C:/RPCS1/rpcs1.log"));
    CHECK_EQ(resolve_path(base, "logs/../x.log"), std::filesystem::path("C:/RPCS1/x.log"));
    CHECK(resolve_path(base, "").empty());
#ifdef _WIN32
    CHECK_EQ(resolve_path(base, "D:/outro/x.log"), std::filesystem::path("D:/outro/x.log"));
#endif
}
