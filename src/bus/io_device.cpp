#include "bus/io_device.hpp"

#include <utility>

#include "core/log.hpp"

namespace rpcs1::bus {

UnimplementedDevice::UnimplementedDevice(std::string name, int phase)
    : name_(std::move(name)), phase_(phase) {}

u32 UnimplementedDevice::io_read(u32 offset, AccessWidth width) {
    report("Leitura", offset, width, 0);
    return 0;
}

void UnimplementedDevice::io_write(u32 offset, u32 value, AccessWidth width) {
    report("Escrita", offset, width, value);
}

void UnimplementedDevice::report(const char* kind, u32 offset, AccessWidth width, u32 value) {
    ++accesses_;
    // Chave por registrador e tipo de acesso: avisa só na primeira vez.
    const u32 key = (offset << 1) | (kind[0] == 'E' ? 1u : 0u);
    if (!reported_.insert(key).second) {
        return;
    }
    log::warn(log::Category::Bus,
              "{} de {} em {} (+{:03X}h, valor {:08X}h): dispositivo não implementado (Fase {})",
              kind, to_string(width), name_, offset, value, phase_);
}

} // namespace rpcs1::bus
