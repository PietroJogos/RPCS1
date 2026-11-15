#include "framework/rpcs1_test.hpp"
#include "system/clock.hpp"
#include "system/scheduler.hpp"

using namespace rpcs1;
using namespace rpcs1::system;

TEST_CASE("scheduler/vazio, agendamento, ordem e cancelamento") {
    Scheduler s;
    CHECK_EQ(s.next_time(), Scheduler::never);
    CHECK(!s.pop_due(1'000'000).has_value());

    s.schedule(EventId::Timer2, 300);
    s.schedule(EventId::Video, 100);
    s.schedule(EventId::Timer0, 100); // empate com Video: menor EventId primeiro
    CHECK_EQ(s.next_time(), u64{100});
    CHECK(!s.pop_due(99).has_value());

    auto a = s.pop_due(1000);
    REQUIRE(a.has_value());
    CHECK(a->id == EventId::Video);
    auto b = s.pop_due(1000);
    REQUIRE(b.has_value());
    CHECK(b->id == EventId::Timer0);
    CHECK_EQ(s.next_time(), u64{300});

    s.schedule(EventId::Timer2, 50); // reagendar substitui
    CHECK_EQ(s.scheduled(EventId::Timer2), std::optional<u64>(50));
    s.cancel(EventId::Timer2);
    CHECK(!s.scheduled(EventId::Timer2).has_value());
    CHECK_EQ(s.next_time(), Scheduler::never);
}

TEST_CASE("scheduler/fonte de tempo") {
    Scheduler s;
    CHECK_EQ(s.now(), u64{0});
    u64 cycles = 1234;
    s.set_time_source(&cycles);
    CHECK_EQ(s.now(), u64{1234});
    cycles = 99;
    CHECK_EQ(s.now(), u64{99});
}

TEST_CASE("clock/conversão CPU ↔ vídeo é exata e sem deriva") {
    const ClockRatio ntsc(video_clock_ntsc_hz);
    const ClockRatio pal(video_clock_pal_hz);
    // Um segundo de CPU = exatamente o número de ciclos do cristal de vídeo.
    CHECK_EQ(ntsc.from_cpu(cpu_clock_hz), video_clock_ntsc_hz);
    CHECK_EQ(pal.from_cpu(cpu_clock_hz), video_clock_pal_hz);
    // Uma hora de emulação sem estouro nem deriva.
    CHECK_EQ(ntsc.from_cpu(cpu_clock_hz * 3600), video_clock_ntsc_hz * 3600);
    // to_cpu_ceil é o menor ciclo da CPU que alcança o ciclo de vídeo pedido.
    for (u64 v : {u64{1}, u64{2}, u64{3413}, u64{897619}, u64{123456789}, u64{53693175}}) {
        const u64 t = ntsc.to_cpu_ceil(v);
        CHECK(ntsc.from_cpu(t) >= v);
        CHECK(t == 0 || ntsc.from_cpu(t - 1) < v);
    }
    CHECK_EQ(cpu_clock_hz, u64{44100} * cpu_cycles_per_audio_sample);
}
