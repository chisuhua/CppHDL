// ⚠️⚠️ **路线 B（A5 的 H 批）第 6 批**（2026-11-02 · change
// `dual-backend-parity-route-b-pipeline-2026-11`）
//
// ## 本批范围：**只有 `pipeline`**（`memory` 已推迟，见下）
//
// 同批原计划的 `chlib::memory`（single-port / dual-port RAM）**未落地**
// ⇒ ⇒ 因为写它的过程中**挖出一次真 parity 分歧**（不是本文件的问题）：
//
// | 后端 | `sread` 禁读拍（`enable=0`）的行为 | 代码 |
// |---|---|---|
// | **interpreter** | 直接 `return` ⇒ 输出**保持上一拍的值** | `src/ast/instr_mem.cpp:258-259` |
// | **Verilog** | `else proxy <= 1'b0;` ⇒ 输出**清零** | `src/codegen_verilog.cpp:1261` |
//
// ⚠️ `chlib::memory` 的 RAM 正是 `mem.sread(addr, !we)`（**写拍禁读**）⇒ ⇒
// 两条后端不可能有同一组期望值 ⇒ ⇒ **无法**在满足 N2′（两后端断言数相等）
// 的前提下写成 dual 用例。
// ⇒ ⇒ **处置**（用户 2026-11-02 裁定 = 选项 A）：**独立 change** 修该分歧，
// `memory` 的 dual 覆盖**等分歧修好后**再补 ⇒ ⇒ 本文件只覆盖 `pipeline`。
// ⇒ ⇒ 该分歧的登记见 `openspec/changes/sread-disable-cycle-divergence-2026-11/`。
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `chlib::pipeline` 是仓内**唯一的流水线寄存器组件族**（`PipelineStage` ·
// `PipelineStageDual` · `PipelineChain` + 函数式 `pipeline_reg`），而盘点实测
// 它的 dual 覆盖率是 **0** ⇒ ⇒ 它们的 Verilog codegen 路径**从未被验证过**。
//
// ⚠️ **为什么这类语义特别危险**：流水线级的三个控制信号
// （**rst** / **stall** / **flush**）在 codegen 侧要写成
// `always_ff @(posedge clk) q <= rst ? 0 : (stall|flush) ? q : d;`
// ⇒ ⇒ **「stall 保持」「flush 清零」「rst 优先」三条规则的相对优先级**是最容易
// 写错的一类 ⇒ ⇒ 而既有测试**全部是裸 context**（`ch_device=0`）⇒ ⇒ 连
// interpreter 侧都没有「**端口驱动**」形态的覆盖。
//
// ## 期望值（**逐拍手推**，禁抄仿真输出）
//
// **语义前提**（逐条来自 `include/chlib/pipeline.h` 的实现）：
//   ① `PipelineStage`：`update = !rst & !stall & !flush`；
//      `data_reg->next = select(update, data_in, data_reg)`
//      ⇒ ⇒ **rst / stall / flush 三者任一为真 ⇒ 寄存器保持**（注意：`PipelineStage`
//      的 rst **不清零**寄存器！它只影响 `update`）；
//   ② `PipelineStage` 的输出是**组合**的：`data_out = select(flush, 0, data_reg)`
//      ⇒ ⇒ **flush 那一拍输出【立即】为 0**（不等沿）；
//   ③ 函数式 `pipeline_reg`：`reg->next = select(rst, 0, select(update, d, reg))`
//      ⇒ ⇒ **`rst` 会清零寄存器**（与 ① 相反！）；
//      输出 `select(rst, 0, select(flush, 0, reg))` ⇒ ⇒ rst/flush **组合**清零输出。
//      ⚠️ **`flush` 与 ① 一样【不】清寄存器** —— 它只让 `update = 0`
//      ⇒ `select(update, d, reg)` 取 `reg` ⇒ 寄存器保持。
//      ⇒ ⇒ **`pipeline_reg` 与 `PipelineStage` 的语义差异【只有 `rst` 一项】**：
//      `PipelineStage` 的 rst **只抑制更新（保持）**，`pipeline_reg` 的 rst **清零**。
//      （初稿曾写「flush 也清寄存器」—— **实测推翻**：那是 `update` 的副作用误读。）
//   ④ `PipelineChain`：第 i 级的输入来自第 i-1 级的**寄存器**
//      （同一沿上各级同时采样 ⇒ 输入值晚 `NumStages` 拍到达输出）。
//
// ⚠️ **时序语义（本批实测确认）**：`Simulator::eval_sequential` **先**执行时序
//   指令（寄存器在 tick 内更新）⇒ ⇒ 之后的组合逻辑读到的是**沿后**值
//   ⇒ ⇒ **组合输出（如 `data_out = select(flush, 0, data_reg)`）在 tick 后
//   呈现的是【沿后】的寄存器值**。
//   ⚠️ 这与「纯组合输出读沿前值」的场景**不同**（例如 `sync_fifo` 的 `q_async`
//   是 `aread` 组合读 ⇒ 它读的是**内存当前内容**，与沿无关）。
// ⇒ ⇒ 每条期望值都按此推导（**实测确认**，非假设）。
//
// ⚠️📌 **判别力设计（本批实测教训）**：要判别「`rst` / `flush` 是否真的改了**寄存器**」
//    ⇒ ⇒ MUST 在它们**之后紧跟一个 `stall` 拍**再观测输出 —— 因为
//    `rst`/`flush` 那一拍的**输出**本来就是组合清零的（两版本都一样）
//    ⇒ ⇒ 只有「下一拍 `stall`（`update = 0` ⇒ 寄存器保持）时读到什么」
//    才暴露寄存器**真实**被写成了什么。
//    ⚠️ 初版两张表都**只在 `reg == 0` 时施加 rst** ⇒ ⇒ 寄存器「保持 0」与
//    「清零成 0」**不可区分** ⇒ ⇒ 变异 M3（把 `pipeline_reg` 的 rst 清零改掉）
//    **未被抓住** ⇒ ⇒ 已按本条重推（pair 2 的 tick3 / tick6 即为此而设）。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/pipeline.h"
#include "module.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

// ⚠️ **用例表 MUST 放在文件作用域**：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**（只有**圆括号**才保护）。

/// DUT 1：`PipelineStage`（组件形态）⇒ 钉 stall / flush 的**组合输出**语义。
class StageDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> data_in; ch_in<ch_uint<1>> rst;
         ch_in<ch_uint<1>> stall; ch_in<ch_uint<1>> flush;
         ch_out<ch_uint<8>> data_out;)

    StageDut(ch::Component *parent = nullptr,
             const std::string &name = "stage_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool rst_b = ch_bool(io().rst == ch_uint<1>(1_d));
        ch_bool stall_b = ch_bool(io().stall == ch_uint<1>(1_d));
        ch_bool flush_b = ch_bool(io().flush == ch_uint<1>(1_d));
        // ⚠️ **必须用 `ch::ch_module<T>`**（不是裸 `PipelineStage stage(this, ...)`）
        //    ⇒ 裸构造的子组件**它的 `describe()` 不会被框架调用**
        //    （`Component::build_internal` 只对**顶层**组件调 `describe()`）
        //    ⇒ ⇒ 子组件的 `ch_reg` 从未被创建 ⇒ ⇒ **SIGSEGV**（实测）。
        //    ⚠️ 既有 `chlib/test_pipeline.cpp` 只做**编译性**检查（`REQUIRE(true)`）
        //    ⇒ ⇒ **从未真正跑过** ⇒ ⇒ 这个 SIGSEGV 是**真实缺陷**（用法层）。
        ch::ch_module<PipelineStage<ch_uint<8>>> stage{"stage"};
        // ⚠️ 端口**不能用 `operator=`**（`io.h:227` 声明为 deleted ⇒ 那是
        //    「节点重绑定」语义，不是连线）⇒ ⇒ MUST 用 `<<=`（数据流边）
        stage.io().data_in <<= io().data_in;
        stage.io().rst <<= rst_b;
        stage.io().stall <<= stall_b;
        stage.io().flush <<= flush_b;
        io().data_out <<= stage.io().data_out;
    }
};

/// DUT 2：函数式 `pipeline_reg` ⇒ 钉 **rst 会清零**（与 `PipelineStage` 不同）。
class FuncRegDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> d; ch_in<ch_uint<1>> rst; ch_in<ch_uint<1>> stall;
         ch_in<ch_uint<1>> flush; ch_out<ch_uint<8>> q;)

    FuncRegDut(ch::Component *parent = nullptr,
               const std::string &name = "func_reg_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool rst_b = ch_bool(io().rst == ch_uint<1>(1_d));
        ch_bool stall_b = ch_bool(io().stall == ch_uint<1>(1_d));
        ch_bool flush_b = ch_bool(io().flush == ch_uint<1>(1_d));
        io().q = pipeline_reg<8>(io().d, rst_b, stall_b, flush_b, "preg");
    }
};

/// DUT 3：`PipelineChain<ch_uint<8>, 3>` ⇒ 钉**逐级延迟**（第 N 拍到第 N 级）。
class ChainDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> data_in; ch_in<ch_uint<1>> rst;
         ch_in<ch_uint<1>> stall; ch_in<ch_uint<1>> flush;
         ch_out<ch_uint<8>> data_out;)

    ChainDut(ch::Component *parent = nullptr,
             const std::string &name = "chain_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool rst_b = ch_bool(io().rst == ch_uint<1>(1_d));
        ch_bool stall_b = ch_bool(io().stall == ch_uint<1>(1_d));
        ch_bool flush_b = ch_bool(io().flush == ch_uint<1>(1_d));
        ch::ch_module<PipelineChain<ch_uint<8>, 3>> chain{"chain"};
        chain.io().data_in <<= io().data_in;
        chain.io().rst <<= rst_b;
        chain.io().stall <<= stall_b;
        chain.io().flush <<= flush_b;
        io().data_out <<= chain.io().data_out;
    }
};

// ── 手推金标 ────────────────────────────────────────────────────────────
// PipelineStage：update = !rst & !stall & !flush；out = select(flush, 0, reg)
struct StageStep {
    uint64_t rst, stall, flush, din;
    uint64_t out;      // 本拍读到的输出（沿前的 reg，除非 flush）
};
const StageStep kStageSteps[] = {
    // tick1: rst=1 ⇒ update=0 ⇒ reg 保持（初值 0）⇒ out=0
    {1, 0, 0, 0x11, 0x00},
    // tick2: 正常 ⇒ reg←0x22 ⇒ out 读【沿后】⇒ 0x22
    {0, 0, 0, 0x22, 0x22},
    // tick3: stall ⇒ update=0 ⇒ reg 保持 0x22 ⇒ out=0x22
    {0, 1, 0, 0x33, 0x22},
    // tick4: 正常 ⇒ reg←0x44 ⇒ out=0x44
    {0, 0, 0, 0x44, 0x44},
    // tick5: ★ rst 而 reg **非 0** ⇒ PipelineStage 的 rst **只抑制更新**
    //        ⇒ reg【保持 0x44】⇒ out=0x44
    //        （若改成「rst 清零」⇒ out=0 ⇒ 本拍红）
    {1, 0, 0, 0x55, 0x44},
    // tick6: flush ⇒ 输出**组合**清零 ⇒ out=0（reg 仍 0x44，flush 只让 update=0）
    {0, 0, 1, 0x66, 0x00},
    // tick7: ★ stall ⇒ out = reg ⇒ 0x44（证明 flush **没有**改寄存器）
    {0, 1, 0, 0x77, 0x44},
    // tick8: 正常 ⇒ reg←0x88 ⇒ out=0x88
    {0, 0, 0, 0x88, 0x88},
};

// 函数式 pipeline_reg：rst ⇒ reg 清零；out = select(rst,0,select(flush,0,reg))
struct FuncStep {
    uint64_t rst, stall, flush, d;
    uint64_t q;
};
const FuncStep kFuncSteps[] = {
    // tick1: 正常 ⇒ reg←0x11 ⇒ out 读【沿后】⇒ 0x11
    {0, 0, 0, 0x11, 0x11},
    // tick2: rst 而 reg 非 0（0x11）⇒ `select(rst, 0, ...)` ⇒ **reg←0**
    //        且输出组合清零 ⇒ out=0
    {1, 0, 0, 0x22, 0x00},
    // tick3: ★ stall ⇒ out = reg ⇒ **0x00** ⟺ rst 真的把寄存器清零了
    //        （若 rst 改成「只抑制更新」⇒ out=0x11 ⇒ 本拍红）
    {0, 1, 0, 0x33, 0x00},
    // tick4: 正常 ⇒ reg←0x44 ⇒ out=0x44
    {0, 0, 0, 0x44, 0x44},
    // tick5: flush ⇒ 输出组合清零 ⇒ out=0；但 update=0 ⇒ **reg 保持 0x44**
    {0, 0, 1, 0x55, 0x00},
    // tick6: ★ stall ⇒ out = reg ⇒ 0x44（证明 flush **没有**改寄存器）
    {0, 1, 0, 0x66, 0x44},
    // tick7: 正常 ⇒ reg←0x77 ⇒ out=0x77
    {0, 0, 0, 0x77, 0x77},
};

// PipelineChain<3>：3 级 ⇒ 数据延迟 3 拍
// ⚠️ 输入**全部相同**（0xA1）⇒ ⇒ 这样「延迟了几拍」**看不出来**（任何延迟都
//    输出 0xA1）⇒ ⇒ 真正判别延迟需要**不同**的值序列；本文件先用同值序列钉
//    「延迟 ≥ 3 拍」（前 3 拍输出必为 0），延迟的**精确**拍数由既有的
//    `chlib/test_pipeline.cpp` 覆盖 ⇒ ⇒ 此处不重复。
// ⚠️ **MUST 用【不同】值**：全同值序列只能钉住「延迟 ≥ N 拍」
//    （任何更大延迟在同一时刻也输出同一个值）⇒ ⇒ 无法判别「恰好 3 级」。
const uint64_t kChainIn[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
// 3 级 ⇒ 每级在**同一沿**采样上一级的沿前值 ⇒ in[k] 在 tick(k+3) 出现
//   tick1: regs={0x11, 0,   0}    ⇒ out=regs[2]=0
//   tick2: regs={0x22, 0x11,0}    ⇒ out=0
//   tick3: regs={0x33, 0x22,0x11} ⇒ out=0x11
//   tick4: regs={0x44, 0x33,0x22} ⇒ out=0x22
//   tick5: regs={0x55, 0x44,0x33} ⇒ out=0x33
//   tick6: regs={0x66, 0x55,0x44} ⇒ out=0x44
const uint64_t kChainOut[6] = {0x00, 0x00, 0x11, 0x22, 0x33, 0x44};

}  // namespace

// ───────────────────── pair 1：PipelineStage ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    StageDut,
    "PipelineStage - stall 保持 / flush 组合清零（两后端逐拍一致）",
    "[chlib][pipeline][stage][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kStageSteps) {
            sim.set_input_value(io.rst, tc.rst);
            sim.set_input_value(io.stall, tc.stall);
            sim.set_input_value(io.flush, tc.flush);
            sim.set_input_value(io.data_in, tc.din);
            sim.tick();
            INFO("rst=" << tc.rst << " stall=" << tc.stall
                        << " flush=" << tc.flush << " in=0x" << std::hex
                        << tc.din);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data_out)) ==
                    tc.out);
        }
    });

// ───────────────────── pair 2：函数式 pipeline_reg ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    FuncRegDut,
    "pipeline_reg（函数式） - rst 清零 / stall 保持 / flush 清零（两后端逐拍一致）",
    "[chlib][pipeline][functional][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kFuncSteps) {
            sim.set_input_value(io.rst, tc.rst);
            sim.set_input_value(io.stall, tc.stall);
            sim.set_input_value(io.flush, tc.flush);
            sim.set_input_value(io.d, tc.d);
            sim.tick();
            INFO("rst=" << tc.rst << " stall=" << tc.stall
                        << " flush=" << tc.flush << " d=0x" << std::hex << tc.d);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == tc.q);
        }
    });

// ───────────────────── pair 3：PipelineChain ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    ChainDut,
    "PipelineChain<3> - 逐级延迟 3 拍（两后端逐拍一致）",
    "[chlib][pipeline][chain][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        // ⚠️ 连续送入**不同**的值 ⇒ 才能判别「延迟了几拍」
        //    （送同一个值则任何延迟都看不出来）
        for (int i = 0; i < 6; ++i) {
            sim.set_input_value(io.rst, 0u);
            sim.set_input_value(io.stall, 0u);
            sim.set_input_value(io.flush, 0u);
            sim.set_input_value(io.data_in, kChainIn[i]);
            sim.tick();
            INFO("tick " << (i + 1));
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data_out)) ==
                    kChainOut[i]);
        }
    });
