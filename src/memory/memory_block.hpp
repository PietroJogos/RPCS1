// Bloco de memória de tamanho fixo (potência de dois) com acessos de 8/16/32 bits.
//
// Usado pela RAM principal, scratchpad e BIOS ROM. O endereço recebido é um
// deslocamento dentro do bloco; bits acima do tamanho são ignorados (as linhas de
// endereço inexistentes do chip), o que produz espelhamento natural. O alinhamento
// é responsabilidade do barramento.
#pragma once

#include <memory>
#include <span>

#include "core/types.hpp"
#include "memory/byte_access.hpp"

namespace rpcs1::memory {

template <usize Size>
class MemoryBlock {
    static_assert(Size >= 4 && (Size & (Size - 1)) == 0, "tamanho deve ser potência de dois");

public:
    static constexpr usize size = Size;
    static constexpr u32 mask = static_cast<u32>(Size - 1);

    MemoryBlock() : bytes_(std::make_unique<u8[]>(Size)) {}

    MemoryBlock(const MemoryBlock&) = delete;
    MemoryBlock& operator=(const MemoryBlock&) = delete;
    MemoryBlock(MemoryBlock&&) noexcept = default;
    MemoryBlock& operator=(MemoryBlock&&) noexcept = default;

    [[nodiscard]] u8 read8(u32 offset) const noexcept { return load_le8(at(offset)); }
    [[nodiscard]] u16 read16(u32 offset) const noexcept { return load_le16(at(offset & ~1u)); }
    [[nodiscard]] u32 read32(u32 offset) const noexcept { return load_le32(at(offset & ~3u)); }

    void write8(u32 offset, u8 value) noexcept { store_le8(at(offset), value); }
    void write16(u32 offset, u16 value) noexcept { store_le16(at(offset & ~1u), value); }
    void write32(u32 offset, u32 value) noexcept { store_le32(at(offset & ~3u), value); }

    void fill(u8 value) noexcept {
        for (usize i = 0; i < Size; ++i) {
            bytes_[i] = value;
        }
    }

    [[nodiscard]] std::span<u8> bytes() noexcept { return {bytes_.get(), Size}; }
    [[nodiscard]] std::span<const u8> bytes() const noexcept { return {bytes_.get(), Size}; }

private:
    [[nodiscard]] u8* at(u32 offset) noexcept { return bytes_.get() + (offset & mask); }
    [[nodiscard]] const u8* at(u32 offset) const noexcept { return bytes_.get() + (offset & mask); }

    std::unique_ptr<u8[]> bytes_;
};

// RAM principal: 2 MiB (consoles de varejo).
using MainRam = MemoryBlock<2 * MiB>;

// Scratchpad: 1 KiB (cache de dados exposta como RAM rápida).
using Scratchpad = MemoryBlock<1 * KiB>;

} // namespace rpcs1::memory
