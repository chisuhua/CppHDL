// tests/test_mod_width_dual.cpp
//
// Cross-backend tripwire for the `%` (mod) **result-width**口径.
//
// ============================================================================
// 为什么有这个用例（2026-11-20）
// ============================================================================
// `chlib::round_robin_selector` 的 `(idx + i) % N` 在产品化时触发过 Verilator
// `WIDTHTRUNC` ⇒ 挖出两套宽度口径：
//
//   · **框架**：`x % 字面量(N)` ⇒ 结果宽 = `compute_bit_width(N-1)`
//              （include/core/operators_width.h 的 mod 特化）
//              `x % y`（y 非字面量）⇒ 结果宽 = **RHS 的类型宽**
//   · **Verilog**：运算自然宽 = `max(两侧)`，赋值处按节点声明宽截断
//              （卫生产物把该截断写成显式 size cast：
//                src/codegen_verilog.cpp::print_binary_op）
//
// 当时怀疑这两套口径在 N 非 2 的幂时会让两后端**取值不同**
// （猜测 `a=4, N=3 ⇒ 框架 0 / Verilog 1`）⇒ ⇒ 实测**不成立**，原因是数学上闭合：
//   余数 ∈ [0, N-1] ⇒ `compute_bit_width(N-1)` 恰好够；
//   信号除数 y（N bit）⇒ 余数 ≤ 2^N - 2 ⇒ N bit 也够。
// ⇒ ⇒ 即「截断永不丢有效位」。
//
// 本用例把这条**闭合性**变成常驻判据，而不是只留一份 /tmp 点测记录：
//   ① 编译期：`static_assert` 钉住框架的**定宽公式**（改公式 = 立刻编译失败）；
//   ② 运行期：四条**手推**期望值 × 两个后端 ⇒ 钉住「截断不丢有效位」。
//
// ============================================================================
// 期望值（**手推**，不从任一后端的输出抄）
// ============================================================================
//   a=4,  b=3 ⇒ a%3=1 · a%5=4 · a%2=0 · a%b=1 · 3%b=0
//   a=9,  b=5 ⇒ a%3=0 · a%5=4 · a%2=1 · a%b=4 · 3%b=3
//   a=6,  b=7 ⇒ a%3=0 · a%5=1 · a%2=0 · a%b=6 · 3%b=3
//   a=15, b=2 ⇒ a%3=0 · a%5=0 · a%2=1 · a%b=1 · 3%b=1
//
// 判别力：`a%5` 在 a=4 / a=9 处结果为 4（占满 3 bit）⇒ 若结果宽被收窄到 2 bit
// 就会读成 0；`a%b` 在 b=7、a=6 处为 6（占满 3 bit）⇒ 同型。这类值是本用例
// 特意挑的「**必须用满结果宽**」的拍 —— 只取 0/1 的拍对截断**无判别力**。
//
// ⚠️ 边界：`% 0` 是未定义（框架侧走 C++ 取模 ⇒ UB；Verilog 侧结果为 X）
//   ⇒ ⇒ 本用例**只驱动 b ≥ 1**，不把「除零」纳入语义。

#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include <cstdint>
#include <utility>

using namespace ch;
using namespace ch::core;

namespace {

// ① 编译期：框架侧定宽公式（改了就必须同步改本断言与上述注释）
//    `a % 3` ⇒ compute_bit_width(2) = 2 bit
//    `a % 5` ⇒ compute_bit_width(4) = 3 bit
//    `a % b`（b 为 3 bit 信号）⇒ RHS 类型宽 = 3 bit
static_assert(ch_width_v<decltype(std::declval<ch_uint<4>>() % 3_d)> == 2,
              "框架的 `x % 字面量(3)` 结果宽应为 compute_bit_width(2) == 2");
static_assert(ch_width_v<decltype(std::declval<ch_uint<4>>() % 5_d)> == 3,
              "框架的 `x % 字面量(5)` 结果宽应为 compute_bit_width(4) == 3");
static_assert(ch_width_v<decltype(std::declval<ch_uint<4>>() % std::declval<ch_uint<3>>())> == 3,
              "框架的 `x % y` 结果宽应为 RHS 类型宽（3）");

// 输出统一 8 bit：让「截断后又被扩回」与「本来就宽」在顶层端口上不可区分地
// 暴露同一个值 —— 顶层端口是 Verilator 后端唯一可观测面。
class ModWidthDut : public ch::Component {
  public:
    __io(
        ch_in<ch_uint<4>> a{"a"};
        ch_in<ch_uint<3>> b{"b"};
        ch_out<ch_uint<8>> o_mod3{"o_mod3"};
        ch_out<ch_uint<8>> o_mod5{"o_mod5"};
        ch_out<ch_uint<8>> o_mod2{"o_mod2"};
        ch_out<ch_uint<8>> o_mod_sig{"o_mod_sig"};
        ch_out<ch_uint<8>> o_mod_lit_lhs{"o_mod_lit_lhs"};)

    ModWidthDut(ch::Component *parent = nullptr,
                const std::string &name = "mod_width_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().o_mod3 = io().a % 3_d;
        io().o_mod5 = io().a % 5_d;
        io().o_mod2 = io().a % 2_d;
        io().o_mod_sig = io().a % io().b;
        io().o_mod_lit_lhs = 3_d % io().b;
    }
};

// 手推期望表（文件作用域：C 预处理器按**逗号**切分宏实参，花括号不保护
// ⇒ ⇒ 带逗号的初始化列表 MUST NOT 出现在 DUAL_BACKEND_TEST_PAIR 的 body 里）
struct ModWidthCase {
    uint64_t a;
    uint64_t b;
    uint64_t mod3;
    uint64_t mod5;
    uint64_t mod2;
    uint64_t mod_sig;
    uint64_t mod_lit_lhs;
};

constexpr ModWidthCase kModWidthCases[] = {
    {4, 3, 1, 4, 0, 1, 0},
    {9, 5, 0, 4, 1, 4, 3},
    {6, 7, 0, 1, 0, 6, 3},
    {15, 2, 0, 0, 1, 1, 1},
};

} // namespace

DUAL_BACKEND_TEST_PAIR(
    ModWidthDut,
    "mod result width: framework fixed width == Verilog natural width (hand-derived)",
    "[dual][arith][mod-width]",
    [&]() {
        auto &io = dev.instance().io();

        for (const auto &c : kModWidthCases) {
            sim.set_input_value(io.a, c.a);
            sim.set_input_value(io.b, c.b);
            sim.tick();

            INFO("a=" << c.a << " b=" << c.b);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.o_mod3)) ==
                    c.mod3);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.o_mod5)) ==
                    c.mod5);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.o_mod2)) ==
                    c.mod2);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.o_mod_sig)) ==
                    c.mod_sig);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.o_mod_lit_lhs)) ==
                    c.mod_lit_lhs);
        }
    });
