// Controlador de DMA: 7 canais em 1F801080h–1F8010EFh, DPCR (F0h), DICR (F4h). (Fase 6)
//
// Fontes: psx-spx "DMA Channels" e medições em SCPH-5501 do psx.wiki ("dma2", "dma-otc",
// "dmatime", "dma-chain-looping", "dma-chopping").
//   Canal n: MADR (+0, bits 0–23), BCR (+4), CHCR (+8, espelho em +Ch).
//   Início: CHCR bit 24 + habilitação do canal no DPCR + (DREQ do dispositivo ou CHCR bit 28).
//   Modos: 0 = burst (BCR = palavras, 0 → 10000h), 1 = slice (BS × BA, um bloco por DREQ),
//          2 = lista encadeada (cabeçalho: bits 24–31 = palavras, 0–23 = próximo; FFFFFFh = fim).
//   OTC (canal 6): escreve a tabela de ordenação para baixo; última entrada = 00FFFFFFh.
//   DICR: flag do canal ligado ao fim se a habilitação do canal (bit 16+n) estiver ligada —
//   medido: independe do bit 23; bit 31 = bit 15 OR (bit 23 AND algum flag); IRQ3 na borda 0→1.
//
// Modelo de tempo (D-036): dados de burst/slice/OTC são movidos no início e o canal fica ocupado
// pela duração estimada; listas avançam nó a nó no tempo emulado; a CPU não é paralisada.
#pragma once

#include <array>
#include <string_view>

#include "bus/bus.hpp"
#include "bus/io_device.hpp"
#include "core/types.hpp"
#include "dma/dma_port.hpp"
#include "interrupts/interrupt_controller.hpp"
#include "system/scheduler.hpp"

namespace rpcs1::dma {

namespace chcr {
inline constexpr u32 to_device = 1u << 0;
inline constexpr u32 step_back = 1u << 1;
inline constexpr u32 chopping = 1u << 8;
inline constexpr u32 sync_shift = 9;
inline constexpr u32 busy = 1u << 24;
inline constexpr u32 trigger = 1u << 28;
inline constexpr u32 write_mask = 0x71770703u;      // canais 0–5 (medido)
inline constexpr u32 otc_write_mask = 0x51000000u;  // canal 6: bit 1 sempre 1
} // namespace chcr

class DmaController final : public bus::IoDevice {
public:
    static constexpr u32 dpcr_reset = 0x07654321u;
    static constexpr u32 list_end = 0x00FFFFFFu;

    DmaController(bus::Bus& bus, system::Scheduler& scheduler, interrupts::InterruptController& irq);

    void reset();
    void attach_port(Channel channel, DmaPort& port) noexcept;
    // Um dispositivo avisa que seu DREQ mudou (canais aguardando podem começar/continuar).
    void request_changed(Channel channel);

    void on_event(u64 time);

    // bus::IoDevice
    [[nodiscard]] std::string_view name() const noexcept override { return "DMA"; }
    [[nodiscard]] u32 io_read(u32 offset, bus::AccessWidth width) override;
    void io_write(u32 offset, u32 value, bus::AccessWidth width) override;

    // Inspeção (testes/depuração).
    [[nodiscard]] u32 madr(u32 n) const noexcept { return channels_[n].madr; }
    [[nodiscard]] u32 bcr(u32 n) const noexcept { return channels_[n].bcr; }
    [[nodiscard]] u32 chcr_value(u32 n) const noexcept { return channels_[n].chcr; }
    [[nodiscard]] u32 dpcr() const noexcept { return dpcr_; }
    [[nodiscard]] u32 dicr() const noexcept { return dicr_; }
    [[nodiscard]] bool active(u32 n) const noexcept { return channels_[n].active; }
    [[nodiscard]] u64 words_transferred(u32 n) const noexcept { return channels_[n].words; }

    // Ciclos por palavra usados para a duração estimada (psx-spx; ver D-036).
    [[nodiscard]] static u32 cycles_per_word(u32 n) noexcept;

private:
    enum class Kind : u8 { None, Timed, List, Slice };

    struct ChannelState {
        u32 madr = 0;
        u32 bcr = 0;
        u32 chcr = 0;
        bool active = false;
        Kind kind = Kind::None;
        u64 start_time = 0;
        u64 end_time = 0;   // Timed: fim previsto
        u64 list_time = 0;  // List: horário em que o próximo nó começa
        u32 slice_remaining = 0;
        u64 slice_total = 0;
        u64 words = 0;
    };

    void try_start(u32 n, u64 now);
    void try_start_all(u64 now);
    void run_burst(u32 n, u64 now);
    void run_slice(u32 n, u64 now, bool forced_first);
    void run_otc(u32 n, u64 now);
    void run_list(u32 n, u64 until);
    void finish(u32 n, u64 time);
    void reschedule();

    [[nodiscard]] bool enabled(u32 n) const noexcept { return (dpcr_ >> (4 * n + 3)) & 1u; }
    [[nodiscard]] u32 priority(u32 n) const noexcept { return (dpcr_ >> (4 * n)) & 7u; }
    [[nodiscard]] bool dreq(u32 n) const noexcept;

    bool ram_read(u32 address, u32& value);
    bool ram_write(u32 address, u32 value);
    void bus_error();
    void write_dicr(u32 value, u32 written_mask);
    void update_master_flag();

    bus::Bus& bus_;
    system::Scheduler& scheduler_;
    interrupts::InterruptController& irq_;
    std::array<DmaPort*, 6> ports_{};
    std::array<PlaceholderPort, 6> placeholders_;
    std::array<ChannelState, channel_count> channels_{};
    u32 dpcr_ = dpcr_reset;
    u32 dicr_ = 0;
};

} // namespace rpcs1::dma
