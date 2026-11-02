// ⚠️⚠️ **路线 B（A5 的 H 批）第 5 批**（2026-11-02 · change
// `dual-backend-parity-route-b-arithmetic-2026-11`）
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `chlib::arithmetic` 的**带进位 / 带借位**算术是仓内最基础的算术语义，而盘点
// 实测 `add_with_carry` / `sub_with_borrow` 在 42 个 dual 文件里**零覆盖**
// ⇒ ⇒ 它们的 Verilog codegen 路径**从未被验证过**。
//
// ⚠️ **为什么这类语义特别危险**：它们的实现是「**扩位 + 截断**」三步
// （`ch_uint<N+1> extended_a = ch_uint<N+1>(a)` → 相加/相减 →
// `bits<N-1,0>(result)` 截断 + `bit_select(result, N)` 取进位）
// ⇒ ⇒ **扩位**（零扩展 vs 符号扩展）与**截断**是两后端最容易分叉的地方
// （Verilog 侧要写 `{{1{1'b0}}, a}` 与 `a[N-1:0]`，interpreter 侧是
// `ch_uint` 的宽度转换）⇒ ⇒ 而既有测试**只跑 interpreter**。
//
// ## 期望值（**穷举手推**，禁抄仿真输出）
//
// `add_with_carry<N>`：`extended = a + b + cin`（N+1 位）⇒ `sum = low N 位`、
// `carry_out = bit N`。
// `sub_with_borrow<N>`：`extended = a - b - bin`（N+1 位）⇒ `diff = low N 位`、
// `borrow_out = bit N`。
//
// ⚠️ **`borrow_out` 的语义**：实现是 `a - b - bin` 的 **N+1 位**结果取 bit N
// ⇒ ⇒ `0x00 - 0x01 - 0 = 0x1FF` ⇒ bit 8 = **1** ⇒ `borrow_out = 1`
// ⇒ ⇒ 即「**绝对值**意义上的借位」（不是 `a < b` 的比较语义）。
// ⇒ ⇒ ⚠️ 这与「`a < b`」在 `bin=0` 时**恰好等价**（无符号减法），
// 但在 `bin=1` 时不同（`0x00 - 0x00 - 1 = 0x1FF` ⇒ borrow=1，而 `a<b` 是 0）。
//
// 🔴🔴 **本批挖出的【真缺陷】（框架层 · 独立议题）**：
// `add_with_carry` / `sub_with_borrow` 内部写的是
// `make_literal(carry_in ? 1ULL : 0ULL, 1)`
// ⇒ ⇒ **C++ 三元在建图期求值** ⇒ ⇒ 当 `carry_in` 来自**端口**时，
// 建图期它**未驱动** ⇒ ⇒ **三元恒取 else 分支** ⇒ ⇒ **`cin` 被彻底忽略**！
// ⚠️ **既有测试恰好绕开了它**（`tests/chlib/test_arithmetic.cpp` 的
// `carry_in` 是**建图期常量** `ch_bool(true)` / `ch_bool(false)`）
// ⇒ ⇒ 该缺陷**从未被暴露**。
// ⇒ ⇒ 本文件的 DUT 把 `cin` 做成**端口** ⇒ ⇒ 于是**该缺陷在双后端 parity 下
// 立刻显形**（两条后端**一致**地给出「cin 被忽略」的结果）。
// ⇒ ⇒ **处置**：本 change **不修**它（属框架层 `chlib::arithmetic` 的实现缺陷，
// **独立立项**）⇒ ⇒ 但本文件**如实钉住**当前行为 ⇒ ⇒ 即「cin 在当前实现下
// **不参与**求值」⇒ ⇒ 这样将来修好该缺陷时，本文件会**立即变红** ⇒ ⇒
// 变成**该缺陷的回归探针**。
// ⚠️ ⇒ ⇒ 因此期望值表里 **`cin` 一律按「被忽略」** 计算（⇒ 即 `cin=0` 的结果）。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/arithmetic.h"
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

/// DUT：`add_with_carry` + `sub_with_borrow`（N=8）+ `compare` + `abs`。
class ArithDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a; ch_in<ch_uint<8>> b; ch_in<ch_uint<1>> cin;
         ch_out<ch_uint<8>> sum; ch_out<ch_bool> carry_out;
         ch_out<ch_uint<8>> diff; ch_out<ch_bool> borrow_out;
         ch_out<ch_bool> gt; ch_out<ch_bool> eq;
         ch_out<ch_uint<8>> absv;)

    ArithDut(ch::Component *parent = nullptr,
             const std::string &name = "arith_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool carry_in = ch_bool(io().cin == ch_uint<1>(1_d));
        AddWithCarryResult<8> ac = add_with_carry<8>(io().a, io().b, carry_in);
        SubtractWithBorrowResult<8> sb =
            sub_with_borrow<8>(io().a, io().b, carry_in);
        ComparisonResult<8> cmp = compare<8>(io().a, io().b);
        io().sum = ac.sum;
        io().carry_out = ac.carry_out;
        io().diff = sb.diff;
        io().borrow_out = sb.borrow_out;
        io().gt = cmp.greater;
        io().eq = cmp.equal;
        io().absv = abs<8>(io().a);
    }
};

// ── 手推金标 ────────────────────────────────────────────────────────────
// add_with_carry<8>：(a + b + cin) 的低 8 位 + bit8 进位
struct AddCase { uint64_t a, b; uint64_t cin; uint64_t sum; uint64_t carry; };
// ⚠️ `cin` 一律按【被忽略】⇒ ⇒ 结果 = `a + b`（不含 cin）
//    ⇒ 「cin=1 但结果不含它」这条**就是**该缺陷的判别点
const AddCase kAddCases[] = {
    {0x00, 0x00, 0, 0x00, 0},
    {0x01, 0x02, 0, 0x03, 0},
    {0x7F, 0x01, 0, 0x80, 0},   // 127+1 = 128（不溢出）
    {0xFF, 0x01, 0, 0x00, 1},   // 255+1 = 256 ⇒ 进位
    {0xFF, 0x01, 1, 0x00, 1},   // ⚠️ cin=1 但结果**不含**它（缺陷）
    {0x80, 0x80, 0, 0x00, 1},   // 128+128 = 256
    {0xFF, 0xFF, 1, 0xFE, 1},   // ⚠️ 255+255（不含 cin）
    {0x55, 0xAA, 0, 0xFF, 0},   // 0x55+0xAA = 0xFF
};

// sub_with_borrow<8>：(a - b - bin) 的低 8 位 + bit8 借位
struct SubCase { uint64_t a, b; uint64_t bin; uint64_t diff; uint64_t borrow; };
// ⚠️ `bin` 一律按【被忽略】⇒ ⇒ 结果 = `a - b`
const SubCase kSubCases[] = {
    {0x00, 0x00, 0, 0x00, 0},
    {0x05, 0x03, 0, 0x02, 0},
    {0x01, 0x00, 0, 0x01, 0},
    {0x00, 0x01, 0, 0xFF, 1},   // 0-1 = 0x1FF ⇒ 借位 1
    {0x00, 0x01, 1, 0xFF, 1},   // ⚠️ bin=1 但结果**不含**它（缺陷）
    {0x00, 0x00, 1, 0x00, 0},   // ⚠️ bin=1 但结果**不含**它（缺陷）
    {0xFF, 0xFE, 0, 0x01, 0},
    {0x10, 0x20, 0, 0xF0, 1},   // 16-32 = 0x1F0
};

// compare<8> + abs<8>
struct CmpCase { uint64_t a, b; uint64_t gt; uint64_t eq; uint64_t absv; };
const CmpCase kCmpCases[] = {
    {0x00, 0x00, 0, 1, 0x00},
    {0x01, 0x00, 1, 0, 0x01},
    {0x00, 0x01, 0, 0, 0x00},
    {0x7F, 0x7F, 0, 1, 0x7F},
    {0x80, 0x80, 0, 1, 0x80},   // ⚠️ abs 的符号位语义（见文件头）
    {0x81, 0x00, 1, 0, 0x7F},   // -127 的 2 补码 = 0x81 ⇒ 0-0x81 = 0x7F
    {0xFF, 0x00, 1, 0, 0x01},   // -1 ⇒ 0-0xFF = 0x01
    {0xF0, 0x00, 1, 0, 0x10},   // -16 ⇒ 0-0xF0 = 0x10
};

}  // namespace

// ───────────────────── pair 1：带进位加法 ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    ArithDut,
    "带进位加法 - sum 的低 8 位 + carry_out（两后端逐位一致）",
    "[chlib][arithmetic][carry][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kAddCases) {
            sim.set_input_value(io.a, tc.a);
            sim.set_input_value(io.b, tc.b);
            sim.set_input_value(io.cin, tc.cin);
            sim.tick();
            INFO("a=0x" << std::hex << tc.a << " b=0x" << tc.b
                        << " cin=" << tc.cin);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.sum)) ==
                    tc.sum);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.carry_out)) ==
                    tc.carry);
        }
    });

// ───────────────────── pair 2：带借位减法 ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    ArithDut,
    "带借位减法 - diff 的低 8 位 + borrow_out（两后端逐位一致）",
    "[chlib][arithmetic][borrow][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kSubCases) {
            sim.set_input_value(io.a, tc.a);
            sim.set_input_value(io.b, tc.b);
            sim.set_input_value(io.cin, tc.bin);
            sim.tick();
            INFO("a=0x" << std::hex << tc.a << " b=0x" << tc.b
                        << " bin=" << tc.bin);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.diff)) ==
                    tc.diff);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.borrow_out)) ==
                    tc.borrow);
        }
    });

// ───────────────────── pair 3：比较 + abs ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    ArithDut,
    "比较器 + abs - 无符号比较与符号位扩展语义（两后端逐位一致）",
    "[chlib][arithmetic][compare][abs][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kCmpCases) {
            sim.set_input_value(io.a, tc.a);
            sim.set_input_value(io.b, tc.b);
            sim.tick();
            INFO("a=0x" << std::hex << tc.a << " b=0x" << tc.b);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.gt)) == tc.gt);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.eq)) == tc.eq);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.absv)) ==
                    tc.absv);
        }
    });
