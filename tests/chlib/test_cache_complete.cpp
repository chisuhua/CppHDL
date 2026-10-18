/**
 * @file test_cache_complete.cpp
 * @brief cache-complete-rewrite Step 3 — ICacheComplete / DCacheComplete 实例化与行为测试
 *
 * 4 个 TEST_CASE (handoff §3.3, ADR-039 §6 Step 3):
 * 1. icache miss-fill-then-hit [cache][complete][icache][miss-fill]
 * 2. dcache read miss-fill     [cache][complete][dcache][miss-fill]
 * 3. 2-way multi-port aread 隔离性 [cache][complete][isolation][g1]
 * 4. 同周期 hit-aread + miss-fill-write [cache][complete][concurrency][g2]
 *
 * 这个文件初始创建用于 R1 hard-gate build probe (cache-complete-doa-note.md §6.1.R1):
 * "architect 阶段必须做 cmake build probe 实测, 不能仅 grep"
 *
 * 完整 4 个 TEST_CASE 在 Step 3 commit 2 落地.
 * Step 1+2 commit 1 的临时 probe = 这个文件 + 仅 icache/dcache instantiation 两个空 TEST_CASE.
 */

#include "catch_amalgamated.hpp"
#include "cpu/cache/i_cache_complete.h"
#include "cpu/cache/d_cache_complete.h"
#include "simulator.h"

using namespace chlib;

// =============================================================================
// Step 1 build probe: 验证 ICacheComplete / DCacheComplete 可实例化且编译通过
// =============================================================================

TEST_CASE("ICacheComplete instantiation (build probe)", "[cache][complete][build-probe]") {
    ch::core::context ctx("icache_complete_build_probe");
    ch::core::ctx_swap swap(&ctx);
    ch::ch_device<ICacheComplete> ic_dev;
    REQUIRE(true); // 占位: 仅验证编译通过 + 不崩溃
}

TEST_CASE("DCacheComplete instantiation (build probe)", "[cache][complete][build-probe]") {
    ch::core::context ctx("dcache_complete_build_probe");
    ch::core::ctx_swap swap(&ctx);
    ch::ch_device<DCacheComplete> dc_dev;
    REQUIRE(true);
}