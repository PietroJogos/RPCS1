#include "gpu/gpu.hpp"

#include <algorithm>

#include "core/log.hpp"
#include "dma/dma_controller.hpp"

namespace rpcs1::gpu {

namespace {

using log::Category;

// Fim de uma polilinha: primeira palavra de um vértice com (palavra AND F000F000h) = 50005000h
// (psx-spx; o valor usual é 55555555h).
constexpr bool is_polyline_terminator(u32 word) noexcept {
    return (word & 0xF000F000u) == 0x50005000u;
}

// Divisor do dotclock por GP1(08h): bit 6 → 368 pontos (7); senão bits 0–1 → 256/320/512/640.
constexpr u32 dot_divider_for(u32 mode) noexcept {
    if (mode & 0x40u) {
        return 7;
    }
    constexpr u32 table[4] = {10, 8, 5, 4};
    return table[mode & 3u];
}

// Tamanho de cópia/transferência: ((valor − 1) AND máscara) + 1 → 0 vira o máximo.
constexpr u32 transfer_size(u32 value, u32 mask) noexcept {
    return ((value - 1u) & mask) + 1u;
}

} // namespace

Gpu::Gpu(VideoTiming& video, interrupts::InterruptController& irq) : video_(video), irq_(irq) {
    power_on();
}

void Gpu::power_on() {
    vram_.clear();
    vram_2mb_ = false;
    gpuread_latch_ = 0;
    discarded_draws_ = 0;
    reported_opcodes_.reset();
    reset_command();
}

void Gpu::reset_command() {
    // GP1(00h) = GP1(01h) + GP1(02h) + GP1(03h, 1) + GP1(04h, 0) + GP1(05h, 0) +
    //            GP1(06h, C00200h) + GP1(07h, 040010h) + GP1(08h, 0) + GP0(E1h..E6h, 0).
    fifo_count_ = 0;
    expected_ = 0;
    polyline_ = false;
    upload_active_ = false;
    download_active_ = false; // ⚠️ não documentado; a transferência pendente é abandonada
    irq_flag_ = false;
    display_disabled_ = true;
    dma_direction_ = 0;
    display_x_ = 0;
    display_y_ = 0;
    range_x1_ = 0x200;
    range_x2_ = 0xC00;
    range_y1_ = 0x010;
    range_y2_ = 0x100;
    display_mode_ = 0;
    texpage_ = 0;
    texture_window_ = 0;
    area_top_left_ = 0;
    area_bottom_right_ = 0;
    draw_offset_ = 0;
    mask_set_ = false;
    mask_check_ = false;
    update_video_timing();
}

// ------------------------------------------------------------- tamanho dos comandos

u32 Gpu::command_words(u32 command) noexcept {
    const u32 op = command >> 24;
    switch (op >> 5) {
    case 0: return op == 0x02 ? 3 : 1; // 02h preenchimento; demais: NOP, 01h, 1Fh
    case 1: {                          // polígonos 20h–3Fh
        const bool gouraud = (op & 0x10u) != 0;
        const u32 vertices = (op & 0x08u) ? 4 : 3;
        const u32 per_vertex = (op & 0x04u) ? 2 : 1; // vértice (+ coordenada de textura)
        return 1 + vertices * per_vertex + (gouraud ? vertices - 1 : 0);
    }
    case 2: // linhas 40h–5Fh
        if (op & 0x08u) {
            return 0; // polilinha: termina com o marcador 5000_5000h
        }
        return (op & 0x10u) ? 4 : 3;
    case 3: { // retângulos 60h–7Fh
        const u32 size = (op >> 3) & 3u; // 0 = variável, 1 = 1×1, 2 = 8×8, 3 = 16×16
        return 2 + ((op & 0x04u) ? 1 : 0) + (size == 0 ? 1 : 0);
    }
    case 4: return 4; // 80h cópia VRAM→VRAM
    case 5: return 3; // A0h CPU→VRAM (+ dados)
    case 6: return 3; // C0h VRAM→CPU
    default: return 1; // E1h–E6h atributos; demais: NOP
    }
}

// --------------------------------------------------------------------------- GP0

void Gpu::gp0_write(u32 word) {
    if (upload_active_) {
        upload_halfword(static_cast<u16>(word));
        if (upload_active_) {
            upload_halfword(static_cast<u16>(word >> 16));
        }
        return; // com largura ímpar a metade alta da última palavra é descartada
    }
    if (polyline_) {
        feed_polyline(word);
        return;
    }
    if (fifo_count_ == 0) {
        expected_ = command_words(word);
        if (expected_ == 0) {
            polyline_ = true;
            polyline_gouraud_ = ((word >> 24) & 0x10u) != 0;
            polyline_words_ = 1;
            discard_draw(word >> 24);
            return;
        }
    }
    fifo_[fifo_count_++] = word;
    if (fifo_count_ >= expected_) {
        execute_command();
        fifo_count_ = 0;
        expected_ = 0;
    }
}

void Gpu::feed_polyline(u32 word) {
    // Palavras após o comando: plana = v0, v1, v2...; gouraud = v0, c1, v1, c2, v2...
    // O marcador só é reconhecido no início de um vértice, depois dos dois primeiros.
    const u32 index = polyline_words_;
    const bool vertex_start = polyline_gouraud_ ? (index >= 4 && (index & 1u) == 0) : index >= 3;
    ++polyline_words_;
    if (vertex_start && is_polyline_terminator(word)) {
        polyline_ = false;
        polyline_words_ = 0;
    }
}

void Gpu::execute_command() {
    const u32 op = fifo_[0] >> 24;
    switch (op >> 5) {
    case 0:
        if (op == 0x02) {
            execute_fill();
        } else if (op == 0x1F) {
            if (!irq_flag_) {
                irq_flag_ = true;
                irq_.request(interrupts::Irq::Gpu);
            }
        }
        // 00h, 01h (limpar cache de textura: sem cache emulado ainda) e 03h–1Eh: sem efeito.
        break;
    case 1:
    case 2:
    case 3: discard_draw(op); break;
    case 4: execute_copy(); break;
    case 5: start_upload(); break;
    case 6: start_download(); break;
    default: set_attribute(fifo_[0]); break;
    }
}

void Gpu::discard_draw(u32 opcode) {
    ++discarded_draws_;
    if (!reported_opcodes_.test(opcode & 0xFFu)) {
        reported_opcodes_.set(opcode & 0xFFu);
        log::warn(Category::Gpu,
                  "GP0({:02X}h): comando de desenho ainda não implementado (Fase 8); descartado",
                  opcode & 0xFFu);
    }
}

void Gpu::write_masked(u32 x, u32 y, u16 value) {
    // GP0(E6h): bit 1 = não sobrescrever pixels com bit 15; bit 0 = gravar com bit 15 ligado.
    if (mask_check_ && (vram_.get(x, y) & 0x8000u)) {
        return;
    }
    vram_.set(x, y, static_cast<u16>(value | (mask_set_ ? 0x8000u : 0u)));
}

void Gpu::execute_fill() {
    // GP0(02h): X em passos de 16, largura arredondada para cima em múltiplos de 16
    // (3F1h..3FFh → 400h; 400h → 0); ignora máscara, área e deslocamento de desenho.
    const u16 color = command_color_to_rgb555(fifo_[0] & 0x00FFFFFFu);
    const u32 x0 = fifo_[1] & 0x3F0u;
    const u32 y0 = (fifo_[1] >> 16) & 0x1FFu;
    const u32 w = ((fifo_[2] & 0x3FFu) + 0x0Fu) & ~0x0Fu;
    const u32 h = (fifo_[2] >> 16) & 0x1FFu;
    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            vram_.set(x0 + x, y0 + y, color);
        }
    }
}

void Gpu::execute_copy() {
    const u32 sx = fifo_[1] & 0x3FFu;
    const u32 sy = (fifo_[1] >> 16) & 0x1FFu;
    const u32 dx = fifo_[2] & 0x3FFu;
    const u32 dy = (fifo_[2] >> 16) & 0x1FFu;
    const u32 w = transfer_size(fifo_[3] & 0xFFFFu, 0x3FFu);
    const u32 h = transfer_size(fifo_[3] >> 16, 0x1FFu);
    // ⚠️ Cópia pixel a pixel, linha por linha; o comportamento real com origem e destino
    // sobrepostos (cópia em blocos) não é reproduzido (D-042).
    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            write_masked(dx + x, dy + y, vram_.get(sx + x, sy + y));
        }
    }
}

void Gpu::start_upload() {
    upload_ = {};
    upload_.x = fifo_[1] & 0x3FFu;
    upload_.y = (fifo_[1] >> 16) & 0x1FFu;
    upload_.w = transfer_size(fifo_[2] & 0xFFFFu, 0x3FFu);
    upload_.h = transfer_size(fifo_[2] >> 16, 0x1FFu);
    upload_.remaining = static_cast<u64>(upload_.w) * upload_.h;
    upload_active_ = true;
}

void Gpu::upload_halfword(u16 value) {
    write_masked(upload_.x + upload_.cx, upload_.y + upload_.cy, value);
    if (++upload_.cx == upload_.w) {
        upload_.cx = 0;
        ++upload_.cy;
    }
    if (--upload_.remaining == 0) {
        upload_active_ = false;
    }
}

void Gpu::start_download() {
    download_ = {};
    download_.x = fifo_[1] & 0x3FFu;
    download_.y = (fifo_[1] >> 16) & 0x1FFu;
    download_.w = transfer_size(fifo_[2] & 0xFFFFu, 0x3FFu);
    download_.h = transfer_size(fifo_[2] >> 16, 0x1FFu);
    download_.remaining = static_cast<u64>(download_.w) * download_.h;
    download_active_ = true;
}

u32 Gpu::gpuread() {
    if (!download_active_) {
        return gpuread_latch_; // último valor (transferência ou GP1(10h))
    }
    u32 value = 0;
    for (u32 half = 0; half < 2 && download_.remaining > 0; ++half) {
        const u16 pixel = vram_.get(download_.x + download_.cx, download_.y + download_.cy);
        value |= static_cast<u32>(pixel) << (16 * half);
        if (++download_.cx == download_.w) {
            download_.cx = 0;
            ++download_.cy;
        }
        --download_.remaining;
    }
    // ⚠️ Com total ímpar, a metade alta da última palavra é lida como 0 (D-042).
    if (download_.remaining == 0) {
        download_active_ = false;
    }
    gpuread_latch_ = value;
    return value;
}

void Gpu::set_attribute(u32 word) {
    switch (word >> 24) {
    case 0xE1:
        // Bits 0–10 (GPUSTAT 0–10), 11 = base Y 2 da textura (GPUSTAT 15; só tem efeito com
        // 2 MiB de VRAM; ⚠️ armazenado mesmo com GP1(09h) = 0), 12–13 = espelhamento de
        // retângulos texturizados (v2).
        texpage_ = word & 0x3FFFu;
        break;
    case 0xE2: texture_window_ = word & 0xFFFFFu; break;
    case 0xE3: area_top_left_ = word & 0xFFFFFu; break;
    case 0xE4: area_bottom_right_ = word & 0xFFFFFu; break;
    case 0xE5: draw_offset_ = word & 0x3FFFFFu; break;
    case 0xE6:
        mask_set_ = (word & 1u) != 0;
        mask_check_ = (word & 2u) != 0;
        break;
    default: break; // E0h, E7h–FFh: NOP
    }
}

// --------------------------------------------------------------------------- GP1

void Gpu::gp1_write(u32 word) {
    const u32 command = (word >> 24) & 0x3Fu; // 40h–FFh espelham 00h–3Fh
    const u32 param = word & 0x00FFFFFFu;
    switch (command) {
    case 0x00: reset_command(); break;
    case 0x01:
        fifo_count_ = 0;
        expected_ = 0;
        polyline_ = false;
        upload_active_ = false;
        break;
    case 0x02: irq_flag_ = false; break;
    case 0x03: display_disabled_ = (param & 1u) != 0; break;
    case 0x04: dma_direction_ = param & 3u; break;
    case 0x05:
        display_x_ = param & 0x3FFu;
        display_y_ = (param >> 10) & 0x1FFu;
        break;
    case 0x06:
        range_x1_ = param & 0xFFFu;
        range_x2_ = (param >> 12) & 0xFFFu;
        update_video_timing();
        break;
    case 0x07:
        range_y1_ = param & 0x3FFu;
        range_y2_ = (param >> 10) & 0x3FFu;
        update_video_timing();
        break;
    case 0x08:
        display_mode_ = param & 0x7Fu; // ⚠️ bit 7 (inversão, só v1) ignorado
        update_video_timing();
        break;
    case 0x09: vram_2mb_ = (param & 1u) != 0; break; // VRAM de 1 MiB: Y já dá a volta em 512
    default:
        if (command >= 0x10 && command <= 0x1F) {
            // GPU v2: índice nos bits 0–3; índices sem informação mantêm o GPUREAD.
            switch (param & 0x0Fu) {
            case 0x02: gpuread_latch_ = texture_window_; break;
            case 0x03: gpuread_latch_ = area_top_left_; break;
            case 0x04: gpuread_latch_ = area_bottom_right_; break;
            case 0x05: gpuread_latch_ = draw_offset_; break;
            case 0x07: gpuread_latch_ = 2; break; // versão da GPU
            case 0x08: gpuread_latch_ = 0; break;
            default: break;
            }
        }
        // 0Ah–0Fh e 20h–3Fh: sem efeito na GPU v2 (GP1(20h) é do v1).
        break;
    }
}

void Gpu::update_video_timing() {
    DisplayTiming t;
    t.mode = (display_mode_ & 0x08u) ? VideoMode::Pal : VideoMode::Ntsc;
    t.interlaced = (display_mode_ & 0x20u) != 0;
    t.dot_divider = dot_divider_for(display_mode_);
    t.x1 = range_x1_;
    t.x2 = range_x2_;
    t.y1 = range_y1_;
    t.y2 = range_y2_;
    video_.set_display_timing(t);
}

bool Gpu::vertical_480() const noexcept {
    return (display_mode_ & 0x24u) == 0x24u; // 480 linhas só com entrelaçamento
}

// ----------------------------------------------------------------------- GPUSTAT

u32 Gpu::gpustat() const noexcept {
    const bool interlaced = (display_mode_ & 0x20u) != 0;
    const bool receiving_primitive =
        polyline_ || (fifo_count_ > 0 && ((fifo_[0] >> 29) == 1 || (fifo_[0] >> 29) == 2));
    const bool ready_cmd = fifo_count_ == 0 && !polyline_ && !upload_active_;
    const bool ready_send = download_active_;
    const bool ready_dma = !receiving_primitive;

    u32 s = texpage_ & 0x7FFu;                     // 0–10: página de textura (E1h)
    s |= (mask_set_ ? 1u : 0u) << 11;
    s |= (mask_check_ ? 1u : 0u) << 12;
    s |= (interlaced ? (video_.odd_field() ? 1u : 0u) : 1u) << 13; // ⚠️ campo; 1 sem entrelaçar
    s |= ((texpage_ >> 11) & 1u) << 15;            // base Y 2 da textura (E1h bit 11)
    s |= ((display_mode_ >> 6) & 1u) << 16;        // 368 pontos
    s |= (display_mode_ & 3u) << 17;               // resolução horizontal
    s |= (vertical_480() ? 1u : 0u) << 19;
    s |= ((display_mode_ >> 3) & 1u) << 20;        // PAL
    s |= ((display_mode_ >> 4) & 1u) << 21;        // 24 bits
    s |= (interlaced ? 1u : 0u) << 22;
    s |= (display_disabled_ ? 1u : 0u) << 23;
    s |= (irq_flag_ ? 1u : 0u) << 24;
    bool dreq = false;
    switch (dma_direction_) {
    case 1: dreq = true; break; // ⚠️ "FIFO não cheio": o FIFO não enche (execução instantânea)
    case 2: dreq = ready_dma; break;
    case 3: dreq = ready_send; break;
    default: break;
    }
    s |= (dreq ? 1u : 0u) << 25;
    s |= (ready_cmd ? 1u : 0u) << 26;
    s |= (ready_send ? 1u : 0u) << 27;
    s |= (ready_dma ? 1u : 0u) << 28;
    s |= dma_direction_ << 29;
    // Bit 31: linha par/ímpar sendo exibida; 0 durante o VBLANK. ⚠️ No modo 480i reflete o campo.
    bool odd_line = false;
    if (!video_.in_vblank()) {
        odd_line = vertical_480() ? video_.odd_field() : (video_.vertical_line() & 1u) != 0;
    }
    s |= (odd_line ? 1u : 0u) << 31;
    return s;
}

// ------------------------------------------------------------------- E/S e DMA

u32 Gpu::io_read(u32 offset, bus::AccessWidth width) {
    const u32 value = (offset & ~3u) == 0 ? gpuread() : gpustat();
    return (value >> ((offset & 3u) * 8)) & bus::width_mask(width);
}

void Gpu::io_write(u32 offset, u32 value, bus::AccessWidth /*width*/) {
    // ⚠️ SB/SH: o registrador inteiro da CPU é tratado como a palavra escrita (D-042).
    const bool before = dma_request();
    if ((offset & ~3u) == 0) {
        gp0_write(value);
    } else {
        gp1_write(value);
    }
    if (dma_request() != before) {
        notify_dma();
    }
}

bool Gpu::dma_request() const noexcept {
    return ((gpustat() >> 25) & 1u) != 0;
}

void Gpu::notify_dma() {
    if (dma_ != nullptr) {
        dma_->request_changed(dma::Channel::Gpu);
    }
}

// ---------------------------------------------------------------------- exibição

DisplayInfo Gpu::display_info() const noexcept {
    DisplayInfo d;
    d.start_x = display_x_;
    d.start_y = display_y_;
    d.depth24 = (display_mode_ & 0x10u) != 0;
    d.enabled = !display_disabled_;
    // Pontos por linha = (((X2 − X1) / divisor) + 2) AND NOT 3 (psx-spx). ⚠️ X2 limitado ao
    // fim da linha de vídeo; Y2 ao fim do quadro.
    const bool pal = (display_mode_ & 0x08u) != 0;
    const u32 x2 = std::min(range_x2_, pal ? 3406u : 3413u);
    if (x2 > range_x1_) {
        d.width = (((x2 - range_x1_) / dot_divider_for(display_mode_)) + 2) & ~3u;
    }
    const u32 lines = pal ? 314u : 263u;
    const u32 y2 = std::min(range_y2_, lines);
    if (y2 > range_y1_) {
        d.height = (y2 - range_y1_) * (vertical_480() ? 2u : 1u);
    }
    return d;
}

void Gpu::render_display(video::Framebuffer& out) const {
    const DisplayInfo d = display_info();
    if (d.width == 0 || d.height == 0) {
        out.resize(320, 240, 0); // sem área visível: tela preta
        return;
    }
    out.resize(d.width, d.height, 0);
    if (!d.enabled) {
        return; // GP1(03h) = 1: a saída de vídeo fica preta
    }
    for (u32 y = 0; y < d.height; ++y) {
        const u32 vy = d.start_y + y;
        for (u32 x = 0; x < d.width; ++x) {
            u32 rgb = 0;
            if (d.depth24) {
                // 24 bits: fluxo de bytes R, G, B a partir de X×2 (X em halfwords).
                const u32 bx = d.start_x * 2 + x * 3;
                rgb = (static_cast<u32>(vram_.byte_at(bx, vy)) << 16) |
                      (static_cast<u32>(vram_.byte_at(bx + 1, vy)) << 8) |
                      vram_.byte_at(bx + 2, vy);
            } else {
                rgb = rgb555_to_rgb888(vram_.get(d.start_x + x, vy));
            }
            out.set_pixel(x, y, rgb);
        }
    }
}

} // namespace rpcs1::gpu
