// Gera uma BIOS SINTÉTICA de 512 KiB para testes de fumaça. Não contém nenhum código
// da Sony: é um programa mínimo montado pelo codificador de testes do RPCS1.
//
// Programa (em BFC00000h):
//   escreve 7 no display POST (1F802041h), executa um laço de 100 iterações e então
//   chega a uma instrução MFC0 — que a CPU da Fase 3 ainda não implementa (Fase 4).
// Instruções executadas antes do MFC0: 5 + 100 * 3 = 305.
#include <cstdio>
#include <fstream>
#include <vector>

#include "cpu/mips_assembler.hpp"

using namespace rpcs1;
using namespace rpcs1::test::asm_;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "uso: rpcs1_make_test_bios <saida.bin>\n");
        return 2;
    }
    const std::vector<u32> program = {
        lui(t0, 0x1F80),
        ori(t0, t0, 0x2041),
        addiu(t1, zero, 7),
        sb(t1, 0, t0),        // POST = 7
        addiu(t2, zero, 100),
        addiu(t2, t2, -1),    // laço:
        bne(t2, zero, -2),
        nop(),
        mfc0(t3, 12),         // limite da Fase 3
    };
    std::vector<u8> image(512 * 1024, 0);
    for (usize i = 0; i < program.size(); ++i) {
        for (usize k = 0; k < 4; ++k) {
            image[i * 4 + k] = static_cast<u8>(program[i] >> (8 * k));
        }
    }
    std::ofstream out(argv[1], std::ios::binary);
    out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    return out ? 0 : 1;
}
