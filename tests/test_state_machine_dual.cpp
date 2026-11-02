// ⚠️⚠️ **路线 B（A5 的 H 批）· 2026-11-02**（change
// `dual-backend-parity-route-b-fsm-2026-11`）
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `ch_state_machine` 是仓内**唯一**的 FSM DSL，而它的**运行期转移**在
// **verilator codegen 路径上从未被验证过**。这不是理论风险 —— 同一个缺陷类
// 已经在仓内**两次真实发生**、且两次都被「只有 parity」或「只有结构断言」的
// 用例**掩盖**：
//   · `examples/spinalhdl-ported/uart/uart_tx.cpp` —— 生成的 Verilog 里状态
//     寄存器是 `q <= q`（**自环**）、`is_in(...)` 的比较节点**零消费者**
//     ⇒ FSM **从未工作过**；
//   · `tests/test_verilator_complex_e2e.cpp` 的 `SpiFsmFixture` —— 状态卡死在
//     `TRANSFER`、`shift` 恒 0（`STOP` 不可达）。
// ⇒ ⇒ 本文件把「**状态转移 + 条件转移 + 状态解码**」放到**两条后端**上，
// 用**手推**的状态序列钉住 ⇒ ⇒ 若 codegen 再次退化成自环/丢转移 ⇒ **红**。
//
// ⚠️ **它不是 `tests/test_state_machine.cpp` 的复刻**：那个文件覆盖的是
// **API 形状**（被移除的符号不存在 · 构图期拒绝 · `resolve_or` 默认尾部）
// ⇒ 其中 6/8 个用例是**编译期 / 构图期**判据（对两后端是同一段 C++ 代码
// ⇒ dual 化**零增量价值**）。本文件覆盖**运行期时序**，原文件**没有**这条。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 语义前提（**由两条已知良好的 interpreter 断言反推、并与 RTL 寄存器语义一致**）：
//   ① 状态寄存器（`state_reg`）在**时钟沿**更新 ⇒ 沿**之后**读到的是**新**状态；
//   ② 组合逻辑读到的是**沿之前**的状态；`cyc` 与 `state_reg` **同沿**推进；
//   ③ `transition_when` 是**电平敏感**（条件为真的那一拍就转移）；
//   ④ 转移的 `select` 树按「**后注册者优先**」合成，但各状态守卫 `is_in(S)`
//      两两互斥 ⇒ 顺序不影响结果。
//
//   `tests/test_state_machine.cpp` 的两条已知断言（互证 ①②）：
//     · `StateMachineDut`（**无**转移）tick1 后 = 0（A）—— 枚举 0 == 复位值；
//     · `DefaultTailDut`（A→B **无条件**）tick1 读到 `val`=7（**A 的贡献**）、
//       tick2 读到 99（**B** 未被覆盖 ⇒ 默认尾部）⇒ 沿前状态驱动组合逻辑、
//       沿后状态是新值。
//
// **DUT 1**（无条件链 A→B→C；C 为终态）：
//   tick:      1     2     3
//   state:  B(1)  C(2)  C(2)      ← 沿后值
//   val:  0x22  0x33  0x33        ← 组合解码，与 state 同源
//
// **DUT 2**（条件转移；`cyc` 为 3 位自由计数器，沿前值 = tick−1）：
//   tick:        1     2     3     4     5     6
//   cyc(沿后):   1     2     3     4     5     6
//   pre-edge cyc:0     1     2     3     4     5
//   cond A→B (cyc==2): F  F  T  F  F  F   ⇒ tick3 沿后 state=B
//   cond B→C (cyc==4): —  —  —  F  T  —   ⇒ tick5 沿后 state=C
//   state(沿后): 0(A) 0(A) 1(B) 1(B) 2(C) 2(C)
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)
//    ⇒ 证明 verilator 半边**真的跑了**（缺席时只有 1 倍）。
#include "catch_amalgamated.hpp"

#include "chlib/state_machine.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/reg.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace ch::core;

namespace {

enum class SmState : uint8_t { A = 0, B = 1, C = 2 };

/// DUT 1：**无条件**链 A→B→C（C 为终态）⇒ 判别「状态寄存器是不是自环」
/// 以及「`transition_when` 的 select 树有没有真的发射」。
class FsmChainDut : public ch::Component {
public:
    __io(ch_out<ch_uint<2>> dbg_state; ch_out<ch_uint<8>> dbg_val;)

    FsmChainDut(ch::Component *parent = nullptr,
                const std::string &name = "fsm_chain_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        chlib::ch_state_machine<SmState, 3> sm;
        // ⚠️ 闭包内**只**注册转移（陷阱 A：`build()` 会对**每个**状态执行一次
        //    `on_active` ⇒ 在里面写 `reg->next` / 输出会被后执行者覆盖）。
        sm.state(SmState::A).on_active(
            [&] { sm.transition_when(ch_bool(true), SmState::B); });
        sm.state(SmState::B).on_active(
            [&] { sm.transition_when(ch_bool(true), SmState::C); });
        sm.state(SmState::C).on_active([] {});
        sm.set_entry(SmState::A);
        sm.build();

        io().dbg_state = sm.current_state_uint();
        // 组合解码（**不用寄存器** ⇒ 与 `dbg_state` 同源 ⇒ 无拍差歧义）
        io().dbg_val = select(sm.is_in(SmState::A), ch_uint<8>(0x11_h),
                       select(sm.is_in(SmState::B), ch_uint<8>(0x22_h),
                                                    ch_uint<8>(0x33_h)));
    }
};

/// DUT 2：**条件**转移（条件来自自由计数器 ⇒ 每拍求值、电平敏感）。
class FsmConditionalDut : public ch::Component {
public:
    __io(ch_out<ch_uint<2>> dbg_state; ch_out<ch_uint<3>> dbg_cyc;)

    FsmConditionalDut(ch::Component *parent = nullptr,
                      const std::string &name = "fsm_cond_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<3>> cyc(0_d, "cyc");
        chlib::ch_state_machine<SmState, 3> sm;
        sm.state(SmState::A).on_active([&] {
            sm.transition_when(cyc == ch_uint<3>(2_d), SmState::B);
        });
        sm.state(SmState::B).on_active([&] {
            sm.transition_when(cyc == ch_uint<3>(4_d), SmState::C);
        });
        sm.state(SmState::C).on_active([] {});
        sm.set_entry(SmState::A);
        sm.build();

        cyc->next = cyc + ch_uint<3>(1_d);
        io().dbg_cyc = cyc;
        io().dbg_state = sm.current_state_uint();
    }
};

// ⚠️ 用例表 MUST 放在**文件作用域**：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    花括号内的逗号会被预处理器当**参数分隔**（只有圆括号才保护）。
// 手推金标（见文件头 §DUT 1）
const uint64_t kChainState[3] = {1, 2, 2};
const uint64_t kChainVal[3] = {0x22, 0x33, 0x33};

// 手推金标（见文件头 §DUT 2）
const uint64_t kCondState[6] = {0, 0, 1, 1, 2, 2};
const uint64_t kCondCyc[6] = {1, 2, 3, 4, 5, 6};

}  // namespace

// ───────────────────────── DUT 1 ─────────────────────────
DUAL_BACKEND_TEST_PAIR(
    FsmChainDut,
    "FSM 无条件链 A→B→C - 状态转移与状态解码两后端逐拍一致",
    "[state_machine][adr050][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (int i = 0; i < 3; ++i) {
            sim.tick();
            INFO("tick " << (i + 1));
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dbg_state)) ==
                    kChainState[i]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dbg_val)) ==
                    kChainVal[i]);
        }
    });

// ───────────────────────── DUT 2 ─────────────────────────
DUAL_BACKEND_TEST_PAIR(
    FsmConditionalDut,
    "FSM 条件转移（电平敏感） - 状态序列与计数器两后端逐拍一致",
    "[state_machine][adr050][dual][conditional][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (int i = 0; i < 6; ++i) {
            sim.tick();
            INFO("tick " << (i + 1));
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dbg_cyc)) ==
                    kCondCyc[i]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dbg_state)) ==
                    kCondState[i]);
        }
    });
