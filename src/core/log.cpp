#include "core/log.hpp"

#include <algorithm>
#include <cstdio>

#include "core/strings.hpp"

namespace rpcs1::log {

std::string_view to_string(Level level) noexcept {
    switch (level) {
    case Level::Trace: return "TRACE";
    case Level::Debug: return "DEBUG";
    case Level::Info: return "INFO";
    case Level::Warn: return "WARN";
    case Level::Error: return "ERROR";
    case Level::Fatal: return "FATAL";
    case Level::Off: return "OFF";
    }
    return "?";
}

std::optional<Level> parse_level(std::string_view text) noexcept {
    text = strings::trim(text);
    struct Entry {
        std::string_view name;
        Level level;
    };
    static constexpr Entry entries[] = {
        {"trace", Level::Trace}, {"debug", Level::Debug},   {"info", Level::Info},
        {"warn", Level::Warn},   {"warning", Level::Warn},  {"error", Level::Error},
        {"fatal", Level::Fatal}, {"off", Level::Off},       {"none", Level::Off},
    };
    for (const auto& entry : entries) {
        if (strings::iequals(text, entry.name)) {
            return entry.level;
        }
    }
    return std::nullopt;
}

std::string_view to_string(Category category) noexcept {
    switch (category) {
    case Category::General: return "General";
    case Category::Core: return "Core";
    case Category::Config: return "Config";
    case Category::Frontend: return "Frontend";
    case Category::Video: return "Video";
    case Category::Audio: return "Audio";
    case Category::Input: return "Input";
    case Category::Cpu: return "CPU";
    case Category::Bus: return "Bus";
    case Category::Memory: return "Memory";
    case Category::Gpu: return "GPU";
    case Category::Gte: return "GTE";
    case Category::Mdec: return "MDEC";
    case Category::Spu: return "SPU";
    case Category::Cdrom: return "CDROM";
    case Category::Bios: return "BIOS";
    case Category::Dma: return "DMA";
    case Category::Timers: return "Timers";
    case Category::Irq: return "IRQ";
    case Category::Sio: return "SIO";
    case Category::Pad: return "Pad";
    case Category::MemCard: return "MemCard";
    case Category::Debug: return "Debug";
    case Category::Count: break;
    }
    return "?";
}

std::string format_record(const Record& record) {
    const auto ms = record.elapsed.count();
    return std::format("[{:>5}.{:03}] [{:<5}] [{:<8}] {}", ms / 1000, ms % 1000,
                       to_string(record.level), to_string(record.category), record.message);
}

// ---------------------------------------------------------------- destinos

void ConsoleSink::write(const Record& /*record*/, std::string_view formatted) {
    std::fwrite(formatted.data(), 1, formatted.size(), stderr);
    std::fputc('\n', stderr);
}

void ConsoleSink::flush() {
    std::fflush(stderr);
}

Result<std::shared_ptr<FileSink>> FileSink::open(const std::filesystem::path& path) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return make_error(ErrorCode::IoError, "não foi possível criar a pasta do log: " +
                                                      ec.message());
        }
    }
    std::ofstream stream(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!stream) {
        return make_error(ErrorCode::IoError, "não foi possível abrir o arquivo de log");
    }
    return std::shared_ptr<FileSink>(new FileSink(std::move(stream)));
}

void FileSink::write(const Record& /*record*/, std::string_view formatted) {
    stream_.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
    stream_.put('\n');
}

void FileSink::flush() {
    stream_.flush();
}

void MemorySink::write(const Record& record, std::string_view /*formatted*/) {
    std::lock_guard lock(mutex_);
    records_.push_back(record);
}

std::vector<Record> MemorySink::records() const {
    std::lock_guard lock(mutex_);
    return records_;
}

usize MemorySink::size() const {
    std::lock_guard lock(mutex_);
    return records_.size();
}

void MemorySink::clear() {
    std::lock_guard lock(mutex_);
    records_.clear();
}

// ------------------------------------------------------------------ logger

Logger::Logger()
    : global_level_(static_cast<u8>(Level::Info)), start_(std::chrono::steady_clock::now()) {
    for (auto& level : category_levels_) {
        level.store(inherit, std::memory_order_relaxed);
    }
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(Level level) noexcept {
    global_level_.store(static_cast<u8>(level), std::memory_order_relaxed);
}

Level Logger::level() const noexcept {
    return static_cast<Level>(global_level_.load(std::memory_order_relaxed));
}

void Logger::set_category_level(Category category, std::optional<Level> level) noexcept {
    const auto index = static_cast<usize>(category);
    if (index >= category_count) {
        return;
    }
    category_levels_[index].store(level ? static_cast<u8>(*level) : inherit,
                                  std::memory_order_relaxed);
}

Level Logger::effective_level(Category category) const noexcept {
    const auto index = static_cast<usize>(category);
    if (index < category_count) {
        const u8 value = category_levels_[index].load(std::memory_order_relaxed);
        if (value != inherit) {
            return static_cast<Level>(value);
        }
    }
    return level();
}

bool Logger::should_log(Level level, Category category) const noexcept {
    if (level == Level::Off) {
        return false;
    }
    const Level threshold = effective_level(category);
    return threshold != Level::Off && level >= threshold;
}

void Logger::add_sink(std::shared_ptr<Sink> sink) {
    if (!sink) {
        return;
    }
    std::lock_guard lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::remove_sink(const std::shared_ptr<Sink>& sink) {
    std::lock_guard lock(mutex_);
    sinks_.erase(std::remove(sinks_.begin(), sinks_.end(), sink), sinks_.end());
}

void Logger::clear_sinks() {
    std::lock_guard lock(mutex_);
    sinks_.clear();
}

void Logger::write(Level level, Category category, std::string message) {
    if (!should_log(level, category)) {
        return;
    }
    Record record;
    record.level = level;
    record.category = category;
    record.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_);
    record.message = std::move(message);
    const std::string formatted = format_record(record);

    std::lock_guard lock(mutex_);
    for (const auto& sink : sinks_) {
        sink->write(record, formatted);
    }
    // Erros graves são gravados imediatamente para não se perderem em um travamento.
    if (level >= Level::Error) {
        for (const auto& sink : sinks_) {
            sink->flush();
        }
    }
}

void Logger::flush() {
    std::lock_guard lock(mutex_);
    for (const auto& sink : sinks_) {
        sink->flush();
    }
}

} // namespace rpcs1::log
