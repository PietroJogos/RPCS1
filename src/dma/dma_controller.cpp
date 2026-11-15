#include "dma/dma_controller.hpp"

#include <algorithm>
#include <utility>

#include "core/log.hpp"

namespace rpcs1::dma {

namespace {

using log::Category;

struct PortInfo {
    const char* name;
    int phase;       // 0 = porta de expansão (nenhum dispositivo previsto)
    u32 read_value;  // valor lido sem dispositivo
};

// PIO: sem dispositivo na porta, cada palavra lê FFFFFFFFh (medido).
constexpr std::array<PortInfo, 6> port_info = {{
    {"MDECin", 10, 0},
    {"MDECout", 10, 0},
    {"GPU", 7, 0},
    {"CD-ROM", 12, 0},
    {"SPU", 11, 0},
    {"PIO (porta de expansão)", 0, 0xFFFFFFFFu},
}};

template <usize... I>
std::array<PlaceholderPort, sizeof...(I)> make_placeholders(std::index_sequence<I...>) {
    return {PlaceholderPort(port_info[I].name, port_info[I].phase, port_info[I].read_value)...};
}

// Ciclos por palavra (psx-spx, tabela de taxas). ⚠️ Medido: SPU ≈ 8/palavra em burst; o CD-ROM
// depende do registrador CDROM_DELAY (24 com o valor da BIOS). Ver D-036.
constexpr std::array<u32, channel_count> rates = {1, 1, 1, 24, 4, 20, 1};

// Lista encadeada: ≈10 ciclos por nó + 1 por palavra (medido, "dmatime").
constexpr u64 list_node_cycles = 10;
// Tempo emulado máximo processado de uma lista por evento (listas infinitas não travam).
constexpr u64 list_slice_cycles = 2048;

constexpr u32 dicr_rw_mask = 0x00FF803Fu;   // bits 0–5, 15, 16–23 (bit 6 lê 0, medido)
constexpr u32 dicr_flags = 0x7F000000u;     // bits 24–30: escrever 1 limpa
constexpr u32 dicr_master = 0x80000000u;

constexpr u32 count16(u32 value) noexcept {
    const u32 n = value & 0xFFFFu;
    return n == 0 ? 0x10000u : n;
}

} // namespace

std::string_view to_string(Channel channel) noexcept {
    switch (channel) {
    case Channel::MdecIn: return "MDECin";
    case Channel::MdecOut: return "MDECout";
    case Channel::Gpu: return "GPU";
    case Channel::Cdrom: return "CD-ROM";
    case Channel::Spu: return "SPU";
    case Channel::Pio: return "PIO";
    case Channel::Otc: return "OTC";
    }
    return "?";
}

// ------------------------------------------------------------- porta marcadora

PlaceholderPort::PlaceholderPort(std::string name, int phase, u32 read_value)
    : name_(std::move(name)), phase_(phase), read_value_(read_value) {}

u32 PlaceholderPort::dma_read() {
    ++words_;
    report("leitura");
    return read_value_;
}

void PlaceholderPort::dma_write(u32 /*word*/) {
    ++words_;
    report("escrita");
}

void PlaceholderPort::report(const char* kind) {
    bool& flag = kind[0] == 'l' ? reported_read_ : reported_write_;
    if (flag) {
        return;
    }
    flag = true;
    if (phase_ == 0) {
        log::warn(Category::Dma, "DMA ({}) no canal {}: nenhum dispositivo conectado", kind, name_);
    } else {
        log::warn(Category::Dma, "DMA ({}) no canal {}: dispositivo não implementado (Fase {})",
                  kind, name_, phase_);
    }
}

// ----------------------------------------------------------------- controlador

DmaController::DmaController(bus::Bus& bus, system::Scheduler& scheduler,
                             interrupts::InterruptController& irq)
    : bus_(bus), scheduler_(scheduler), irq_(irq),
      placeholders_(make_placeholders(std::make_index_sequence<6>{})) {
    for (usize i = 0; i < ports_.size(); ++i) {
        ports_[i] = &placeholders_[i];
    }
    reset();
}

void DmaController::reset() {
    channels_ = {};
    channels_[6].chcr = chcr::step_back; // bit 1 do OTC sempre 1
    dpcr_ = dpcr_reset;
    dicr_ = 0;
    scheduler_.cancel(system::EventId::Dma);
}

void DmaController::attach_port(Channel channel, DmaPort& port) noexcept {
    const auto n = static_cast<u32>(channel);
    if (n < ports_.size()) {
        ports_[n] = &port;
    }
}

u32 DmaController::cycles_per_word(u32 n) noexcept {
    return n < channel_count ? rates[n] : 1;
}

bool DmaController::dreq(u32 n) const noexcept {
    return n < ports_.size() && ports_[n]->dma_request();
}

// --------------------------------------------------------------- acesso à RAM

bool DmaController::ram_read(u32 address, u32& value) {
    const bus::ReadResult r = bus_.read32(address & 0x00FFFFFCu);
    if (!r.ok()) {
        bus_error();
        value = 0;
        return false;
    }
    value = r.value;
    return true;
}

bool DmaController::ram_write(u32 address, u32 value) {
    // Fora da janela de RAM (ex.: OTC que passa de 000000h para FFFFFCh) nada é gravado.
    if (bus_.write32(address & 0x00FFFFFCu, value) != bus::BusFault::None) {
        bus_error();
        return false;
    }
    return true;
}

void DmaController::bus_error() {
    // psx-spx: acesso fora da RAM liga o bit 15 (força o bit 31 e o IRQ3).
    dicr_ |= 0x00008000u;
    update_master_flag();
}

// ------------------------------------------------------------------ DICR/IRQ3

void DmaController::update_master_flag() {
    const bool before = (dicr_ & dicr_master) != 0;
    const bool after = (dicr_ & 0x00008000u) != 0 ||
                       ((dicr_ & 0x00800000u) != 0 && (dicr_ & dicr_flags) != 0);
    dicr_ = (dicr_ & ~dicr_master) | (after ? dicr_master : 0u);
    if (!before && after) {
        irq_.request(interrupts::Irq::Dma);
    }
}

void DmaController::write_dicr(u32 value, u32 written_mask) {
    // Só os bytes efetivamente escritos participam (flags de outros bytes não são limpos).
    const u32 rw = dicr_rw_mask & written_mask;
    dicr_ = (dicr_ & ~rw) | (value & rw);
    dicr_ &= ~(value & dicr_flags & written_mask); // 1 limpa o flag; 0 mantém
    update_master_flag();
}

// ------------------------------------------------------------- início/término

void DmaController::try_start_all(u64 now) {
    // Ordem de prioridade do DPCR (0 = maior); empate: canal de número maior primeiro.
    std::array<u32, channel_count> order{};
    for (u32 i = 0; i < channel_count; ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [this](u32 a, u32 b) {
        if (priority(a) != priority(b)) return priority(a) < priority(b);
        return a > b;
    });
    for (u32 n : order) {
        try_start(n, now);
    }
}

void DmaController::try_start(u32 n, u64 now) {
    ChannelState& c = channels_[n];
    if (c.active || (c.chcr & chcr::busy) == 0 || !enabled(n)) {
        return;
    }
    const bool forced = (c.chcr & chcr::trigger) != 0;
    if (!forced && !dreq(n)) {
        return; // aguarda o DREQ do dispositivo
    }
    c.chcr &= ~chcr::trigger; // limpo ao começar (medido)
    c.active = true;
    c.start_time = now;
    if (n == 6) {
        run_otc(n, now);
        return;
    }
    switch ((c.chcr >> chcr::sync_shift) & 3u) {
    case 1:
        c.slice_remaining = (c.bcr >> 16) == 0 ? 0x10000u : (c.bcr >> 16);
        c.slice_total = 0;
        run_slice(n, now, forced);
        break;
    case 2:
        c.kind = Kind::List;
        c.list_time = now;
        run_list(n, now);
        break;
    default: // 0 = burst; ⚠️ 3 (reservado) tratado como burst
        run_burst(n, now);
        break;
    }
}

void DmaController::finish(u32 n, u64 time) {
    (void)time;
    ChannelState& c = channels_[n];
    c.chcr &= ~chcr::busy;
    c.active = false;
    c.kind = Kind::None;
    // Medido no SCPH-5501: o flag liga com a habilitação do canal, mesmo sem o bit 23.
    if (dicr_ & (1u << (16 + n))) {
        dicr_ |= 1u << (24 + n);
    }
    update_master_flag();
    log::debug(Category::Dma, "Canal {} concluído ({} palavras no total)",
               to_string(static_cast<Channel>(n)), c.words);
}

// ---------------------------------------------------------------- transferências

void DmaController::run_otc(u32 n, u64 now) {
    ChannelState& c = channels_[n];
    // Direção, passo e modo são ignorados; a tabela é escrita para baixo e cada entrada aponta
    // para a anterior; a última recebe o marcador de fim. MADR e BCR não mudam (medido).
    const u32 count = count16(c.bcr);
    u32 address = c.madr & 0x00FFFFFCu;
    for (u32 i = 0; i < count; ++i) {
        const u32 value = (i + 1 == count) ? list_end : ((address - 4) & 0x00FFFFFFu);
        ram_write(address, value);
        address = (address - 4) & 0x00FFFFFCu;
    }
    c.words += count;
    c.kind = Kind::Timed;
    c.end_time = now + static_cast<u64>(count) * rates[n];
}

void DmaController::run_burst(u32 n, u64 now) {
    ChannelState& c = channels_[n];
    const u32 count = count16(c.bcr); // a metade alta do BCR é ignorada no modo 0
    const bool to_device = (c.chcr & chcr::to_device) != 0;
    const u32 step = (c.chcr & chcr::step_back) ? 0xFFFFFFFCu : 4u;
    u32 address = c.madr & 0x00FFFFFCu;
    for (u32 i = 0; i < count; ++i) {
        if (to_device) {
            u32 word = 0;
            ram_read(address, word);
            ports_[n]->dma_write(word);
        } else {
            ram_write(address, ports_[n]->dma_read());
        }
        address = (address + step) & 0x00FFFFFCu;
    }
    c.words += count;
    // Sem chopping MADR e BCR ficam como estavam; com chopping avançam até o fim (medido).
    if (c.chcr & chcr::chopping) {
        c.madr = address;
        c.bcr &= 0xFFFF0000u;
    }
    c.kind = Kind::Timed;
    c.end_time = now + static_cast<u64>(count) * rates[n];
}

void DmaController::run_slice(u32 n, u64 now, bool forced_first) {
    ChannelState& c = channels_[n];
    const u32 block = count16(c.bcr);
    const bool to_device = (c.chcr & chcr::to_device) != 0;
    const u32 step = (c.chcr & chcr::step_back) ? 0xFFFFFFFCu : 4u;
    bool forced = forced_first;
    while (c.slice_remaining > 0) {
        if (!forced && !dreq(n)) {
            c.kind = Kind::Slice; // aguarda o próximo DREQ
            return;
        }
        forced = false;
        u32 address = c.madr & 0x00FFFFFCu;
        for (u32 i = 0; i < block; ++i) {
            if (to_device) {
                u32 word = 0;
                ram_read(address, word);
                ports_[n]->dma_write(word);
            } else {
                ram_write(address, ports_[n]->dma_read());
            }
            address = (address + step) & 0x00FFFFFCu;
        }
        // MADR aponta para o próximo bloco; BA conta para baixo; BS permanece (medido).
        c.madr = address;
        --c.slice_remaining;
        c.bcr = (c.bcr & 0xFFFFu) | ((c.slice_remaining & 0xFFFFu) << 16);
        c.words += block;
        c.slice_total += block;
    }
    c.kind = Kind::Timed;
    c.end_time = std::max(now + static_cast<u64>(block) * rates[n],
                          c.start_time + c.slice_total * rates[n]);
}

void DmaController::run_list(u32 n, u64 until) {
    ChannelState& c = channels_[n];
    u64 t = c.list_time;
    const u64 limit = until + list_slice_cycles;
    while (t <= limit) {
        const u32 node = c.madr & 0x00FFFFFFu;
        if (node & 0x00800000u) {
            // FFFFFFh: fim normal. Outros endereços com bit 23 (medidos: FFFFFEh, 800000h):
            // erro de barramento. ⚠️ Generalizado para qualquer endereço com o bit 23 ligado.
            if (node != list_end) {
                bus_error();
            }
            c.kind = Kind::Timed;
            c.end_time = t;
            return;
        }
        u32 header = 0;
        if (!ram_read(node, header)) {
            c.kind = Kind::Timed;
            c.end_time = t;
            return;
        }
        const u32 words = header >> 24;
        // ⚠️ O cabeçalho não é enviado ao dispositivo (só as palavras de dados).
        for (u32 k = 1; k <= words; ++k) {
            u32 word = 0;
            ram_read(node + 4 * k, word);
            ports_[n]->dma_write(word);
        }
        c.words += words;
        t += list_node_cycles + static_cast<u64>(words) * rates[n];
        c.madr = header & 0x00FFFFFFu;
    }
    c.list_time = t; // continua no próximo evento (a CPU roda entre os nós)
}

void DmaController::request_changed(Channel channel) {
    const auto n = static_cast<u32>(channel);
    if (n >= channel_count) {
        return;
    }
    const u64 now = scheduler_.now();
    ChannelState& c = channels_[n];
    if (!c.active) {
        try_start(n, now);
    } else if (c.kind == Kind::Slice) {
        run_slice(n, now, false);
    }
    reschedule();
}

void DmaController::on_event(u64 time) {
    for (u32 n = 0; n < channel_count; ++n) {
        ChannelState& c = channels_[n];
        if (!c.active) {
            continue;
        }
        if (c.kind == Kind::List && c.list_time <= time) {
            run_list(n, time);
        }
        if (c.kind == Kind::Timed && c.end_time <= time) {
            finish(n, c.end_time);
        }
    }
    reschedule();
}

void DmaController::reschedule() {
    u64 next = system::Scheduler::never;
    for (const ChannelState& c : channels_) {
        if (!c.active) continue;
        if (c.kind == Kind::Timed) next = std::min(next, c.end_time);
        if (c.kind == Kind::List) next = std::min(next, c.list_time);
    }
    if (next == system::Scheduler::never) {
        scheduler_.cancel(system::EventId::Dma);
    } else {
        scheduler_.schedule(system::EventId::Dma, next);
    }
}

// ------------------------------------------------------------------- registradores

u32 DmaController::io_read(u32 offset, bus::AccessWidth width) {
    const u32 ch = offset / 0x10;
    const u32 reg = (offset % 0x10) / 4;
    const u32 lane = offset & 3u;
    u32 value = 0;
    if (ch < channel_count) {
        const ChannelState& c = channels_[ch];
        value = reg == 0 ? c.madr : reg == 1 ? c.bcr : c.chcr; // +Ch espelha o CHCR
    } else {
        switch (reg) {
        case 0: value = dpcr_; break;
        case 1: value = dicr_; break;
        case 2: value = 0; break;           // ⚠️ 1F8010F8h: varia no hardware; lido como 0
        default: value = 0x00FFFFFFu; break; // 1F8010FCh: medido 00FFFFFFh, ignora escritas
        }
    }
    return (value >> (lane * 8)) & bus::width_mask(width);
}

void DmaController::io_write(u32 offset, u32 value, bus::AccessWidth width) {
    const u32 ch = offset / 0x10;
    const u32 reg = (offset % 0x10) / 4;
    const u32 lane = offset & 3u;
    const u64 now = scheduler_.now();

    // Medido: SB/SH em BCR e CHCR alteram só os próprios bytes. ⚠️ Aplicado a todos os
    // registradores do DMA (o psx-spx menciona SH tratado como escrita de 32 bits; D-037).
    const u32 lane_mask = bus::width_mask(width) << (lane * 8);
    const u32 shifted = width == bus::AccessWidth::Word ? value : (value << (lane * 8));
    const auto merge = [&](u32 old) { return (old & ~lane_mask) | (shifted & lane_mask); };

    if (ch < channel_count) {
        ChannelState& c = channels_[ch];
        switch (reg) {
        case 0: c.madr = merge(c.madr) & 0x00FFFFFFu; break;
        case 1: c.bcr = merge(c.bcr); break;
        default: {
            const u32 v = merge(c.chcr);
            c.chcr = ch == 6 ? (v & chcr::otc_write_mask) | chcr::step_back : (v & chcr::write_mask);
            if (c.active && (c.chcr & chcr::busy) == 0) {
                // Bit 24 zerado durante a transferência: ela para (o BCR guarda o restante).
                c.active = false;
                c.kind = Kind::None;
            }
            try_start(ch, now);
            break;
        }
        }
    } else {
        switch (reg) {
        case 0:
            dpcr_ = merge(dpcr_);
            try_start_all(now); // habilitar um canal já armado o inicia (medido)
            break;
        case 1: write_dicr(shifted, lane_mask); break;
        default: break;
        }
    }
    reschedule();
}

} // namespace rpcs1::dma
