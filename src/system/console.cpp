#include "system/console.hpp"

namespace rpcs1::system {

Console::Console() : cpu_(bus_) {
    bus_.attach_device(bus::IoSlot::Interrupts, irq_);
    cpu_.set_interrupt_line(&irq_);
}

void Console::power_on() {
    bus_.power_on();
    irq_.reset();
    cpu_.reset();
}

void Console::reset() {
    bus_.reset();
    irq_.reset();
    cpu_.reset();
}

Status Console::load_bios(const std::filesystem::path& path) {
    return bus_.bios().load_file(path);
}

} // namespace rpcs1::system
