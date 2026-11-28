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
    // ⚠️ **2026-11-28（S4c-1a）重写**：`ICacheSync` 的 AXI 侧已是 **AXI4-full 读通道**
    //    ⇒ ⇒ 读 miss 需 **4 个 R beat**；`ready` 保持 I 侧语义（「请求已被接受」）
    //    ⇒ ⇒ 填充完成**下一拍** `ready` 与 `data` 同拍有效。
    ch::core::context ctx("icache_pipe");
    ch::core::ctx_swap swap(&ctx);

    ch::ch_device<ICacheSync> dev;
    ch::Simulator sim(dev.context());
    auto& icache = dev.instance();

    // cycle 1: 冷读 → tag/valid 均无效 → miss, 并发起 AR（4-beat burst）
    // ⚠️ 本拍**不拉** `ar_ready` ⇒ 观察 `ar_valid` 保持（AXI：VALID 不等 READY）
    sim.set_input_value(icache.io().addr,     0x1000u);
    sim.set_input_value(icache.io().req,      1u);
    sim.set_input_value(icache.io().ar_ready, 0u);
    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().miss))     == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ar_valid)) == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ar_addr))  == 0x1000u);
    // 手推 burst 参数：ARLEN=3（4 beat）· ARSIZE=2（4 B）· ARBURST=INCR(1)
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ar_len))   == 3u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ar_size))  == 2u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ar_burst)) == 1u);
    // `ready` = 「请求已被接受」⇒ 填充尚未完成 ⇒ 0（**不是** D 侧的组合「数据可用」）
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ready))    == 0u);

    // cycle 2: AR 握手 ⇒ 下一拍 `fill_active` 置位
    sim.set_input_value(icache.io().ar_ready, 1u);
    sim.tick();

    // cycle 3..6: 4 个 R beat（lane 0 = 0xDEADBEEF）
    sim.set_input_value(icache.io().req,      0u);
    sim.set_input_value(icache.io().ar_ready, 0u);
    for (int b = 0; b < 4; ++b) {
        sim.set_input_value(icache.io().r_valid, 1u);
        sim.set_input_value(icache.io().r_data,  b == 0 ? 0xDEADBEEFu : 0u);
        sim.set_input_value(icache.io().r_last,  b == 3 ? 1u : 0u);
        sim.tick();
    }
    sim.set_input_value(icache.io().r_valid, 0u);
    sim.set_input_value(icache.io().r_last,  0u);
    sim.set_input_value(icache.io().r_data,  0u);

    // cycle 8: 同址再读 → hit, 数据来自 cache (非 r_data)
    sim.set_input_value(icache.io().addr,     0x1000u);
    sim.set_input_value(icache.io().req,      1u);
    sim.set_input_value(icache.io().ar_ready, 0u);
    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().miss))     == 0u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().data))     == 0xDEADBEEFu);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(icache.io().ar_valid)) == 0u);

    // Rv32iPipelineWithCache 已删除，未来重建后恢复流水线联调
    SKIP("Rv32iPipelineWithCache not implemented; cache-level assertions above passed.");
}

TEST_CASE("Cache-Pipeline - D-Cache Integration", "[integration][dcache]") {
    // ⚠️ **2026-11-23（S3）重写**：`DCacheSync` 的 AXI 侧已是 **AXI4 full 5 通道**
    //    ⇒ ⇒ 读 miss 需 **4 个 R beat**；`ready`/`miss` 是**组合**（对当前 addr），
    //    `rdata` 是**寄存**（完成那拍捕获、保持到下一次读完成）。
    ch::core::context ctx("dcache_pipe");
    ch::core::ctx_swap swap(&ctx);

    ch::ch_device<DCacheSync> dev;
    ch::Simulator sim(dev.context());
    auto& dcache = dev.instance();

    // 默认态（未驱动端口读到未定义值 ⇒ ⇒ 全部显式驱动）
    sim.set_input_value(dcache.io().wdata, 0u);
    sim.set_input_value(dcache.io().strbe, 0xFu);
    sim.set_input_value(dcache.io().ar_ready, 0u);
    sim.set_input_value(dcache.io().r_valid,  0u);
    sim.set_input_value(dcache.io().r_data,   0u);
    sim.set_input_value(dcache.io().r_last,   0u);
    sim.set_input_value(dcache.io().r_resp,   0u);
    sim.set_input_value(dcache.io().aw_ready, 0u);
    sim.set_input_value(dcache.io().w_ready,  0u);
    sim.set_input_value(dcache.io().b_valid,  0u);
    sim.set_input_value(dcache.io().b_resp,   0u);

    // cycle 1: 冷读 ⇒ miss + AR（4-beat burst · line 基址对齐）
    // ⚠️ 本拍**不拉** `ar_ready` ⇒ 观察 `ar_valid` 保持（AXI：VALID 不等 READY）
    sim.set_input_value(dcache.io().addr,     0x2000u);
    sim.set_input_value(dcache.io().req,      1u);
    sim.set_input_value(dcache.io().we,       0u);
    sim.set_input_value(dcache.io().ar_ready, 0u);
    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().miss))     == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().ar_valid)) == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().ar_addr))  == 0x2000u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().ar_len))   == 3u);
    // ⚠️ miss ⇒ `ready=0`（流水线用 S2 的 `data_ready` 反压等待，不是固定延迟）
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().ready))    == 0u);

    // cycle 2: `ar_ready=1` ⇒ **AR 握手**（不握手则 fill 永不启动 ⇒ beat 全丢）
    sim.set_input_value(dcache.io().ar_ready, 1u);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().ar_valid)) == 0u);

    // cycle 3..6: 4 个 R beat（lane0 = 0x12345678，其余 0）
    sim.set_input_value(dcache.io().req,      0u);
    sim.set_input_value(dcache.io().ar_ready, 0u);
    for (int b = 0; b < 4; ++b) {
        sim.set_input_value(dcache.io().r_valid, 1u);
        sim.set_input_value(dcache.io().r_data,  b == 0 ? 0x12345678u : 0u);
        sim.set_input_value(dcache.io().r_last,  b == 3 ? 1u : 0u);
        sim.tick();
    }
    sim.set_input_value(dcache.io().r_valid, 0u);
    sim.set_input_value(dcache.io().r_last,  0u);

    // cycle 6: 同址再读 ⇒ hit（ready 与请求**同拍**）
    sim.set_input_value(dcache.io().addr, 0x2000u);
    sim.set_input_value(dcache.io().req,  1u);
    sim.set_input_value(dcache.io().we,   0u);
    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().miss))  == 0u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().ready)) == 1u);

    // cycle 7: `rdata` 在**下一拍**可用（寄存输出）
    sim.tick();
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().rdata)) == 0x12345678u);

    // cycle 8: 写（write-through 单 beat）⇒ AW 握手那拍完成
    sim.set_input_value(dcache.io().addr,     0x6000u);
    sim.set_input_value(dcache.io().wdata,    0xCAFEF00Du);
    sim.set_input_value(dcache.io().req,      1u);
    sim.set_input_value(dcache.io().we,       1u);
    sim.set_input_value(dcache.io().aw_ready, 1u);
    sim.set_input_value(dcache.io().w_ready,  1u);
    sim.tick();

    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().aw_valid)) == 1u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().aw_addr))  == 0x6000u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().aw_len))   == 0u);
    REQUIRE(static_cast<uint64_t>(sim.get_port_value(dcache.io().w_data))   == 0xCAFEF00Du);

    SKIP("Rv32iPipelineWithCache not implemented; cache-level assertions above passed.");
}

