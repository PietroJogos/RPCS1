#include <string>
#include <vector>

#include "core/cli.hpp"
#include "core/unicode.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;

namespace {

Result<CommandLine> parse(std::vector<std::string> args) {
    return parse_command_line(args);
}

} // namespace

TEST_CASE("cli/sem argumentos usa padrões") {
    auto r = parse({});
    REQUIRE(r.ok());
    const auto& cl = r.value();
    CHECK(!cl.show_help);
    CHECK(!cl.show_version);
    CHECK(!cl.headless);
    CHECK(!cl.config_path.has_value());
    CHECK(!cl.log_level.has_value());
    CHECK(!cl.exit_after_ms.has_value());
}

TEST_CASE("cli/opções simples") {
    auto r = parse({"--help", "-v", "--headless"});
    REQUIRE(r.ok());
    CHECK(r.value().show_help);
    CHECK(r.value().show_version);
    CHECK(r.value().headless);
    CHECK(parse({"-h"}).value().show_help);
    CHECK(parse({"--version"}).value().show_version);
}

TEST_CASE("cli/opções com valor nas duas formas") {
    auto a = parse({"--config", "C:/Jogos PS1/rpcs1.ini", "--log-level", "debug",
                    "--exit-after", "500"});
    REQUIRE(a.ok());
    CHECK_EQ(*a.value().config_path, std::filesystem::path("C:/Jogos PS1/rpcs1.ini"));
    CHECK(*a.value().log_level == log::Level::Debug);
    CHECK_EQ(*a.value().exit_after_ms, u32{500});

    auto b = parse({"--config=x.ini", "--log-level=warn", "--exit-after=1"});
    REQUIRE(b.ok());
    CHECK_EQ(*b.value().config_path, std::filesystem::path("x.ini"));
    CHECK(*b.value().log_level == log::Level::Warn);
    CHECK_EQ(*b.value().exit_after_ms, u32{1});
}

TEST_CASE("cli/caminho com acentos é preservado em UTF-8") {
    auto r = parse({"--config", "C:/Configurações/Ação.ini"});
    REQUIRE(r.ok());
    CHECK_EQ(unicode::path_to_utf8(*r.value().config_path),
             std::string("C:/Configurações/Ação.ini"));
}

TEST_CASE("cli/erros") {
    const std::vector<std::vector<std::string>> invalid = {
        {"--config"},                 // falta valor
        {"--config="},                // caminho vazio
        {"--log-level", "alto"},      // nível inválido
        {"--exit-after", "0"},        // abaixo do mínimo
        {"--exit-after", "abc"},      // não numérico
        {"--exit-after", "999999999999"},
        {"--headless=1"},             // opção sem valor recebendo valor
        {"--nao-existe"},
        {"-x"},
        {"jogo.cue"},                 // abrir discos ainda não é suportado
    };
    for (const auto& args : invalid) {
        auto r = parse(args);
        CHECK(!r.ok());
    }
    auto positional = parse({"jogo.cue"});
    REQUIRE(!positional.ok());
    CHECK(positional.error().code == ErrorCode::Unsupported);
}

TEST_CASE("cli/--cpu-steps") {
    auto a = parse({"--cpu-steps", "1000"});
    REQUIRE(a.ok());
    CHECK_EQ(*a.value().cpu_steps, u64{1000});
    auto b = parse({"--cpu-steps=10000000000"});
    REQUIRE(b.ok());
    CHECK_EQ(*b.value().cpu_steps, u64{10000000000ull});
    CHECK(!parse({"--cpu-steps", "0"}).ok());
    CHECK(!parse({"--cpu-steps", "10000000001"}).ok());
    CHECK(!parse({"--cpu-steps", "muito"}).ok());
    CHECK(!parse({}).value().cpu_steps.has_value());
}

TEST_CASE("cli/texto de uso menciona as opções") {
    const std::string usage = usage_text();
    for (const char* option : {"--help", "--version", "--config", "--log-level", "--headless",
                               "--exit-after", "--cpu-steps"}) {
        CHECK(usage.find(option) != std::string::npos);
    }
}
