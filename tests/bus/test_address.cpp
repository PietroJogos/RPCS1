#include "bus/address.hpp"
#include "framework/rpcs1_test.hpp"

using namespace rpcs1;
using namespace rpcs1::bus;

TEST_CASE("address/segmentos nas fronteiras") {
    CHECK(segment_of(0x00000000) == Segment::Kuseg);
    CHECK(segment_of(0x7FFFFFFF) == Segment::Kuseg);
    CHECK(segment_of(0x80000000) == Segment::Kseg0);
    CHECK(segment_of(0x9FFFFFFF) == Segment::Kseg0);
    CHECK(segment_of(0xA0000000) == Segment::Kseg1);
    CHECK(segment_of(0xBFFFFFFF) == Segment::Kseg1);
    CHECK(segment_of(0xC0000000) == Segment::Kseg2);
    CHECK(segment_of(0xFFFFFFFF) == Segment::Kseg2);
}

TEST_CASE("address/tradução para endereço físico") {
    // Os três aliases do vetor de reset e do início da RAM.
    CHECK_EQ(to_physical(0xBFC00000), u32{0x1FC00000});
    CHECK_EQ(to_physical(0x9FC00000), u32{0x1FC00000});
    CHECK_EQ(to_physical(0x1FC00000), u32{0x1FC00000});
    CHECK_EQ(to_physical(0x80000080), u32{0x00000080});
    CHECK_EQ(to_physical(0xA0000080), u32{0x00000080});
    CHECK_EQ(to_physical(0x9F801070), u32{0x1F801070});
    CHECK_EQ(to_physical(0xBFFFFFFF), u32{0x1FFFFFFF});
    // KSEG2 não é traduzido.
    CHECK_EQ(to_physical(0xFFFE0130), u32{0xFFFE0130});
    // Propriedade: KSEG0 e KSEG1 sempre caem nos 512 MiB físicos.
    for (u32 offset = 0; offset < 0x20000000u; offset += 0x00F00001u) {
        CHECK_EQ(to_physical(0x80000000u + offset), offset);
        CHECK_EQ(to_physical(0xA0000000u + offset), offset);
    }
}

TEST_CASE("address/cache somente em KUSEG e KSEG0") {
    CHECK(is_cached(0x00001000));
    CHECK(is_cached(0x80001000));
    CHECK(!is_cached(0xA0001000));
    CHECK(!is_cached(0xFFFE0130));
}

TEST_CASE("address/alinhamento por largura") {
    for (u32 a = 0; a < 8; ++a) {
        CHECK(is_aligned(a, AccessWidth::Byte));
        CHECK_EQ(is_aligned(a, AccessWidth::Half), (a % 2) == 0);
        CHECK_EQ(is_aligned(a, AccessWidth::Word), (a % 4) == 0);
    }
    CHECK_EQ(width_mask(AccessWidth::Byte), u32{0xFF});
    CHECK_EQ(width_mask(AccessWidth::Half), u32{0xFFFF});
    CHECK_EQ(width_mask(AccessWidth::Word), u32{0xFFFFFFFF});
}
