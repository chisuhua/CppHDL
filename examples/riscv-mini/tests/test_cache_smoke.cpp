/**
 * @file test_cache_smoke.cpp
 * @brief riscv-mini cache wrapper: 可实例化 + **行为**断言
 *
 * 修复历史 (2026-10-20, change `fix-riscv-mini-cache-prefetch-sigsegv`):
 *   - i_cache_prefetch.h 旧版用 `ch_reg<ch_bool>[N]` + runtime init for-loop
 *     → SIGSEGV (cache-complete-doa-note §6.1 R1 C-style 数组架构 bug)
 *   - 改用 `ch_mem<ch_uint<1>, N>` 替代, 默认 0, instantiate OK
 *
 * d_cache_write_buffer.h review (2026-10-20): buffer 用普通 struct 数组
 * (WriteBufferEntry buffer[4]), 不是 ch_reg 数组, **未发现 SIGSEGV bug**.
 *
 * ⚠️ 2026-10-23（change `riscv-mini-test-triage-2026-10`，第四轮对抗审稿
 *    SHOULD_FIX #1）：原实现只有 `REQUIRE(dev.context() != nullptr)` ——
 *    该断言**不可证伪**（`ch_device` 传的是自己的非空 context，构造成功即恒
 *    非 null；构造失败时 ctor 就崩了，断言根本执行不到）⇒ 信息量等同于被删掉
 *    的那个 build-probe 用例。现改为**可证伪的行为断言**。
 *
 * ⚠️ 实测挖出的缺陷（本 change 不修，已登记在 proposal §登记的框架缺陷 #5）：
 *    两个 wrapper 里 `if (io().req == ch_bool(true) && io().we == ch_bool(true))`
 *    这类分支，其条件在 **建图期**求值（`ch_bool` 有 `explicit operator bool()`
 *    ⇒ C++ `if` 拿到的是「当前节点是否非零」，而不是一个运行期条件）⇒
 *    **分支体内的 `io().xxx <<=` 从不发射** ⇒ `DCacheWriteBuffer::ready` /
 *    两个 wrapper 的 `axi_valid` **恒为未驱动**（读到 0）。
 *    ⇒ 下面用 `miss`（无条件驱动，`i_cache_prefetch.h:126`）作为**主**行为断言；
 *      对未驱动的端口则钉住「它确实是未驱动」这一事实（缺陷修好后本断言会转红
 *      ⇒ 那时应连同缺陷登记一起更新）。
 *
 * 作者: build-agent (cache-complete-rewrite Session B, handoff §3.4)
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "simulator.h"

#include "cpu/cache/i_cache_prefetch.h"
#include "cpu/cache/d_cache_write_buffer.h"

using namespace ch::core;
using namespace chlib;

static inline bool to_bool(const sdata_type& val) { return val != 0; }

// ===== 【FIXED via ch_mem】实例化 + 冷 miss 透出 =====
TEST_CASE("ICachePrefetch instantiation + cold-miss surfaced",
          "[riscv-mini][cache][smoke][fixed]") {
    ch::ch_device<ICachePrefetch> dev;
    ch::Simulator sim(dev.context());

    sim.set_input_value(dev.instance().io().addr, 0x80000000);
    sim.set_input_value(dev.instance().io().req, 1);
    sim.set_input_value(dev.instance().io().axi_ready, 1);
    sim.set_input_value(dev.instance().io().axi_resp_valid, 0);
    sim.set_input_value(dev.instance().io().axi_data, 0);
    sim.tick();

    // `io().miss <<= cache_miss` 是**无条件**驱动（`i_cache_prefetch.h:126`）
    // ⇒ wrapper 内部 `icache.io().addr` 从未被连接（恒 0）⇒ 冷缓存必 miss。
    // 这条断言钉住「内层 miss 被如实透出到顶层端口」。变异（改成恒 0）即转红。
    REQUIRE(to_bool(sim.get_port_value(dev.instance().io().miss)) == true);
}

// ===== 【no SIGSEGV per review】写缓冲：未驱动端口的当前事实 =====
TEST_CASE("DCacheWriteBuffer instantiation + documented-undriven outputs",
          "[riscv-mini][cache][smoke][no-issue]") {
    ch::ch_device<DCacheWriteBuffer> dev;
    ch::Simulator sim(dev.context());

    sim.set_input_value(dev.instance().io().addr, 0x2000);
    sim.set_input_value(dev.instance().io().wdata, 0xCAFE);
    sim.set_input_value(dev.instance().io().req, 1);
    sim.set_input_value(dev.instance().io().we, 1);
    sim.set_input_value(dev.instance().io().axi_ready, 1);
    sim.set_input_value(dev.instance().io().axi_resp_valid, 0);
    sim.tick();

    // ⚠️ 见文件头「实测挖出的缺陷」：`ready` / `axi_valid` 只在**建图期求值**的
    //    `if` 分支里被驱动 ⇒ 分支永不发射 ⇒ 两者恒为未驱动（读到 0）。
    //    断言它们为 0 = 钉住**当前事实**（不是「这是对的」）；缺陷修好后本断言
    //    会转红，那时 MUST 连同 proposal §登记的框架缺陷 #5 一起更新。
    REQUIRE(to_bool(sim.get_port_value(dev.instance().io().ready)) == false);
    REQUIRE(to_bool(sim.get_port_value(dev.instance().io().axi_valid)) == false);
}
