// Lado do dispositivo de um canal de DMA (Fase 6).
#pragma once

#include <string>
#include <string_view>

#include "core/types.hpp"

namespace rpcs1::dma {

enum class Channel : u8 { MdecIn = 0, MdecOut = 1, Gpu = 2, Cdrom = 3, Spu = 4, Pio = 5, Otc = 6 };
inline constexpr u32 channel_count = 7;

[[nodiscard]] std::string_view to_string(Channel channel) noexcept;

class DmaPort {
public:
    virtual ~DmaPort() = default;
    // Pedido de DMA (DREQ) do dispositivo. Sem ele, sync 0/1 só começam com CHCR bit 28.
    [[nodiscard]] virtual bool dma_request() const noexcept = 0;
    virtual u32 dma_read() = 0;            // dispositivo → RAM
    virtual void dma_write(u32 word) = 0;  // RAM → dispositivo
};

// Marcador de um dispositivo ainda não implementado: aceita escritas, lê `read_value` e
// mantém DREQ ativo, avisando no log uma vez (mesma política do UnimplementedDevice, D-015).
class PlaceholderPort final : public DmaPort {
public:
    PlaceholderPort(std::string name, int phase, u32 read_value);

    [[nodiscard]] bool dma_request() const noexcept override { return true; }
    u32 dma_read() override;
    void dma_write(u32 word) override;

    [[nodiscard]] u64 words() const noexcept { return words_; }

private:
    void report(const char* kind);

    std::string name_;
    int phase_;
    u32 read_value_;
    u64 words_ = 0;
    bool reported_read_ = false;
    bool reported_write_ = false;
};

} // namespace rpcs1::dma
