// Interpretador da CPU MIPS R3000A do PlayStation.
//
// Fase 3: conjunto de instruções sem coprocessadores, delay slot de desvio, load delay slot,
//         MULT/DIV com latência, detecção de exceções.
// Fase 4: COP0 (SR, CAUSE, EPC, BadVaddr, TAR, PRID, registradores de depuração),
//         MFC0/MTC0/RFE, despacho das exceções aos vetores, interrupções (CAUSE bit 10 e
//         interrupções de software), modo usuário, verificação de CU0–CU3, I-cache e
//         isolamento de cache (SR.IsC).
//
// Limites atuais (docs/PROGRESS.md):
//   - COP2/GTE (COP2, LWC2, SWC2 com CU2 = 1) para a CPU como "não implementado" (Fase 9).
//   - Breakpoints de hardware (BPC/BDA/DCIC) são armazenados, mas não disparam (Fase 23).
//   - Ciclos: 1 por instrução + bloqueios de MFHI/MFLO. Memória/cache: Fase 5.
#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "bus/bus.hpp"
#include "core/types.hpp"
#include "cpu/cop0.hpp"
#include "cpu/icache.hpp"
#include "cpu/instruction.hpp"
#include "cpu/interrupt_line.hpp"

namespace rpcs1::cpu {

// Códigos de exceção (CAUSE.ExcCode).
enum class ExceptionCode : u8 {
    Interrupt = 0x00,
    AddressErrorLoad = 0x04,  // AdEL: leitura/busca desalinhada ou fora do KUSEG em modo usuário
    AddressErrorStore = 0x05, // AdES: escrita desalinhada ou fora do KUSEG em modo usuário
    BusErrorInstruction = 0x06,
    BusErrorData = 0x07,
    Syscall = 0x08,
    Breakpoint = 0x09,
    ReservedInstruction = 0x0A,
    CoprocessorUnusable = 0x0B,
    Overflow = 0x0C,
};

[[nodiscard]] std::string_view to_string(ExceptionCode code) noexcept;

struct ExceptionRecord {
    ExceptionCode code = ExceptionCode::Interrupt;
    u32 instruction_address = 0; // instrução que gerou a exceção (ou que seria executada)
    u32 epc = 0;                 // = instruction_address, ou o desvio anterior se em delay slot
    bool branch_delay = false;   // CAUSE.BD
    bool branch_taken = false;   // CAUSE.BT (só com BD)
    u32 branch_target = 0;       // TAR (só com BD e BT)
    std::optional<u32> bad_vaddr; // somente AdEL/AdES
    u8 coprocessor = 0;          // CAUSE.CE (só CpU)
};

struct UnimplementedRecord {
    u32 instruction_address = 0;
    u32 word = 0;
    int phase = 0; // fase do roadmap que implementará a instrução
};

enum class StepResult : u8 {
    Executed,      // instrução executada normalmente
    Exception,     // exceção ou interrupção atendida: o PC agora está no vetor
    Unimplemented, // instrução que depende de componente ainda inexistente (CPU parada)
    Halted,        // CPU parada; nada foi executado
};

class Cpu {
public:
    static constexpr u32 reset_vector = 0xBFC00000;
    static constexpr u32 div_latency = 36;

    explicit Cpu(bus::Bus& bus);

    Cpu(const Cpu&) = delete;
    Cpu& operator=(const Cpu&) = delete;

    // PC = BFC00000h; COP0 e I-cache reiniciados. ⚠️ GPRs, HI e LO são zerados (D-023).
    void reset();

    // Linha de interrupção de hardware (controlador I_STAT/I_MASK). Pode ser nula.
    void set_interrupt_line(const InterruptLine* line) noexcept { irq_line_ = line; }

    StepResult step();
    // Executa até `max_steps` passos ou até a CPU parar (exceções não param a execução).
    // Retorna o número de instruções executadas.
    u64 run(u64 max_steps);

    [[nodiscard]] u32 gpr(u32 index) const noexcept { return regs_[index & 31]; }
    void set_gpr(u32 index, u32 value) noexcept;
    [[nodiscard]] u32 pc() const noexcept { return pc_; }
    void set_pc(u32 address) noexcept;
    [[nodiscard]] u32 hi() const noexcept { return hi_; }
    [[nodiscard]] u32 lo() const noexcept { return lo_; }

    [[nodiscard]] Cop0& cop0() noexcept { return cop0_; }
    [[nodiscard]] const Cop0& cop0() const noexcept { return cop0_; }
    [[nodiscard]] const InstructionCache& icache() const noexcept { return icache_; }
    [[nodiscard]] bool irq_line() const noexcept { return irq_line_ && irq_line_->irq_asserted(); }

    [[nodiscard]] u64 cycles() const noexcept { return cycles_; }
    // Referência estável ao contador de ciclos: base de tempo do scheduler (Fase 5).
    [[nodiscard]] const u64& cycle_counter() const noexcept { return cycles_; }
    [[nodiscard]] u64 instructions() const noexcept { return instructions_; }
    [[nodiscard]] u64 exceptions_taken() const noexcept { return exceptions_; }

    [[nodiscard]] bool halted() const noexcept { return halted_; }
    [[nodiscard]] const std::optional<ExceptionRecord>& last_exception() const noexcept {
        return last_exception_;
    }
    [[nodiscard]] const std::optional<UnimplementedRecord>& last_unimplemented() const noexcept {
        return last_unimplemented_;
    }

    [[nodiscard]] static u32 mult_latency(u32 rs, bool is_signed) noexcept;

private:
    struct PendingLoad {
        u32 reg = 0; // 0 = nenhuma
        u32 value = 0;
    };

    enum class Flow : u8 { Continue, Exception, Unimplemented };

    Flow fetch_and_execute();
    Flow execute(Instruction instr);
    Flow execute_special(Instruction instr);
    Flow execute_cop0(Instruction instr);
    Flow execute_coprocessor(Instruction instr);
    Flow execute_load(Instruction instr);
    Flow execute_store(Instruction instr);

    // Acesso a dados: alinhamento, modo usuário, isolamento de cache e barramento.
    Flow load_data(bus::AccessWidth width, u32 address, u32& value);
    Flow store_data(bus::AccessWidth width, u32 address, u32 value);
    [[nodiscard]] bool isolated(u32 address) const noexcept;
    [[nodiscard]] u32 bcc() noexcept { return bus_.memory_control().cache_control(); }

    void write_reg(u32 index, u32 value) noexcept;
    void schedule_load(u32 index, u32 value) noexcept;
    [[nodiscard]] u32 merge_source(u32 index) const noexcept;

    void branch(u32 target) noexcept;
    void mark_delay_slot() noexcept;
    void wait_hilo() noexcept;

    Flow raise(ExceptionCode code, std::optional<u32> bad_vaddr = std::nullopt);
    Flow raise_unusable(u32 coprocessor);
    Flow unimplemented(Instruction instr, int phase);
    Flow fault_from_bus(bus::BusFault fault, bool store, u32 address);

    bus::Bus& bus_;
    const InterruptLine* irq_line_ = nullptr;
    Cop0 cop0_;
    InstructionCache icache_;

    std::array<u32, 32> regs_{};
    u32 hi_ = 0;
    u32 lo_ = 0;
    u64 hilo_ready_cycle_ = 0;

    u32 pc_ = reset_vector;
    u32 next_pc_ = reset_vector + 4;

    bool next_in_delay_slot_ = false;
    bool next_branch_taken_ = false;
    u32 next_branch_target_ = 0;

    u32 current_pc_ = 0;
    bool current_in_delay_slot_ = false;
    bool current_branch_taken_ = false;
    u32 current_branch_target_ = 0;
    PendingLoad incoming_load_{};
    PendingLoad pending_load_{};
    u8 pending_coprocessor_ = 0;

    u64 cycles_ = 0;
    u64 instructions_ = 0;
    u64 exceptions_ = 0;

    bool halted_ = false;
    std::optional<ExceptionRecord> last_exception_;
    std::optional<UnimplementedRecord> last_unimplemented_;
};

} // namespace rpcs1::cpu
