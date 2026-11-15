#include "cpu/disassembler.hpp"

#include <array>
#include <format>

#include "cpu/instruction.hpp"

namespace rpcs1::cpu {

namespace {

constexpr std::array<std::string_view, 32> names = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0",   "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
};

std::string_view r(u32 index) {
    return names[index & 31];
}

std::string hex16(u32 v) {
    return std::format("0x{:X}", v & 0xFFFF);
}

std::string signed16(u32 v) {
    const auto s = static_cast<s16>(v & 0xFFFF);
    return s < 0 ? std::format("-0x{:X}", -static_cast<s32>(s)) : std::format("0x{:X}", s);
}

std::string special(Instruction in) {
    switch (in.funct()) {
    case fn::sll:
        if (in.word == 0) return "nop";
        return std::format("sll {}, {}, {}", r(in.rd()), r(in.rt()), in.shamt());
    case fn::srl: return std::format("srl {}, {}, {}", r(in.rd()), r(in.rt()), in.shamt());
    case fn::sra: return std::format("sra {}, {}, {}", r(in.rd()), r(in.rt()), in.shamt());
    case fn::sllv: return std::format("sllv {}, {}, {}", r(in.rd()), r(in.rt()), r(in.rs()));
    case fn::srlv: return std::format("srlv {}, {}, {}", r(in.rd()), r(in.rt()), r(in.rs()));
    case fn::srav: return std::format("srav {}, {}, {}", r(in.rd()), r(in.rt()), r(in.rs()));
    case fn::jr: return std::format("jr {}", r(in.rs()));
    case fn::jalr: return std::format("jalr {}, {}", r(in.rd()), r(in.rs()));
    case fn::syscall: return std::format("syscall 0x{:X}", in.code20());
    case fn::break_: return std::format("break 0x{:X}", in.code20());
    case fn::mfhi: return std::format("mfhi {}", r(in.rd()));
    case fn::mthi: return std::format("mthi {}", r(in.rs()));
    case fn::mflo: return std::format("mflo {}", r(in.rd()));
    case fn::mtlo: return std::format("mtlo {}", r(in.rs()));
    case fn::mult: return std::format("mult {}, {}", r(in.rs()), r(in.rt()));
    case fn::multu: return std::format("multu {}, {}", r(in.rs()), r(in.rt()));
    case fn::div: return std::format("div {}, {}", r(in.rs()), r(in.rt()));
    case fn::divu: return std::format("divu {}, {}", r(in.rs()), r(in.rt()));
    default: break;
    }
    static constexpr std::array<std::string_view, 12> alu = {
        "add", "addu", "sub", "subu", "and", "or", "xor", "nor", "", "", "slt", "sltu"};
    const u32 f = in.funct();
    if (f >= fn::add && f <= fn::sltu && !alu[f - fn::add].empty()) {
        return std::format("{} {}, {}, {}", alu[f - fn::add], r(in.rd()), r(in.rs()), r(in.rt()));
    }
    return std::format("(inválida SPECIAL {:02X}h)", f);
}

} // namespace

std::string_view register_name(u32 index) noexcept {
    return names[index & 31];
}

std::string disassemble(u32 word, u32 address) {
    const Instruction in{word};
    const u32 branch_target = address + 4 + (in.simm16() << 2);

    switch (in.opcode()) {
    case op::special: return special(in);
    case op::bcondz: {
        const bool ge = (in.rt() & 1u) != 0;
        const bool link = (in.rt() & 0x1Eu) == 0x10u;
        return std::format("{}{} {}, 0x{:08X}", ge ? "bgez" : "bltz", link ? "al" : "", r(in.rs()),
                           branch_target);
    }
    case op::j: return std::format("j 0x{:08X}", ((address + 4) & 0xF0000000u) | (in.target26() << 2));
    case op::jal:
        return std::format("jal 0x{:08X}", ((address + 4) & 0xF0000000u) | (in.target26() << 2));
    case op::beq:
        return std::format("beq {}, {}, 0x{:08X}", r(in.rs()), r(in.rt()), branch_target);
    case op::bne:
        return std::format("bne {}, {}, 0x{:08X}", r(in.rs()), r(in.rt()), branch_target);
    case op::blez: return std::format("blez {}, 0x{:08X}", r(in.rs()), branch_target);
    case op::bgtz: return std::format("bgtz {}, 0x{:08X}", r(in.rs()), branch_target);
    case op::addi: return std::format("addi {}, {}, {}", r(in.rt()), r(in.rs()), signed16(in.word));
    case op::addiu: return std::format("addiu {}, {}, {}", r(in.rt()), r(in.rs()), signed16(in.word));
    case op::slti: return std::format("slti {}, {}, {}", r(in.rt()), r(in.rs()), signed16(in.word));
    case op::sltiu: return std::format("sltiu {}, {}, {}", r(in.rt()), r(in.rs()), signed16(in.word));
    case op::andi: return std::format("andi {}, {}, {}", r(in.rt()), r(in.rs()), hex16(in.word));
    case op::ori: return std::format("ori {}, {}, {}", r(in.rt()), r(in.rs()), hex16(in.word));
    case op::xori: return std::format("xori {}, {}, {}", r(in.rt()), r(in.rs()), hex16(in.word));
    case op::lui: return std::format("lui {}, {}", r(in.rt()), hex16(in.word));
    case op::cop0:
    case op::cop1:
    case op::cop2:
    case op::cop3: {
        const u32 n = in.opcode() - op::cop0;
        if ((word >> 25) & 1u) { // comando do coprocessador
            if (n == 0 && in.funct() == 0x10) return "rfe";
            return std::format("cop{} 0x{:07X}", n, word & 0x01FFFFFF);
        }
        switch (in.rs()) {
        case 0x00: return std::format("mfc{} {}, ${}", n, r(in.rt()), in.rd());
        case 0x02: return std::format("cfc{} {}, ${}", n, r(in.rt()), in.rd());
        case 0x04: return std::format("mtc{} {}, ${}", n, r(in.rt()), in.rd());
        case 0x06: return std::format("ctc{} {}, ${}", n, r(in.rt()), in.rd());
        default: return std::format("cop{} 0x{:07X}", n, word & 0x03FFFFFF);
        }
    }
    default: break;
    }

    static constexpr std::array<std::string_view, 32> memory_ops = {
        "lb", "lh", "lwl", "lw", "lbu", "lhu", "lwr", "", "sb", "sh", "swl", "sw", "", "", "swr", "",
        "lwc0", "lwc1", "lwc2", "lwc3", "", "", "", "", "swc0", "swc1", "swc2", "swc3", "", "", "", ""};
    if (in.opcode() >= 0x20 && !memory_ops[in.opcode() - 0x20].empty()) {
        const bool cop = in.opcode() >= 0x30;
        return std::format("{} {}{}, {}({})", memory_ops[in.opcode() - 0x20], cop ? "$" : "",
                           cop ? std::format("{}", in.rt()) : std::string(r(in.rt())),
                           signed16(in.word), r(in.rs()));
    }
    return std::format("(inválida {:02X}h)", in.opcode());
}

} // namespace rpcs1::cpu
