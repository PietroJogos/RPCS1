// BIOS ROM de 512 KiB.
//
// O conteúdo vem exclusivamente de um arquivo fornecido pelo usuário; o RPCS1 não
// contém BIOS. Sem arquivo carregado a ROM permanece zerada e `loaded()` é falso.
// A identificação de versão (hashes) é trabalho da Fase 15.
#pragma once

#include <filesystem>
#include <span>

#include "core/error.hpp"
#include "memory/memory_block.hpp"

namespace rpcs1::memory {

class BiosRom {
public:
    static constexpr usize size = 512 * KiB;

    // Exige exatamente 512 KiB.
    [[nodiscard]] Status load(std::span<const u8> image);
    [[nodiscard]] Status load_file(const std::filesystem::path& path);
    void clear() noexcept;

    [[nodiscard]] bool loaded() const noexcept { return loaded_; }

    [[nodiscard]] u8 read8(u32 offset) const noexcept { return rom_.read8(offset); }
    [[nodiscard]] u16 read16(u32 offset) const noexcept { return rom_.read16(offset); }
    [[nodiscard]] u32 read32(u32 offset) const noexcept { return rom_.read32(offset); }

    [[nodiscard]] std::span<const u8> bytes() const noexcept { return rom_.bytes(); }

private:
    MemoryBlock<size> rom_;
    bool loaded_ = false;
};

} // namespace rpcs1::memory
