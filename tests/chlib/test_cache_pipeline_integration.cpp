/**
 * @file test_cache_pipeline_integration.cpp
 * @brief Cache-流水线集成测试
 *
 * 验证 ICache / DCache 可独立实例化、并具备与流水线主控对接所需的
 * CPU 侧请求接口和 AXI 主设备接口。该测试集不再依赖已删除的
 * `rv32i_pipeline_cache.h`（`Rv32iPipelineWithCache`），所有需要流水线
 * 联调的子用例通过 SKIP 保留，以便未来重建 Rv32iPipelineWithCache 后
 * 一键恢复。
 *
 * 设计要点:
 * - 每个 TEST_CASE 使用单独的 ch::ch_device<T>，保证独立 context。
 * - 多 cache 协同（Dual / AXI 双向）测试用 SKIP 标注，等待
 *   Rv32iPipelineWithCache 重建后切换为 ch::ch_module 容器测试。
 */

#include "catch_amalgamated.hpp"
#include "cpu/cache/i_cache.h"
#include "cpu/cache/d_cache.h"
#include "simulator.h"

using namespace chlib;

TEST_CASE("Cache-Pipeline Integration - Type Check", "[integration][cache]") {
    ch::core::context ctx("cache_pipe_int");
    ch::core::ctx_swap swap(&ctx);

    // 验证 ICache / DCache 类型存在并可实例化
    ch::ch_device<ICache> icache_dev;
    ch::ch_device<DCache> dcache_dev;

    // 静态检查：继承 ch::Component
    STATIC_REQUIRE(std::is_base_of_v<ch::Component, ICache>);
    STATIC_REQUIRE(std::is_base_of_v<ch::Component, DCache>);

    REQUIRE(true);
}

TEST_CASE("Cache-Pipeline - I-Cache Integration", "[integration][icache]") {
    ch::core::context ctx("icache_pipe");
    ch::core::ctx_swap swap(&ctx);

    ch::ch_device<ICache> dev;
    ch::Simulator sim(dev.context());
    auto& icache = dev.instance();

    // CPU 侧请求接口
    sim.set_input_value(icache.io().addr, 0x1000u);
    sim.set_input_value(icache.io().req,  1u);
    // AXI 侧：返回数据 + 响应有效
    sim.set_input_value(icache.io().axi_data,       0xDEADBEEFu);
    sim.set_input_value(icache.io().axi_resp_valid, 1u);
    sim.set_input_value(icache.io().axi_ready,      1u);

    sim.tick();

    // ready = req（直通），miss = false，data = axi_data
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ready)) == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().miss))  == 0u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().data))  == 0xDEADBEEFu);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().axi_addr))  == 0x1000u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().axi_valid)) == 1u);

    // Rv32iPipelineWithCache 已删除，未来重建后恢复流水线联调
    SKIP("Rv32iPipelineWithCache not implemented; cache-only smoke test only.");
}

TEST_CASE("Cache-Pipeline - D-Cache Integration", "[integration][dcache]") {
    ch::core::context ctx("dcache_pipe");
    ch::core::ctx_swap swap(&ctx);

    ch::ch_device<DCache> dev;
    ch::Simulator sim(dev.context());
    auto& dcache = dev.instance();

    // CPU 侧写请求
    sim.set_input_value(dcache.io().addr,  0x2000u);
    sim.set_input_value(dcache.io().wdata, 0xCAFEF00Du);
    sim.set_input_value(dcache.io().we,    1u);
    sim.set_input_value(dcache.io().req,   1u);
    // AXI 侧
    sim.set_input_value(dcache.io().axi_rdata,      0x12345678u);
    sim.set_input_value(dcache.io().axi_resp_valid, 1u);
    sim.set_input_value(dcache.io().axi_ready,      1u);

    sim.tick();

    // ready = req，miss = false，rdata = axi_rdata
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().ready)) == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().miss))  == 0u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().rdata)) == 0x12345678u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_addr))  == 0x2000u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_wdata)) == 0xCAFEF00Du);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_we))    == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_valid)) == 1u);

    SKIP("Rv32iPipelineWithCache not implemented; cache-only smoke test only.");
}

TEST_CASE("Cache-Pipeline - Dual Cache Access", "[integration][dual]") {
    ch::core::context ctx("dual_cache");
    ch::core::ctx_swap swap(&ctx);

    // 同时实例化 I-Cache 和 D-Cache（独立 context 仅做类型/拓扑校验）。
    // 真正跨 cache 协同需要 Rv32iPipelineWithCache 顶层容器。
    ch::ch_device<ICache> icache_dev;
    ch::ch_device<DCache> dcache_dev;
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
    ch::ch_device<ICache> icache_dev;
    ch::ch_device<DCache> dcache_dev;

    // I-Cache 突发：连续 2 周期
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
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ready))     == 1u);

        sim.set_input_value(icache.io().axi_ready,      1u);
        sim.set_input_value(icache.io().axi_data,       0xABCD0001u);
        sim.set_input_value(icache.io().axi_resp_valid, 1u);
        sim.tick();
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
        sim.set_input_value(dcache.io().axi_ready,      1u);
        sim.set_input_value(dcache.io().axi_resp_valid, 1u);
        sim.set_input_value(dcache.io().axi_rdata,      0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_addr))  == 0x5000u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_wdata)) == 0xBEEF0002u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().axi_we))    == 1u);
    }

    SKIP("AXI cross-cache burst test needs Rv32iPipelineWithCache wrapper; "
         "individual cache AXI channels verified above.");
}
