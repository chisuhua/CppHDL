// ⚠️⚠️ **路线 B（A5 的 H 批）第 2 批**（2026-11-02 · change
// `dual-backend-parity-route-b-conditional-2026-11`）
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `chlib::compose_guard_chain`（`include/chlib/guard_contribution.h`）是 ADR-051
// D10 从 `ch_state_machine::contrib` 与 `_if` 两处**抽取**出来的**共享**组合器
// ⇒ ⇒ 它是仓内**唯一**一处「把多条 (守卫, 值) 合成一条 `select` 链」的代码。
//
// ⚠️ **为什么它值得双后端覆盖**（不是「新组件加测试」）：
//
// 1. **它是「优先级方向」的唯一定义点**。抽取前 ADR-050 的内联 `compose()`
//    用**正向**状态序迭代 ⇒ ⇒ **高编号** state 优先；抽取后改为**先收集的在外层**
//    ⇒ ⇒ **低编号** state 优先 ⇒ ⇒ ⇒ **优先级被静默翻转**（ADR-051 已订正，
//    并把它写成契约）。
//
// 2. ⚠️ **但那条契约在 ADR-050 侧是【运行期不可观测】的**：状态守卫是
//    `is_in(S)` ⇒ ⇒ **两两互斥是构造保证** ⇒ ⇒ 同一时刻至多一个守卫为真
//    ⇒ ⇒ **无论链序如何，结果逐位相同** ⇒ ⇒ ⇒ **运行期金标钉不住它**
//    ⇒ ⇒ 它只能靠**结构级**断言钉住（`tests/chlib/test_guard_contribution.cpp`
//    断言最外层 mux 的守卫 = 第一个贡献的守卫）。
//    ⇒ ⇒ **这正是「结构断言」的盲区**：它断言的是**图结构**，不是**行为**。
//    ⇒ ⇒ ⇒ **本文件提供的是「同一份组合器在行为面的一致性」**：让**非互斥**
//    的守卫走同一条链 ⇒ ⇒ 链序**运行期可观测** ⇒ ⇒ 于是「方向」这条契约
//    **第一次**有了**运行期**判据。
//
// 3. **它与 `_if` 的 DSL 是同一条链**：`_if` 的「先匹配者胜」在 `if_stmt_dual`
//    里被覆盖；而 `compose_guard_chain` 是那条链的**唯一实现** ⇒ ⇒ 两个文件
//    合起来才覆盖「合成器 + 语法层」的完整面。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// `compose_guard_chain` 的语义（头文件「语义（**先匹配者胜**）」段）：
//   ```
//   acc = tail;
//   for (i = n-1 .. 0)  acc = select(guards[i], values[i], acc);
//   ```
// ⇒ 结果 = `select(g0, v0, select(g1, v1, … tail))` ⇒ **g0 命中时 v0 胜**
// ⇒ ⇒ **先收集**的贡献位于链的**外层** ⇒ ⇒ 链序**运行期可观测**
// （守卫**非互斥**时）。
//
// 本文件的守卫刻意**非互斥**（`g0` 与 `g1` 可同时为真）⇒ ⇒ 链序**可判别**：
//   · g0=1, g1=1 ⇒ **g0 胜** ⇒ 取 v0（**不是** v1）
//   · g0=0, g1=1 ⇒ 取 v1
//   · g0=0, g1=0 ⇒ 取 tail
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)
//    ⇒ 证明 verilator 半边**真的跑了**（缺席时只有 1 倍）。
#include "catch_amalgamated.hpp"

#include "chlib/guard_contribution.h"
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
using namespace chlib;

namespace {

/// DUT：**两条非互斥**的守卫贡献 ⇒ 链序「先匹配者胜」在**运行期可观测**。
///
/// ⚠️ **守卫 MUST 是【组合直读】**（不经寄存器中转）—— 实测记录：
///    ① `ch_in<ch_bool>` **不能**绑到 `const ch_bool&`（ADR-051 的 E3）；
///    ② 用 `ch_reg<ch_bool>` 中转会让守卫**多两拍**延迟（一拍给寄存器、
///       一拍因组合逻辑读到的是沿**前**值）⇒ ⇒ 金标会退化成难读的偏移序列
///       ⇒ ⇒ 本文件**不用**中转；
///    ③ `port<ch_uint<N>>` **没有** `bits<>` 成员（实测编译失败）
///       ⇒ ⇒ 改用「窄端口 + 比较」：`port<ch_uint<1>> == ch_uint<1>(1_d)`
///       ⇒ ⇒ 得到 `ch_bool`（**组合**、**同拍**）。
class GuardChainDut : public ch::Component {
public:
    __io(ch_in<ch_uint<1>> g0_in; ch_in<ch_uint<1>> g1_in;
         ch_out<ch_uint<8>> q;)

    GuardChainDut(ch::Component *parent = nullptr,
                  const std::string &name = "guard_chain_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // ⚠️ 组合直读 ⇒ 与输入**同拍**（无中转延迟）
        ch_bool g0(io().g0_in == ch_uint<1>(1_d));
        ch_bool g1(io().g1_in == ch_uint<1>(1_d));

        ch_reg<ch_uint<8>> acc(0_d, "acc");
        // ⚠️ **两条贡献**，按**优先级顺序**（靠前者优先）⇒ ⇒ g0 在链的**外层**
        std::vector<guard_contribution> contribs;
        contribs.push_back({g0, ch_uint<8>(10_d).impl()});
        contribs.push_back({g1, ch_uint<8>(20_d).impl()});
        // ⚠️ `tail` = 显式默认（**不是**寄存器保持）⇒ 便于逐项判别
        acc->next = compose_guard_chain<8>(contribs, ch_uint<8>(30_d));
        io().q = acc;
    }
};

}  // namespace

// ───────────────────────── DUT ─────────────────────────
DUAL_BACKEND_TEST_PAIR(
    GuardChainDut,
    "守卫贡献链 - 先匹配者胜（g0 与 g1 非互斥 ⇒ 链序运行期可观测）",
    "[chlib][guard_contribution][adr051][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        // ⚠️ 守卫是**组合直读**（无中转）⇒ 与输入**同拍** ⇒
        //    ⇒ 每一拍读到的就是「本拍输入」对应的合成结果。
        //
        // 手推（守卫非互斥 ⇒ 链序可判别）：
        //   拍 1：g0=0,g1=0 ⇒ 无守卫命中 ⇒ tail = 30
        //   拍 2：g0=1,g1=0 ⇒ g0 命中 ⇒ 10
        //   拍 3：g0=0,g1=1 ⇒ g1 命中 ⇒ 20
        //   拍 4：g0=1,g1=1 ⇒ **g0 胜**（先匹配者胜）⇒ 10（**不是** 20）
        sim.set_input_value(io.g0_in, 0u);
        sim.set_input_value(io.g1_in, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 30u);

        sim.set_input_value(io.g0_in, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 10u);

        sim.set_input_value(io.g0_in, 0u);
        sim.set_input_value(io.g1_in, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 20u);

        sim.set_input_value(io.g0_in, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == 10u);
    });
