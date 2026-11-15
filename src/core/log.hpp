// Sistema de logging próprio do RPCS1.
//
// - Níveis de severidade: Trace, Debug, Info, Warn, Error, Fatal (e Off).
// - Categorias por componente (CPU, GPU, CDROM...), cada uma com nível opcional.
// - Vários destinos (console, arquivo, memória para testes).
// - Seguro para uso por múltiplas threads.
// - A formatação só acontece se a mensagem passar pelo filtro de nível.
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/types.hpp"

namespace rpcs1::log {

enum class Level : u8 { Trace = 0, Debug, Info, Warn, Error, Fatal, Off };

[[nodiscard]] std::string_view to_string(Level level) noexcept;
[[nodiscard]] std::optional<Level> parse_level(std::string_view text) noexcept;

enum class Category : u8 {
    General = 0,
    Core,
    Config,
    Frontend,
    Video,
    Audio,
    Input,
    Cpu,
    Bus,
    Memory,
    Gpu,
    Gte,
    Mdec,
    Spu,
    Cdrom,
    Bios,
    Dma,
    Timers,
    Irq,
    Sio,
    Pad,
    MemCard,
    Debug,
    Count
};

inline constexpr usize category_count = static_cast<usize>(Category::Count);

[[nodiscard]] std::string_view to_string(Category category) noexcept;

struct Record {
    Level level = Level::Info;
    Category category = Category::General;
    std::chrono::milliseconds elapsed{0}; // tempo desde a criação do logger
    std::string message;
};

// Formato: "[    1.234] [INFO ] [Core    ] mensagem"
[[nodiscard]] std::string format_record(const Record& record);

class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(const Record& record, std::string_view formatted) = 0;
    virtual void flush() {}
};

// Escreve em stderr (UTF-8).
class ConsoleSink final : public Sink {
public:
    void write(const Record& record, std::string_view formatted) override;
    void flush() override;
};

// Escreve em arquivo (UTF-8), truncando-o na abertura.
class FileSink final : public Sink {
public:
    static Result<std::shared_ptr<FileSink>> open(const std::filesystem::path& path);
    void write(const Record& record, std::string_view formatted) override;
    void flush() override;

private:
    explicit FileSink(std::ofstream stream) : stream_(std::move(stream)) {}
    std::ofstream stream_;
};

// Guarda registros em memória (usado em testes e, futuramente, no visualizador de logs).
class MemorySink final : public Sink {
public:
    void write(const Record& record, std::string_view formatted) override;
    [[nodiscard]] std::vector<Record> records() const;
    [[nodiscard]] usize size() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<Record> records_;
};

class Logger {
public:
    Logger();

    // Logger global do processo.
    static Logger& instance();

    void set_level(Level level) noexcept;
    [[nodiscard]] Level level() const noexcept;

    // Sobrescreve o nível de uma categoria; std::nullopt volta a usar o nível global.
    void set_category_level(Category category, std::optional<Level> level) noexcept;
    [[nodiscard]] Level effective_level(Category category) const noexcept;

    [[nodiscard]] bool should_log(Level level, Category category) const noexcept;

    void add_sink(std::shared_ptr<Sink> sink);
    void remove_sink(const std::shared_ptr<Sink>& sink);
    void clear_sinks();

    void write(Level level, Category category, std::string message);
    void flush();

private:
    static constexpr u8 inherit = 0xFF;

    std::atomic<u8> global_level_;
    std::array<std::atomic<u8>, category_count> category_levels_;
    std::chrono::steady_clock::time_point start_;
    std::mutex mutex_;
    std::vector<std::shared_ptr<Sink>> sinks_;
};

template <typename... Args>
void write(Level level, Category category, std::format_string<Args...> fmt, Args&&... args) {
    Logger& logger = Logger::instance();
    if (!logger.should_log(level, category)) {
        return;
    }
    logger.write(level, category, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void trace(Category c, std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Trace, c, fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void debug(Category c, std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Debug, c, fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void info(Category c, std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Info, c, fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void warn(Category c, std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Warn, c, fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void error(Category c, std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Error, c, fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void fatal(Category c, std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Fatal, c, fmt, std::forward<Args>(args)...);
}

} // namespace rpcs1::log
