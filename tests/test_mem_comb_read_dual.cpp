// ⚠️⚠️ **路线 A（D 批）—— `ch_mem` 写口 × `aread` 的交互进双后端 parity**
// （2026-11-02 · D 批第 2 个文件）
//
// ## 立项理由（不是凑数 —— 这里先交代「为什么只有 2 个 pair」）
//
// `tests/test_mem_comb_read.cpp` 有 8 个 TEST_CASE。逐个核对**双后端增量**后，
// 只有 **2 个**真正需要新增文件（下表）。这个结论与交接文档里「6 可转」的
// 初步判断**不同**，差别逐条登记如下（全部是**读实现 + 变异实测**得出的，
// 不是推断）：
//
// | 原用例 | 结论 | 依据 |
// |---|---|---|
// | `:203` aread 同拍可见 | ❌ **不转** | ① 断言形态是「输入端口驱动地址 → `tick()` → 读」⇒ **无法区分 0/1-cycle**（见下方 T2 段）；② 其数据通路（`aread` → `ch_out`，输入地址直连）**已在** `tests/test_mem_connection_dual.cpp` 的 pair 2 覆盖 |
// | `:247` sread 1-cycle | ❌ **不转** | 同上（数据通路对应 `test_mem_connection_dual.cpp` 的 pair 1） |
// | `:289` aread + 寄存器地址（0-cycle） | ❌ **不转** | **已被** `tests/test_mem_read_timing_dual.cpp` 覆盖，且用的是**同一判别形态**（寄存器驱动地址）与**同一组初值/期望值**（0xE5 / 0xD4 / 0xE5…）⇒ 再写一份 = **零增量重复** |
// | `:323` sread + 寄存器地址（1-cycle） | ❌ **不转** | 同上（该文件的 pair 同时断言 aread 与 sread 两侧） |
// | `:368` aread codegen emits assign | ❌ **不转** | **codegen 文本断言**，与跑哪个后端无关 ⇒ 双后端零增量 |
// | `:439` generated Verilog references no undeclared | ❌ **不转** | 同上 |
// | `:474` 跨拍写后读 | ✅ **转** | 裸 `ch_mem` 的**写口**在双后端下**没有任何文件覆盖**（实测：`test_mem_connection_dual` / `test_mem_read_timing_dual` / `test_mem_sread_disable_dual` 的 `.write(` 计数全为 **0**；`tests/chlib/test_memory_dual.cpp` 的写口是 `chlib::memory` 的 RAM 族，**不是** `ch_mem` 本身）⇒ 见 pair 1 |
// | `:531` 同拍写 + aread（写后值） | ✅ **转** | 同上 ⇒ 见 pair 2 |
//
// ## 🔴 T2：为什么 `:203` / `:247` 那类用例**不可能**有判别力（这条要记住）
//
// 断言形态 `set_input_value(addr) → tick() → get_port_value()` 对
// 「0-cycle vs 1-cycle」**完全不可判别**：地址是**输入端口**，TB 在 `tick()`
// **之前**就把新值写进 `data_map_` ⇒ 沿上采样到的是**新**地址
// ⇒ 「0-cycle 读 mem[新]」与「1-cycle 在沿上采样 mem[新]」**观测值相同**。
//
// ⚠️ **"tick 之间观测"也救不了**：`Simulator::get_port_value` 在两后端都**只读
// `data_map_`**（不查 live Vtop，`include/simulator.h:113-140`），而
// `set_input_value` 不触发组合求值 ⇒ 两侧都读到**上一拍**的值 ⇒ 对 0/1-cycle
// 同样不可判别。
//
// ⇒ **唯一能判别的形态 = 地址由「在同一条沿提交的 `ch_reg`」驱动**（沿前值 ≠
// 沿后值）⇒ 这正是 `test_mem_read_timing_dual.cpp` 已经做的事
// ⇒ ⇒ 所以本文件**不再重复时序**，只做写口交互。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 初始化（16 × 8bit，两两不同 ⇒ 任何地址错位立刻可见）：
//   [0]=0xA1 [1]=0xB2 [2]=0xC3 [3]=0xD4 [4]=0xE5 [5]=0xF6 [6]=0x07 [7]=0x18
//   [8]=0x29 [9]=0x3A [10]=0x4B [11]=0x5C [12]=0x6D [13]=0x7E [14]=0x8F [15]=0x90
//
// DUT：1 个 `aread`（0-cycle，地址 = `io.addr_async`）+ 1 个写口
//      （地址 = `io.addr_sync`，数据 = `io.wdata`，使能 = `io.we`）。
//
// ### pair 1（跨拍写后读 + `we=0` **不写**的负向对照）
//   tick0: addr_async=2, we=0                    ⇒ 0xC3
//   tick1: addr_async=5, addr_sync=5, we=0, wdata=0x00
//                                                ⇒ 0xF6（写之前 ⇒ 初值）
//                                                ⚠️ 若 `we` 被忽略 ⇒ 这一拍就会把
//                                                mem[5] 写成 0x00 ⇒ 读回 0x00（实测 M3 在此转红）
//   tick2: addr_sync=5, wdata=0xFF, we=1         ⇒ 写 mem[5]=0xFF（本拍不断言）
//   tick3: addr_async=5, addr_sync=5, wdata=0x77, **we=0**
//                                                ⇒ **0xFF**
//   ⚠️ 这条断言同时判别**三种**失败模式：
//      · 写没落地（写口接错 / 地址被忽略）⇒ 0xF6
//      · `we=0` 被忽略（恒写）⇒ **0x77**
//      · 写与读串到别的槽 ⇒ 其它初值
//   ⚠️ 原文件 `:474` **没有**这个负向对照（它的 `we=0` 拍从未验证「真的不写」）
//      ⇒ 本条是**新增**断言，故在文件头显式登记。
//
// ### pair 2（同拍「写 + aread」⇒ 读到**写后**值）
//   语义前提：`assign proxy = mem[addr];` 在 NBA 更新**之后**重算 ⇒ 当拍读到写后值。
//   tick0: addr_async=addr_sync=0, we=0, wdata=0（稳定拍，避免依赖未驱动端口）
//   tick1: we=1, wdata=0xEE, addr=2 ⇒ 0xEE  ⚠️ 1-cycle aread 会读沿前 mem[2]=0xC3
//   tick2: we=1, wdata=0x5A, addr=9 ⇒ 0x5A  ⚠️ 1-cycle ⇒ 0x3A
//   tick3: we=1, wdata=0x33, addr=0 ⇒ 0x33  ⚠️ 1-cycle ⇒ 0xA1
//   tick4: we=0, addr_async=2       ⇒ 0xEE（持久化 ⇒ 「只在写的那一拍可见」会被抓）
//
// ## 变异证据（每条变异**先确认仍可编译**；还原用 `cp` 后重建）
//   | 变异 | 内容 | 转红的 pair（实测行号） |
//   |---|---|---|
//   | M1 | codegen：`aread` 改emit `always_ff`（1-cycle） | pair 2（`:217`，verilator 半边） |
//   | M2 | interpreter：`aread` 归入 `sequential_instr_list_`（历史 drift） | pair 2（`:217`，interpreter 半边） |
//   | M3 | codegen：写口忽略 `we`（恒写） | pair 1（**`:185` 先红** —— 那一拍的 `we=0` 尝试把 mem[5] 写成 0x00）+ pair 2（`:224` 持久化拍被 0x00 覆盖） |
//   | M4 | codegen：写口忽略地址（恒写 slot 0） | pair 1（`:196`：读 addr 5 得到 `0xF6` 而非 `0xFF`）+ pair 2（`:217`） |
//   ⇒ 4 条变异**全部被抓**，且**每条 pair 都有 ≥2 条变异能抓它**（无填充 pair）。
//   ⚠️ 如实登记：M3 下 `:196` 那条断言**没有被独立触发**（`:185` 先失败并中止本
//      pair）⇒ 该断言仍是有效判据（语义上「we 被忽略 ⇒ 0x77」），只是本变异
//      在更早的一拍就已经把它区分出来了。
//
// ⚠️ **MUST 驱动全部 4 个输入端口**（`addr_sync` / `addr_async` / `wdata` /
//    `we`）：仓内 ANTI-PATTERNS 已登记「未驱动端口读到的是未定义值」，双后端下
//    两侧的未定义值未必相同 ⇒ 依赖默认值 = 让测试「碰巧」通过。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
//    本文件 = 3 + 4 = **7**（interpreter-only）/ **14**（双后端）。
//
// ⚠️ 原文件 `tests/test_mem_comb_read.cpp` **MUST NOT 修改**（它仍以单后端跑 8 个
//    用例，含 2 个 codegen 文本断言）⇒ 本文件只做**增量**。
#include "catch_amalgamated.hpp"

#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/mem.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ⚠️ **用例表 MUST 放在文件作用域**（陷阱 T3）：`DUAL_BACKEND_TEST_PAIR` 的
//    body 参数里**花括号内的逗号**会被预处理器当成**参数分隔**（只有圆括号保护）。

// ── DUT：1 个 aread + 1 个写口 ──
class MemWriteReadDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr_sync; ch_in<ch_uint<4>> addr_async;
         ch_in<ch_uint<8>> wdata; ch_in<ch_bool> we;
         ch_out<ch_uint<8>> rdata_async;)

    MemWriteReadDut(ch::Component *parent = nullptr,
                    const std::string &name = "mem_write_read")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 16 个两两不同的初值（见文件头）⇒ 地址错位立刻可见。
        const std::vector<uint64_t> init = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6,
                                            0x07, 0x18, 0x29, 0x3A, 0x4B, 0x5C,
                                            0x6D, 0x7E, 0x8F, 0x90};
        ch_mem<ch_uint<8>, 16> mem(init, "wr_read_mem");

        // aread：0-cycle（`assign proxy = mem[addr];`）
        io().rdata_async <<= mem.aread(io().addr_async, "async_port");

        // 写口（地址用 addr_sync；`we` 由 TB 驱动）
        mem.write(ch_uint<4>(io().addr_sync.impl()),
                  ch_uint<8>(io().wdata.impl()), ch_bool(io().we.impl()));
    }
};

// 全部 4 个输入端口的一次性驱动（避免任何「未驱动端口」）。
struct Inputs {
    uint64_t addr_sync;
    uint64_t addr_async;
    uint64_t wdata;
    uint64_t we;
};

void drive(ch::Simulator &sim, MemWriteReadDut::io_type &io, const Inputs &in) {
    sim.set_input_value(io.addr_sync, in.addr_sync);
    sim.set_input_value(io.addr_async, in.addr_async);
    sim.set_input_value(io.wdata, in.wdata);
    sim.set_input_value(io.we, in.we);
}

uint64_t rd(ch::Simulator &sim,
            const ch::core::port<ch_uint<8>,
                                 ch::core::output_direction> &p) {
    return static_cast<uint64_t>(sim.get_port_value(p));
}

// ── pair 2 的同拍写序列（addr, wdata）──
struct RaceExpect {
    uint64_t addr;
    uint64_t wdata;
};
const RaceExpect kRace[] = {{0x2, 0xEE}, {0x9, 0x5A}, {0x0, 0x33}};

}  // namespace

// ───────────── pair 1：跨拍写后读 + `we=0` 不写的负向对照 ─────────────
DUAL_BACKEND_TEST_PAIR(
    MemWriteReadDut,
    "ch_mem 跨拍写后读：aread 在写提交后的拍读到新值，且 we=0 不写（两后端一致）",
    "[mem][comb-read][write-read][dual]",
    [&]() {
        auto &io = dev.instance().io();

        // tick0: 读 addr 2（初值 0xC3）
        drive(sim, io, Inputs{/*addr_sync=*/0, /*addr_async=*/0x2,
                              /*wdata=*/0x00, /*we=*/0});
        sim.tick();
        REQUIRE(rd(sim, io.rdata_async) == 0xC3);

        // tick1: 读 addr 5（写之前 ⇒ 初值 0xF6）
        drive(sim, io, Inputs{0x5, 0x5, 0x00, 0});
        sim.tick();
        REQUIRE(rd(sim, io.rdata_async) == 0xF6);

        // tick2: 写 mem[5] = 0xFF（本拍不断言 —— 同拍读后写属 pair 2）
        drive(sim, io, Inputs{0x5, 0x5, 0xFF, 1});
        sim.tick();

        // tick3: **we=0** 却给了 wdata=0x77 ⇒ 写 MUST 被忽略。
        //   0xFF ⇒ 写已落地且 we=0 生效（两个条件同时成立）
        //   0xF6 ⇒ 写没落地            · 0x77 ⇒ we=0 被忽略
        drive(sim, io, Inputs{0x5, 0x5, 0x77, 0});
        sim.tick();
        REQUIRE(rd(sim, io.rdata_async) == 0xFF);
    });

// ───────────── pair 2：同拍「写 + aread」⇒ 读到写后值 ─────────────
DUAL_BACKEND_TEST_PAIR(
    MemWriteReadDut,
    "ch_mem 同拍写与 aread：读到写后值（post-write，两后端一致）",
    "[mem][comb-read][race][dual]",
    [&]() {
        auto &io = dev.instance().io();

        // 稳定拍（避免依赖未驱动的初值）
        drive(sim, io, Inputs{0, 0, 0x00, 0});
        sim.tick();

        for (const auto &tc : kRace) {
            drive(sim, io, Inputs{/*addr_sync=*/tc.addr,
                                  /*addr_async=*/tc.addr, /*wdata=*/tc.wdata,
                                  /*we=*/1});
            sim.tick();
            INFO("addr=" << tc.addr << " wdata=" << tc.wdata);
            REQUIRE(rd(sim, io.rdata_async) == tc.wdata);
        }

        // 关掉写后再读最后写过的 addr 2 ⇒ 值持久
        // （抓「只在写的那一拍可见 / 写口把值泄到下一拍才生效」）
        drive(sim, io, Inputs{0x2, 0x2, 0x00, 0});
        sim.tick();
        REQUIRE(rd(sim, io.rdata_async) == 0xEE);
    });
