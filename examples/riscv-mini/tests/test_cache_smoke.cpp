/**
 * @file test_cache_smoke.cpp
 * @brief riscv-mini cache wrapper: 可实例化 + **行为**断言（组合逻辑真发射）
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
 * ⚠️⚠️ 2026-10-28（change `riscv-mini-cache-wrapper-live-logic-2026-10`）：
 *    两个 wrapper 里 `if (io().req == ch_bool(true) && io().we == ch_bool(true))`
 *    这类分支，其条件在 **建图期**求值（`ch_bool` 有 `explicit operator bool()`
 *    ⇒ C++ `if` 拿到的是「当前节点是否非零」，而不是一个运行期条件）⇒
 *    **分支体内的 `io().xxx <<=` 从不发射** ⇒ `DCacheWriteBuffer::ready` /
 *    两个 wrapper 的 `axi_valid` / `axi_addr` / `axi_wdata` / `axi_we`
 *    **恒为未驱动**（读到 0）。
 *    ⇒ **该缺陷已修**（改用无条件组合赋值）⇒ 本文件原先钉住「未驱动 ⇒ 读 0」
 *      的两条断言**已反转**，且**反转方式**是「**同一 TEST_CASE 内依次改变
 *      输入 ⇒ 逐个输出断言跟随自己的输入变化**」——
 *      **不是**把 `== false` 改成 `== true`（后者只证明「有驱动」，
 *      证不出「随输入变化」）。
 *    ⇒ 另补了**判别力最强**的一条：内层 cache 的 **fill → hit** 路径
 *      （`axi_resp_valid` + `axi_data` ⇒ 内层填充 ⇒ 再读同一块 ⇒
 *      **hit** ⇒ `miss` 变 0 且 `data` 返回**缓存值**）
 *      ⇒ 只有「内层 cache 的输入真的被驱动了」才会发生 ⇒ 这是
 *      「内层 5 个输入端口 MUST 被全部驱动」这条契约的**可执行**证据。
 *
 * 作者: build-agent (cache-complete-rewrite Session B, handoff §3.4)
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "simulator.h"

#include "dual_backend_fixture.h"
#include "hier_corpus_fixture.h"

#include "cpu/cache/i_cache_prefetch.h"
#include "cpu/cache/d_cache_write_buffer.h"

using namespace ch::core;
using namespace chlib;

static inline bool to_bool(const sdata_type& val) { return val != 0; }

// ⚠️ **双后端「真的跑了」的自证**（2026-10-28，对抗审稿 SHOULD_FIX）：
// `tests/dual_backend_fixture.h` 的 verilator 半边在「verilator 不在 PATH」或
// 「`try_initialize` 失败」时是 **WARN + 提前 return** ⇒ **不是失败**
// ⇒ 半边被跳过时测试**照样全绿**（capability `dual-backend-test-coverage` 的
// R4/R5 就是这么规定的）⇒ 「8 个用例全绿」**本身不证明** verilator 半边执行过。
// ⇒ 本宏在每个用例体里打印**当前后端** ⇒ 失败时能从输出直接看出是哪个后端；
// ⇒ **正向证据**取自**断言数差**（实测）：
//    verilator 在 PATH ⇒ **58 断言**；不在 PATH（半边 WARN 跳过）⇒ **29 断言**
//    ⇒ 差值 **29** = verilator 半边 ⇒ 它**真的执行了**。
//    ⚠️ 不要用「输出里有 `Verilator Report` 编译行」当证据 —— Verilator 的
//    编译产物按 SHA-1 **缓存** ⇒ 第二次运行**不会**打印编译行（但半边**确实**
//    跑了，用的是缓存的 `libVtop.so`）。
#define CACHE_SMOKE_BACKEND_INFO()                                             \
    INFO("backend = "                                                         \
         << (ch_test::dual_backend::current_kind() ==                         \
                     ch_test::dual_backend::BackendKind::Verilator             \
                 ? "verilator"                                                \
                 : "interpreter"))

// ===========================================================================
// ① 【FIXED via ch_mem】实例化 + 冷 miss 透出
//
// ⚠️ 主 spec `cpu-cache-subsystem` 的 Scenario「instantiation smoke 测试必通过」
//    依赖 **`[fixed]` 这个 tag** ⇒ 本用例 MUST 保留该 tag。
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(ICachePrefetch,
                        "ICachePrefetch instantiation + cold-miss surfaced",
                        "[riscv-mini][cache][smoke][fixed][dual][verilator][e2e]",
                        [&]() {
    CACHE_SMOKE_BACKEND_INFO();
    auto &io = dev.instance().io();
    sim.set_input_value(io.addr, 0x80000000u);
    sim.set_input_value(io.req, 1u);
    sim.set_input_value(io.axi_ready, 1u);
    sim.set_input_value(io.axi_resp_valid, 0u);
    sim.set_input_value(io.axi_data, 0u);
    sim.tick();

    // 冷缓存必 miss（内层的 valid 位从未被置 1 ⇒ 永远不可能 hit）
    // ⚠️ 反向期望：**若内层被真正驱动**（`addr`/`req` 接上、且 `axi_resp_valid`
    //    拉高过）⇒ 内层会填充 ⇒ 再读同一块会 hit ⇒ `miss` 变 0。
    //    （「未驱动 ⇒ miss 恒 0」这个说法**不准确**：未驱动端口读到的是
    //      **未定义值**，不保证是 0 ⇒ 判别力由 test ③ 的 fill→hit 路径承担。）
    REQUIRE(to_bool(sim.get_port_value(io.miss)) == true);
});

// ===========================================================================
// ② ICachePrefetch：**每个输出都跟随自己的输入**
//
// ⚠️ 判别力来源：每条断言都带**反向期望**（写坏时会是多少），
//    且「跟随」是**逐个输入 → 逐个输出**的粒度 ——
//    只断言「输出非零」只能证明「有驱动」，证不出「随输入变化」。
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(ICachePrefetch,
                        "ICachePrefetch outputs follow their own inputs",
                        "[riscv-mini][cache][wrapper][live][dual][verilator][e2e]",
                        [&]() {
    CACHE_SMOKE_BACKEND_INFO();
    auto &io = dev.instance().io();
    // ⚠️ 刻意用**不对齐**的地址（`0x…42` / `0x…52`）⇒ 断言才能区分
    //    「**块对齐**」与「直通原值」两种语义 ⇒ 若用已对齐地址
    //    （`0x…40` / `0x…50`），两种语义**给出同一个值** ⇒ 零判别力。
    sim.set_input_value(io.addr, 0x80000042u);
    sim.set_input_value(io.req, 1u);
    sim.set_input_value(io.axi_ready, 1u);
    sim.set_input_value(io.axi_resp_valid, 0u);
    sim.set_input_value(io.axi_data, 0x11111111u);
    sim.tick();

    // ① `axi_addr` = 被请求地址的**块地址**（`addr & 0xFFFFFFF0`）
    //    ⇒ 块地址 **≠** 原地址（`0x80000042` → `0x80000040`）⇒ 钉住「对齐」
    //    ⚠️ 反向期望：未驱动 ⇒ `0`；不跟随 ⇒ 恒为基线值
    REQUIRE(sim.get_port_value(io.axi_addr) == 0x80000040u);
    // ② `axi_valid` 跟随 `req`（内层 `miss` 恒 1 ⇒ `axi_valid = miss & req`）
    //    ⚠️ 反向期望：未驱动 ⇒ 恒 `0`
    REQUIRE(to_bool(sim.get_port_value(io.axi_valid)) == true);
    // ③ `data` 跟随 `axi_data`（内层 `hit = 0` ⇒ `data = axi_data`）
    //    ⚠️ 反向期望：未驱动 ⇒ 恒 `0`
    REQUIRE(sim.get_port_value(io.data) == 0x11111111u);
    // ④ `ready` 跟随 `axi_ready`（内层 `hit = 0` ⇒ `ready = axi_ready`）
    //    ⚠️ 反向期望：未驱动 ⇒ 恒 `0`
    REQUIRE(to_bool(sim.get_port_value(io.ready)) == true);

    // ---- 逐个改输入 ⇒ 对应输出 MUST 跟随 ----
    // ⑤ 换块地址 ⇒ `axi_addr` 跟随（仍**不对齐** ⇒ 仍能验出「对齐」语义）
    sim.set_input_value(io.addr, 0x80000052u);
    sim.tick();
    //    ⚠️ 反向期望：恒 `0x80000040`（不跟随）；或 `0x80000052`（未对齐 = 直通）
    REQUIRE(sim.get_port_value(io.axi_addr) == 0x80000050u);

    // ⑥ 拉低 `req` ⇒ `axi_valid` 变 0
    sim.set_input_value(io.req, 0u);
    sim.tick();
    //    ⚠️ 反向期望：恒 `1`（不跟随）
    REQUIRE(to_bool(sim.get_port_value(io.axi_valid)) == false);

    // ⑦ 拉低 `axi_ready` ⇒ `ready` 变 0，但 **`axi_valid` 不变**
    //    ⇒ 证明 `axi_valid` **不依赖** `axi_ready`
    //      （AXI：「VALID **不得**等 READY 才拉高」⇒ READY 为低时该请求
    //        仍 MUST 保持在总线上等待握手）
    sim.set_input_value(io.req, 1u);
    sim.set_input_value(io.axi_ready, 0u);
    sim.tick();
    //    ⚠️ 反向期望：`ready` 恒 `1`；`axi_valid` 恒 `0`（引入了 READY 依赖）
    REQUIRE(to_bool(sim.get_port_value(io.ready)) == false);
    REQUIRE(to_bool(sim.get_port_value(io.axi_valid)) == true);

    // ⑧ 改 `axi_data` ⇒ `data` 跟随
    sim.set_input_value(io.axi_data, 0x22222222u);
    sim.tick();
    //    ⚠️ 反向期望：恒 `0x11111111`（不跟随）
    REQUIRE(sim.get_port_value(io.data) == 0x22222222u);
});

// ===========================================================================
// ③ ICachePrefetch：内层 cache 被**真正驱动**（hit / miss 可区分）
//
// ⚠️ 这是本文件**判别力最强**的一条：内层 cache 的 `addr` / `req` /
//    `axi_ready` / `axi_data` / `axi_resp_valid` **全部未驱动**时，
//    内层永远停在「冷 miss」⇒ 「内层输入 MUST 被全部驱动」这条契约
//    **完全不可观测** ⇒ 本用例把「填充 → 命中」这条**真实**路径跑出来。
//
// ⚠️ 内层是 **1-cycle `sread`** ⇒ hit 的到达拍数取决于内层实现
//    （**不属于**本 wrapper 的契约）⇒ 用**有界等待**而不是钉死拍数；
//    契约本身（hit **可达**）由 `REQUIRE` 保证。
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(ICachePrefetch,
                        "ICachePrefetch inner cache is really driven "
                        "(hit/miss distinguishable)",
                        "[riscv-mini][cache][wrapper][live][dual][verilator][e2e]",
                        [&]() {
    CACHE_SMOKE_BACKEND_INFO();
    auto &io = dev.instance().io();
    const uint64_t kBlock = 0x80000040u;
    const uint64_t kOtherBlock = 0x80000050u;
    const uint64_t kCached = 0xAAAA0000u;

    sim.set_input_value(io.addr, kBlock);
    sim.set_input_value(io.req, 1u);
    sim.set_input_value(io.axi_ready, 1u);
    sim.set_input_value(io.axi_resp_valid, 0u);
    sim.set_input_value(io.axi_data, 0u);
    sim.tick();

    // 冷 ⇒ miss（且 `axi_valid` 拉高 = 向 AXI 要数据）
    // ⚠️ 反向期望：内层未被驱动 ⇒ `miss` 恒 `0`
    REQUIRE(to_bool(sim.get_port_value(io.miss)) == true);
    REQUIRE(to_bool(sim.get_port_value(io.axi_valid)) == true);

    // ---- 触发内层填充（`axi_resp_valid` + `axi_data`）⇒ 下一轮读同一块应 hit ----
    sim.set_input_value(io.axi_resp_valid, 1u);
    sim.set_input_value(io.axi_data, kCached);
    bool hit = false;
    for (unsigned i = 0; i < 8u; ++i) {
        sim.tick();
        if (!to_bool(sim.get_port_value(io.miss))) { hit = true; break; }
    }
    // ⚠️ 反向期望：内层 `axi_resp_valid`/`axi_data` 未被驱动 ⇒ 永远 miss ⇒ 恒 `false`
    REQUIRE(hit == true);
    // hit ⇒ 不再向 AXI 要数据（`axi_valid` 落 0）
    // ⚠️ 反向期望：恒 `1`
    REQUIRE(to_bool(sim.get_port_value(io.axi_valid)) == false);

    // ---- 停 fill + 把 `axi_data` 清 0 ⇒ `data` 仍返回**缓存里的值** ----
    // ⇒ 这证明读的是 **cache** 而不是 AXI 数据 ⇒ 内层真的被填充了
    sim.set_input_value(io.axi_resp_valid, 0u);
    sim.set_input_value(io.axi_data, 0u);
    sim.tick();
    sim.tick();
    // ⚠️ 反向期望：恒 `0`（`axi_data` = 0 ⇒ 若读的是 AXI 数据就必须是 0）
    REQUIRE(sim.get_port_value(io.data) == kCached);

    // ---- 换块 ⇒ 回到 miss（证明 hit 不是「恒 0」）----
    sim.set_input_value(io.addr, kOtherBlock);
    bool back_to_miss = false;
    for (unsigned i = 0; i < 8u; ++i) {
        sim.tick();
        if (to_bool(sim.get_port_value(io.miss))) { back_to_miss = true; break; }
    }
    // ⚠️ 反向期望：恒 `false`（`miss` 粘在 0）
    REQUIRE(back_to_miss == true);
});

// ===========================================================================
// ④ DCacheWriteBuffer：**每个输出都跟随自己的输入**
//
// ⚠️ `ready` / `axi_valid` / `axi_addr` / `axi_wdata` / `axi_we` ——
//    修复前：`ready` / `axi_valid` 恒未驱动；`axi_addr` / `axi_wdata` 的驱动
//    **被注释掉**；`axi_we` **从未被驱动过** ⇒ 5 个输出里 5 个都是死的。
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(DCacheWriteBuffer,
                        "DCacheWriteBuffer outputs follow their own inputs",
                        "[riscv-mini][cache][wrapper][live][dual][verilator][e2e]",
                        [&]() {
    CACHE_SMOKE_BACKEND_INFO();
    auto &io = dev.instance().io();
    // ⚠️ 刻意用**不对齐**的地址（`0x2003` / `0x3005`）⇒ 断言才能区分
    //    「**直通原值**」与「块对齐」两种语义 ⇒ `DCacheWriteBuffer` 的契约是
    //    **直通**（**不**做块对齐）⇒ `axi_addr` MUST **等于** `addr` 输入。
    sim.set_input_value(io.addr, 0x2003u);
    sim.set_input_value(io.wdata, 0xCAFEu);
    sim.set_input_value(io.req, 1u);
    sim.set_input_value(io.we, 1u);
    sim.set_input_value(io.axi_ready, 1u);
    sim.set_input_value(io.axi_resp_valid, 0u);
    sim.tick();

    // ① `ready` = `req & we & axi_ready`（语义 =「本次写已被 AXI 接受」）
    //    ⚠️ 反向期望：恒 `0`（未驱动）
    REQUIRE(to_bool(sim.get_port_value(io.ready)) == true);
    // ② `axi_valid` = `req & we`
    //    ⚠️ 反向期望：恒 `0`（未驱动）
    REQUIRE(to_bool(sim.get_port_value(io.axi_valid)) == true);
    // ③ `axi_addr` 跟随 `addr`（直通式 ⇒ **不做**块对齐 ⇒ 保留 offset 位）
    //    ⚠️ 反向期望：恒 `0`（驱动被注释掉）；或被对齐成 `0x2000`（错加了对齐）
    REQUIRE(sim.get_port_value(io.axi_addr) == 0x2003u);
    // ④ `axi_wdata` 跟随 `wdata`
    //    ⚠️ 反向期望：恒 `0`（驱动被注释掉）
    REQUIRE(sim.get_port_value(io.axi_wdata) == 0xCAFEu);
    // ⑤ `axi_we` 跟随 `we`
    //    ⚠️ 反向期望：恒 `0`（**从未**被驱动过）
    REQUIRE(to_bool(sim.get_port_value(io.axi_we)) == true);

    // ---- 逐个改输入 ⇒ 对应输出 MUST 跟随 ----
    // ⑥ 拉低 `req` ⇒ `ready` / `axi_valid` 变 0
    sim.set_input_value(io.req, 0u);
    sim.tick();
    //    ⚠️ 反向期望：恒 `1`（不跟随）
    REQUIRE(to_bool(sim.get_port_value(io.ready)) == false);
    REQUIRE(to_bool(sim.get_port_value(io.axi_valid)) == false);

    // ⑦ 拉低 `we` ⇒ `axi_we` 变 0（且 `axi_valid` 变 0）
    sim.set_input_value(io.req, 1u);
    sim.set_input_value(io.we, 0u);
    sim.tick();
    //    ⚠️ 反向期望：恒 `1`（不跟随）
    REQUIRE(to_bool(sim.get_port_value(io.axi_we)) == false);
    REQUIRE(to_bool(sim.get_port_value(io.axi_valid)) == false);

    // ⑧ 拉低 `axi_ready` ⇒ `ready` 变 0，但 **`axi_valid` 不变**
    //    ⇒ 证明 `axi_valid` **不依赖** `axi_ready`
    //      （AXI：「VALID **不得**等 READY 才拉高」⇒ 若含 READY，
    //        READY 为低时这次写**根本不出现在 AXI 总线上**
    //        ⇒ 而本 wrapper **无状态/队列**可保留它 ⇒ **写被静默丢弃**）
    sim.set_input_value(io.we, 1u);
    sim.set_input_value(io.axi_ready, 0u);
    sim.tick();
    //    ⚠️ 反向期望：`ready` 恒 `1`；`axi_valid` 恒 `0`（引入了 READY 依赖）
    REQUIRE(to_bool(sim.get_port_value(io.ready)) == false);
    REQUIRE(to_bool(sim.get_port_value(io.axi_valid)) == true);

    // ⑨ 改 `addr` / `wdata` ⇒ `axi_addr` / `axi_wdata` 跟随
    sim.set_input_value(io.addr, 0x3005u);
    sim.set_input_value(io.wdata, 0xBEEFu);
    sim.tick();
    //    ⚠️ 反向期望：恒 `0x2003` / `0xCAFE`（不跟随）；或被对齐成 `0x3000`
    REQUIRE(sim.get_port_value(io.axi_addr) == 0x3005u);
    REQUIRE(sim.get_port_value(io.axi_wdata) == 0xBEEFu);
});

// ═══════════════════════════════════════════════════════════════════════════
// 语料级层次化回归 · **产物级**（残留 1b · T0）
//
// ⚠️ **为什么这个 DUT 需要它**：`ICachePrefetch` 是**真实语料**里**唯一**「子组件 +
//   自带 `ch_mem`」的 DUT ⇒ ⇒ 它覆盖「mem 在模块内自用、跨层只走端口」的**合法**
//   形态（★ 对照 `src/hierarchy_analysis.cpp` 的「跨模块 mem」硬拒绝）
//   ⇒ ⇒ ⇒ 它是 10 个复合 DUT 里**结构最特别**的一个。
//
// ⚠️ **判据在共享 fixture 里**（`tests/hier_corpus_fixture.h`）⇒ ⇒ 这里只负责
//    「**用哪些真实 DUT**」这个事实。
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("corpus hierarchy regression (product level): ICachePrefetch",
          "[hierarchy][corpus][t0][riscv-mini][cache]") {
    const std::string dir = ch_test::make_temp_dir("_corpus_i_cache_prefetch");
    REQUIRE_FALSE(dir.empty());
    const auto r =
        ch_test::hier_corpus::check_product_level<ICachePrefetch>("i_cache_prefetch", dir);
    INFO("error: " << r.error);
    REQUIRE(r.ok);
    REQUIRE(r.module_count >= 2u);       // ★ ForceLayer 下真的分层了
    REQUIRE(r.output_port_count >= 1u);   // ★ 顶层有可观测的 output
    REQUIRE(r.deterministic);             // ★ 两次发射逐字节相同
    REQUIRE(r.differs_from_flat);         // ★ 分层真的改变了产物
    // ⚠️ Catch2 的 `REQUIRE` **不支持** `||` ⇒ ⇒ 先算布尔再断言（★ 仓内已登记的坑）
    const bool lint_accounted = r.lint_skipped || r.lint_ok;
    REQUIRE(lint_accounted);  // ★ verilator 不可用 ⇒ 显式 skip
}

// ═══════════════════════════════════════════════════════════════════════════
// 语料级层次化回归 · **行为级**（残留 1b · T1）
//
// ⚠️ **结构类 C3**：「root 持 `ch_mem` + 子组件带 AXI 端口」⇒ ⇒ `ICachePrefetch`
//   是**既有语料**里**唯一**这个形态的 DUT ⇒ ⇒ ⇒ 它覆盖「mem 在模块内自用、
//   跨层只走端口」的**合法**形态（★ 对照 `src/hierarchy_analysis.cpp` 的
//   「跨模块 mem」硬拒绝 ⇒ ⇒ 本形态证明「mem 不必跨层」也可行）。
//
// ⚠️ **已知观测偏差（★ 登记在 `tests/hier_corpus_registry.md`）**：内层 `miss`(1 拍
//   `sread`) 与 `axi_addr`(当拍地址) 存在 1 拍错位 ⇒ ⇒ ⇒ **比对时 MUST NOT 把它当
//   缺陷** ⇒ ⇒ 但它**不影响**本次比对（★ 比的是 flat ≡ hier ⇒ ⇒ 两侧**同样**错位）。
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("corpus hierarchy regression (behaviour level): ICachePrefetch - root mem + child AXI ports",
          "[hierarchy][corpus][t1][riscv-mini][cache][mem-in-module]") {
    // ⚠️ **verilator 不可用 ⇒ WARN-skip**（★ 与 `DUAL_BACKEND_TEST_PAIR` 的既有约定
    //    同款 ⇒ ⇒ 否则 macOS/Windows 的 `-DBUILD_VERILATOR=OFF` 本地开发会**新增硬
    //    失败**；⇒ ⇒ ★ 与本 change 的 T0（`lint_skipped` 显式分支）**自洽**）
    if (!ch_test::tool_available("verilator")) {
        WARN("verilator not on PATH; corpus hierarchy regression (behaviour level) skipped");
        return;
    }
    const std::string dir = ch_test::make_temp_dir("_corpus_t1_icache");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<ICachePrefetch> dev;

    const auto products =
        ch::hier_eq::emit(dev.instance(), dir, ch::Granularity::ForceLayer);
    REQUIRE(products.ok);

    std::vector<ch::hier_eq::DrivenInput> inputs;
    inputs.push_back(ch::hier_eq::DrivenInput{
        "addr", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().addr, v);
        }});
    inputs.push_back(ch::hier_eq::DrivenInput{
        "req", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().req, v);
        }});
    inputs.push_back(ch::hier_eq::DrivenInput{
        "axi_ready", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().axi_ready, v);
        }});
    inputs.push_back(ch::hier_eq::DrivenInput{
        "axi_resp_valid", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().axi_resp_valid, v);
        }});
    inputs.push_back(ch::hier_eq::DrivenInput{
        "axi_data", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().axi_data, v);
        }});

    // ⚠️ 观测**只**取**顶层**的 `ch_out`（★ 子组件的 `ch_out` 在层次化顶层**不可观测**
    //    ⇒ ⇒ 观测它 ⇒ ⇒ helper 会**机械拒绝**（前缀 `observation-not-visible`））
    std::vector<ch::hier_eq::Observation> observations;
    observations.push_back(ch::hier_eq::make_observation(
        "axi_addr", dev.instance().io().axi_addr,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().axi_addr));
        }));
    observations.push_back(ch::hier_eq::make_observation(
        "axi_valid", dev.instance().io().axi_valid,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().axi_valid));
        }));
    observations.push_back(ch::hier_eq::make_observation(
        "miss", dev.instance().io().miss,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().miss));
        }));

    // ⚠️ 激励逐拍变化（★ 地址变化 ⇒ ⇒ `axi_addr` 变化 ⇒ ⇒ 观测**携带信息**
    //    ⇒ ⇒ 否则被 `no-varying-signal` 拒绝）
    const std::vector<std::vector<std::uint64_t>> stim = {
        {0x80000000u, 1u, 1u, 0u, 0u},
        {0x80000004u, 1u, 1u, 0u, 0u},
        {0x80000008u, 1u, 1u, 0u, 0u},
        {0x8000000Cu, 1u, 1u, 0u, 0u},
        {0x80000010u, 0u, 1u, 0u, 0u},
        {0x80000014u, 1u, 1u, 0u, 0u},
    };
    const auto r = ch::hier_eq::compare(dev.context(), dir, products.hier_path, products.ports,
                                        inputs, observations, stim);
    INFO("error: " << r.error);
    INFO("first mismatch: " << r.first_mismatch);
    REQUIRE(r.ok);
    REQUIRE(r.equal);
    REQUIRE(r.mismatch_count == 0u);
}

