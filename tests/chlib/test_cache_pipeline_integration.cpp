/**
 * @file test_cache_pipeline_integration.cpp
 * @brief Cache-流水线集成测试
 *
 * 验证 ICache / DCache 可独立实例化、并具备与流水线主控对接所需的
 * CPU 侧请求接口和 AXI 主设备接口。该测试集不依赖已删除的
 * `rv32i_pipeline_cache.h`（`Rv32iPipelineWithCache`），所有需要流水线
 * 联调的子用例通过 SKIP 保留，以便未来重建 Rv32iPipelineWithCache 后
 * 一键恢复。
 *
 * 设计要点:
 * - 每个 TEST_CASE 使用单独的 ch::ch_device<T>，保证独立 context。
 * - 多 cache 协同（Dual / AXI 双向）测试用 SKIP 标注，等待
 *   Rv32iPipelineWithCache 重建后切换为 ch::ch_module 容器测试。
 *
 * 实现说明 (cache-stub-physical-removal-2026-10, ADR-037 物理删除):
 * - 本文件原先实例化 pass-through stub (`ICache` / `DCache` 类, 已删除)，
 *   断言的是 stub 自身的直通语义 (`ready := req, miss := false,
 *   data := axi_data`) —— 即 "stub 测 stub" 的同义反复, 无回归价值。
 * - 现改为实例化真实实现 `chlib::ICacheSync` / `chlib::DCacheSync`
 *   (ADR-039 ch_mem 重写 + ADR-041 Sync/Async 双变体), 断言真实的
 *   cold-miss → AXI fill → refilled-hit 时序。
 * - 时序模板取自 `tests/chlib/test_cache_sync.cpp` (已 dual-backend 验证),
 *   未新造时序。
 */

#include "catch_amalgamated.hpp"
#include "cpu/cache/d_cache_sync.h"
#include "cpu/cache/i_cache_sync.h"
#include "simulator.h"

using namespace chlib;

TEST_CASE("Cache-Pipeline Integration - Type Check", "[integration][cache]") {
    ch::core::context ctx("cache_pipe_int");
    ch::core::ctx_swap swap(&ctx);

    // 验证 ICacheSync / DCacheSync 类型存在并可实例化
    ch::ch_device<ICacheSync> icache_dev;
    ch::ch_device<DCacheSync> dcache_dev;

    // 静态检查：继承 ch::Component
    STATIC_REQUIRE(std::is_base_of_v<ch::Component, ICacheSync>);
    STATIC_REQUIRE(std::is_base_of_v<ch::Component, DCacheSync>);

    REQUIRE(true);
}

TEST_CASE("Cache-Pipeline - I-Cache Integration", "[integration][icache]") {
    ch::core::context ctx("icache_pipe");
    ch::core::ctx_swap swap(&ctx);

    ch::ch_device<ICacheSync> dev;
    ch::Simulator sim(dev.context());
    auto& icache = dev.instance();

    // cycle 1: 冷读 → tag/valid 均无效 → miss, 并发起 AXI 读请求
    sim.set_input_value(icache.io().addr, 0x1000u);
    sim.set_input_value(icache.io().req,  1u);
    sim.set_input_value(icache.io().axi_ready, 1u);
    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().miss))      == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().axi_valid)) == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().axi_addr))  == 0x1000u);
    // miss 时 ready 跟随 axi_ready (真实 cache 语义, 非 stub 的 ready := req)
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ready))     == 1u);

    // cycle 2: AXI 响应带回数据 → 填充 way0
    sim.set_input_value(icache.io().req,  0u);
    sim.set_input_value(icache.io().axi_resp_valid, 1u);
    sim.set_input_value(icache.io().axi_data, 0xDEADBEEFu);
    sim.tick();

    // cycle 3: 同址再读 → hit, 数据来自 cache (非 axi_data)
    sim.set_input_value(icache.io().addr, 0x1000u);
    sim.set_input_value(icache.io().req,  1u);
    sim.set_input_value(icache.io().axi_ready, 0u);
    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().miss)) == 0u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().data)) == 0xDEADBEEFu);

    // Rv32iPipelineWithCache 已删除，未来重建后恢复流水线联调
    SKIP("Rv32iPipelineWithCache not implemented; cache-level assertions above passed.");
}

TEST_CASE("Cache-Pipeline - D-Cache Integration", "[integration][dcache]") {
    ch::core::context ctx("dcache_pipe");
    ch::core::ctx_swap swap(&ctx);

    ch::ch_device<DCacheSync> dev;
    ch::Simulator sim(dev.context());
    auto& dcache = dev.instance();

    // cycle 1: 冷读 → miss, 并发起 AXI 读请求
    sim.set_input_value(dcache.io().addr,  0x2000u);
    sim.set_input_value(dcache.io().req,   1u);
    sim.set_input_value(dcache.io().we,    0u);
    sim.set_input_value(dcache.io().axi_ready, 1u);
    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().miss))      == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_valid)) == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_addr))  == 0x2000u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().ready))     == 1u);

    // cycle 2: AXI 响应带回数据 → 填充 way0
    sim.set_input_value(dcache.io().req,   0u);
    sim.set_input_value(dcache.io().we,    0u);
    sim.set_input_value(dcache.io().axi_resp_valid, 1u);
    sim.set_input_value(dcache.io().axi_rdata, 0x12345678u);
    sim.tick();

    // cycle 3: 同址再读 → hit, rdata 来自 cache
    sim.set_input_value(dcache.io().addr,  0x2000u);
    sim.set_input_value(dcache.io().req,   1u);
    sim.set_input_value(dcache.io().we,    0u);
    sim.set_input_value(dcache.io().axi_ready, 0u);
    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().miss))  == 0u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().rdata)) == 0x12345678u);

    // cycle 4: 写通道 (write-through) → AXI 侧透写地址/数据/写使能
    sim.set_input_value(dcache.io().addr,  0x6000u);
    sim.set_input_value(dcache.io().wdata, 0xCAFEF00Du);
    sim.set_input_value(dcache.io().req,   1u);
    sim.set_input_value(dcache.io().we,    1u);
    sim.set_input_value(dcache.io().axi_ready, 1u);
    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_we))    == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_wdata)) == 0xCAFEF00Du);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_valid)) == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_addr))  == 0x6000u);

    SKIP("Rv32iPipelineWithCache not implemented; cache-level assertions above passed.");
}

TEST_CASE("Cache-Pipeline - Dual Cache Access", "[integration][dual]") {
    ch::core::context ctx("dual_cache");
    ch::core::ctx_swap swap(&ctx);

    // 同时实例化 I-Cache 和 D-Cache（独立 context 仅做类型/拓扑校验）。
    // 真正跨 cache 协同需要 Rv32iPipelineWithCache 顶层容器。
    ch::ch_device<ICacheSync> icache_dev;
    ch::ch_device<DCacheSync> dcache_dev;
    REQUIRE(icache_dev.context() != nullptr);
    REQUIRE(dcache_dev.context() != nullptr);
    REQUIRE(icache_dev.context() != dcache_dev.context());

    SKIP("Dual-cache cross-device test needs Rv32iPipelineWithCache wrapper; "
         "individual caches verified in [icache] and [dcache] cases.");
}

TEST_CASE("Cache-Pipeline - AXI Interface", "[integration][axi]") {
    ch::core::context ctx("axi_cache");
    ch::core::ctx_swap swap(&ctx);

    // AXI 通道验证在独立 ICache / DCache 上下文中进行（ch::ch_device 隔离）。
    ch::ch_device<ICacheSync> icache_dev;
    ch::ch_device<DCacheSync> dcache_dev;

    // I-Cache：miss 期间 AXI 请求挂起，ready 跟随 axi_ready
    {
        ch::Simulator sim(icache_dev.context());
        auto& icache = icache_dev.instance();

        sim.set_input_value(icache.io().addr, 0x4000u);
        sim.set_input_value(icache.io().req,  1u);
        sim.set_input_value(icache.io().axi_ready,      0u);
        sim.set_input_value(icache.io().axi_resp_valid, 0u);
        sim.tick();

        // axi_valid 应保持挂起直到 ready
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().axi_valid)) == 1u);
        // 真实 cache: miss 时 ready = axi_ready (stub 恒为 1)
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ready))     == 0u);

        sim.set_input_value(icache.io().axi_ready,      1u);
        sim.set_input_value(icache.io().axi_data,       0xABCD0001u);
        sim.set_input_value(icache.io().axi_resp_valid, 1u);
        sim.tick();

        REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ready)) == 1u);
        // miss 期间数据直通 AXI 数据通道
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().data)) == 0xABCD0001u);
    }

    // D-Cache 写 AXI 通道
    {
        ch::Simulator sim(dcache_dev.context());
        auto& dcache = dcache_dev.instance();
        sim.set_input_value(dcache.io().addr,  0x5000u);
        sim.set_input_value(dcache.io().wdata, 0xBEEF0002u);
        sim.set_input_value(dcache.io().we,    1u);
        sim.set_input_value(dcache.io().req,   1u);
        sim.set_input_value(dcache.io().axi_ready, 1u);
        sim.set_input_value(dcache.io().axi_resp_valid, 0u);
        sim.set_input_value(dcache.io().axi_rdata, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_addr))  == 0x5000u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_wdata)) == 0xBEEF0002u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_we))    == 1u);
    }

    SKIP("AXI cross-cache burst test needs Rv32iPipelineWithCache wrapper; "
         "individual cache AXI channels verified above.");
}
