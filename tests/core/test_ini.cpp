#include <string>

#include "core/ini.hpp"
#include "core/types.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;

TEST_CASE("ini/leitura básica") {
    auto doc = IniDocument::parse("[System]\nmodel = SCPH-5501\nregion=PAL\n\n[Video]\nscale = 3\n");
    REQUIRE(doc.ok());
    CHECK_EQ(doc.value().get("System", "model"), std::optional<std::string>("SCPH-5501"));
    CHECK_EQ(doc.value().get("System", "region"), std::optional<std::string>("PAL"));
    CHECK_EQ(doc.value().get("Video", "scale"), std::optional<std::string>("3"));
    CHECK(!doc.value().get("Video", "inexistente").has_value());
    CHECK(!doc.value().get("Inexistente", "scale").has_value());
}

TEST_CASE("ini/busca não diferencia maiúsculas") {
    auto doc = IniDocument::parse("[BIOS]\nPath = x.bin\n");
    REQUIRE(doc.ok());
    CHECK_EQ(doc.value().get("bios", "PATH"), std::optional<std::string>("x.bin"));
}

TEST_CASE("ini/comentários, BOM e CRLF") {
    auto doc = IniDocument::parse("\xEF\xBB\xBF; topo\r\n\r\n# seção\r\n[Log]\r\n; nível\r\nlevel = debug\r\n");
    REQUIRE(doc.ok());
    CHECK_EQ(doc.value().get("Log", "level"), std::optional<std::string>("debug"));
    REQUIRE_EQ(doc.value().header_comments.size(), usize{1});
    CHECK_EQ(doc.value().header_comments[0], std::string("; topo"));
    const auto* section = doc.value().find_section("log");
    REQUIRE(section != nullptr);
    REQUIRE_EQ(section->comments.size(), usize{1});
    CHECK_EQ(section->comments[0], std::string("# seção"));
    REQUIRE_EQ(section->entries.size(), usize{1});
    CHECK_EQ(section->entries[0].comments[0], std::string("; nível"));
}

TEST_CASE("ini/valores entre aspas preservam espaços") {
    auto doc = IniDocument::parse("[BIOS]\npath = \"  C:/Minha BIOS/scph5501.bin  \"\n");
    REQUIRE(doc.ok());
    CHECK_EQ(doc.value().get("BIOS", "path"),
             std::optional<std::string>("  C:/Minha BIOS/scph5501.bin  "));
}

TEST_CASE("ini/valor vazio e '=' dentro do valor") {
    auto doc = IniDocument::parse("[A]\nvazio =\nexpr = a=b\n");
    REQUIRE(doc.ok());
    CHECK_EQ(doc.value().get("A", "vazio"), std::optional<std::string>(""));
    CHECK_EQ(doc.value().get("A", "expr"), std::optional<std::string>("a=b"));
}

TEST_CASE("ini/chave repetida: vale a última; seções repetidas se mesclam") {
    auto doc = IniDocument::parse("[A]\nk = 1\n[B]\nx = 0\n[a]\nk = 2\nj = 3\n");
    REQUIRE(doc.ok());
    CHECK_EQ(doc.value().get("A", "k"), std::optional<std::string>("2"));
    CHECK_EQ(doc.value().get("A", "j"), std::optional<std::string>("3"));
    CHECK_EQ(doc.value().sections().size(), usize{2});
}

TEST_CASE("ini/erros informam a linha") {
    struct Case {
        const char* text;
        const char* expected;
    };
    const Case cases[] = {
        {"[A]\nsem igual\n", "linha 2"},
        {"k = v\n", "linha 1: chave fora"},
        {"[A\n", "linha 1: cabeçalho"},
        {"[ ]\n", "nome de seção vazio"},
        {"[A]\n = v\n", "chave vazia"},
        {"[A]\nk = \"aberta\n", "aspas"},
    };
    for (const auto& c : cases) {
        auto doc = IniDocument::parse(c.text);
        CHECK(!doc.ok());
        if (!doc.ok()) {
            CHECK(doc.error().code == ErrorCode::ParseError);
            CHECK(doc.error().message.find(c.expected) != std::string::npos);
        }
    }
}

TEST_CASE("ini/texto vazio é válido") {
    auto doc = IniDocument::parse("");
    REQUIRE(doc.ok());
    CHECK(doc.value().sections().empty());
}

TEST_CASE("ini/serialize e parse fazem ida e volta") {
    IniDocument doc;
    doc.header_comments = {"; cabeçalho"};
    doc.set("System", "model", "SCPH-5501");
    doc.set_comment("System", "model", "linha 1\nlinha 2");
    doc.set("BIOS", "path", " espaço na borda ");
    doc.set("BIOS", "vazio", "");
    doc.set("Log", "file", "logs/rpcs1.log");
    doc.trailing_comments = {"; fim"};

    const std::string text = doc.serialize();
    auto parsed = IniDocument::parse(text);
    REQUIRE(parsed.ok());
    CHECK_EQ(parsed.value().get("System", "model"), std::optional<std::string>("SCPH-5501"));
    CHECK_EQ(parsed.value().get("BIOS", "path"), std::optional<std::string>(" espaço na borda "));
    CHECK_EQ(parsed.value().get("BIOS", "vazio"), std::optional<std::string>(""));
    CHECK_EQ(parsed.value().get("Log", "file"), std::optional<std::string>("logs/rpcs1.log"));
    CHECK_EQ(parsed.value().serialize(), text); // estável após a segunda passagem
    REQUIRE_EQ(parsed.value().trailing_comments.size(), usize{1});
    CHECK_EQ(parsed.value().trailing_comments[0], std::string("; fim"));
}

TEST_CASE("ini/save_file e load_file") {
    test::TempDir dir;
    IniDocument doc;
    doc.set("Video", "scale", "4");
    const auto path = dir.path() / "sub" / "config.ini";
    REQUIRE(doc.save_file(path).ok());
    auto loaded = IniDocument::load_file(path);
    REQUIRE(loaded.ok());
    CHECK_EQ(loaded.value().get("Video", "scale"), std::optional<std::string>("4"));

    auto missing = IniDocument::load_file(dir.path() / "nao_existe.ini");
    REQUIRE(!missing.ok());
    CHECK(missing.error().code == ErrorCode::NotFound);
}
