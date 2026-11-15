// Ambiente para testes da CPU: barramento real + CPU, com programa carregado na RAM.
#pragma once

#include <stdexcept>
#include <vector>

#include "bus/bus.hpp"
#include "cpu/cpu.hpp"
#include "framework/rpcs1_test.hpp"
#include "mips_assembler.hpp"

namespace rpcs1::test {

class Machine {
public:
    static constexpr u32 code_base = 0x80010000; // KSEG0, como os PS-EXE
    static constexpr u32 data_base = 0x80002000;

    explicit Machine(const std::vector<u32>& program, u32 at = code_base) {
        load(program, at);
        cpu.set_pc(at);
    }

    void load(const std::vector<u32>& words, u32 at) {
        for (usize i = 0; i < words.size(); ++i) {
            if (bus.write32(at + static_cast<u32>(i * 4), words[i]) != bus::BusFault::None) {
                throw std::runtime_error("falha ao carregar programa de teste");
            }
        }
    }

    // Executa `count` passos; falha o teste se algum não for uma execução normal.
    void steps(int count) {
        for (int i = 0; i < count; ++i) {
            const auto r = cpu.step();
            if (r != cpu::StepResult::Executed) {
                report_failure(__FILE__, __LINE__,
                               "passo " + std::to_string(i) + " não executou normalmente");
                throw RequireFailure{};
            }
        }
    }

    // Executa até a CPU parar (exceção ou não implementada) ou atingir o limite.
    cpu::StepResult run_until_stop(int limit = 100000) {
        for (int i = 0; i < limit; ++i) {
            const auto r = cpu.step();
            if (r != cpu::StepResult::Executed) {
                return r;
            }
        }
        return cpu::StepResult::Executed;
    }

    [[nodiscard]] u32 reg(u32 index) const { return cpu.gpr(index); }
    [[nodiscard]] u32 word(u32 address) { return bus.read32(address).value; }
    void poke32(u32 address, u32 value) {
        if (bus.write32(address, value) != bus::BusFault::None) {
            throw std::runtime_error("falha ao escrever dado de teste");
        }
    }

    bus::Bus bus;
    cpu::Cpu cpu{bus};
};

} // namespace rpcs1::test
