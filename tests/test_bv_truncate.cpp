// tests/test_bitvector_operations.cpp
#include "catch_amalgamated.hpp"

#include "bv/bitvector.h"
#include <cstdint>
#include <string>
#include <sstream> // For std::ostringstream if needed for to_bitstring helper

using namespace ch::internal;

// --- Unit Tests for Truncating Operations ---

TEST_CASE("bitvector: Truncating Addition", "[truncation][arithmetic]") {
    constexpr uint32_t WIDE_WIDTH = 33;
    constexpr uint32_t NARROW_WIDTH = 4;
    bitvector<uint64_t> wide_bv1(WIDE_WIDTH, uint64_t(15)); // 0b1111
    bitvector<uint64_t> wide_bv2(WIDE_WIDTH, uint64_t(1));  // 0b0001
    bitvector<uint64_t> narrow_dst(NARROW_WIDTH);           // 0b0000

    REQUIRE(wide_bv1.size() == WIDE_WIDTH);
    REQUIRE(wide_bv2.size() == WIDE_WIDTH);
    REQUIRE(narrow_dst.size() == NARROW_WIDTH);
    REQUIRE(static_cast<uint64_t>(wide_bv1) == 15);
    REQUIRE(static_cast<uint64_t>(wide_bv2) == 1);
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0);

    // Perform truncating addition: narrow_dst = wide_bv1 + wide_bv2
    ch::internal::bv_add_truncate<uint64_t>(&narrow_dst, &wide_bv1, &wide_bv2);

    // Verify result
    REQUIRE(narrow_dst.size() == NARROW_WIDTH); // Size unchanged
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0); // 15 + 1 = 16, truncated to 4 bits (16 % 16 = 0)
    INFO("Result bitstring: " << to_bitstring(narrow_dst));
}

TEST_CASE("bitvector: Truncating Subtraction", "[truncation][arithmetic]") {
    constexpr uint32_t WIDE_WIDTH = 33;
    constexpr uint32_t NARROW_WIDTH = 4;
    bitvector<uint64_t> wide_bv1(WIDE_WIDTH, uint64_t(15)); // 0b1111
    bitvector<uint64_t> wide_bv2(WIDE_WIDTH, uint64_t(1));  // 0b0001
    bitvector<uint64_t> narrow_dst(NARROW_WIDTH);           // 0b0000

    REQUIRE(wide_bv1.size() == WIDE_WIDTH);
    REQUIRE(wide_bv2.size() == WIDE_WIDTH);
    REQUIRE(narrow_dst.size() == NARROW_WIDTH);
    REQUIRE(static_cast<uint64_t>(wide_bv1) == 15);
    REQUIRE(static_cast<uint64_t>(wide_bv2) == 1);
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0);

    // Perform truncating subtraction: narrow_dst = wide_bv1 - wide_bv2
    ch::internal::bv_sub_truncate<uint64_t>(&narrow_dst, &wide_bv1, &wide_bv2);

    // Verify result
    REQUIRE(narrow_dst.size() == NARROW_WIDTH); // Size unchanged
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 14); // 15 - 1 = 14, fits in 4 bits
    INFO("Result bitstring: " << to_bitstring(narrow_dst));
}

TEST_CASE("bitvector: Truncating Multiplication", "[truncation][arithmetic]") {
    constexpr uint32_t WIDE_WIDTH = 33;
    constexpr uint32_t NARROW_WIDTH = 4;
    bitvector<uint64_t> wide_bv1(WIDE_WIDTH, uint64_t(3)); // 0b0011
    bitvector<uint64_t> wide_bv2(WIDE_WIDTH, uint64_t(5)); // 0b0101
    bitvector<uint64_t> narrow_dst(NARROW_WIDTH);          // 0b0000

    REQUIRE(wide_bv1.size() == WIDE_WIDTH);
    REQUIRE(wide_bv2.size() == WIDE_WIDTH);
    REQUIRE(narrow_dst.size() == NARROW_WIDTH);
    REQUIRE(static_cast<uint64_t>(wide_bv1) == 3);
    REQUIRE(static_cast<uint64_t>(wide_bv2) == 5);
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0);

    // Perform truncating multiplication: narrow_dst = wide_bv1 * wide_bv2
    ch::internal::bv_mul_truncate<uint64_t>(&narrow_dst, &wide_bv1, &wide_bv2);

    // Verify result
    REQUIRE(narrow_dst.size() == NARROW_WIDTH); // Size unchanged
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 15); // 3 * 5 = 15, fits in 4 bits
    INFO("Result bitstring: " << to_bitstring(narrow_dst));
}

TEST_CASE("bitvector: Truncating Bitwise AND", "[truncation][bitwise]") {
    constexpr uint32_t WIDE_WIDTH = 33;
    constexpr uint32_t NARROW_WIDTH = 4;
    bitvector<uint64_t> wide_bv1(WIDE_WIDTH, uint64_t(0b11110)); // 0b11110 (30)
    bitvector<uint64_t> wide_bv2(WIDE_WIDTH, uint64_t(0b01010)); // 0b01010 (10)
    bitvector<uint64_t> narrow_dst(NARROW_WIDTH);                // 0b0000

    REQUIRE(wide_bv1.size() == WIDE_WIDTH);
    REQUIRE(wide_bv2.size() == WIDE_WIDTH);
    REQUIRE(narrow_dst.size() == NARROW_WIDTH);
    REQUIRE(static_cast<uint64_t>(wide_bv1) == 30);
    REQUIRE(static_cast<uint64_t>(wide_bv2) == 10);
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0);

    // Perform truncating AND: narrow_dst = wide_bv1 & wide_bv2
    ch::internal::bv_and_truncate<uint64_t>(&narrow_dst, &wide_bv1, &wide_bv2);

    // Verify result
    REQUIRE(narrow_dst.size() == NARROW_WIDTH); // Size unchanged
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 10); // 30 & 10 = 10, fits in 4 bits
    INFO("Result bitstring: " << to_bitstring(narrow_dst));
}

TEST_CASE("bitvector: Truncating Bitwise OR", "[truncation][bitwise]") {
    constexpr uint32_t WIDE_WIDTH = 33;
    constexpr uint32_t NARROW_WIDTH = 4;
    bitvector<uint64_t> wide_bv1(WIDE_WIDTH, uint64_t(0b1000)); // 0b1000 (8)
    bitvector<uint64_t> wide_bv2(WIDE_WIDTH, uint64_t(0b0001)); // 0b0001 (1)
    bitvector<uint64_t> narrow_dst(NARROW_WIDTH);               // 0b0000

    REQUIRE(wide_bv1.size() == WIDE_WIDTH);
    REQUIRE(wide_bv2.size() == WIDE_WIDTH);
    REQUIRE(narrow_dst.size() == NARROW_WIDTH);
    REQUIRE(static_cast<uint64_t>(wide_bv1) == 8);
    REQUIRE(static_cast<uint64_t>(wide_bv2) == 1);
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0);

    // Perform truncating OR: narrow_dst = wide_bv1 | wide_bv2
    ch::internal::bv_or_truncate<uint64_t>(&narrow_dst, &wide_bv1, &wide_bv2);

    // Verify result
    REQUIRE(narrow_dst.size() == NARROW_WIDTH); // Size unchanged
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 9); // 8 | 1 = 9, fits in 4 bits
    INFO("Result bitstring: " << to_bitstring(narrow_dst));
}

TEST_CASE("bitvector: Truncating Bitwise XOR", "[truncation][bitwise]") {
    constexpr uint32_t WIDE_WIDTH = 33;
    constexpr uint32_t NARROW_WIDTH = 4;
    bitvector<uint64_t> wide_bv1(WIDE_WIDTH, uint64_t(0b1010)); // 0b1010 (10)
    bitvector<uint64_t> wide_bv2(WIDE_WIDTH, uint64_t(0b0110)); // 0b0110 (6)
    bitvector<uint64_t> narrow_dst(NARROW_WIDTH);               // 0b0000

    REQUIRE(wide_bv1.size() == WIDE_WIDTH);
    REQUIRE(wide_bv2.size() == WIDE_WIDTH);
    REQUIRE(narrow_dst.size() == NARROW_WIDTH);
    REQUIRE(static_cast<uint64_t>(wide_bv1) == 10);
    REQUIRE(static_cast<uint64_t>(wide_bv2) == 6);
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0);

    // Perform truncating XOR: narrow_dst = wide_bv1 ^ wide_bv2
    ch::internal::bv_xor_truncate<uint64_t>(&narrow_dst, &wide_bv1, &wide_bv2);

    // Verify result
    REQUIRE(narrow_dst.size() == NARROW_WIDTH); // Size unchanged
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 12); // 10 ^ 6 = 12, fits in 4 bits
    INFO("Result bitstring: " << to_bitstring(narrow_dst));
}

TEST_CASE("bitvector: Truncating Bitwise NOT", "[truncation][bitwise]") {
    constexpr uint32_t WIDE_WIDTH = 33;
    constexpr uint32_t NARROW_WIDTH = 4;
    bitvector<uint64_t> wide_src(WIDE_WIDTH, uint64_t(0b0001)); // 0b0001 (1)
    bitvector<uint64_t> narrow_dst(NARROW_WIDTH);              // 0b0000

    REQUIRE(wide_src.size() == WIDE_WIDTH);
    REQUIRE(narrow_dst.size() == NARROW_WIDTH);
    REQUIRE(static_cast<uint64_t>(wide_src) == 1);
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0);

    // Perform truncating NOT: narrow_dst = ~wide_src
    ch::internal::bv_inv_truncate<uint64_t>(&narrow_dst, &wide_src);

    // Verify result
    REQUIRE(narrow_dst.size() == NARROW_WIDTH); // Size unchanged
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 14); // ~0b0001 (4-bit) = 0b1110 = 14
    INFO("Result bitstring: " << to_bitstring(narrow_dst));
}

TEST_CASE("bitvector: Truncating Left Shift", "[truncation][shift]") {
    constexpr uint32_t WIDE_WIDTH = 33;
    constexpr uint32_t NARROW_WIDTH = 4;
    bitvector<uint64_t> wide_src(WIDE_WIDTH, uint64_t(0b0001)); // 0b0001 (1)
    bitvector<uint64_t> narrow_dst(NARROW_WIDTH);              // 0b0000

    REQUIRE(wide_src.size() == WIDE_WIDTH);
    REQUIRE(narrow_dst.size() == NARROW_WIDTH);
    REQUIRE(static_cast<uint64_t>(wide_src) == 1);
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0);

    // Perform truncating left shift: narrow_dst = wide_src << 2
    ch::internal::bv_shl_truncate<uint64_t>(&narrow_dst, &wide_src, 2);

    // Verify result
    REQUIRE(narrow_dst.size() == NARROW_WIDTH); // Size unchanged
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 4); // 0b0001 << 2 = 0b0100 = 4, fits in 4 bits
    INFO("Result bitstring: " << to_bitstring(narrow_dst));
}

TEST_CASE("bitvector: Truncating Right Shift", "[truncation][shift]") {
    constexpr uint32_t WIDE_WIDTH = 33;
    constexpr uint32_t NARROW_WIDTH = 4;
    bitvector<uint64_t> wide_src(WIDE_WIDTH, uint64_t(0b1000)); // 0b1000 (8)
    bitvector<uint64_t> narrow_dst(NARROW_WIDTH);              // 0b0000

    REQUIRE(wide_src.size() == WIDE_WIDTH);
    REQUIRE(narrow_dst.size() == NARROW_WIDTH);
    REQUIRE(static_cast<uint64_t>(wide_src) == 8);
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 0);

    // Perform truncating right shift: narrow_dst = wide_src >> 1
    ch::internal::bv_shr_truncate<uint64_t>(&narrow_dst, &wide_src, 1);

    // Verify result
    REQUIRE(narrow_dst.size() == NARROW_WIDTH); // Size unchanged
    REQUIRE(static_cast<uint64_t>(narrow_dst) == 4); // 0b1000 >> 1 = 0b0100 = 4, fits in 4 bits
    INFO("Result bitstring: " << to_bitstring(narrow_dst));
}

// --- END Unit Tests for Truncating Operations ---
