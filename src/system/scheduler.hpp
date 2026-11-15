// Agendador de eventos por ciclo da CPU (Fase 5).
//
// Cada componente tem no máximo um evento pendente, identificado por EventId. O console
// despacha, antes de cada instrução, os eventos cujo horário já chegou, em ordem de tempo
// (empates: ordem do EventId). O tempo "agora" é o contador de ciclos da CPU.
#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "core/types.hpp"

namespace rpcs1::system {

enum class EventId : u8 { Video = 0, Timer0, Timer1, Timer2, Dma, Count };

[[nodiscard]] std::string_view to_string(EventId id) noexcept;

struct DueEvent {
    EventId id;
    u64 time;
};

class Scheduler {
public:
    static constexpr u64 never = ~u64{0};

    void reset() noexcept;

    // Tempo atual (ciclos da CPU). A fonte é definida pelo Console.
    void set_time_source(const u64* cycles) noexcept { now_ = cycles; }
    [[nodiscard]] u64 now() const noexcept { return now_ ? *now_ : 0; }

    void schedule(EventId id, u64 time) noexcept;
    void cancel(EventId id) noexcept;
    [[nodiscard]] std::optional<u64> scheduled(EventId id) const noexcept;

    [[nodiscard]] u64 next_time() const noexcept { return next_; }

    // Remove e devolve o evento mais antigo com horário <= time, se houver.
    [[nodiscard]] std::optional<DueEvent> pop_due(u64 time) noexcept;

private:
    void recompute_next() noexcept;

    static constexpr usize count = static_cast<usize>(EventId::Count);
    std::array<u64, count> times_ = [] {
        std::array<u64, count> a{};
        a.fill(never);
        return a;
    }();
    u64 next_ = never;
    const u64* now_ = nullptr;
};

} // namespace rpcs1::system
