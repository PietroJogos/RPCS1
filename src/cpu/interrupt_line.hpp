// Linha de interrupção de hardware que chega à CPU (CAUSE bit 10).
#pragma once

namespace rpcs1::cpu {

class InterruptLine {
public:
    virtual ~InterruptLine() = default;
    // Verdadeiro enquanto (I_STAT AND I_MASK) != 0. Não é um latch.
    [[nodiscard]] virtual bool irq_asserted() const noexcept = 0;
};

} // namespace rpcs1::cpu
