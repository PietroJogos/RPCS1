// Codificador de instruções MIPS I para testes (montador mínimo, sem dependências).
//
// Permite escrever programas de teste em C++ sem toolchain externa, por exemplo:
//   const std::vector<u32> program = {asm_::lui(t0, 0x1234), asm_::ori(t0, t0, 0x5678)};
// Deslocamentos de desvio são em INSTRUÇÕES relativas à instrução seguinte ao desvio,
// como no hardware: alvo = (endereço do desvio + 4) + offset * 4.
#pragma once

#include "core/types.hpp"
#include "cpu/instruction.hpp"

namespace rpcs1::test::asm_ {

// Registradores por nome convencional.
inline constexpr u32 zero = 0, at = 1, v0 = 2, v1 = 3, a0 = 4, a1 = 5, a2 = 6, a3 = 7, t0 = 8,
                     t1 = 9, t2 = 10, t3 = 11, t4 = 12, t5 = 13, t6 = 14, t7 = 15, s0 = 16, s1 = 17,
                     s2 = 18, s3 = 19, s4 = 20, s5 = 21, s6 = 22, s7 = 23, t8 = 24, t9 = 25,
                     k0 = 26, k1 = 27, gp = 28, sp = 29, fp = 30, ra = 31;

constexpr u32 r_type(u32 funct, u32 rs, u32 rt, u32 rd, u32 shamt = 0) {
    return (rs & 31) << 21 | (rt & 31) << 16 | (rd & 31) << 11 | (shamt & 31) << 6 | (funct & 63);
}
constexpr u32 i_type(u32 opcode, u32 rs, u32 rt, u32 imm) {
    return (opcode & 63) << 26 | (rs & 31) << 21 | (rt & 31) << 16 | (imm & 0xFFFF);
}
constexpr u32 j_type(u32 opcode, u32 target_address) {
    return (opcode & 63) << 26 | ((target_address >> 2) & 0x03FFFFFF);
}
constexpr u32 imm(s32 value) {
    return static_cast<u32>(value) & 0xFFFF;
}

constexpr u32 nop() { return 0; }

// ALU registrador-registrador.
constexpr u32 add(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::add, rs, rt, rd); }
constexpr u32 addu(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::addu, rs, rt, rd); }
constexpr u32 sub(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::sub, rs, rt, rd); }
constexpr u32 subu(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::subu, rs, rt, rd); }
constexpr u32 and_(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::and_, rs, rt, rd); }
constexpr u32 or_(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::or_, rs, rt, rd); }
constexpr u32 xor_(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::xor_, rs, rt, rd); }
constexpr u32 nor(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::nor, rs, rt, rd); }
constexpr u32 slt(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::slt, rs, rt, rd); }
constexpr u32 sltu(u32 rd, u32 rs, u32 rt) { return r_type(cpu::fn::sltu, rs, rt, rd); }

// Deslocamentos.
constexpr u32 sll(u32 rd, u32 rt, u32 sa) { return r_type(cpu::fn::sll, 0, rt, rd, sa); }
constexpr u32 srl(u32 rd, u32 rt, u32 sa) { return r_type(cpu::fn::srl, 0, rt, rd, sa); }
constexpr u32 sra(u32 rd, u32 rt, u32 sa) { return r_type(cpu::fn::sra, 0, rt, rd, sa); }
constexpr u32 sllv(u32 rd, u32 rt, u32 rs) { return r_type(cpu::fn::sllv, rs, rt, rd); }
constexpr u32 srlv(u32 rd, u32 rt, u32 rs) { return r_type(cpu::fn::srlv, rs, rt, rd); }
constexpr u32 srav(u32 rd, u32 rt, u32 rs) { return r_type(cpu::fn::srav, rs, rt, rd); }

// HI/LO, multiplicação e divisão.
constexpr u32 mfhi(u32 rd) { return r_type(cpu::fn::mfhi, 0, 0, rd); }
constexpr u32 mflo(u32 rd) { return r_type(cpu::fn::mflo, 0, 0, rd); }
constexpr u32 mthi(u32 rs) { return r_type(cpu::fn::mthi, rs, 0, 0); }
constexpr u32 mtlo(u32 rs) { return r_type(cpu::fn::mtlo, rs, 0, 0); }
constexpr u32 mult(u32 rs, u32 rt) { return r_type(cpu::fn::mult, rs, rt, 0); }
constexpr u32 multu(u32 rs, u32 rt) { return r_type(cpu::fn::multu, rs, rt, 0); }
constexpr u32 div(u32 rs, u32 rt) { return r_type(cpu::fn::div, rs, rt, 0); }
constexpr u32 divu(u32 rs, u32 rt) { return r_type(cpu::fn::divu, rs, rt, 0); }

// Saltos por registrador e exceções.
constexpr u32 jr(u32 rs) { return r_type(cpu::fn::jr, rs, 0, 0); }
constexpr u32 jalr(u32 rd, u32 rs) { return r_type(cpu::fn::jalr, rs, 0, rd); }
constexpr u32 syscall(u32 code = 0) { return (code & 0xFFFFF) << 6 | cpu::fn::syscall; }
constexpr u32 break_(u32 code = 0) { return (code & 0xFFFFF) << 6 | cpu::fn::break_; }

// ALU com imediato.
constexpr u32 addi(u32 rt, u32 rs, s32 v) { return i_type(cpu::op::addi, rs, rt, imm(v)); }
constexpr u32 addiu(u32 rt, u32 rs, s32 v) { return i_type(cpu::op::addiu, rs, rt, imm(v)); }
constexpr u32 slti(u32 rt, u32 rs, s32 v) { return i_type(cpu::op::slti, rs, rt, imm(v)); }
constexpr u32 sltiu(u32 rt, u32 rs, s32 v) { return i_type(cpu::op::sltiu, rs, rt, imm(v)); }
constexpr u32 andi(u32 rt, u32 rs, u32 v) { return i_type(cpu::op::andi, rs, rt, v); }
constexpr u32 ori(u32 rt, u32 rs, u32 v) { return i_type(cpu::op::ori, rs, rt, v); }
constexpr u32 xori(u32 rt, u32 rs, u32 v) { return i_type(cpu::op::xori, rs, rt, v); }
constexpr u32 lui(u32 rt, u32 v) { return i_type(cpu::op::lui, 0, rt, v); }

// Loads e stores: (rt, deslocamento, base).
constexpr u32 lb(u32 rt, s32 off, u32 base) { return i_type(cpu::op::lb, base, rt, imm(off)); }
constexpr u32 lbu(u32 rt, s32 off, u32 base) { return i_type(cpu::op::lbu, base, rt, imm(off)); }
constexpr u32 lh(u32 rt, s32 off, u32 base) { return i_type(cpu::op::lh, base, rt, imm(off)); }
constexpr u32 lhu(u32 rt, s32 off, u32 base) { return i_type(cpu::op::lhu, base, rt, imm(off)); }
constexpr u32 lw(u32 rt, s32 off, u32 base) { return i_type(cpu::op::lw, base, rt, imm(off)); }
constexpr u32 lwl(u32 rt, s32 off, u32 base) { return i_type(cpu::op::lwl, base, rt, imm(off)); }
constexpr u32 lwr(u32 rt, s32 off, u32 base) { return i_type(cpu::op::lwr, base, rt, imm(off)); }
constexpr u32 sb(u32 rt, s32 off, u32 base) { return i_type(cpu::op::sb, base, rt, imm(off)); }
constexpr u32 sh(u32 rt, s32 off, u32 base) { return i_type(cpu::op::sh, base, rt, imm(off)); }
constexpr u32 sw(u32 rt, s32 off, u32 base) { return i_type(cpu::op::sw, base, rt, imm(off)); }
constexpr u32 swl(u32 rt, s32 off, u32 base) { return i_type(cpu::op::swl, base, rt, imm(off)); }
constexpr u32 swr(u32 rt, s32 off, u32 base) { return i_type(cpu::op::swr, base, rt, imm(off)); }

// Desvios condicionais: offset em instruções.
constexpr u32 beq(u32 rs, u32 rt, s32 off) { return i_type(cpu::op::beq, rs, rt, imm(off)); }
constexpr u32 bne(u32 rs, u32 rt, s32 off) { return i_type(cpu::op::bne, rs, rt, imm(off)); }
constexpr u32 blez(u32 rs, s32 off) { return i_type(cpu::op::blez, rs, 0, imm(off)); }
constexpr u32 bgtz(u32 rs, s32 off) { return i_type(cpu::op::bgtz, rs, 0, imm(off)); }
constexpr u32 bltz(u32 rs, s32 off) { return i_type(cpu::op::bcondz, rs, 0x00, imm(off)); }
constexpr u32 bgez(u32 rs, s32 off) { return i_type(cpu::op::bcondz, rs, 0x01, imm(off)); }
constexpr u32 bltzal(u32 rs, s32 off) { return i_type(cpu::op::bcondz, rs, 0x10, imm(off)); }
constexpr u32 bgezal(u32 rs, s32 off) { return i_type(cpu::op::bcondz, rs, 0x11, imm(off)); }
constexpr u32 bcondz_raw(u32 rs, u32 rt_field, s32 off) {
    return i_type(cpu::op::bcondz, rs, rt_field, imm(off));
}

// Saltos absolutos: recebe o endereço de destino completo (os 4 bits altos vêm do PC).
constexpr u32 j(u32 target_address) { return j_type(cpu::op::j, target_address); }
constexpr u32 jal(u32 target_address) { return j_type(cpu::op::jal, target_address); }

// Coprocessadores (apenas para testar o tratamento de instruções ainda não implementadas).
constexpr u32 mfc0(u32 rt, u32 rd) { return i_type(cpu::op::cop0, 0x00, rt, (rd & 31) << 11); }
constexpr u32 rfe() { return 0x42000010; }

} // namespace rpcs1::test::asm_
