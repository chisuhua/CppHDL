#include "catch_amalgamated.hpp"

#include "bv/bitvector.h"
#include <cstdint>
#include <random>

// 假设 bv_cmp, extract_bits, bitwidth_v, bv_is_neg 在 ch::internal 中
using namespace ch::internal;


// ---------- Tests ----------

TEST_CASE("bv_cmp: scalar unsigned", "[bv_cmp][scalar][unsigned]") {
    uint32_t a = 0x12345678;
    uint32_t b = 0x12345679;

    REQUIRE(bv_cmp<false>(&a, 0, &b, 0, 32) == -1);
    REQUIRE(bv_cmp<false>(&a, 4, &b, 4, 28) == 0); // high 28 bits are equal!
    REQUIRE(bv_cmp<false>(&a, 0, &a, 0, 32) == 0);

    // Real unaligned unequal test
    uint32_t c = 0x12345688;
    REQUIRE(bv_cmp<false>(&a, 4, &c, 4, 28) == -1);
}

TEST_CASE("bv_cmp: scalar signed", "[bv_cmp][scalar][signed]") {
    uint32_t neg = 0xFFFFFFFF; // -1 (32-bit)
    uint32_t pos = 0x00000001; // +1
    REQUIRE(bv_cmp<true>(&neg, 0, &pos, 0, 32) == -1);
    REQUIRE(bv_cmp<true>(&pos, 0, &neg, 0, 32) == 1);

    // 8-bit signed in 32-bit word
    uint32_t a8 = 0x000000FF; // -1 as 8-bit
    uint32_t b8 = 0x00000001; // +1 as 8-bit
    REQUIRE(bv_cmp<true>(&a8, 0, &b8, 0, 8) == -1);
}

TEST_CASE("bv_cmp: vector unsigned", "[bv_cmp][vector][unsigned]") {
    uint32_t x[] = {0x11111111, 0x22222222};
    uint32_t y[] = {0x11111111, 0x22222223};
    REQUIRE(bv_cmp<false>(x, 0, y, 0, 64) == -1);
    REQUIRE(bv_cmp<false>(x, 0, x, 0, 64) == 0);
    REQUIRE(bv_cmp<false>(x, 4, y, 4, 60) == -1);
}

TEST_CASE("bv_cmp: vector signed", "[bv_cmp][vector][signed]") {
    uint32_t neg64[] = {0xFFFFFFFF, 0xFFFFFFFF}; // -1 (64-bit)
    uint32_t pos64[] = {0x00000001, 0x00000000}; // +1
    REQUIRE(bv_cmp<true>(neg64, 0, pos64, 0, 64) == -1);

    uint32_t big_neg[] = {0x00000000, 0x80000000}; // MSB=1 → negative
    uint32_t small_pos[] = {0xFFFFFFFF, 0x7FFFFFFF}; // MSB=0 → positive
    REQUIRE(bv_cmp<true>(big_neg, 0, small_pos, 0, 64) == -1);
}

TEST_CASE("bv_cmp: edge cases", "[bv_cmp][edge]") {
    uint32_t zero = 0;
    uint32_t ones = 0xFFFFFFFF;

    // Zero length
    REQUIRE(bv_cmp<false>(&zero, 0, &ones, 0, 0) == 0);
    REQUIRE(bv_cmp<true>(&zero, 0, &ones, 0, 0) == 0);

    // Single bit
    REQUIRE(bv_cmp<false>(&zero, 0, &ones, 0, 1) == -1); // 0 < 1 (unsigned)
    REQUIRE(bv_cmp<true>(&zero, 31, &ones, 31, 1) == 1);  // +0 > -1 (signed)

    // Cross-word extraction
    uint32_t data[] = {0xFFFFFFFF, 0x00000001};
    uint32_t expected = 0xFFFFFFFF;
    uint32_t extracted = extract_bits(data, 1, 32);
    REQUIRE(extracted == expected);
}

TEST_CASE("bv_cmp: random unsigned", "[bv_cmp][random][unsigned]") {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<uint64_t> dist;

    for (int trial = 0; trial < 1000; ++trial) {
        uint64_t a = dist(rng);
        uint64_t b = dist(rng);
        int expected = (a < b) ? -1 : (a > b) ? 1 : 0;
        int actual = bv_cmp<false>(
            reinterpret_cast<const uint32_t*>(&a), 0,
            reinterpret_cast<const uint32_t*>(&b), 0,
            64
        );
        CAPTURE(a, b, expected, actual); // 在失败时打印变量
        REQUIRE(actual == expected);
    }
}
