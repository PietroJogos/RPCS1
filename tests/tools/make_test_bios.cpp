// Gera uma BIOS SINTÉTICA de 512 KiB para testes de fumaça. Não contém nenhum código
// da Sony: é um programa mínimo montado pelo codificador de testes do RPCS1.
//
// Programa (em BFC00000h; após o reset SR.BEV = 1, então exceções vão para BFC00180h):
//   POST = 7; SYSCALL → tratador (POST = 8, retorna para EPC + 4 com RFE);
//   POST = 9; liga CU2 e executa um comando da GTE — que só existirá na Fase 9.
// Instruções executadas: 4 + 7 (tratador) + 5 = 16, com 1 exceção.
#include <cstdio>
#include <fstream>
#include <vector>

#include "cpu/mips_assembler.hpp"

using namespace rpcs1;
using namespace rpcs1::test::asm_;

namespace {

void place(std::vector<u8>& image, u32 offset, const std::vector<u32>& words) {
    for (usize i = 0; i < words.size(); ++i) {
        for (usize k = 0; k < 4; ++k) {
            image[offset + i * 4 + k] = static_cast<u8>(words[i] >> (8 * k));
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "uso: rpcs1_make_test_bios <saida.bin>\n");
        return 2;
    }
    std::vector<u8> image(512 * 1024, 0);
    place(image, 0x000, {
        lui(t0, 0x1F80),
        ori(t0, t0, 0x2041),
        addiu(t1, zero, 7),
        sb(t1, 0, t0),      // POST = 7
        syscall(),          // → BFC00180h
        addiu(t1, zero, 9),
        sb(t1, 0, t0),      // POST = 9
        lui(t2, 0x4000),    // SR.CU2
        mtc0(t2, c0_sr),
        nop(),
        gte_rtps(),         // limite atual: GTE (Fase 9)
    });
    place(image, 0x180, {   // tratador de exceção (BEV = 1)
        addiu(t1, zero, 8),
        sb(t1, 0, t0),      // POST = 8
        mfc0(k0, c0_epc),
        nop(),
        addiu(k0, k0, 4),
        jr(k0),
        rfe(),
    });
    std::ofstream out(argv[1], std::ios::binary);
    out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    return out ? 0 : 1;
}
