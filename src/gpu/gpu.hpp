// GPU: portas GP0/GPUREAD (1F801810h) e GP1/GPUSTAT (1F801814h), VRAM e exibição (Fase 7).
//
// Fontes: psx-spx "GPU" e psx.wiki (hardware/gpu, gp1, gpustat, gp0-transfer, gp0-drawing,
// vram; testes gpustat-dma-bits e gpu-polyline-dma em SCPH-5501).
//
// Implementado nesta fase: decodificação de todos os comandos GP0 com o número correto de
// palavras (o FIFO fica sincronizado), preenchimento (02h), cópias VRAM↔VRAM (80h),
// CPU→VRAM (A0h) e VRAM→CPU (C0h) com as regras de máscara de GP0(E6h), atributos E1h–E6h,
// IRQ1 (1Fh), todos os comandos GP1, GPUSTAT, porta do canal 2 de DMA, parâmetros de
// exibição (que alimentam a temporização de vídeo) e a imagem da área de exibição.
//
// NÃO implementado (Fase 8): desenho de polígonos, linhas e retângulos (20h–7Fh). Esses
// comandos são consumidos e descartados com aviso no log. A execução é instantânea: não há
// tempo de execução de comandos nem limite real do FIFO (⚠️ D-040).
// Versão emulada: GPU v2 (⚠️ adotada para o SCPH-5501, D-041).
#pragma once

#include <array>
#include <bitset>
#include <string_view>

#include "bus/io_device.hpp"
#include "core/types.hpp"
#include "dma/dma_port.hpp"
#include "gpu/video_timing.hpp"
#include "gpu/vram.hpp"
#include "interrupts/interrupt_controller.hpp"
#include "video/framebuffer.hpp"

namespace rpcs1::dma {
class DmaController;
}

namespace rpcs1::gpu {

struct DisplayInfo {
    u32 start_x = 0;
    u32 start_y = 0;
    u32 width = 0;   // pixels visíveis por linha
    u32 height = 0;  // linhas visíveis (×2 no modo 480 entrelaçado)
    bool depth24 = false;
    bool enabled = false;
};

class Gpu final : public bus::IoDevice, public dma::DmaPort {
public:
    static constexpr u32 gpustat_after_reset = 0x14802000u;

    Gpu(VideoTiming& video, interrupts::InterruptController& irq);

    // Ligar: VRAM zerada (⚠️ conteúdo real indefinido), GP1(09h) = 0 e GP1(00h).
    // GP1(00h) não altera GP1(09h).
    void power_on();
    [[nodiscard]] bool vram_2mb_enabled() const noexcept { return vram_2mb_; }
    // GP1(00h). Não altera a VRAM nem GP1(09h).
    void reset_command();

    void set_dma(dma::DmaController* dma) noexcept { dma_ = dma; }

    void gp0_write(u32 word);
    void gp1_write(u32 word);
    [[nodiscard]] u32 gpuread();
    [[nodiscard]] u32 gpustat() const noexcept;

    // bus::IoDevice: +0 = GP0 (escrita) / GPUREAD (leitura); +4 = GP1 / GPUSTAT.
    [[nodiscard]] std::string_view name() const noexcept override { return "GPU"; }
    [[nodiscard]] u32 io_read(u32 offset, bus::AccessWidth width) override;
    void io_write(u32 offset, u32 value, bus::AccessWidth width) override;

    // dma::DmaPort (canal 2). DREQ conforme GP1(04h).
    [[nodiscard]] bool dma_request() const noexcept override;
    u32 dma_read() override { return gpuread(); }
    void dma_write(u32 word) override { gp0_write(word); }

    [[nodiscard]] DisplayInfo display_info() const noexcept;
    // Converte a área de exibição da VRAM em imagem RGB (preta com a exibição desligada).
    void render_display(video::Framebuffer& out) const;

    [[nodiscard]] Vram& vram() noexcept { return vram_; }
    [[nodiscard]] const Vram& vram() const noexcept { return vram_; }
    [[nodiscard]] u64 discarded_draw_commands() const noexcept { return discarded_draws_; }

    // Número de palavras (comando + parâmetros) de um comando GP0; 0 = polilinha (variável).
    [[nodiscard]] static u32 command_words(u32 command) noexcept;

private:
    struct Rect {
        u32 x = 0, y = 0, w = 0, h = 0;
        u32 cx = 0, cy = 0; // posição atual dentro do retângulo
        u64 remaining = 0;  // halfwords restantes
    };

    void execute_command();
    void execute_fill();
    void execute_copy();
    void start_upload();
    void start_download();
    void upload_halfword(u16 value);
    void set_attribute(u32 word);
    void discard_draw(u32 opcode);
    void write_masked(u32 x, u32 y, u16 value);
    void feed_polyline(u32 word);
    void update_video_timing();
    void notify_dma();
    [[nodiscard]] bool vertical_480() const noexcept;

    VideoTiming& video_;
    interrupts::InterruptController& irq_;
    dma::DmaController* dma_ = nullptr;
    Vram vram_;

    // Montagem de comandos GP0.
    std::array<u32, 16> fifo_{};
    u32 fifo_count_ = 0;
    u32 expected_ = 0;
    bool polyline_ = false;
    bool polyline_gouraud_ = false;
    u32 polyline_words_ = 0;

    bool upload_active_ = false;
    bool download_active_ = false;
    Rect upload_;
    Rect download_;
    u32 gpuread_latch_ = 0;

    // Atributos de desenho (GP0 E1h–E6h).
    u32 texpage_ = 0;        // E1h bits 0–13
    u32 texture_window_ = 0; // E2h bits 0–19
    u32 area_top_left_ = 0;  // E3h bits 0–19
    u32 area_bottom_right_ = 0;
    u32 draw_offset_ = 0;    // E5h bits 0–21
    bool mask_set_ = false;   // E6h bit 0
    bool mask_check_ = false; // E6h bit 1

    // Exibição (GP1).
    bool display_disabled_ = true;
    u32 dma_direction_ = 0;
    u32 display_x_ = 0;
    u32 display_y_ = 0;
    u32 range_x1_ = 0x200;
    u32 range_x2_ = 0x200 + 256 * 10;
    u32 range_y1_ = 0x010;
    u32 range_y2_ = 0x010 + 240;
    u32 display_mode_ = 0; // GP1(08h) bits 0–6
    bool vram_2mb_ = false; // GP1(09h) bit 0 (v2): endereços Y 512–1023; sem efeito com 1 MiB
    bool irq_flag_ = false;               // GPUSTAT bit 24 (GP0(1Fh) liga, GP1(02h) limpa)

    u64 discarded_draws_ = 0;
    std::bitset<256> reported_opcodes_;
};

} // namespace rpcs1::gpu
