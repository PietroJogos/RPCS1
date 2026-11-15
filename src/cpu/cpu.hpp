// Interpretador da CPU MIPS R3000A do PlayStation (Fase 3).
//
// Implementa o conjunto de instruções sem coprocessadores: ALU, deslocamentos, loads e
// stores (inclusive LWL/LWR/SWL/SWR), desvios e saltos com delay slot, load delay slot,
// MULT/DIV com HI/LO e sua latência, SYSCALL/BREAK e detecção de todas as exceções
// geradas por essas instruções.
//
// Limites desta fase (documentados em docs/PROGRESS.md):
//   - Exceções são DETECTADAS e registradas com EPC/BD/BT/BadVaddr corretos, mas o desvio
//     para o vetor de exceção, SR/CAUSE/EPC no COP0 e RFE pertencem à Fase 4. Até lá a CPU
//     para (`halted()`) na instrução que gerou a exceção.
//   - Instruções de coprocessador (COP0–3, LWCn, SWCn) param a CPU como "não implementadas"
//     (COP0: Fase 4; COP2/GTE: Fase 9).
//   - A contagem de ciclos considera 1 ciclo por instrução + bloqueios de MFHI/MFLO.
//     Tempos de acesso à memória, I-cache e fila de escrita entram na Fase 5.
#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "bus/bus.hpp"
#include "core/types.hpp"
#include "cpu/instruction.hpp"

namespace rpcs1::cpu {

// Códigos de exceção (CAUSE.ExcCode).
enum class ExceptionCode : u8 {
    Interrupt = 0x00,
    AddressErrorLoad = 0x04,  // AdEL: leitura/busca desalinhada
    AddressErrorStore = 0x05, // AdES: escrita desalinhada
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
    u32 instruction_address = 0; // endereço da instrução que gerou a exceção
    u32 epc = 0;                 // = instruction_address, ou o desvio anterior se em delay slot
    bool branch_delay = false;   // CAUSE.BD
    bool branch_taken = false;   // CAUSE.BT (só com BD)
    u32 branch_target = 0;       // COP0 TAR (só com BD)
    std::optional<u32> bad_vaddr; // somente AdEL/AdES
};

struct UnimplementedRecord {
    u32 instruction_address = 0;
    u32 word = 0;
    int phase = 0; // fase do roadmap que implementará a instrução
};

enum class StepResult : u8 {
    Executed,      // instrução executada normalmente
    Exception,     // exceção detectada (ver last_exception())
    Unimplemented, // instrução que depende de componente ainda inexistente
    Halted,        // CPU parada; nada foi executado
};

class Cpu {
public:
    static constexpr u32 reset_vector = 0xBFC00000;

    // Latências (psx-spx, "Multiply/divide"): MULT/MULTU dependem apenas de rs.
    static constexpr u32 div_latency = 36;

    explicit Cpu(bus::Bus& bus);

    Cpu(const Cpu&) = delete;
    Cpu& operator=(const Cpu&) = delete;

    // PC = BFC00000h. ⚠️ GPRs, HI e LO após o reset não são documentados: o RPCS1 os zera.
    void reset();

    StepResult step();
    // Executa até `max_instructions` instruções ou até parar. Retorna o número executado.
    u64 run(u64 max_instructions);

    [[nodiscard]] u32 gpr(u32 index) const noexcept { return regs_[index & 31]; }
    void set_gpr(u32 index, u32 value) noexcept;
    [[nodiscard]] u32 pc() const noexcept { return pc_; }
    // Define o PC da próxima instrução (fora de qualquer delay slot).
    void set_pc(u32 address) noexcept;
    [[nodiscard]] u32 hi() const noexcept { return hi_; }
    [[nodiscard]] u32 lo() const noexcept { return lo_; }

    [[nodiscard]] u64 cycles() const noexcept { return cycles_; }
    [[nodiscard]] u64 instructions() const noexcept { return instructions_; }

    [[nodiscard]] bool halted() const noexcept { return halted_; }
    [[nodiscard]] const std::optional<ExceptionRecord>& last_exception() const noexcept {
        return last_exception_;
    }
    [[nodiscard]] const std::optional<UnimplementedRecord>& last_unimplemented() const noexcept {
        return last_unimplemented_;
    }

    // Latência de MULT/MULTU para um dado rs (6, 9 ou 13 ciclos).
    [[nodiscard]] static u32 mult_latency(u32 rs, bool is_signed) noexcept;

private:
    struct PendingLoad {
        u32 reg = 0; // 0 = nenhuma
        u32 value = 0;
    };

    enum class Flow : u8 { Continue, Exception, Unimplemented };

    Flow execute(Instruction instr);
    Flow execute_special(Instruction instr);
    Flow execute_load(Instruction instr);
    Flow execute_store(Instruction instr);

    // Escrita imediata (ALU, link): cancela uma carga pendente do mesmo registrador.
    void write_reg(u32 index, u32 value) noexcept;
    // Carga: o valor só fica visível após a próxima instrução (load delay slot).
    void schedule_load(u32 index, u32 value) noexcept;
    // Valor de rt visto por LWL/LWR: inclui uma carga pendente do mesmo registrador.
    [[nodiscard]] u32 merge_source(u32 index) const noexcept;

    void branch(u32 target) noexcept;
    void mark_delay_slot() noexcept;
    void wait_hilo() noexcept;

    Flow raise(ExceptionCode code, std::optional<u32> bad_vaddr = std::nullopt);
    Flow unimplemented(Instruction instr, int phase);
    Flow fault_from_bus(bus::BusFault fault, bool store, u32 address);

    bus::Bus& bus_;

    std::array<u32, 32> regs_{};
    u32 hi_ = 0;
    u32 lo_ = 0;
    u64 hilo_ready_cycle_ = 0;

    u32 pc_ = reset_vector;       // instrução a executar agora
    u32 next_pc_ = reset_vector + 4;

    // Delay slot: a instrução em pc_ está no delay slot de um desvio?
    bool next_in_delay_slot_ = false;
    bool next_branch_taken_ = false;
    u32 next_branch_target_ = 0;

    // Estado da instrução em execução.
    u32 current_pc_ = 0;
    bool current_in_delay_slot_ = false;
    bool current_branch_taken_ = false;
    u32 current_branch_target_ = 0;
    PendingLoad incoming_load_{}; // carga da instrução anterior (aplicada ao fim desta)
    PendingLoad pending_load_{};  // carga desta instrução (aplicada ao fim da próxima)

    u64 cycles_ = 0;
    u64 instructions_ = 0;

    bool halted_ = false;
    std::optional<ExceptionRecord> last_exception_;
    std::optional<UnimplementedRecord> last_unimplemented_;
};

} // namespace rpcs1::cpu
