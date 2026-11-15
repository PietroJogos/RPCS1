#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/log.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;
using log::Category;
using log::Level;

namespace {

// Isola o logger global: instala um MemorySink e restaura o estado ao sair.
struct LogCapture {
    std::shared_ptr<log::MemorySink> sink = std::make_shared<log::MemorySink>();
    Level previous;

    explicit LogCapture(Level level) : previous(log::Logger::instance().level()) {
        auto& logger = log::Logger::instance();
        logger.set_level(level);
        logger.add_sink(sink);
    }
    ~LogCapture() {
        auto& logger = log::Logger::instance();
        logger.remove_sink(sink);
        logger.set_level(previous);
        for (usize i = 0; i < log::category_count; ++i) {
            logger.set_category_level(static_cast<Category>(i), std::nullopt);
        }
    }
};

// Tipo cuja formatação é contada, para provar que mensagens filtradas não são formatadas.
int g_format_calls = 0;
struct FormatProbe {};

} // namespace

template <>
struct std::formatter<FormatProbe> : std::formatter<int> {
    auto format(const FormatProbe&, std::format_context& ctx) const {
        ++g_format_calls;
        return std::formatter<int>::format(0, ctx);
    }
};

TEST_CASE("log/parse_level e to_string") {
    CHECK(log::parse_level("trace") == Level::Trace);
    CHECK(log::parse_level("DEBUG") == Level::Debug);
    CHECK(log::parse_level(" info ") == Level::Info);
    CHECK(log::parse_level("warning") == Level::Warn);
    CHECK(log::parse_level("Error") == Level::Error);
    CHECK(log::parse_level("fatal") == Level::Fatal);
    CHECK(log::parse_level("off") == Level::Off);
    CHECK(!log::parse_level("verbose").has_value());
    CHECK_EQ(log::to_string(Level::Warn), std::string_view("WARN"));
    CHECK_EQ(log::to_string(Category::Cdrom), std::string_view("CDROM"));
}

TEST_CASE("log/todas as categorias têm nome") {
    for (usize i = 0; i < log::category_count; ++i) {
        CHECK(log::to_string(static_cast<Category>(i)) != std::string_view("?"));
    }
}

TEST_CASE("log/filtra por nível global") {
    LogCapture capture(Level::Warn);
    log::debug(Category::Core, "descartada {}", 1);
    log::info(Category::Core, "descartada");
    log::warn(Category::Core, "aviso {}", 2);
    log::error(Category::Gpu, "erro {}", 3);
    const auto records = capture.sink->records();
    REQUIRE_EQ(records.size(), usize{2});
    CHECK(records[0].level == Level::Warn);
    CHECK_EQ(records[0].message, std::string("aviso 2"));
    CHECK(records[1].category == Category::Gpu);
    CHECK_EQ(records[1].message, std::string("erro 3"));
}

TEST_CASE("log/nível por categoria sobrepõe o global") {
    LogCapture capture(Level::Error);
    log::Logger::instance().set_category_level(Category::Cpu, Level::Trace);
    log::trace(Category::Cpu, "cpu trace");
    log::trace(Category::Gpu, "gpu trace (descartada)");
    log::Logger::instance().set_category_level(Category::Cpu, std::nullopt);
    log::info(Category::Cpu, "cpu info (descartada)");
    const auto records = capture.sink->records();
    REQUIRE_EQ(records.size(), usize{1});
    CHECK_EQ(records[0].message, std::string("cpu trace"));
}

TEST_CASE("log/nível Off desativa tudo") {
    LogCapture capture(Level::Off);
    log::fatal(Category::Core, "não deve aparecer");
    CHECK_EQ(capture.sink->size(), usize{0});
    CHECK(!log::Logger::instance().should_log(Level::Off, Category::Core));
}

TEST_CASE("log/mensagens filtradas não são formatadas") {
    LogCapture capture(Level::Error);
    g_format_calls = 0;
    log::debug(Category::Core, "{}", FormatProbe{});
    CHECK_EQ(g_format_calls, 0);
    CHECK_EQ(capture.sink->size(), usize{0});
    log::error(Category::Core, "{}", FormatProbe{});
    CHECK_EQ(g_format_calls, 1);
    CHECK_EQ(capture.sink->size(), usize{1});
}

TEST_CASE("log/format_record") {
    log::Record r;
    r.level = Level::Info;
    r.category = Category::Bios;
    r.elapsed = std::chrono::milliseconds(12345);
    r.message = "BIOS carregada";
    const std::string text = log::format_record(r);
    CHECK_EQ(text, std::string("[   12.345] [INFO ] [BIOS    ] BIOS carregada"));
}

TEST_CASE("log/remove_sink interrompe a entrega") {
    LogCapture capture(Level::Info);
    log::info(Category::Core, "um");
    log::Logger::instance().remove_sink(capture.sink);
    log::info(Category::Core, "dois");
    CHECK_EQ(capture.sink->size(), usize{1});
}

TEST_CASE("log/seguro com múltiplas threads") {
    LogCapture capture(Level::Info);
    constexpr int thread_count = 8;
    constexpr int per_thread = 500;
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([t] {
            for (int i = 0; i < per_thread; ++i) {
                log::info(Category::Debug, "thread {} mensagem {}", t, i);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    CHECK_EQ(capture.sink->size(), static_cast<usize>(thread_count * per_thread));
}
