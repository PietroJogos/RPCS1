#include "cpu/cpu.hpp"

#include <climits>

#include "core/log.hpp"
#include "cpu/disassembler.hpp"

namespace rpcs1::cpu {

namespace {

using log::Category;

constexpr s32 as_signed(u32 v) noexcept {
    return static_cast<s32>(v);
}

// Overflow em complemento de dois: os operandos têm o mesmo sinal e o resultado não.
constexpr bool add_overflows(u32 a, u32 b, u32 result) noexcept {
    return ((a ^ result) & (b ^ result) & 0x80000000u) != 0;
}
constexpr bool sub_overflows(u32 a, u32 b, u32 result) noexcept {
    return ((a ^ b) & (a ^ result) & 0x80000000u) != 0;
}

} // namespace

std::string_view to_string(ExceptionCode code) noexcept {
    switch (code) {
    case ExceptionCode::Interrupt: return "Int";
    case ExceptionCode::AddressErrorLoad: return "AdEL";
    case ExceptionCode::AddressErrorStore: return "AdES";
    case ExceptionCode::BusErrorInstruction: return "IBE";
    case ExceptionCode::BusErrorData: return "DBE";
    case ExceptionCode::Syscall: return "Sys";
    case ExceptionCode::Breakpoint: return "Bp";
    case ExceptionCode::ReservedInstruction: return "RI";
    case ExceptionCode::CoprocessorUnusable: return "CpU";
    case ExceptionCode::Overflow: return "Ov";
    }
    return "?";
}

Cpu::Cpu(bus::Bus& bus) : bus_(bus) {
    reset();
}

void Cpu::reset() {
    regs_.fill(0);
    hi_ = 0;
    lo_ = 0;
    hilo_ready_cycle_ = 0;
    pc_ = reset_vector;
    next_pc_ = reset_vector + 4;
    next_in_delay_slot_ = false;
    next_branch_taken_ = false;
    next_branch_target_ = 0;
    incoming_load_ = {};
    pending_load_ = {};
    cycles_ = 0;
    instructions_ = 0;
    halted_ = false;
    last_exception_.reset();
    last_unimplemented_.reset();
}

void Cpu::set_gpr(u32 index, u32 value) noexcept {
    index &= 31;
    if (index != 0) {
        regs_[index] = value;
    }
}

void Cpu::set_pc(u32 address) noexcept {
    pc_ = address;
    next_pc_ = address + 4;
    next_in_delay_slot_ = false;
    next_branch_taken_ = false;
    next_branch_target_ = 0;
}

u32 Cpu::mult_latency(u32 rs, bool is_signed) noexcept {
    // psx-spx: a duração depende apenas do primeiro operando (rs).
    if (is_signed) {
        if (rs < 0x00000800u || rs >= 0xFFFFF800u) return 6;
        if (rs < 0x00100000u || rs >= 0xFFF00000u) return 9;
        return 13;
    }
    if (rs < 0x00000800u) return 6;
    if (rs < 0x00100000u) return 9;
    return 13;
}

u64 Cpu::run(u64 max_instructions) {
    u64 executed = 0;
    while (executed < max_instructions && step() == StepResult::Executed) {
        ++executed;
    }
    return executed;
}

StepResult Cpu::step() {
    if (halted_) {
        return StepResult::Halted;
    }

    // A instrução atual herda o estado de delay slot criado pela anterior.
    current_pc_ = pc_;
    current_in_delay_slot_ = next_in_delay_slot_;
    current_branch_taken_ = next_branch_taken_;
    current_branch_target_ = next_branch_target_;
    next_in_delay_slot_ = false;
    next_branch_taken_ = false;
    next_branch_target_ = 0;

    // A carga da instrução anterior só se completa ao fim desta (load delay slot).
    incoming_load_ = pending_load_;
    pending_load_ = {};

    Flow flow;
    const bus::ReadResult fetched = bus_.fetch_instruction(current_pc_);
    if (!fetched.ok()) {
        flow = fetched.fault == bus::BusFault::AddressError
                   ? raise(ExceptionCode::AddressErrorLoad, current_pc_)
                   : raise(ExceptionCode::BusErrorInstruction);
    } else {
        pc_ = next_pc_;
        next_pc_ = pc_ + 4;
        flow = execute(Instruction{fetched.value});
    }

    // psx-spx: se uma exceção ocorre logo após uma carga, a carga se completa mesmo assim.
    if (incoming_load_.reg != 0) {
        regs_[incoming_load_.reg] = incoming_load_.value;
    }
    incoming_load_ = {};
    // Uma instrução não implementada não foi executada: não consome ciclo.
    if (flow != Flow::Unimplemented) {
        ++cycles_;
    }

    if (flow == Flow::Continue) {
        ++instructions_;
        return StepResult::Executed;
    }

    // Fase 3: a CPU para na instrução causadora; o despacho ao vetor é a Fase 4.
    halted_ = true;
    pending_load_ = {};
    pc_ = current_pc_;
    next_pc_ = current_pc_ + 4;
    next_in_delay_slot_ = false;
    next_branch_taken_ = false;
    return flow == Flow::Exception ? StepResult::Exception : StepResult::Unimplemented;
}

void Cpu::write_reg(u32 index, u32 value) noexcept {
    if (index == 0) {
        return;
    }
    regs_[index] = value;
    // A escrita desta instrução ocorre depois da carga pendente: a carga é descartada.
    if (incoming_load_.reg == index) {
        incoming_load_.reg = 0;
    }
}

void Cpu::schedule_load(u32 index, u32 value) noexcept {
    if (index == 0) {
        return;
    }
    // ⚠️ Duas cargas seguidas no mesmo registrador: pelo pipeline de 5 estágios a primeira
    // ainda se completa ao fim desta instrução (não é cancelada) — não documentado no psx-spx.
    pending_load_ = PendingLoad{index, value};
}

u32 Cpu::merge_source(u32 index) const noexcept {
    // psx-spx: LWL e LWR podem ser seguidas sem delay slot mesmo com o mesmo registrador.
    if (index != 0 && incoming_load_.reg == index) {
        return incoming_load_.value;
    }
    return regs_[index];
}

void Cpu::mark_delay_slot() noexcept {
    next_in_delay_slot_ = true;
}

void Cpu::branch(u32 target) noexcept {
    next_pc_ = target;
    next_branch_taken_ = true;
    next_branch_target_ = target;
}

void Cpu::wait_hilo() noexcept {
    // Ler HI/LO durante MULT/DIV bloqueia a CPU até o resultado ficar pronto.
    if (cycles_ < hilo_ready_cycle_) {
        cycles_ = hilo_ready_cycle_;
    }
}

Cpu::Flow Cpu::raise(ExceptionCode code, std::optional<u32> bad_vaddr) {
    ExceptionRecord record;
    record.code = code;
    record.instruction_address = current_pc_;
    record.branch_delay = current_in_delay_slot_;
    record.epc = current_in_delay_slot_ ? current_pc_ - 4 : current_pc_;
    record.branch_taken = current_in_delay_slot_ && current_branch_taken_;
    record.branch_target = record.branch_taken ? current_branch_target_ : 0;
    // BadVaddr só é atualizado por AdEL/AdES.
    if (code == ExceptionCode::AddressErrorLoad || code == ExceptionCode::AddressErrorStore) {
        record.bad_vaddr = bad_vaddr;
    }
    last_exception_ = record;
    log::debug(Category::Cpu, "Exceção {} em {:08X}h (EPC {:08X}h{})", to_string(code),
               current_pc_, record.epc, record.branch_delay ? ", delay slot" : "");
    return Flow::Exception;
}

Cpu::Flow Cpu::unimplemented(Instruction instr, int phase) {
    last_unimplemented_ = UnimplementedRecord{current_pc_, instr.word, phase};
    log::warn(Category::Cpu,
              "CPU parou em {:08X}h: \"{}\" ({:08X}h) requer componente ainda não implementado (Fase {})",
              current_pc_, disassemble(instr.word, current_pc_), instr.word, phase);
    return Flow::Unimplemented;
}

Cpu::Flow Cpu::fault_from_bus(bus::BusFault fault, bool store, u32 address) {
    if (fault == bus::BusFault::AddressError) {
        return raise(store ? ExceptionCode::AddressErrorStore : ExceptionCode::AddressErrorLoad,
                     address);
    }
    return raise(ExceptionCode::BusErrorData);
}

Cpu::Flow Cpu::execute(Instruction instr) {
    const u32 rs = regs_[instr.rs()];
    const u32 rt = regs_[instr.rt()];

    switch (instr.opcode()) {
    case op::special: return execute_special(instr);

    case op::bcondz: {
        // rt: bit 0 = "maior ou igual a zero"; 10h/11h = BLTZAL/BGEZAL (link).
        // ⚠️ Outros valores de rt: tratados como BLTZ/BGEZ pelo bit 0, sem link (não documentado).
        const bool ge = (instr.rt() & 1u) != 0;
        const bool link = (instr.rt() & 0x1Eu) == 0x10u;
        const bool taken = ge ? as_signed(rs) >= 0 : as_signed(rs) < 0; // rs lido antes do link
        const u32 target = pc_ + (instr.simm16() << 2);
        mark_delay_slot();
        if (link) {
            write_reg(31, current_pc_ + 8); // gravado mesmo sem desvio
        }
        if (taken) {
            branch(target);
        }
        return Flow::Continue;
    }
    case op::j:
    case op::jal: {
        const u32 target = (pc_ & 0xF0000000u) | (instr.target26() << 2);
        mark_delay_slot();
        if (instr.opcode() == op::jal) {
            write_reg(31, current_pc_ + 8);
        }
        branch(target);
        return Flow::Continue;
    }
    case op::beq:
    case op::bne:
    case op::blez:
    case op::bgtz: {
        bool taken = false;
        switch (instr.opcode()) {
        case op::beq: taken = rs == rt; break;
        case op::bne: taken = rs != rt; break;
        case op::blez: taken = as_signed(rs) <= 0; break;
        default: taken = as_signed(rs) > 0; break;
        }
        mark_delay_slot();
        if (taken) {
            branch(pc_ + (instr.simm16() << 2));
        }
        return Flow::Continue;
    }

    case op::addi: {
        const u32 result = rs + instr.simm16();
        if (add_overflows(rs, instr.simm16(), result)) {
            return raise(ExceptionCode::Overflow);
        }
        write_reg(instr.rt(), result);
        return Flow::Continue;
    }
    case op::addiu: write_reg(instr.rt(), rs + instr.simm16()); return Flow::Continue;
    case op::slti:
        write_reg(instr.rt(), as_signed(rs) < as_signed(instr.simm16()) ? 1u : 0u);
        return Flow::Continue;
    case op::sltiu:
        // O imediato tem extensão de sinal, mas a comparação é sem sinal.
        write_reg(instr.rt(), rs < instr.simm16() ? 1u : 0u);
        return Flow::Continue;
    case op::andi: write_reg(instr.rt(), rs & instr.imm16()); return Flow::Continue;
    case op::ori: write_reg(instr.rt(), rs | instr.imm16()); return Flow::Continue;
    case op::xori: write_reg(instr.rt(), rs ^ instr.imm16()); return Flow::Continue;
    case op::lui: write_reg(instr.rt(), instr.imm16() << 16); return Flow::Continue;

    case op::cop0:
    case op::cop1:
    case op::cop3:
    case op::lwc0:
    case op::lwc1:
    case op::lwc3:
    case op::swc0:
    case op::swc1:
    case op::swc3:
        // COP0 e o tratamento de CU1/CU3 (inexistentes no PS1) dependem do SR: Fase 4.
        return unimplemented(instr, 4);
    case op::cop2:
    case op::lwc2:
    case op::swc2:
        return unimplemented(instr, 9); // GTE

    case op::lb:
    case op::lh:
    case op::lwl:
    case op::lw:
    case op::lbu:
    case op::lhu:
    case op::lwr: return execute_load(instr);

    case op::sb:
    case op::sh:
    case op::swl:
    case op::sw:
    case op::swr: return execute_store(instr);

    default:
        // 14h–1Fh, 27h, 2Ch, 2Dh, 2Fh, 34h–37h, 3Ch–3Fh (psx-spx, "Illegal Opcodes").
        return raise(ExceptionCode::ReservedInstruction);
    }
}

Cpu::Flow Cpu::execute_special(Instruction instr) {
    const u32 rs = regs_[instr.rs()];
    const u32 rt = regs_[instr.rt()];
    const u32 rd = instr.rd();

    switch (instr.funct()) {
    case fn::sll: write_reg(rd, rt << instr.shamt()); return Flow::Continue;
    case fn::srl: write_reg(rd, rt >> instr.shamt()); return Flow::Continue;
    case fn::sra: write_reg(rd, static_cast<u32>(as_signed(rt) >> instr.shamt())); return Flow::Continue;
    case fn::sllv: write_reg(rd, rt << (rs & 31u)); return Flow::Continue;
    case fn::srlv: write_reg(rd, rt >> (rs & 31u)); return Flow::Continue;
    case fn::srav: write_reg(rd, static_cast<u32>(as_signed(rt) >> (rs & 31u))); return Flow::Continue;

    case fn::jr:
        mark_delay_slot();
        branch(rs);
        return Flow::Continue;
    case fn::jalr:
        // O alvo é lido antes da escrita do link (importa quando rd == rs).
        mark_delay_slot();
        write_reg(rd, current_pc_ + 8);
        branch(rs);
        return Flow::Continue;

    case fn::syscall: return raise(ExceptionCode::Syscall);
    case fn::break_: return raise(ExceptionCode::Breakpoint);

    case fn::mfhi:
        wait_hilo();
        write_reg(rd, hi_);
        return Flow::Continue;
    case fn::mflo:
        wait_hilo();
        write_reg(rd, lo_);
        return Flow::Continue;
    // ⚠️ MTHI/MTLO durante MULT/DIV em andamento: não documentado; gravam imediatamente.
    case fn::mthi: hi_ = rs; return Flow::Continue;
    case fn::mtlo: lo_ = rs; return Flow::Continue;

    case fn::mult: {
        const s64 product = static_cast<s64>(as_signed(rs)) * static_cast<s64>(as_signed(rt));
        hi_ = static_cast<u32>(static_cast<u64>(product) >> 32);
        lo_ = static_cast<u32>(product);
        hilo_ready_cycle_ = cycles_ + 1 + mult_latency(rs, true);
        return Flow::Continue;
    }
    case fn::multu: {
        const u64 product = static_cast<u64>(rs) * static_cast<u64>(rt);
        hi_ = static_cast<u32>(product >> 32);
        lo_ = static_cast<u32>(product);
        hilo_ready_cycle_ = cycles_ + 1 + mult_latency(rs, false);
        return Flow::Continue;
    }
    case fn::div: {
        const s32 n = as_signed(rs);
        const s32 d = as_signed(rt);
        if (d == 0) {
            // Sem exceção: HI = dividendo, LO = -1 (dividendo >= 0) ou +1 (dividendo < 0).
            hi_ = rs;
            lo_ = n >= 0 ? 0xFFFFFFFFu : 1u;
        } else if (n == INT_MIN && d == -1) {
            hi_ = 0;
            lo_ = 0x80000000u;
        } else {
            lo_ = static_cast<u32>(n / d);
            hi_ = static_cast<u32>(n % d);
        }
        hilo_ready_cycle_ = cycles_ + 1 + div_latency;
        return Flow::Continue;
    }
    case fn::divu:
        if (rt == 0) {
            hi_ = rs;
            lo_ = 0xFFFFFFFFu;
        } else {
            lo_ = rs / rt;
            hi_ = rs % rt;
        }
        hilo_ready_cycle_ = cycles_ + 1 + div_latency;
        return Flow::Continue;

    case fn::add: {
        const u32 result = rs + rt;
        if (add_overflows(rs, rt, result)) {
            return raise(ExceptionCode::Overflow);
        }
        write_reg(rd, result);
        return Flow::Continue;
    }
    case fn::addu: write_reg(rd, rs + rt); return Flow::Continue;
    case fn::sub: {
        const u32 result = rs - rt;
        if (sub_overflows(rs, rt, result)) {
            return raise(ExceptionCode::Overflow);
        }
        write_reg(rd, result);
        return Flow::Continue;
    }
    case fn::subu: write_reg(rd, rs - rt); return Flow::Continue;
    case fn::and_: write_reg(rd, rs & rt); return Flow::Continue;
    case fn::or_: write_reg(rd, rs | rt); return Flow::Continue;
    case fn::xor_: write_reg(rd, rs ^ rt); return Flow::Continue;
    case fn::nor: write_reg(rd, ~(rs | rt)); return Flow::Continue;
    case fn::slt: write_reg(rd, as_signed(rs) < as_signed(rt) ? 1u : 0u); return Flow::Continue;
    case fn::sltu: write_reg(rd, rs < rt ? 1u : 0u); return Flow::Continue;

    default:
        // 01h, 05h, 0Ah, 0Bh, 0Eh, 0Fh, 14h–17h, 1Ch–1Fh, 28h, 29h, 2Ch–3Fh.
        return raise(ExceptionCode::ReservedInstruction);
    }
}

Cpu::Flow Cpu::execute_load(Instruction instr) {
    const u32 address = regs_[instr.rs()] + instr.simm16();
    const u32 target = instr.rt();

    switch (instr.opcode()) {
    case op::lb:
    case op::lbu: {
        const auto r = bus_.read8(address);
        if (!r.ok()) return fault_from_bus(r.fault, false, address);
        schedule_load(target, instr.opcode() == op::lb
                                  ? static_cast<u32>(static_cast<s32>(static_cast<s8>(r.value)))
                                  : (r.value & 0xFFu));
        return Flow::Continue;
    }
    case op::lh:
    case op::lhu: {
        const auto r = bus_.read16(address);
        if (!r.ok()) return fault_from_bus(r.fault, false, address);
        schedule_load(target, instr.opcode() == op::lh
                                  ? static_cast<u32>(static_cast<s32>(static_cast<s16>(r.value)))
                                  : (r.value & 0xFFFFu));
        return Flow::Continue;
    }
    case op::lw: {
        const auto r = bus_.read32(address);
        if (!r.ok()) return fault_from_bus(r.fault, false, address);
        schedule_load(target, r.value);
        return Flow::Continue;
    }
    case op::lwl:
    case op::lwr: {
        // Nunca geram erro de alinhamento: leem a palavra alinhada e mesclam bytes em rt.
        const auto r = bus_.read32(address & ~3u);
        if (!r.ok()) return fault_from_bus(r.fault, false, address);
        const u32 word = r.value;
        const u32 current = merge_source(target);
        u32 result = 0;
        if (instr.opcode() == op::lwl) {
            switch (address & 3u) {
            case 0: result = (current & 0x00FFFFFFu) | (word << 24); break;
            case 1: result = (current & 0x0000FFFFu) | (word << 16); break;
            case 2: result = (current & 0x000000FFu) | (word << 8); break;
            default: result = word; break;
            }
        } else {
            switch (address & 3u) {
            case 0: result = word; break;
            case 1: result = (current & 0xFF000000u) | (word >> 8); break;
            case 2: result = (current & 0xFFFF0000u) | (word >> 16); break;
            default: result = (current & 0xFFFFFF00u) | (word >> 24); break;
            }
        }
        schedule_load(target, result);
        return Flow::Continue;
    }
    default: break;
    }
    return raise(ExceptionCode::ReservedInstruction);
}

Cpu::Flow Cpu::execute_store(Instruction instr) {
    const u32 address = regs_[instr.rs()] + instr.simm16();
    // SB/SH colocam os 32 bits do registrador no barramento; o barramento decide os bytes.
    const u32 value = regs_[instr.rt()];

    bus::BusFault fault = bus::BusFault::None;
    switch (instr.opcode()) {
    case op::sb: fault = bus_.write8(address, value); break;
    case op::sh: fault = bus_.write16(address, value); break;
    case op::sw: fault = bus_.write32(address, value); break;
    case op::swl:
    case op::swr: {
        // Grava apenas os bytes selecionados da palavra alinhada.
        // ⚠️ Em portas de E/S o resultado é desconhecido (psx-spx); aqui viram escritas de byte.
        const u32 aligned = address & ~3u;
        const u32 offset = address & 3u;
        u32 word = 0;
        u32 mask = 0;
        if (instr.opcode() == op::swl) {
            word = value >> (24 - offset * 8);
            mask = (2u << offset) - 1u; // bytes 0..offset
        } else {
            word = value << (offset * 8);
            mask = (0xFu << offset) & 0xFu; // bytes offset..3
        }
        if (mask == 0xFu) {
            fault = bus_.write32(aligned, word);
        } else {
            for (u32 i = 0; i < 4 && fault == bus::BusFault::None; ++i) {
                if ((mask >> i) & 1u) {
                    fault = bus_.write8(aligned + i, word >> (i * 8));
                }
            }
        }
        break;
    }
    default: return raise(ExceptionCode::ReservedInstruction);
    }
    if (fault != bus::BusFault::None) {
        return fault_from_bus(fault, true, address);
    }
    return Flow::Continue;
}

} // namespace rpcs1::cpu
