#include "cpu/icache.hpp"

namespace rpcs1::cpu {

void InstructionCache::reset() noexcept {
    // ⚠️ O conteúdo da cache ao ligar é indefinido; a BIOS a limpa antes de usá-la.
    lines_ = {};
    hits_ = 0;
    misses_ = 0;
}

bus::ReadResult InstructionCache::fetch(bus::Bus& bus, u32 vaddr, u32 bcc_value) {
    Line& entry = lines_[index_of(vaddr)];
    const u32 word = word_of(vaddr);
    const u32 tag = tag_of(vaddr);

    if (entry.tag == tag && ((entry.valid >> word) & 1u) != 0) {
        ++hits_;
        return bus::ReadResult{entry.words[word]};
    }
    ++misses_;

    const bus::ReadResult first = bus.fetch_instruction(vaddr);
    if (!first.ok()) {
        return first; // nada é colocado na cache
    }

    // Preenche da palavra acessada até o fim da linha (sem dar a volta).
    // IBLKSZ = 0 limita a falha na palavra 0 a duas palavras; 2–3 são "inválidos" (⚠️ = 4).
    const u32 iblksz = (bcc_value >> bcc::iblksz_shift) & 3u;
    const u32 end = (word == 0 && iblksz == 0) ? 2u : 4u;
    u8 filled = static_cast<u8>(1u << word);
    entry.words[word] = first.value;
    const u32 line_base = vaddr & ~0xFu;
    for (u32 w = word + 1; w < end; ++w) {
        const bus::ReadResult next = bus.fetch_instruction(line_base + w * 4);
        if (!next.ok()) {
            break;
        }
        entry.words[w] = next.value;
        filled = static_cast<u8>(filled | (1u << w));
    }

    // ⚠️ Mesma tag com palavras inválidas: as novas palavras somam-se às já válidas.
    if (entry.tag == tag) {
        entry.valid = static_cast<u8>(entry.valid | filled);
    } else {
        entry.tag = tag;
        entry.valid = filled;
    }
    return first;
}

void InstructionCache::isolated_store(u32 vaddr, u32 value, u32 bcc_value) noexcept {
    // ⚠️ Com IS1 = 0 não há destino documentado: a escrita é descartada.
    if ((bcc_value & bcc::is1) == 0) {
        return;
    }
    Line& entry = lines_[index_of(vaddr)];
    if ((bcc_value & bcc::tag_test) != 0) {
        // A tag vem do endereço da escrita; a validade, dos 4 bits baixos do dado.
        entry.tag = tag_of(vaddr);
        entry.valid = static_cast<u8>(value & 0xFu);
    } else {
        entry.words[word_of(vaddr)] = value;
    }
}

u32 InstructionCache::isolated_load(u32 vaddr, u32 bcc_value) const noexcept {
    if ((bcc_value & bcc::is1) == 0) {
        return 0;
    }
    const Line& entry = lines_[index_of(vaddr)];
    if ((bcc_value & bcc::tag_test) != 0) {
        // Bits 31–5 contêm "restos" de código no hardware (⚠️ lidos como zero aqui).
        return entry.valid | (entry.tag == tag_of(vaddr) ? 0x10u : 0u);
    }
    return entry.words[word_of(vaddr)];
}

} // namespace rpcs1::cpu
