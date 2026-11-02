// ⚠️⚠️ **路线 A（E 批）—— `ICacheAsync` / `DCacheAsync` 的 0-cycle 语义进双后端 parity**
// （2026-11-02）
//
// ## 🔴 本文件同时是**一次契约矛盾的裁决装置**（这是它排第一的理由）
//
// capability `dual-backend-test-coverage` 里**两条条款自相矛盾**：
//   - Requirement「cache 双后端 MUST 按 Sync/Async 分类」明写：
//     Async dual-run **可以**走 parity，「『Async 强制 parity 会永远红』这一前提
//     **已不成立**，禁令解除」；
//   - 但另一个 Requirement 的 Scenario「spec R6 排除清单遵守」仍写：
//     runtime test 涉及 `ICacheAsync`/`DCacheAsync` ⇒ **MUST NOT** 用
//     `DUAL_BACKEND_TEST_PAIR`（理由还是那条**已被消除**的 skew）。
//
// ⇒ ⇒ **本文件用事实裁决它**：真绿 ⇒ 前提确已消除（Scenario 应订正）；
// 真红 ⇒ 前提未消除（禁令应保留）**且**发现一个真 parity 分歧（价值高于覆盖本身）。
// ⚠️ 订正 Scenario 时 **MUST NOT 改标题** —— CLI 按**标题**匹配 Scenario
//    （ADR-052 / ADR-053 已两次踩过）。
//
// ## 为什么这是「新文件」而不是「转换」
//
// ① 原用例存在且能跑（`tests/chlib/test_cache_complete.cpp` 的 `:303` / `:332` / `:611`）
//    ⇒ 本文件**沿用其激励序列**，但期望值来源升级为**手推**，并**新增**原用例
//    **从未断言**的维度（见下「增量」段）。
// ② 只取 **Async 3 个用例**；**Sync 6 个刻意不做** —— `tests/chlib/test_cache_sync.cpp`
//    已是 dual（10 pair）⇒ 转 Sync = **重复计数**（4 问 ①）。
// ⚠️ **原文件 MUST NOT 修改**。
//
// ---
// ## 🔴🔴 陷阱 T6（本文件实测挖出）：**填充在同一 `tick()` 内就可见** ⇒
// 「填充拍之后」`miss` / `axi_valid` **不可观测**
//
// 机制：`aread` 是 **0-cycle 组合**读，而填充是 `ch_mem::write`（**沿上**提交）；
// `Simulator::tick()` 的最后一步是 **`eval_combinational()`** ⇒ ⇒ 沿之后
// **再求值一次**组合逻辑 ⇒ ⇒ 于是「本拍刚写进去的 valid/tag」在**同一 tick 的
// 收尾求值里就被 `aread` 读到** ⇒ ⇒ `hit=1` ⇒ ⇒ **`miss` 已经是 0**、
// `axi_valid`（= miss & req）**已经是 0**。
// ⇒ ⇒ **要观测 `miss=1` / `axi_valid=1`，MUST 用「冷地址 + `axi_resp_valid=0`」的拍**
// （不填充 ⇒ 下一拍仍是 miss）⇒ ⇒ 这正是本文件**每一组的第一拍**在做的事。
// ⚠️ 我首版把「填充拍」的 `miss` 期望写成 1 ⇒ ⇒ **两后端一致地给 0** ⇒ ⇒
// 按「两后端一致 ⇒ 同一处推导错」判据，改的是**期望值**，不是实现。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 语义逐条对照 `include/cpu/cache/i_cache_async.h` / `d_cache_async.h`：
//
//     ICacheAsync:  hit = valid_way & (tag_way == tag) ; miss = !hit
//                   data  = hit ? cache_data : axi_data      ← **miss 直通 axi_data**
//                   ready = hit ? 1 : axi_ready              ← **miss 跟随 axi_ready**
//                   axi_valid = miss & req ; axi_addr = addr & 0xFFFFFFF0
//                   do_fill = axi_resp_valid & miss ; fill_way0_en = do_fill & !valid_way0
//     DCacheAsync:  另加
//                   rdata     = hit ? cache_data : axi_rdata
//                   ready     = is_read ? (hit ? 1 : axi_ready) : axi_ready   (is_read = !we)
//                   axi_valid = miss_read ? 1 : we            (miss_read = miss & is_read)
//                   axi_we = we ; axi_wdata = wdata
//                   do_write  = hit & we ; data_mem_wayN.write(..., do_write & hit_wayN)
//
// ### pair 1（`ICacheAsync`，addr 0x1000）
//   s1 **冷 + 不填充**（resp_valid=0, axi_ready=0, axi_data=0xAAAA1111）
//      ⇒ miss=1 · data=0xAAAA1111（**直通**）· ready=**0**（跟随 axi_ready）
//      · axi_valid=**1** · axi_addr=0x1000
//   s2 **冷 + 填充**（resp_valid=1, axi_ready=1, axi_data=0xDEADBEEF）
//      ⇒ 沿上填 way0 ⇒ 收尾求值已 hit ⇒ miss=**0** · data=0xDEADBEEF（**来自 cache**）
//      · ready=1 · axi_valid=**0** · axi_addr=0x1000
//   s3 **AXI 全关** ⇒ hit：miss=0 · data=0xDEADBEEF · ready=1 · axi_valid=0
//
// ### pair 2（`DCacheAsync` 读，addr 0x2000）
//   s1 冷 + 不填充（axi_rdata=0xAAAA2222, resp_valid=0, axi_ready=0）
//      ⇒ miss=1 · rdata=0xAAAA2222（**直通**）· ready=0 · axi_valid=1 · axi_we=0
//   s2 冷 + 填充（axi_rdata=0x12345678, resp_valid=1, axi_ready=1）
//      ⇒ miss=0 · rdata=0x12345678（来自 cache）· ready=1 · axi_valid=0 · axi_we=0
//   s3 AXI 全关 ⇒ miss=0 · rdata=0x12345678 · ready=1 · axi_valid=0 · axi_we=0
//
// ### pair 3（`DCacheAsync` 写直达，addr 0x3000）
//   c1 读冷 + 填充（axi_rdata=0xDEADC0DE, resp_valid=1, axi_ready=1）
//      ⇒ miss=0（T6：填充已可见）· ready=1 · rdata=0xDEADC0DE · axi_valid=0 · axi_we=0
//   c2 读 + AXI 关 ⇒ hit：miss=0 · ready=1 · rdata=0xDEADC0DE · axi_valid=0
//   c3 **写**（we=1, wdata=0xCAFEBABE, req=1, axi_ready=0）
//      ⇒ miss=0 · ready=**0**（is_read=0 ⇒ ready=axi_ready）· axi_we=1
//      · axi_wdata=0xCAFEBABE · axi_addr=0x3000 · axi_valid=1（=we）
//      · rdata=**0xCAFEBABE**（写沿上落地 ⇒ 收尾 aread 已读到新值，同 T6）
//   c4 读 + AXI 关 ⇒ miss=0 · ready=1 · rdata=**0xCAFEBABE** · axi_valid=0
//   c5 再读一次（= 原用例的观测点）⇒ 与 c4 相同（**持久性**断言）
//
// ## ⭐ 相对原用例的**增量**（原用例没断过的维度）
//   ① **`miss == 1` / `axi_valid == 1`**（原用例**从未**在 miss 拍断言 ⇒
//      填充拍的 miss 不可观测，见 T6）；
//   ② **`ready` 在 miss 时跟随 `axi_ready`**（原用例的 miss 拍 axi_ready 恒 1 ⇒
//      接反也看不出来）；
//   ③ **写直达真的写进了 cache**（c3/c4 的 `rdata == 0xCAFEBABE`；原用例只断
//      「hit 了 / axi_valid==0」⇒ 若 `do_write & hit_way0` 被写成 `& hit_way1`
//      ⇒ way0 永不写 ⇒ 原用例**仍然全绿**）；
//   ④ **`axi_wdata` / `axi_we` / `axi_addr`** 三个主端输出的取值。
//
// ⚠️ **陷阱 T3**：body 参数里**花括号内的逗号**会被预处理器当参数分隔 ⇒ 拍表放**文件作用域**。
// ⚠️ **MUST 每拍逐个显式驱动全部输入端口**（`ICacheAsync` 5 个 / `DCacheAsync` 7 个）。
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

// ⚠️🔴 **必须**先 include `*_sync.h` —— `i_cache_async.h` / `d_cache_async.h`
// **不自包含**：它们用 `ICacheConfig` / `DCacheConfig`（定义在 `*_sync.h`）却
// **不** include 它 ⇒ ⇒ 单独 include async 头**编译不过**（实测
// `'DCacheConfig' was not declared in this scope`）⇒ ⇒ 违反根 AGENTS.md 的
// CONVENTIONS「Headers self-contained」⇒ ⇒ **独立立项**（修它只需各加一行 include，
// 但会改变公开头的依赖图 ⇒ 不在本批做）。
#include "cpu/cache/d_cache_sync.h"
#include "cpu/cache/i_cache_sync.h"
#include "cpu/cache/d_cache_async.h"
#include "cpu/cache/i_cache_async.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

namespace {

// ── pair 1：ICacheAsync（s1 冷+不填充 / s2 冷+填充 / s3 hit / s4 `req=0` 门控）──
// ⚠️ **s4 用另一个冷地址（0x2000）+ `req=0`**：`axi_valid = miss & req` ⇒ ⇒
//    只有「**miss 且 req=0**」这一拍能把 `& req` 门控**区分**出来
//    （s1~s3 的 `req` 恒 1 ⇒ 去掉 `& req` 完全不可见）。
struct ICacheStep {
    uint64_t addr, req, axi_data, axi_resp_valid, axi_ready;
    uint64_t miss, data, ready, axi_valid, axi_addr;
};
const ICacheStep kICache[] = {
    {0x1000, 1, 0xAAAA1111, 0, 0, 1, 0xAAAA1111, 0, 1, 0x1000},  // s1 冷+不填充
    {0x1000, 1, 0xDEADBEEF, 1, 1, 0, 0xDEADBEEF, 1, 0, 0x1000},  // s2 冷+填充（T6）
    {0x1000, 1, 0x00000000, 0, 0, 0, 0xDEADBEEF, 1, 0, 0x1000},  // s3 hit
    {0x2000, 0, 0xBBBB2222, 0, 0, 1, 0xBBBB2222, 0, 0, 0x2000},  // s4 req=0 ⇒ axi_valid=0
};

// ── pair 2：DCacheAsync 读 ──
// ⚠️ **s3 用非对齐地址 0x2004**：`DCacheConfig` 是 `OFFSET_BITS=4 / INDEX_BITS=7`
//    ⇒ `index = addr[10:4]`、`tag = addr[31:11]` ⇒ ⇒ **同一 cache line 内偏移
//    不参与 hit 判定** ⇒ ⇒ s3 仍应 **hit**；而 `axi_addr` MUST 被**块对齐**
//    （`addr & 0xFFFFFFF0`）⇒ ⇒ 这一拍同时钉住「16 字节块粒度」与「块对齐掩码」
//    （⚠️ 首版三条向量**全是** 16 字节对齐的 ⇒ ⇒ 掩码接不接**都看不出来**）。
struct DCacheReadStep {
    uint64_t addr, req, we, wdata, axi_rdata, axi_resp_valid, axi_ready;
    uint64_t miss, rdata, ready, axi_valid, axi_we, axi_addr;
};
const DCacheReadStep kDCacheRead[] = {
    {0x2000, 1, 0, 0, 0xAAAA2222, 0, 0, 1, 0xAAAA2222, 0, 1, 0, 0x2000},  // s1 冷+不填充
    {0x2000, 1, 0, 0, 0x12345678, 1, 1, 0, 0x12345678, 1, 0, 0, 0x2000},  // s2 填充（T6）
    {0x2004, 1, 0, 0, 0x00000000, 0, 0, 0, 0x12345678, 1, 0, 0, 0x2000},  // s3 非对齐 ⇒ 仍 hit
};

// ── pair 3：DCacheAsync 写直达 ──
struct DCacheWriteStep {
    uint64_t req, we, wdata, axi_rdata, axi_resp_valid, axi_ready;
    uint64_t miss, ready, rdata, axi_valid, axi_we, axi_wdata, axi_addr;
};
const DCacheWriteStep kDCacheWrite[] = {
    //  c1 读冷+填充（T6：填充已可见 ⇒ miss=0）
    {1, 0, 0x00000000, 0xDEADC0DE, 1, 1, 0, 1, 0xDEADC0DE, 0, 0, 0x0, 0x3000},
    //  c2 读 + AXI 关 ⇒ hit
    {1, 0, 0x00000000, 0x00000000, 0, 0, 0, 1, 0xDEADC0DE, 0, 0, 0x0, 0x3000},
    //  c3 写 ⇒ ready=0（is_read=0）· axi_valid=1（=we）· 写沿上落地 ⇒ rdata 已是新值
    {1, 1, 0xCAFEBABE, 0x00000000, 0, 0, 0, 0, 0xCAFEBABE, 1, 1, 0xCAFEBABE, 0x3000},
    //  c4 读 + AXI 关 ⇒ ⚠️ 写直达真的落进了 cache（原用例没断过这一点）
    {1, 0, 0x00000000, 0x00000000, 0, 0, 0, 1, 0xCAFEBABE, 0, 0, 0x0, 0x3000},
    //  c5 再读一次 ⇒ 持久性
    {1, 0, 0x00000000, 0x00000000, 0, 0, 0, 1, 0xCAFEBABE, 0, 0, 0x0, 0x3000},
};

}  // namespace

// ───────────── pair 1：`ICacheAsync` miss 直通 → 填充 → hit ─────────────
DUAL_BACKEND_TEST_PAIR(
    chlib::ICacheAsync,
    "ICacheAsync（0-cycle aread）：miss 直通 axi_data / ready 跟随 axi_ready / 填充后 hit",
    "[cache][async][icache][miss-fill][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kICache) {
            sim.set_input_value(io.addr, tc.addr);
            sim.set_input_value(io.req, tc.req);
            sim.set_input_value(io.axi_data, tc.axi_data);
            sim.set_input_value(io.axi_resp_valid, tc.axi_resp_valid);
            sim.set_input_value(io.axi_ready, tc.axi_ready);
            sim.tick();
            INFO("addr=0x" << std::hex << tc.addr << " req=" << std::dec << tc.req
                           << " axi_resp_valid=" << tc.axi_resp_valid
                           << " axi_ready=" << tc.axi_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.miss)) == tc.miss);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data)) == tc.data);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.ready)) == tc.ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.axi_valid)) ==
                    tc.axi_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.axi_addr)) ==
                    tc.axi_addr);
        }
    });

// ───────────── pair 2：`DCacheAsync` 读 miss 直通 → 填充 → hit ─────────────
DUAL_BACKEND_TEST_PAIR(
    chlib::DCacheAsync,
    "DCacheAsync（0-cycle aread）读：miss 直通 axi_rdata / ready 跟随 axi_ready / 填充后 hit",
    "[cache][async][dcache][read][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kDCacheRead) {
            sim.set_input_value(io.addr, tc.addr);
            sim.set_input_value(io.req, tc.req);
            sim.set_input_value(io.we, tc.we);
            sim.set_input_value(io.wdata, tc.wdata);
            sim.set_input_value(io.axi_rdata, tc.axi_rdata);
            sim.set_input_value(io.axi_resp_valid, tc.axi_resp_valid);
            sim.set_input_value(io.axi_ready, tc.axi_ready);
            sim.tick();
            INFO("addr=0x" << std::hex << tc.addr << " axi_resp_valid=" << std::dec
                           << tc.axi_resp_valid << " axi_ready=" << tc.axi_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.miss)) == tc.miss);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rdata)) ==
                    tc.rdata);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.ready)) == tc.ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.axi_valid)) ==
                    tc.axi_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.axi_we)) ==
                    tc.axi_we);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.axi_addr)) ==
                    tc.axi_addr);
        }
    });

// ───────────── pair 3：`DCacheAsync` 写直达（含「写入真的落进 cache」）─────────────
DUAL_BACKEND_TEST_PAIR(
    chlib::DCacheAsync,
    "DCacheAsync 写直达：命中写更新 cache + AXI 主端，同一拍即可读到新值",
    "[cache][async][dcache][write-through][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kDCacheWrite) {
            sim.set_input_value(io.addr, 0x3000u);
            sim.set_input_value(io.req, tc.req);
            sim.set_input_value(io.we, tc.we);
            sim.set_input_value(io.wdata, tc.wdata);
            sim.set_input_value(io.axi_rdata, tc.axi_rdata);
            sim.set_input_value(io.axi_resp_valid, tc.axi_resp_valid);
            sim.set_input_value(io.axi_ready, tc.axi_ready);
            sim.tick();
            INFO("we=" << tc.we << " wdata=0x" << std::hex << tc.wdata);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.miss)) == tc.miss);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.ready)) == tc.ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rdata)) ==
                    tc.rdata);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.axi_valid)) ==
                    tc.axi_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.axi_we)) ==
                    tc.axi_we);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.axi_wdata)) ==
                    tc.axi_wdata);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.axi_addr)) ==
                    tc.axi_addr);
        }
    });
