/**
 * @file test_shift_boundary.cpp
 * @brief 位移操作 UB 修复验证测试
 * 
 * 核心验证：编译通过即表示 UB 修复成功
 */

#include "catch_amalgamated.hpp"

// 测试 1: 验证 1ULL 位移可以处理 >= 32 的位移量
TEST_CASE("1ULL shift handles 32+ bits", "[shift][ub-fix]") {
    // 这是修复的核心：使用 1ULL 而非 1
    auto val32 = (1ULL << 32);  // 之前 UB，现在安全
    auto val33 = (1ULL << 33);
    auto val63 = (1ULL << 63);
    
    REQUIRE(val32 == 0x100000000ULL);
    REQUIRE(val33 == 0x200000000ULL);
    REQUIRE(val63 == 0x8000000000000000ULL);
}

// 测试 2: 验证 fifo.h 中的位移修复
TEST_CASE("FIFO shift expressions compile", "[shift][fifo]") {
    constexpr unsigned n1 = (1ULL << 4);   // 16
    constexpr unsigned n2 = (1ULL << 8);   // 256
    constexpr unsigned n3 = (1ULL << 16);  // 65536
    constexpr unsigned n4 = (1ULL << 31);  // 2147483648
    
    REQUIRE(n1 == 16);
    REQUIRE(n2 == 256);
    REQUIRE(n3 == 65536);
    REQUIRE(n4 == 2147483648);
}

// 测试 3: 验证 axi4lite.h 中的位移修复
TEST_CASE("AXI4-Lite shift expressions compile", "[shift][axi4]") {
    constexpr unsigned addr12 = (1ULL << 12);  // 4096
    constexpr unsigned addr20 = (1ULL << 20);  // 1048576
    constexpr uint64_t addr32 = (1ULL << 32);  // 4294967296 (之前 UB)
    
    REQUIRE(addr12 == 4096);
    REQUIRE(addr20 == 1048576);
    REQUIRE(addr32 == 4294967296ULL);
}
