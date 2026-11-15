// Interface dos dispositivos mapeados na área de E/S (1F801000h–1F801FFFh).
#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

#include "bus/address.hpp"
#include "core/types.hpp"

namespace rpcs1::bus {

class IoDevice {
public:
    virtual ~IoDevice() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    // `offset` é relativo ao início da faixa do dispositivo e pode ser desalinhado em
    // acessos de 8/16 bits. O resultado é truncado pelo barramento à largura pedida.
    [[nodiscard]] virtual u32 io_read(u32 offset, AccessWidth width) = 0;

    // `value` contém os 32 bits do registrador da CPU mesmo em SB/SH: o R3000A coloca o
    // registrador inteiro no barramento e cada dispositivo decide o que aproveitar
    // (psx-spx, "CPU Specifications": escritas de 8/16 bits em registradores de E/S).
    virtual void io_write(u32 offset, u32 value, AccessWidth width) = 0;
};

// Marcador explícito de um periférico que ainda NÃO foi implementado.
//
// Não simula o dispositivo: leituras retornam 0, escritas são descartadas e cada
// registrador acessado gera um aviso no log (uma vez), indicando a fase prevista.
// Serve apenas para que o barramento seja completo e os acessos fiquem visíveis.
class UnimplementedDevice final : public IoDevice {
public:
    UnimplementedDevice(std::string name, int phase);

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }
    [[nodiscard]] u32 io_read(u32 offset, AccessWidth width) override;
    void io_write(u32 offset, u32 value, AccessWidth width) override;

    [[nodiscard]] u64 access_count() const noexcept { return accesses_; }
    [[nodiscard]] int phase() const noexcept { return phase_; }

private:
    void report(const char* kind, u32 offset, AccessWidth width, u32 value);

    std::string name_;
    int phase_;
    u64 accesses_ = 0;
    std::unordered_set<u32> reported_;
};

} // namespace rpcs1::bus
