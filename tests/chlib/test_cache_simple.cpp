/**
 * @file test_cache_simple.cpp
 * @brief Cache 模块编译验证测试
 *
 * 实例化真实的 `chlib::ICacheSync` / `chlib::DCacheSync` (ADR-039 ch_mem 重写 +
 * ADR-041 Sync/Async 双变体) 做类型/编译 smoke。
 *
 * 历史: 本文件原先实例化 pass-through stub (`ICache` / `DCache` 类, 已删除)，
 * 已于 cache-stub-physical-removal-2026-10 物理删除) 并 pragma 抑制
 * `-Wdeprecated-declarations`。现改为真实实现，无需任何警告抑制。
 * 行为级覆盖由 `tests/chlib/test_cache_sync.cpp` (dual-backend) 与
 * `tests/chlib/test_cache_pipeline_integration.cpp` 承接。
 */

#include "catch_amalgamated.hpp"
#include "cpu/cache/d_cache_sync.h"
#include "cpu/cache/i_cache_sync.h"

using namespace chlib;

TEST_CASE("ICache Type Check", "[cache][icache]") {
    ch::core::context ctx("icache_test");
    ch::core::ctx_swap swap(&ctx);
    ICacheSync icache{nullptr, "icache"};
    STATIC_REQUIRE(std::is_base_of_v<ch::Component, ICacheSync>);
    REQUIRE(true);
}

TEST_CASE("DCache Type Check", "[cache][dcache]") {
    ch::core::context ctx("dcache_test");
    ch::core::ctx_swap swap(&ctx);
    DCacheSync dcache{nullptr, "dcache"};
    STATIC_REQUIRE(std::is_base_of_v<ch::Component, DCacheSync>);
    REQUIRE(true);
}
