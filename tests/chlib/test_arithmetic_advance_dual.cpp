// ⚠️⚠️ **路线 B（新文件）—— `chlib/arithmetic_advance.h` 首次进双后端 parity**（2026-11-02）
//
// ## 立项理由
//
// ① **零覆盖**：实测「含 `DUAL_BACKEND_TEST_PAIR(` 调用」的源文件全集里**无任何一个**
//    include `chlib/arithmetic_advance.h` ⇒ ⇒ 该头在双后端矩阵里**完全未被验证**。
//    （它覆盖 CLA / CSA / Wallace / Booth / 非恢复除法 / 定点四则 —— 都是**多级进位链**。）
// ② **既有单后端测试不可转换**：`tests/chlib/test_arithmetic_advance.cpp` 用 **裸 context**
//    （`ctx_swap`，零 `ch_device`，无顶层端口）⇒ ⇒ 命中 capability
//    `dual-backend-test-coverage` 的 R6 排除项 "bare-context tests"
//    ⇒ ⇒ Verilator 后端只能观测**顶层端口** ⇒ **结构上不可转换** ⇒ 本文件是**新写**。
// ③ **并且原计划批次表里根本没有它**（`test_arithmetic_advance.cpp` 不在 D/E/F 任何一批）
//    ⇒ ⇒ 「新写」路线不受批次表约束（组件零覆盖即可立项）。
//
// ## 为什么这个头值得双后端钉
//
// 六个函数里**每一个**都是一条**进位/移位链**，而 `interpreter`（逐节点拓扑求值）与
// `Verilog`（连续赋值 + 位宽截断）在**进位传播**与**中间位宽截断**上是最容易分叉的地方：
// - `carry_lookahead_adder`：先扩到 `N+1` 位再相加 ⇒ **扩位方式**是唯一的承重点
//   （少扩一位 ⇒ 进位丢失，而低位结果**仍然正确** ⇒ 只断 `sum` 抓不到）；
// - `carry_save_adder`：`sum = a^b^c`、`carry = ((a&b)|(b&c)|(a&c)) << 1` ⇒ **先算后移**；
// - `non_restoring_divider`：**两次** `select` 守卫（除零 ⇒ 商全 1、余 0）⇒
//   除法本身在 codegen 侧是一条单独的路径（不是加法链）；
// - `fixed_point_multiplier`：先乘到 `2N` 位、再 `>> Q`、再**截断回 N 位** ⇒ 三处截断点；
// - `fixed_point_divider`：先 `a << Q` 扩到 `2N`、再除、再截断回 N 位。
//
// ---
// ## 🔴 读实现时发现的**三个既有缺陷 / 名不副实**（本批**不修**，如实登记）
//
// ### ① `wallace_tree_multiplier` 与 `booth_multiplier` **逐字相同**
// `include/chlib/arithmetic_advance.h:56-73` 两个函数体完全一致（都是
// `ch_uint<2*N>(a) * ch_uint<2*N>(b)`）⇒ ⇒ **既不是 Wallace 树、也不是 Booth 编码**
// ⇒ ⇒ 属与 `fwft_fifo`（承诺 FWFT 实则不是）、`stream_arbiter_sequence`（注释承诺
// round-robin 实则 priority）**同型**的「名字 over-promise」。
// ⇒ 本批的处置：**断言两者逐位相同**（把「当前事实」钉住）+ 在 CMake 注释与
// AGENTS.md NOTES 里登记 ⇒ **改名/重写 = 独立立项**（改实现会改变生成的 RTL 结构，
// 需 ADR；本 change 只做覆盖，不动实现）。
//
// ### ② `carry_save_adder` 的 `carry` 会**静默截断**
// 签名是 `ch_uint<N> carry`，而内部 `ch_uint<N+1> temp` 再 `temp << 1_d`（**N+1 位**）
// ⇒ ⇒ 赋回 N 位时**最高位（= 真正的 carry-out）被丢掉**。
// ⇒ 本文件**按签名定义的当前行为**断言（即截断后的值），并在向量表里**刻意**放了
// `(0x80, 0x80, 0x00)` 这种「只有 carry-out 置位」的向量 ⇒ ⇒ 该截断被**显式钉住**
// （将来若把 `carry` 改成 `ch_uint<N+1>`，本断言会转红 ⇒ 是一次**有意的**行为变更信号）。
//
// ### ③ `fixed_point_divider` 的除零保护是**死代码**
// `include/chlib/arithmetic_advance.h:125` 是 `if (b == 0_d) { ... return all-ones; }`
// —— 即 **C++ 控制流作用于硬件值**。实测机制（读代码可判，不需要跑）：
// `ch_bool::operator uint64_t()`（`src/lnode/bool.cpp:44-49`）**只对 const 节点返回真值**，
// 非 const 恒返 **0** ⇒ ⇒ 端口形态下 `b == 0_d` 是**非 const** 的 `eq` 节点
// ⇒ `if` **建图期恒假** ⇒ **那条 return 从不发射** ⇒ ⇒ 实际硬件是 `(a<<Q)/b`，
// **b == 0 时无任何保护**。
// ⇒ 本文件**不测** `fixed_point_divider` 的 `b == 0`（interpreter 侧是原生整数除法
// ⇒ **SIGFPE**；为一条已知死代码去触发进程崩溃不值得）⇒ **独立立项**。
// ⚠️ 对照：`non_restoring_divider` 用的是**真 select**（`select(is_zero, all_ones, ...)`）
// ⇒ 那是**真保护** ⇒ 本文件**会**测它的 `b == 0`（商全 1 / 余 0）。
//
// ---
// ## 期望值（**手推**，禁抄仿真输出）
//
// 语义逐条对照实现（`include/chlib/arithmetic_advance.h`），并把公式写成
// 「按位宽截断」的形式（这样手推与实现一一对应）：
//
// - `carry_lookahead_adder<8>(a,b,cin)` ⇒ `t = a+b+cin`；`sum = t & 0xFF`、`carry = t>>8`
// - `carry_save_adder<8>(a,b,c)` ⇒ `sum = a^b^c`；`carry = (((a&b)|(b&c)|(a&c)) << 1) & 0xFF`
//   （最后那步 `& 0xFF` 就是上面缺陷 ② 的截断）
// - `wallace_tree_multiplier<8>` / `booth_multiplier<8>` ⇒ `a*b`（16 位）
// - `non_restoring_divider<8>` ⇒ `b != 0` 时 `q = a/b`、`r = a - q*b`；
//   `b == 0` 时 `q = 0xFF`、`r = 0`
// - `fixed_point_*<16,8>` ⇒ `add = (a+b)&0xFFFF`；`sub = (a-b)&0xFFFF`；
//   `mul = ((a*b) >> 8) & 0xFFFF`；`div = ((a<<8)/b) & 0xFFFF`（**仅 b ≠ 0**，见缺陷 ③）
//
// ⚠️ 向量表**全部放在文件作用域**（陷阱 T3）：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**（只有**圆括号**才保护）。
//
// ⚠️ **MUST 逐个显式驱动全部输入端口**（仓内 ANTI-PATTERNS 已登记「未驱动端口读到的是
//    **未定义值**」，双后端下两侧的未定义值未必相同 ⇒ 依赖默认值 = 碰巧通过）。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/arithmetic_advance.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT 1：CLA（带进位）+ CSA（三操作数）──
class AddersDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a; ch_in<ch_uint<8>> b; ch_in<ch_uint<8>> c;
         ch_in<ch_bool> cin;
         ch_out<ch_uint<8>> cla_sum; ch_out<ch_bool> cla_carry;
         ch_out<ch_uint<8>> csa_sum; ch_out<ch_uint<8>> csa_carry;)

    AddersDut(ch::Component *parent = nullptr,
              const std::string &name = "adv_adders")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_uint<8> a_in(io().a.impl());
        ch_uint<8> b_in(io().b.impl());
        ch_uint<8> c_in(io().c.impl());

        auto cla = chlib::carry_lookahead_adder<8>(
            a_in, b_in, ch_bool(io().cin.impl()));
        io().cla_sum <<= cla.sum;
        io().cla_carry <<= cla.carry_out;

        auto csa = chlib::carry_save_adder<8>(a_in, b_in, c_in);
        io().csa_sum <<= csa.sum;
        io().csa_carry <<= csa.carry;
    }
};

// ── DUT 2：两个乘法器（**逐字相同**，见文件头缺陷 ①）+ 非恢复除法 ──
class MulDivDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a; ch_in<ch_uint<8>> b;
         ch_out<ch_uint<16>> wallace; ch_out<ch_uint<16>> booth;
         ch_out<ch_uint<8>> quot; ch_out<ch_uint<8>> rem;)

    MulDivDut(ch::Component *parent = nullptr,
              const std::string &name = "adv_muldiv")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_uint<8> a_in(io().a.impl());
        ch_uint<8> b_in(io().b.impl());
        io().wallace <<= chlib::wallace_tree_multiplier<8>(a_in, b_in);
        io().booth <<= chlib::booth_multiplier<8>(a_in, b_in);
        auto dv = chlib::non_restoring_divider<8>(a_in, b_in);
        io().quot <<= dv.quotient;
        io().rem <<= dv.remainder;
    }
};

// ── DUT 3：定点四则（N=16, Q=8）──
class FixedPointDut : public ch::Component {
public:
    __io(ch_in<ch_uint<16>> a; ch_in<ch_uint<16>> b;
         ch_out<ch_uint<16>> fadd; ch_out<ch_uint<16>> fsub;
         ch_out<ch_uint<16>> fmul; ch_out<ch_uint<16>> fdiv;)

    FixedPointDut(ch::Component *parent = nullptr,
                  const std::string &name = "adv_fixed_point")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        constexpr unsigned N = 16;
        constexpr unsigned Q = 8;
        ch_uint<N> a_in(io().a.impl());
        ch_uint<N> b_in(io().b.impl());
        io().fadd <<= chlib::fixed_point_adder<N, Q>(a_in, b_in).result;
        io().fsub <<= chlib::fixed_point_subtractor<N, Q>(a_in, b_in).result;
        io().fmul <<= chlib::fixed_point_multiplier<N, Q>(a_in, b_in).result;
        // ⚠️ 只喂 b != 0（见文件头缺陷 ③：除零保护是死代码）
        io().fdiv <<= chlib::fixed_point_divider<N, Q>(a_in, b_in).result;
    }
};

// ── 向量表（手推，公式见文件头）──
struct AddCase {
    uint64_t a, b, c, cin;
    uint64_t cla_sum, cla_carry;
    uint64_t csa_sum, csa_carry;
};
const AddCase kAdd[] = {
    // a, b, c, cin -> cla_sum, cla_carry, csa_sum, csa_carry
    // ⚠️ 两列用的是**不同的三元组**：CLA 吃 (a,b,cin)、CSA 吃 (a,b,c)
    //    ⇒ 本表**首版**把 CSA 列按 (a,b,cin) 猜着填了 ⇒ 2 行错
    //    （`0xFF^0x01^0xFF` 是 0x01 不是 0xFF）⇒ ⇒ 被**双后端一致地**证伪
    //    ⇒ 改为按 `a^b^c` 与 `((a&b)|(b&c)|(a&c))<<1` 逐行重算。
    {0x00, 0x00, 0x00, 0, 0x00, 0, 0x00, 0x00},
    {0x01, 0x02, 0x04, 1, 0x04, 0, 0x07, 0x00},
    {0xFF, 0x01, 0xFF, 0, 0x00, 1, 0x01, 0xFE},
    {0xFF, 0xFF, 0xFF, 1, 0xFF, 1, 0xFF, 0xFE},
    {0x0F, 0x01, 0x0F, 0, 0x10, 0, 0x01, 0x1E},
    // ⚠️ (0x80,0x80,0x00)：只有 carry-out 置位 ⇒ 展示 CSA 的**截断**（缺陷 ②）
    {0x80, 0x80, 0x00, 1, 0x01, 1, 0x00, 0x00},
    {0xA5, 0x5A, 0xFF, 0, 0xFF, 0, 0x00, 0xFE},
    {0x7F, 0x00, 0x00, 1, 0x80, 0, 0x7F, 0x00},
};

struct MulDivCase {
    uint64_t a, b;
    uint64_t product;  // wallace 与 booth 共同的期望值
    uint64_t quot, rem;
};
const MulDivCase kMulDiv[] = {
    // ⚠️ 第 1 条是 **b == 0**：non_restoring_divider 的守卫是**真 select** ⇒ 商全 1 / 余 0
    {0x00, 0x00, 0x0000, 0xFF, 0x00},
    {0x37, 0x00, 0x0000, 0xFF, 0x00},
    {0x00, 0x05, 0x0000, 0x00, 0x00},
    {0x01, 0x01, 0x0001, 0x01, 0x00},
    {0x0A, 0x0A, 0x0064, 0x01, 0x00},
    {0x64, 0x07, 0x02BC, 0x0E, 0x02},
    {0xFF, 0xFF, 0xFE01, 0x01, 0x00},
    {0x80, 0x02, 0x0100, 0x40, 0x00},
    {0xA5, 0x5A, 0x3A02, 0x01, 0x4B},
    {0x13, 0x0B, 0x00D1, 0x01, 0x08},
};

struct FixedCase {
    uint64_t a, b;
    uint64_t fadd, fsub, fmul, fdiv;
};
const FixedCase kFixed[] = {
    {0x0100, 0x0080, 0x0180, 0x0080, 0x0080, 0x0200},
    {0x0200, 0x0300, 0x0500, 0xFF00, 0x0600, 0x00AA},
    {0x0080, 0x0100, 0x0180, 0xFF80, 0x0080, 0x0080},
    {0x1234, 0x0040, 0x1274, 0x11F4, 0x048D, 0x48D0},
    {0xFFFF, 0x0001, 0x0000, 0xFFFE, 0x00FF, 0xFF00},
    {0x0001, 0x0002, 0x0003, 0xFFFF, 0x0000, 0x0080},
    // 下面两条的 fdiv 是**截断到 0** 的形态（商 > 0xFFFF）⇒ 钉住 `fixed_point_divider`
    // 的「2N 位商 → N 位」截断点
    {0x4000, 0x0002, 0x4002, 0x3FFE, 0x0080, 0x0000},
    {0x8000, 0x0004, 0x8004, 0x7FFC, 0x0200, 0x0000},
};

}  // namespace

// ───────────── pair 1：CLA（含 carry_in）+ CSA ─────────────
DUAL_BACKEND_TEST_PAIR(
    AddersDut,
    "chlib CLA（含 carry_in）/ CSA：和与进位（两后端一致）",
    "[chlib][arithmetic_advance][cla][csa][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kAdd) {
            sim.set_input_value(io.a, tc.a);
            sim.set_input_value(io.b, tc.b);
            sim.set_input_value(io.c, tc.c);
            sim.set_input_value(io.cin, tc.cin);
            sim.tick();
            INFO("a=0x" << std::hex << tc.a << " b=0x" << tc.b << " c=0x" << tc.c
                        << " cin=" << std::dec << tc.cin);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.cla_sum)) ==
                    tc.cla_sum);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.cla_carry)) ==
                    tc.cla_carry);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.csa_sum)) ==
                    tc.csa_sum);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.csa_carry)) ==
                    tc.csa_carry);
        }
    });

// ───────────── pair 2：两个乘法器（逐字相同）+ 非恢复除法（含 b==0 守卫）─────────────
DUAL_BACKEND_TEST_PAIR(
    MulDivDut,
    "chlib Wallace/Booth 乘法器 + 非恢复除法（含除零守卫）（两后端一致）",
    "[chlib][arithmetic_advance][mul][div][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kMulDiv) {
            sim.set_input_value(io.a, tc.a);
            sim.set_input_value(io.b, tc.b);
            sim.tick();
            INFO("a=0x" << std::hex << tc.a << " b=0x" << tc.b);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.wallace)) ==
                    tc.product);
            // ⚠️ wallace 与 booth 的实现**逐字相同**（文件头缺陷 ①）⇒ 两条断言
            //    同时钉住「两者一致」与「乘积正确」
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.booth)) ==
                    tc.product);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.quot)) ==
                    tc.quot);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rem)) == tc.rem);
        }
    });

// ───────────── pair 3：定点四则（N=16, Q=8）─────────────
DUAL_BACKEND_TEST_PAIR(
    FixedPointDut,
    "chlib 定点四则（N=16, Q=8）：加/减/乘右移 Q/除左移 Q（两后端一致）",
    "[chlib][arithmetic_advance][fixed_point][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kFixed) {
            sim.set_input_value(io.a, tc.a);
            sim.set_input_value(io.b, tc.b);
            sim.tick();
            INFO("a=0x" << std::hex << tc.a << " b=0x" << tc.b);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.fadd)) ==
                    tc.fadd);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.fsub)) ==
                    tc.fsub);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.fmul)) ==
                    tc.fmul);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.fdiv)) ==
                    tc.fdiv);
        }
    });
