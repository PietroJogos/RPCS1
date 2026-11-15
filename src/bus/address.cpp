#include "bus/address.hpp"

namespace rpcs1::bus {

std::string_view to_string(Segment segment) noexcept {
    switch (segment) {
    case Segment::Kuseg: return "KUSEG";
    case Segment::Kseg0: return "KSEG0";
    case Segment::Kseg1: return "KSEG1";
    case Segment::Kseg2: return "KSEG2";
    }
    return "?";
}

std::string_view to_string(AccessWidth width) noexcept {
    switch (width) {
    case AccessWidth::Byte: return "8 bits";
    case AccessWidth::Half: return "16 bits";
    case AccessWidth::Word: return "32 bits";
    }
    return "?";
}

} // namespace rpcs1::bus
