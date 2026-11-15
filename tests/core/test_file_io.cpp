#include <fstream>
#include <string>
#include <vector>

#include "core/file_io.hpp"
#include "core/unicode.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;

namespace {

void write_raw(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

} // namespace

TEST_CASE("file_io/arquivo inexistente retorna NotFound") {
    test::TempDir dir;
    auto r = file_io::read_binary(dir.path() / "ausente.bin", 1024);
    REQUIRE(!r.ok());
    CHECK(r.error().code == ErrorCode::NotFound);
}

TEST_CASE("file_io/pasta não é aceita como arquivo") {
    test::TempDir dir;
    auto r = file_io::read_binary(dir.path(), 1024);
    REQUIRE(!r.ok());
    CHECK(r.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("file_io/limite de tamanho é respeitado") {
    test::TempDir dir;
    const auto path = dir.path() / "grande.bin";
    write_raw(path, std::string(513, 'x'));
    auto r = file_io::read_binary(path, 512);
    REQUIRE(!r.ok());
    CHECK(r.error().code == ErrorCode::OutOfRange);
    CHECK(file_io::read_binary(path, 513).ok());
}

TEST_CASE("file_io/leitura binária preserva todos os bytes") {
    test::TempDir dir;
    const auto path = dir.path() / "dados.bin";
    std::string content;
    for (int i = 0; i < 256; ++i) {
        content.push_back(static_cast<char>(i));
    }
    write_raw(path, content);
    auto r = file_io::read_binary(path, 4096);
    REQUIRE(r.ok());
    REQUIRE_EQ(r.value().size(), usize{256});
    for (usize i = 0; i < 256; ++i) {
        CHECK_EQ(r.value()[i], static_cast<u8>(i));
    }
}

TEST_CASE("file_io/arquivo vazio") {
    test::TempDir dir;
    const auto path = dir.path() / "vazio.bin";
    write_raw(path, "");
    auto r = file_io::read_binary(path, 16);
    REQUIRE(r.ok());
    CHECK(r.value().empty());
}

TEST_CASE("file_io/write_atomic cria pastas, substitui e não deixa .tmp") {
    test::TempDir dir;
    const auto path = dir.path() / "a" / "b" / "config.ini";
    REQUIRE(file_io::write_atomic(path, std::string_view("primeiro")).ok());
    REQUIRE(file_io::write_atomic(path, std::string_view("segundo")).ok());
    auto r = file_io::read_text(path, 64);
    REQUIRE(r.ok());
    CHECK_EQ(r.value(), std::string("segundo"));
    auto temp = path;
    temp += ".tmp";
    CHECK(!std::filesystem::exists(temp));

    const std::vector<u8> bytes = {0x00, 0xFF, 0x10};
    REQUIRE(file_io::write_atomic(path, std::span<const u8>(bytes)).ok());
    auto b = file_io::read_binary(path, 64);
    REQUIRE(b.ok());
    CHECK(b.value() == bytes);
}

TEST_CASE("file_io/nomes de arquivo com acentos") {
    test::TempDir dir;
    const auto path = dir.path() / unicode::path_from_utf8("cartão_memória_ç.mcd");
    REQUIRE(file_io::write_atomic(path, std::string_view("ok")).ok());
    auto r = file_io::read_text(path, 16);
    REQUIRE(r.ok());
    CHECK_EQ(r.value(), std::string("ok"));
}
