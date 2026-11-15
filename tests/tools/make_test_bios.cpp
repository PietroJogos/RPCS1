// Gera uma BIOS SINTÉTICA de 512 KiB para testes de fumaça. Não contém nenhum código
// da Sony: são programas mínimos montados pelo codificador de testes do RPCS1.
//
// Programa padrão (em BFC00000h; após o reset SR.BEV = 1, então exceções vão para BFC00180h):
//   POST = 7; SYSCALL → tratador (POST = 8, retorna para EPC + 4 com RFE);
//   POST = 9; liga CU2 e executa um comando da GTE — que só existirá na Fase 9.
// Instruções executadas: 4 + 7 (tratador) + 5 = 16, com 1 exceção.
//
// Programa --gpu (Fase 7): configura a exibição 320×240 NTSC de 15 bits pela GP1, preenche
// retângulos coloridos com GP0(02h), envia um preenchimento por DMA (canal 2, lista
// encadeada na RAM), desenha um triângulo amarelo, transfere 4×2 pixels da CPU para a VRAM
// (A0h), copia-os dentro da VRAM (80h), liga a exibição e fica em laço.
//
// Programa --gpu-draw (Fase 8): triângulo gouraud com dithering, triângulo plano, textura de
// 4 bits com CLUT enviada pela CPU e usada num quadrilátero modulado e num sprite 16×16 cru,
// retângulo semitransparente (B/2 + F/2), polilinha, linha gouraud, quadrilátero aditivo
// (B + F) e triângulo deslocado por GP0(E5h).
//
// Nos dois: POST = 1 no início e 2 ao final. O conteúdo esperado das imagens está em
// tests/smoke/gpu_frame.cmake e tests/smoke/gpu_draw.cmake.
#include <cstdio>
#include <cstring>
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

void default_program(std::vector<u8>& image) {
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
}

class Program {
public:
    void li(u32 reg, u32 value) {
        code.push_back(lui(reg, value >> 16));
        code.push_back(ori(reg, reg, value & 0xFFFFu));
    }
    void gp0(u32 value) { li(t2, value); code.push_back(sw(t2, 0, t0)); }
    void gp1(u32 value) { li(t2, value); code.push_back(sw(t2, 0, t1)); }
    void store(u32 address, u32 value) {
        li(t3, address);
        li(t2, value);
        code.push_back(sw(t2, 0, t3));
    }
    void post(u32 value) {
        li(t3, 0x1F802041);
        code.push_back(addiu(t2, zero, static_cast<s32>(value)));
        code.push_back(sb(t2, 0, t3));
    }
    std::vector<u32> code;
};

void gpu_program(std::vector<u8>& image) {
    Program p;
    p.li(t0, 0x1F801810); // GP0
    p.li(t1, 0x1F801814); // GP1
    p.post(1);

    p.gp1(0x00000000); // reset da GPU
    p.gp1(0x08000001); // 320 pontos, NTSC, 15 bits, sem entrelaçamento
    p.gp1(0x06C60260); // X1 = 260h, X2 = C60h (2560 ciclos de vídeo = 320 × 8)
    p.gp1(0x07040010); // Y1 = 10h, Y2 = 100h (240 linhas)
    p.gp1(0x05000000); // início da exibição na VRAM: (0, 0)
    p.gp0(0xE3000000); // área de desenho (0,0)–(319,239)
    p.gp0(0xE403BD3F);

    p.gp0(0x02400000); p.gp0(0x00000000); p.gp0(0x00F00140); // fundo azul-escuro 320×240
    p.gp0(0x020000FF); p.gp0(0x00100010); p.gp0(0x00400060); // vermelho (16,16) 96×64
    p.gp0(0x0200FF00); p.gp0(0x00100070); p.gp0(0x00400060); // verde (112,16)
    p.gp0(0x02FF0000); p.gp0(0x001000D0); p.gp0(0x00400060); // azul (208,16)

    // Lista encadeada na RAM (00001000h): 1 nó com um preenchimento branco (16,96) 288×16.
    p.store(0x00001000, 0x03FFFFFF);
    p.store(0x00001004, 0x02FFFFFF);
    p.store(0x00001008, 0x00600010);
    p.store(0x0000100C, 0x00100120);
    p.gp1(0x04000002);                 // DMA: CPU → GP0
    p.li(t3, 0x1F8010F0);              // DPCR |= 800h (habilita o canal 2)
    p.code.push_back(lw(t4, 0, t3));
    p.code.push_back(nop());
    p.code.push_back(ori(t4, t4, 0x0800));
    p.code.push_back(sw(t4, 0, t3));
    p.store(0x1F8010A0, 0x00001000);   // MADR2
    p.store(0x1F8010A4, 0x00000000);   // BCR2
    p.store(0x1F8010A8, 0x01000401);   // CHCR2: para o dispositivo, lista, iniciar

    // Triângulo amarelo (120,184) (300,184) (120,232); o preenchimento seguinte confere que o
    // FIFO continua alinhado.
    p.gp0(0x2000FFFF); p.gp0(0x00B80078); p.gp0(0x00B8012C); p.gp0(0x00E80078);
    p.gp0(0x02808080); p.gp0(0x00800010); p.gp0(0x00300120); // cinza (16,128) 288×48

    // CPU → VRAM: 4×2 pixels em (32,176).
    p.gp0(0xA0000000); p.gp0(0x00B00020); p.gp0(0x00020004);
    p.gp0(0x03E0001F); // vermelho, verde
    p.gp0(0x7FFF7C00); // azul, branco
    p.gp0(0x7FE003FF); // amarelo, ciano
    p.gp0(0x00007C1F); // magenta, preto
    // VRAM → VRAM: copia o bloco para (64,176).
    p.gp0(0x80000000); p.gp0(0x00B00020); p.gp0(0x00B00040); p.gp0(0x00020004);

    p.gp1(0x03000000); // liga a exibição
    p.post(2);
    p.code.push_back(beq(zero, zero, -1)); // laço infinito
    p.code.push_back(nop());
    place(image, 0x000, p.code);
}

constexpr u32 xy(u32 x, u32 y) { return (y << 16) | x; }

void gpu_draw_program(std::vector<u8>& image) {
    Program p;
    p.li(t0, 0x1F801810); // GP0
    p.li(t1, 0x1F801814); // GP1
    p.post(1);

    p.gp1(0x00000000);
    p.gp1(0x08000001); // 320×240 NTSC, 15 bits
    p.gp1(0x06C60260);
    p.gp1(0x07040010);
    p.gp1(0x05000000);
    p.gp0(0xE3000000); // área de desenho (0,0)–(319,239)
    p.gp0(0xE403BD3F);
    p.gp0(0xE5000000); // sem deslocamento
    p.gp0(0xE100020A); // dithering ligado; página de textura X = 640, 4 bits; B/2 + F/2

    p.gp0(0x02302010); p.gp0(xy(0, 0)); p.gp0(xy(320, 240)); // fundo

    // Triângulo gouraud: vermelho, verde, azul (com dithering).
    p.gp0(0x300000FF); p.gp0(xy(10, 10));
    p.gp0(0x0000FF00); p.gp0(xy(150, 10));
    p.gp0(0x00FF0000); p.gp0(xy(10, 130));
    // Triângulo plano amarelo (sem dithering: plano).
    p.gp0(0x2000FFFF); p.gp0(xy(160, 140)); p.gp0(xy(230, 140)); p.gp0(xy(160, 200));

    // CLUT em (0,480): 0 = transparente, 1 = branco, 2 = laranja (R 31, G 16).
    p.gp0(0xA0000000); p.gp0(xy(0, 480)); p.gp0(xy(16, 1));
    p.gp0(0x7FFF0000); p.gp0(0x0000021F);
    for (int i = 0; i < 6; ++i) p.gp0(0);
    // Textura 16×16 de 4 bits em (640,0): xadrez de células 4×4 (índices 1 e 2).
    p.gp0(0xA0000000); p.gp0(xy(640, 0)); p.gp0(xy(4, 16));
    for (u32 row = 0; row < 16; ++row) {
        const u32 word = ((row / 4) % 2 == 0) ? 0x22221111u : 0x11112222u;
        p.gp0(word);
        p.gp0(word);
    }
    // Quadrilátero texturizado modulado por 808080h (96×96, UV 0..15).
    p.gp0(0x2C808080);
    p.gp0(xy(170, 10)); p.gp0((0x7800u << 16) | 0x0000);
    p.gp0(xy(266, 10)); p.gp0((0x000Au << 16) | 0x000F); // página: X 640, 4 bits
    p.gp0(xy(170, 106)); p.gp0(0x0F00);
    p.gp0(xy(266, 106)); p.gp0(0x0F0F);
    // Sprite 16×16 cru com a mesma textura (página de E1h).
    p.gp0(0x7D000000); p.gp0(xy(280, 10)); p.gp0((0x7800u << 16) | 0x0000);

    // Retângulo semitransparente azul (B/2 + F/2) sobre o triângulo amarelo e o fundo.
    p.gp0(0x62FF0000); p.gp0(xy(180, 150)); p.gp0(xy(100, 60));

    // Polilinha branca (caixa) e linha gouraud vermelho → azul.
    p.gp0(0x48FFFFFF); p.gp0(xy(10, 150)); p.gp0(xy(140, 150)); p.gp0(xy(140, 230));
    p.gp0(xy(10, 230)); p.gp0(xy(10, 150)); p.gp0(0x55555555);
    p.gp0(0x500000FF); p.gp0(xy(20, 160)); p.gp0(0x00FF0000); p.gp0(xy(130, 220));

    // Quadrilátero aditivo (B + F) verde-escuro dentro da caixa.
    p.gp0(0xE100022A);
    p.gp0(0x2A004000); p.gp0(xy(20, 170)); p.gp0(xy(120, 170)); p.gp0(xy(20, 210));
    p.gp0(xy(120, 210));

    // Triângulo ciano em (190,215) deslocado +60 em X por E5h.
    p.gp0(0xE500003C);
    p.gp0(0x20FFFF00); p.gp0(xy(190, 215)); p.gp0(xy(220, 215)); p.gp0(xy(190, 235));
    p.gp0(0xE5000000);

    p.gp1(0x03000000); // liga a exibição
    p.post(2);
    p.code.push_back(beq(zero, zero, -1));
    p.code.push_back(nop());
    place(image, 0x000, p.code);
}

} // namespace

int main(int argc, char** argv) {
    const bool gpu = argc == 3 && std::strcmp(argv[1], "--gpu") == 0;
    const bool gpu_draw = argc == 3 && std::strcmp(argv[1], "--gpu-draw") == 0;
    if (argc != 2 && !gpu && !gpu_draw) {
        std::fprintf(stderr, "uso: rpcs1_make_test_bios [--gpu | --gpu-draw] <saida.bin>\n");
        return 2;
    }
    std::vector<u8> image(512 * 1024, 0);
    if (gpu) {
        gpu_program(image);
    } else if (gpu_draw) {
        gpu_draw_program(image);
    } else {
        default_program(image);
    }
    std::ofstream out(argv[argc - 1], std::ios::binary);
    out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    return out ? 0 : 1;
}
