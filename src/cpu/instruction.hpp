// Campos de uma instrução MIPS I de 32 bits.
//
//   R:  op(31–26) rs(25–21) rt(20–16) rd(15–11) shamt(10–6) funct(5–0)
//   I:  op(31–26) rs(25–21) rt(20–16) imm16(15–0)
//   J:  op(31–26) target26(25–0)
#pragma once

#include "core/types.hpp"

namespace rpcs1::cpu {

struct Instruction {
    u32 word = 0;

    [[nodiscard]] constexpr u32 opcode() const noexcept { return word >> 26; }
    [[nodiscard]] constexpr u32 rs() const noexcept { return (word >> 21) & 0x1F; }
    [[nodiscard]] constexpr u32 rt() const noexcept { return (word >> 16) & 0x1F; }
    [[nodiscard]] constexpr u32 rd() const noexcept { return (word >> 11) & 0x1F; }
    [[nodiscard]] constexpr u32 shamt() const noexcept { return (word >> 6) & 0x1F; }
    [[nodiscard]] constexpr u32 funct() const noexcept { return word & 0x3F; }
    [[nodiscard]] constexpr u32 imm16() const noexcept { return word & 0xFFFF; }
    // Imediato com extensão de sinal (ADDI, ADDIU, SLTI, SLTIU, loads/stores, desvios).
    [[nodiscard]] constexpr u32 simm16() const noexcept {
        return static_cast<u32>(static_cast<s32>(static_cast<s16>(word & 0xFFFF)));
    }
    [[nodiscard]] constexpr u32 target26() const noexcept { return word & 0x03FFFFFF; }
    // Campo de comentário de SYSCALL/BREAK (ignorado pela CPU).
    [[nodiscard]] constexpr u32 code20() const noexcept { return (word >> 6) & 0xFFFFF; }
};

// Opcodes primários.
namespace op {
inline constexpr u32 special = 0x00, bcondz = 0x01, j = 0x02, jal = 0x03, beq = 0x04, bne = 0x05,
                     blez = 0x06, bgtz = 0x07, addi = 0x08, addiu = 0x09, slti = 0x0A, sltiu = 0x0B,
                     andi = 0x0C, ori = 0x0D, xori = 0x0E, lui = 0x0F, cop0 = 0x10, cop1 = 0x11,
                     cop2 = 0x12, cop3 = 0x13, lb = 0x20, lh = 0x21, lwl = 0x22, lw = 0x23,
                     lbu = 0x24, lhu = 0x25, lwr = 0x26, sb = 0x28, sh = 0x29, swl = 0x2A, sw = 0x2B,
                     swr = 0x2E, lwc0 = 0x30, lwc1 = 0x31, lwc2 = 0x32, lwc3 = 0x33, swc0 = 0x38,
                     swc1 = 0x39, swc2 = 0x3A, swc3 = 0x3B;
} // namespace op

// Funções do grupo SPECIAL (opcode 00h).
namespace fn {
inline constexpr u32 sll = 0x00, srl = 0x02, sra = 0x03, sllv = 0x04, srlv = 0x06, srav = 0x07,
                     jr = 0x08, jalr = 0x09, syscall = 0x0C, break_ = 0x0D, mfhi = 0x10,
                     mthi = 0x11, mflo = 0x12, mtlo = 0x13, mult = 0x18, multu = 0x19, div = 0x1A,
                     divu = 0x1B, add = 0x20, addu = 0x21, sub = 0x22, subu = 0x23, and_ = 0x24,
                     or_ = 0x25, xor_ = 0x26, nor = 0x27, slt = 0x2A, sltu = 0x2B;
} // namespace fn

} // namespace rpcs1::cpu
