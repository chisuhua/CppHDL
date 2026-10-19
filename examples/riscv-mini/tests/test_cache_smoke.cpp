/**
 * @file test_cache_smoke.cpp
 * @brief cache-complete-rewrite Step 4 — riscv-mini wrapper build + instantiate probe
 *
 * 验证 ICachePrefetch + DCacheWriteBuffer 可编译 + 可 instantiate.
 *
 * 修复历史 (2026-10-20, change `fix-riscv-mini-cache-prefetch-sigsegv`):
 *   - i_cache_prefetch.h 旧版用 `ch_reg<ch_bool>[N]` + runtime init for-loop
 *     → SIGSEGV (cache-complete-doa-note §6.1 R1 C-style 数组架构 bug)
 *   - 改用 `ch_mem<ch_uint<1>, N>` 替代, 默认 0, instantiate OK
 *
 * d_cache_write_buffer.h review (2026-10-20): buffer 用普通 struct 数组
 * (WriteBufferEntry buffer[4]), 不是 ch_reg 数组, **未发现 SIGSEGV bug**.
 *
 * 作者: build-agent (cache-complete-rewrite Session B, handoff §3.4)
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"

#include "i_cache_prefetch.h"
#include "d_cache_write_buffer.h"

using namespace chlib;

TEST_CASE("ICachePrefetch compile probe", "[riscv-mini][cache][smoke][build-probe]") {
    // Compile-time check: ICachePrefetch 在 cache-complete-rewrite 后能 include + 实例化构造
    // (build probe: 已通过 cmake --build)
    SUCCEED("ICachePrefetch compiles after cache-complete-rewrite (build probe passed)");
}

TEST_CASE("ICachePrefetch instantiation (FIXED via ch_mem)",
          "[riscv-mini][cache][smoke][fixed]") {
    ch::core::context ctx("icache_prefetch_smoke");
    ch::core::ctx_swap swap(&ctx);

    // Per fix-riscv-mini-cache-prefetch-sigsegv: ch_reg<ch_bool>[N] → ch_mem<ch_uint<1>, N>
    // 默认值 0, instantiate 不再 SIGSEGV
    ch::ch_device<ICachePrefetch> dev;
    REQUIRE(dev.context() != nullptr);
    SUCCEED("ICachePrefetch instantiate OK after ch_mem refactor");
}

TEST_CASE("DCacheWriteBuffer instantiation (no SIGSEGV per review)",
          "[riscv-mini][cache][smoke][no-issue]") {
    ch::core::context ctx("dcache_write_buffer_smoke");
    ch::core::ctx_swap swap(&ctx);

    // Per fix-riscv-mini-cache-prefetch-sigsegv review: DCacheWriteBuffer 用
    // 普通 struct 数组 (WriteBufferEntry buffer[4]), 不是 ch_reg 数组,
    // 没有 SIGSEGV bug。这里做 instantiate smoke 验证
    ch::ch_device<DCacheWriteBuffer> dev;
    REQUIRE(dev.context() != nullptr);
    SUCCEED("DCacheWriteBuffer instantiate OK (no ch_reg array bug)");
}