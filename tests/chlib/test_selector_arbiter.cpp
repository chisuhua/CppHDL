#include "catch_amalgamated.hpp"
#include "chlib/selector_arbiter.h"
#include "codegen_dag.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

// ⚠️⚠️ **ADR-065 D2 落地后本文件期望值全部重推**（2026-11-03）
//
// 修复前 `round_robin_arbiter` 的 `UpdatePriority::process` 有一个外层
// `if (result.valid)` —— 条件是**硬件组合值**，而 `ch_bool::operator uint64_t()`
// 只对 **const 节点**返回真值（`src/lnode/bool.cpp:43-49`）⇒ ⇒ 该 `if` 在**建图期
// 恒假** ⇒ ⇒ 整个 `if` 体（含递归展开）**从不执行** ⇒ ⇒ `next_priority` 永不更新
// ⇒ ⇒ `ptr_reg->next` 恒写回原值 ⇒ ⇒ **`next_ptr` 恒 0**。
//
// ⇒ 那意味着本文件**此前全部通过**的期望值都是「指针永远停在 0」的语义。
//   修复后指针真的推进，**8 处期望值随之转红** —— 它们是**钉住了缺陷**，不是
//   发现了新缺陷（ADR-065 §5.4 已预判这一格）。
//
// ⚠️ **读模型**（决定了「tick 后该看到什么」）：
//   `Simulator::tick()` = 沿前组合求值 → 寄存器提交 → **沿后**组合求值
//   ⇒ `get_value()` 在 `tick()` 之后读到的是**指针已前进之后**的组合值。
//
// ⇒ 因此 1 次 `tick()` 后的正确期望 = **从 `(g0 + 1) % N` 开始**循环扫描的
//   首个置位请求（`g0` = 从 0 开始的首个置位请求）。单置位请求时两者相同
//   （所以那些用例的期望值不变）。
//
// ⚠️ **手推，不是抄仿真输出**：模型 = `pos = (ptr + k) % N, k = 0..N-1`，
//   取首个 `request[pos]` 为真的位；`next_ptr = (该位 + 1) % N`。

// Helper function to convert uint value to binary string representation
template <typename T> std::string to_binary_string(T value, size_t width) {
    std::bitset<64> bs(static_cast<uint64_t>(value));
    std::string result = bs.to_string();
    // Return only the requested width bits
    return result.substr(64 - width, width);
}

// 规格模型：从 `start` 开始循环扫描，返回首个置位请求（one-hot）；无请求 ⇒ 0
template <unsigned N>
uint64_t rr_grant_from(uint64_t request, unsigned start) {
    for (unsigned k = 0; k < N; ++k) {
        unsigned pos = (start + k) % N;
        if (request & (1ull << pos)) return (1ull << pos);
    }
    return 0;
}

// 规格模型：`next_ptr` = 被授予位 + 1（环绕）；无授予 ⇒ 保持 `start`
template <unsigned N>
uint64_t rr_next_ptr(uint64_t request, unsigned start) {
    uint64_t g = rr_grant_from<N>(request, start);
    if (g == 0) return start;
    unsigned pos = 0;
    while (((g >> pos) & 1ull) == 0) ++pos;
    return (pos + 1) % N;
}

TEST_CASE("SelectorArbiter: round robin arbiter basic tests",
          "[selector_arbiter][round_robin_arbiter][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_round_robin_arbiter");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin arbiter with multiple requests") {
        ch_uint<4> request(0101_b); // Requests at positions 0 and 2
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 第 1 拍：ptr=0 授予 bit0 ⇒ ptr 前进到 1 ⇒ 沿后扫描从 1 开始 ⇒ bit2
        REQUIRE(sim.get_value(result.grant) == rr_grant_from<4>(0b0101, 1));
        REQUIRE(sim.get_value(result.grant) == 0b0100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);
        REQUIRE(sim.get_value(result.next_ptr) == rr_next_ptr<4>(0b0101, 0));
        // ⚠️ `next_ptr` = **沿上提交的** `ptr_reg`（= 被授予位 + 1），
        //    不是沿后 grant 的后继
        REQUIRE(sim.get_value(result.next_ptr) == 0b0001); // 授予 bit0 ⇒ 1
    }

    SECTION("Round robin arbiter with different request pattern") {
        ch_uint<4> request(1100_b); // Requests at positions 2 and 3
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 第 1 拍：ptr=0 授予 bit2 ⇒ ptr 前进到 3 ⇒ 沿后扫描从 3 开始 ⇒ bit3
        REQUIRE(sim.get_value(result.grant) == rr_grant_from<4>(0b1100, 3));
        REQUIRE(sim.get_value(result.grant) == 0b1000); // Bit 3 set
        REQUIRE(sim.get_value(result.valid) == true);
        REQUIRE(sim.get_value(result.next_ptr) == rr_next_ptr<4>(0b1100, 0));
        REQUIRE(sim.get_value(result.next_ptr) == 0b0011); // 授予 bit2 ⇒ 3
    }

    SECTION("Round robin arbiter with no requests") {
        ch_uint<4> request(0000_b); // No requests
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0b0000); // No grants
        REQUIRE(sim.get_value(result.valid) == false);
    }

    SECTION("Round robin arbiter with all requests") {
        ch_uint<4> request(1111_b); // All requests active
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 第 1 拍：ptr=0 授予 bit0 ⇒ ptr 前进到 1 ⇒ 沿后扫描从 1 开始 ⇒ bit1
        REQUIRE(sim.get_value(result.grant) == rr_grant_from<4>(0b1111, 1));
        REQUIRE(sim.get_value(result.grant) == 0b0010); // Bit 1 set
        REQUIRE(sim.get_value(result.valid) == true);
        REQUIRE(sim.get_value(result.next_ptr) == 0b0001); // 授予 bit0 ⇒ 1
    }

    SECTION("Round robin arbiter sequential behavior") {
        ch_uint<4> request(1101_b); // Requests at positions 0, 2, 3
        RoundRobinArbiterResult<4> result = round_robin_arbiter<4>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 第 1 拍：ptr=0 授予 bit0 ⇒ ptr 前进到 1 ⇒ 沿后扫描从 1 开始 ⇒ bit2
        REQUIRE(sim.get_value(result.grant) == rr_grant_from<4>(0b1101, 1));
        REQUIRE(sim.get_value(result.grant) == 0b0100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);
        REQUIRE(sim.get_value(result.next_ptr) == 0b0001); // 授予 bit0 ⇒ 1
    }
}

TEST_CASE("SelectorArbiter: round robin arbiter extended tests",
          "[selector_arbiter][round_robin_arbiter][extended]") {
    auto ctx = std::make_unique<ch::core::context>(
        "test_round_robin_arbiter_extended");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin arbiter 8-bit width") {
        ch_uint<8> request(0b01010101); // Alternating pattern
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 第 1 拍 ptr=0 授予 bit0 ⇒ ptr 前进到 1 ⇒ 沿后首匹配 = bit2
        REQUIRE(sim.get_value(result.grant) == rr_grant_from<8>(0x55, 1));
        REQUIRE(sim.get_value(result.grant) == 0b00000100); // Bit 2 set
        REQUIRE(sim.get_value(result.valid) == true);
        REQUIRE(sim.get_value(result.next_ptr) == 0b00000001); // 授予 bit0 ⇒ 1
    }

    SECTION("Round robin arbiter with high bit requests") {
        ch_uint<8> request(0b10000000); // Only highest bit set
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0b10000000); // Bit 7 set
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with middle bits") {
        ch_uint<8> request(0b00110000); // Bits 4 and 5 set
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 第 1 拍 ptr=0 授予 bit4 ⇒ ptr 前进到 5 ⇒ 沿后首匹配 = bit5
        REQUIRE(sim.get_value(result.grant) == rr_grant_from<8>(0x30, 5));
        REQUIRE(sim.get_value(result.grant) == 0b00100000); // Bit 5 set
        REQUIRE(sim.get_value(result.valid) == true);
        REQUIRE(sim.get_value(result.next_ptr) == 0b00000101); // 授予 bit4 ⇒ 5
    }

    SECTION("Round robin arbiter with single bit at various positions") {
        for (int i = 0; i < 8; i++) {
            ch_uint<8> request(1U << i); // Only bit i set
            RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

            ch::Simulator sim(ctx.get());
            sim.tick();

            REQUIRE(sim.get_value(result.grant) == (1U << i)); // Same bit set
            REQUIRE(sim.get_value(result.valid) == true);
        }
    }
}

TEST_CASE("SelectorArbiter: round robin arbiter boundary tests",
          "[selector_arbiter][round_robin_arbiter][boundary]") {
    auto ctx = std::make_unique<ch::core::context>(
        "test_round_robin_arbiter_boundary");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin arbiter 1-bit width") {
        ch_uint<1> request(1_b); // Only possible request
        RoundRobinArbiterResult<1> result = round_robin_arbiter<1>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 1); // Only possible grant
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter 2-bit width") {
        ch_uint<2> request(11_b); // Both bits set
        RoundRobinArbiterResult<2> result = round_robin_arbiter<2>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 第 1 拍 ptr=0 授予 bit0 ⇒ ptr 前进到 1 ⇒ 沿后首匹配 = bit1
        REQUIRE(sim.get_value(result.grant) == 0b10); // Bit 1 set
        REQUIRE(sim.get_value(result.valid) == true);
        REQUIRE(sim.get_value(result.next_ptr) == 0b01); // 授予 bit0 ⇒ 1
    }

    SECTION("Round robin arbiter with all possible 3-bit patterns") {
        for (int pattern = 1; pattern < 8; pattern++) { // Skip 0 (no requests)
            ch_uint<3> request(pattern);
            RoundRobinArbiterResult<3> result = round_robin_arbiter<3>(request);

            ch::Simulator sim(ctx.get());
            sim.tick();

            // 第 1 拍从 ptr=0 授予首个置位位 g0 ⇒ ptr 前进到 (g0+1)%3
            // ⇒ 沿后从该位置循环扫描
            unsigned g0 = 0;
            while (((pattern >> g0) & 1u) == 0) ++g0;
            unsigned ptr_after = (g0 + 1) % 3;
            REQUIRE(sim.get_value(result.grant) ==
                    rr_grant_from<3>(static_cast<uint64_t>(pattern),
                                     ptr_after));
            REQUIRE(sim.get_value(result.next_ptr) == ptr_after);
            REQUIRE(sim.get_value(result.valid) == true);
        }
    }

    SECTION("Round robin arbiter boundary: max bit set") {
        ch_uint<16> request(0x8000); // Bit 15 set (highest in 16-bit)
        RoundRobinArbiterResult<16> result = round_robin_arbiter<16>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        REQUIRE(sim.get_value(result.grant) == 0x8000); // Bit 15 set
        REQUIRE(sim.get_value(result.valid) == true);
    }
}

TEST_CASE("SelectorArbiter: round robin arbiter stress tests",
          "[selector_arbiter][round_robin_arbiter][stress]") {
    auto ctx =
        std::make_unique<ch::core::context>("test_round_robin_arbiter_stress");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Round robin arbiter sequential access pattern") {
        ch_uint<8> request(0b11111111); // All bits set
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());

        // Simulate multiple ticks to check round-robin behavior
        // After granting bit 0, next grant should start from bit 1, etc.
        std::vector<uint8_t> grants;
        for (int i = 0; i < 10; i++) {
            sim.tick();
            uint8_t grant = static_cast<uint8_t>(
                static_cast<uint64_t>(sim.get_value(result.grant)));
            grants.push_back(grant);
        }

        // ⚠️ **真正的轮转**（ADR-065 D2）：request 全 1 ⇒ 指针每拍 +1
        //   ⇒ 沿后读到的 grant 序列 = bit1, bit2 … bit7, bit0, bit1, bit2
        //   （第 1 拍授予 bit0 后指针已前进到 1 ⇒ 沿后从 1 开始扫描）
        const uint8_t kRoundRobin[10] = {0x02, 0x04, 0x08, 0x10, 0x20,
                                         0x40, 0x80, 0x01, 0x02, 0x04};
        for (int i = 0; i < 10; i++) REQUIRE(grants[i] == kRoundRobin[i]);
        REQUIRE(sim.get_value(result.valid) == true);
    }

    SECTION("Round robin arbiter with maximum width (64-bit)") {
        // Explicit uint64_t cast: on macOS LP64, `unsigned long` is
        // 64-bit, same as uint64_t, so `0xFFFFFFFFFFFFFFFFUL` is
        // ambiguous between ch_literal_runtime(uint64_t) and
        // ch_literal_runtime(int64_t) constructors. Disambiguating
        // with an explicit type is portable across macOS/Linux.
        ch_uint<64> request(uint64_t(0xFFFFFFFFFFFFFFFFUL)); // All 64 bits set
        RoundRobinArbiterResult<64> result = round_robin_arbiter<64>(request);

        ch::Simulator sim(ctx.get());
        sim.tick();

        // 第 1 拍 ptr=0 授予 bit0 ⇒ ptr 前进到 1 ⇒ 沿后首匹配 = bit1
        REQUIRE(sim.get_value(result.grant) == 0x0000000000000002UL);
        REQUIRE(sim.get_value(result.valid) == true);
        REQUIRE(sim.get_value(result.next_ptr) == 0x0000000000000001UL);
    }

    SECTION("Round robin arbiter with various large widths") {
        // Test 32-bit width
        {
            // Explicit uint32_t cast: see comment above re: macOS LP64
            // unsigned long ambiguity with ch_literal_runtime constructors.
            ch_uint<32> request(uint32_t(0xC0000000UL)); // High bits set
            RoundRobinArbiterResult<32> result =
                round_robin_arbiter<32>(request);

            ch::Simulator sim(ctx.get());
            sim.tick();

            // Print input and output values in binary format as per
            // specification
            auto request_val = sim.get_value(request);
            auto grant_val = sim.get_value(result.grant);
            auto valid_val = sim.get_value(result.valid);

            std::cout << "32-bit round robin arbiter:" << std::endl;
            std::cout << "  request: 0b" << to_binary_string(request_val, 32)
                      << std::endl;
            std::cout << "  grant:   0b" << to_binary_string(grant_val, 32)
                      << std::endl;
            std::cout << "  valid:   " << valid_val << std::endl;

            // 第 1 拍 ptr=0 授予 bit30 ⇒ ptr 前进到 31 ⇒ 沿后首匹配 = bit31
            REQUIRE(grant_val == 0x80000000UL);
            REQUIRE(valid_val == true);
        }

        // Test 16-bit alternating
        {
            ch_uint<16> request(0xAAAA); // Alternating 1010 pattern
            RoundRobinArbiterResult<16> result =
                round_robin_arbiter<16>(request);

            ch::Simulator sim(ctx.get());
            sim.tick();

            // Print input and output values in binary format as per
            // specification
            auto request_val = sim.get_value(request);
            auto grant_val = sim.get_value(result.grant);
            auto valid_val = sim.get_value(result.valid);

            std::cout << "16-bit round robin arbiter:" << std::endl;
            std::cout << "  request: 0b" << to_binary_string(request_val, 16)
                      << std::endl;
            std::cout << "  grant:   0b" << to_binary_string(grant_val, 16)
                      << std::endl;
            std::cout << "  valid:   " << valid_val << std::endl;

            // 第 1 拍 ptr=0 授予 bit1 ⇒ ptr 前进到 2 ⇒ 沿后首匹配 = bit3
            REQUIRE(grant_val == 0x0008);
            REQUIRE(valid_val == true);
        }
    }

    SECTION("Round robin arbiter stress: multiple different patterns") {
        std::vector<std::pair<uint64_t, uint64_t>> test_cases = {
            // ⚠️ 每组期望 = 「从 (g0+1)%64 起的首个置位位」（g0 = 最低置位位）
            {0x0F0F0F0F0F0F0F0F, 0x0000000000000002}, // g0=0  ⇒ ptr=1  ⇒ bit1
            {0xF0F0F0F0F0F0F0F0, 0x0000000000000020}, // g0=4  ⇒ ptr=5  ⇒ bit5
            {0x5555555555555555, 0x0000000000000004}, // g0=0  ⇒ ptr=1  ⇒ bit2
            {0xAAAAAAAAAAAAAAAA, 0x0000000000000008}, // g0=1  ⇒ ptr=2  ⇒ bit3
            {0x00000000FFFFFFFF, 0x0000000000000002}, // g0=0  ⇒ ptr=1  ⇒ bit1
            {0xFFFFFFFF00000000, 0x0000000200000000}  // g0=32 ⇒ ptr=33 ⇒ bit33
        };

        for (size_t i = 0; i < test_cases.size(); ++i) {
            auto [req_val, expected_grant] = test_cases[i];

            std::string ctx_name =
                "test_rr_arbiter_pattern_" + std::to_string(i);
            auto local_ctx = std::make_unique<ch::core::context>(ctx_name);
            ch::core::ctx_swap local_swapper(local_ctx.get());

            ch_uint<64> request(req_val);
            RoundRobinArbiterResult<64> result =
                round_robin_arbiter<64>(request);

            ch::Simulator sim(local_ctx.get());
            sim.tick();

            uint64_t actual_grant =
                static_cast<uint64_t>(sim.get_value(result.grant));
            bool has_requests = req_val != 0;

            REQUIRE(actual_grant == expected_grant);
            REQUIRE(sim.get_value(result.valid) == has_requests);
        }
    }
}

TEST_CASE("SelectorArbiter: round robin arbiter consistency tests",
          "[selector_arbiter][round_robin_arbiter][consistency]") {
    auto ctx = std::make_unique<ch::core::context>(
        "test_round_robin_arbiter_consistency");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Consistency across multiple ticks with same request") {
        ch_uint<8> request(0b01010101); // Fixed pattern
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());

        // Run multiple ticks and verify consistent starting behavior
        for (int i = 0; i < 5; ++i) {
            sim.tick();
            // ⚠️ **真正的轮转**（ADR-065 D2 修复后）：request=0b01010101
            //   ⇒ 指针每拍前进到「被授予位 + 1」⇒ 沿后读到的是从新指针
            //   起的首个置位位 ⇒ 序列（g0 = 最低置位位）：
            //   i=0: ptr0→1 ⇒ bit2；i=1: ptr 2→3 ⇒ bit4；i=2: 4→5 ⇒ bit6；
            //   i=3: 6→0 ⇒ bit0；i=4: 0→1 ⇒ bit2
            const uint64_t kExpected[5] = {0x04, 0x10, 0x40, 0x01, 0x04};
            REQUIRE(sim.get_value(result.grant) == kExpected[i]);
            REQUIRE(sim.get_value(result.valid) == true);
        }
    }

    SECTION("Round robin arbiter with dynamic request changes") {
        ch_uint<8> request;
        request = ch_uint<8>(0xFF); // All bits set initially
        RoundRobinArbiterResult<8> result = round_robin_arbiter<8>(request);

        ch::Simulator sim(ctx.get());

        // 第 1 拍：ptr=0 授予 bit0 ⇒ ptr→1 ⇒ 沿后从 1 起首匹配 = bit1
        sim.tick();
        REQUIRE(sim.get_value(result.grant) == 0x02); // Bit 1
        REQUIRE(sim.get_value(result.valid) == true);

        // 只有 bit7 ⇒ ptr=1 起扫到 bit7 ⇒ ptr→0 ⇒ 沿后仍 bit7
        sim.set_value(request, 0x80);
        sim.tick();
        REQUIRE(sim.get_value(result.grant) == 0x80); // Bit 7 set
        REQUIRE(sim.get_value(result.valid) == true);

        // bits 2..5 ⇒ ptr=0 起授予 bit2 ⇒ ptr→3 ⇒ 沿后从 3 起 = bit3
        sim.set_value(request, 0x3C);
        sim.tick();
        REQUIRE(sim.get_value(result.grant) == 0x08); // Bit 3
        REQUIRE(sim.get_value(result.valid) == true);
    }
}
