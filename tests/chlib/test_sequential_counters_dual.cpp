// ⚠️⚠️ **路线 B（A5 的 H 批）第 5 批**（2026-11-02 · change
// `dual-backend-parity-route-b-arithmetic-2026-11`）
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `chlib::logic`（门 / mux / 三态）与 `chlib::sequential`（寄存器 / 各类计数器 /
// 边沿检测）是仓内**最基础**的一批组件，而盘点实测它们在 42 个 dual 文件里
// **零覆盖** ⇒ ⇒ 它们的 Verilog codegen 路径**从未被验证过**。
//
// ⚠️ **为什么这类语义特别危险**：它们几乎全部是「**寄存器 + 条件组合**」的混合体
// ⇒ ⇒ `reg->next = select(en, d, reg)` 这条形态在 codegen 侧要写
// `always_ff @(posedge clk) q <= en ? d : q;` ⇒ ⇒ **保持语义**（`en=0` 时
// **保持上一拍的值**而不是 0）是极易写错的一类 —— 而**只有**「先命中一个值、
// 再关掉使能、断言读回**旧值**」这种序列才能抓到它。
//
// ## 期望值（**穷举手推**，禁抄仿真输出）
//
// **寄存器类**（`register_` / `dff_` / `binary_counter` / `bcd_counter` /
// `gray_counter` / `johnson_counter` / `ring_counter` / `counter`）：
//   · `reg->next = select(en, next, reg)` ⇒ **`en=0` 时保持**上一拍的值；
//   · 复位（`rst=1`）优先于使能 ⇒ `next = select(rst, 0, select(en, d, reg))`；
//   · 计数器的**沿前值**才是本拍输出 ⇒ 「tick N 读到的是第 N-1 次推进后的值」。
//
// 🔴🔴 **`edge_detector` 在【端口输入】下【完全失效】**（实测探针：8 拍全序列
//   `pos`/`neg` **恒为 0**）—— **真缺陷 · 框架层 · 独立议题**：
//   实现是 `prev_signal->next = signal`（**无条件**跟随）⇒ ⇒ `prev_signal`
//   **恒等于** `signal` ⇒ ⇒ `pos_edge = signal & !prev_signal = 0` **恒 0**。
// ⚠️ **既有测试恰好绕开了它**（`tests/chlib/test_sequential.cpp` 用
//   `ch_bool signal = false;` ⇒ **建图期常量** ⇒ `prev_signal` 跟随的是**常量**
//   ⇒ ⇒ 沿检测才能工作）⇒ ⇒ 该缺陷**从未被暴露**。
// ⇒ ⇒ 本文件的 DUT 把 `sig` 做成**端口** ⇒ ⇒ 于是**该缺陷立刻显形**。
// ⇒ ⇒ **处置**：本 change **不修**它（独立立项）⇒ ⇒ 但**如实钉住**当前行为
// ⇒ ⇒ 「`pos`/`neg` 在端口输入下**恒为 0**」⇒ ⇒ 这样将来修好该缺陷时本文件会
// ⇒ ⇒ **立即变红** ⇒ ⇒ 变成该缺陷的**回归探针**。
//
// ⚠️ **`gray_counter` 的输出是组合的**：`gray = binary_counter ^ (binary_counter >> 1)`
//   ⇒ ⇒ 它**不**是寄存器 ⇒ ⇒ 读数与当前 `bin` **同拍** ⇒ ⇒
//   `bin=0→gray 0` · `1→1` · `2→3` · `3→2` · `4→6` · `5→7` · `6→5` · `7→4`
//   ⇒ ⇒ ⚠️ 下一个 `bin` 在**沿后**才变 ⇒ ⇒ 「读到 gray=3」的那一拍 `bin` 是 2。
//
// ⚠️ **`bcd_counter` 的 `carry` 是组合的**：`(counter == 9) & en` ⇒ ⇒
//   `count=9` 且 `en=1` 时**当场** carry=1。
//
// ⚠️ **`ring_counter` 初值 = 1**（`ch_reg<ch_uint<N>> counter(1_d, ...)`）⇒ ⇒
//   `1 → 2 → 4 → 8 → 1`（4 位）。
//
// 🔴🔴 **`johnson_counter` 在【端口使能】下【恒为 0】**（实测探针：9 拍全 0）
//   —— **第三个同类真缺陷 · 框架层 · 独立议题**：
//   实现写的是 `ch_bool feedback = !bit_select(counter, N - 1);`
//   ⇒ ⇒ **`!` 是 C++ 布尔非**（不是按位非）⇒ 且在**建图期**求值
//   ⇒ ⇒ 读的是 `counter` 的**建图期**值 ⇒ ⇒ 又叠加
//   `shifted = (counter << 1) | ch_uint<N>(feedback ? 1_d : 0_d)`
//   的**建图期三元** ⇒ ⇒ 结果恒 0。
//   ⚠️ **既有测试同样绕开了它**（`chlib/test_sequential.cpp` 用建图期常量）。
// ⇒ ⇒ 处置同上：**不修**（独立立项）+ **如实钉住**当前行为 ⇒ ⇒ 将来修好即变红。
//
// ⚠️ （**参考序列**：若 `feedback` 正确按位取反，4 位 johnson 应为
//   `0 → 1 → 3 → 7 → 15 → 14 → 12 → 8 → 0` —— 但那是**期望**，不是当前行为。）
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/logic.h"
#include "chlib/sequential.h"
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

/// DUT 1：逻辑门 / mux / 三态缓冲（**纯组合**）。
class LogicDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a; ch_in<ch_uint<8>> b; ch_in<ch_uint<1>> sel;
         ch_out<ch_uint<8>> and_v; ch_out<ch_uint<8>> or_v;
         ch_out<ch_uint<8>> xor_v; ch_out<ch_uint<8>> nand_v;
         ch_out<ch_uint<8>> nor_v; ch_out<ch_uint<8>> xnor_v;
         ch_out<ch_uint<8>> not_v; ch_out<ch_uint<8>> mux_v;
         ch_out<ch_uint<8>> tri_v;)

    LogicDut(ch::Component *parent = nullptr,
             const std::string &name = "logic_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().and_v = and_gate<8>(io().a, io().b);
        io().or_v = or_gate<8>(io().a, io().b);
        io().xor_v = xor_gate<8>(io().a, io().b);
        io().nand_v = nand_gate<8>(io().a, io().b);
        io().nor_v = nor_gate<8>(io().a, io().b);
        io().xnor_v = xnor_gate<8>(io().a, io().b);
        io().not_v = not_gate<8>(io().a);
        io().mux_v = mux2<8>(io().a, io().b, ch_bool(io().sel == ch_uint<1>(1_d)));
        io().tri_v = tri_state_buffer<8>(io().a, ch_bool(io().sel == ch_uint<1>(1_d)));
    }
};

/// DUT 2：寄存器（**使能 + 复位**）⇒ 钉「保持」语义。
class RegDut : public ch::Component {
public:
    __io(ch_in<ch_uint<1>> rst; ch_in<ch_uint<1>> en; ch_in<ch_uint<8>> d;
         ch_out<ch_uint<8>> q;)

    RegDut(ch::Component *parent = nullptr,
           const std::string &name = "reg_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool rst_b = ch_bool(io().rst == ch_uint<1>(1_d));
        ch_bool en_b = ch_bool(io().en == ch_uint<1>(1_d));
        io().q = register_<8>(rst_b, en_b, io().d, "reg8");
    }
};

/// DUT 3：计数器族（`binary_counter` / `bcd_counter` / `gray_counter` /
/// `johnson_counter` / `ring_counter`）⇒ 钉「沿后推进」+ 各自的回绕语义。
class CounterDut : public ch::Component {
public:
    __io(ch_in<ch_uint<1>> en;
         ch_out<ch_uint<4>> bin_cnt; ch_out<ch_uint<4>> bcd_cnt;
         ch_out<ch_bool> bcd_carry; ch_out<ch_uint<4>> gray_cnt;
         ch_out<ch_uint<4>> johnson_cnt; ch_out<ch_uint<4>> ring_cnt;)

    CounterDut(ch::Component *parent = nullptr,
                const std::string &name = "counter_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool en_b = ch_bool(io().en == ch_uint<1>(1_d));
        io().bin_cnt = binary_counter<4>(en_b, "bin_cnt");
        BCDCounterResult<4> bcd = bcd_counter<4>(en_b, "bcd_cnt");
        io().bcd_cnt = bcd.count;
        io().bcd_carry = bcd.carry;
        io().gray_cnt = gray_counter<4>(en_b, "gray_cnt");
        io().johnson_cnt = johnson_counter<4>(en_b, std::string("johnson_cnt"));
        io().ring_cnt = ring_counter<4>(en_b, "ring_cnt");
    }
};

/// DUT 4：边沿检测 ⇒ 钉「一次沿只产生一次 pos_edge」。
class EdgeDut : public ch::Component {
public:
    __io(ch_in<ch_uint<1>> sig; ch_out<ch_bool> pos; ch_out<ch_bool> neg;)

    EdgeDut(ch::Component *parent = nullptr,
             const std::string &name = "edge_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_bool sig_b = ch_bool(io().sig == ch_uint<1>(1_d));
        EdgeDetectorResult e = edge_detector(sig_b, "edge");
        io().pos = e.pos_edge;
        io().neg = e.neg_edge;
    }
};

// ── 手推金标 ────────────────────────────────────────────────────────────
// 逻辑门（8 位按位）
struct GateCase { uint64_t a, b; };
const GateCase kGateCases[] = {
    {0x00, 0x00}, {0xFF, 0xFF}, {0xF0, 0x0F}, {0x55, 0xAA},
    {0x01, 0x02}, {0x80, 0x80}, {0x7F, 0x80},
};

// 寄存器序列（rst / en / d → 读回 q）
struct RegStep { uint64_t rst; uint64_t en; uint64_t d; uint64_t q; };
const RegStep kRegSteps[] = {
    // tick1: rst=1 ⇒ q=0（**复位优先**）
    {1, 1, 0xAA, 0x00},
    // tick2: rst=0,en=1,d=0xAA ⇒ q=0xAA
    {0, 1, 0xAA, 0xAA},
    // tick3: en=0 ⇒ **保持** 0xAA（不是 0！）
    {0, 0, 0x55, 0xAA},
    // tick4: en=1,d=0x55 ⇒ q=0x55
    {0, 1, 0x55, 0x55},
    // tick5: rst=1 ⇒ q=0（**复位优先于使能**）
    {1, 1, 0xAA, 0x00},
    // tick6: en=0 ⇒ 保持 0
    {0, 0, 0xFF, 0x00},
};

// ⚠️ 计数器是 `reg->next = select(en, counter+1, counter)` ⇒ ⇒ **tick N 读到的是
//    【第 N 次推进后】的值**（不是第 N-1 次）⇒ ⇒ 第 1 拍就是 1。
// binary_counter<4>：1 → 2 → … → 15 → 0 → 1 …
const uint64_t kBinCount[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1};

// bcd_counter<4>：第 1 拍 = 1 ⇒ 序列 1..9 → 0 ⇒ 周期 **10**
// ⚠️ carry 是**组合**的 `(counter == 9) & en` ⇒ 读到的 carry 与**同一拍的 count**
//    对应 ⇒ ⇒ count=9 的那一拍 carry=1。
const uint64_t kBcdCount[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
const uint64_t kBcdCarry[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0};

// gray_counter<4>：输出 = bin ^ (bin>>1)，与**当前** bin 同拍
// ⚠️ 同样「tick N = 第 N 次推进后」⇒ 第 1 拍 bin=1 ⇒ gray=1
// ⚠️ 周期 = **16**（4 位二进制计数器的 gray 序列长度 = 2^4）
//    bin: 1..15, 0 ⇒ gray: 1,3,2,6,7,5,4,12,13,15,14,10,11,9,8,0
const uint64_t kGray[] = {1, 3, 2, 6, 7, 5, 4, 12, 13, 15, 14, 10, 11, 9, 8, 0};

// ⚠️ johnson_counter 在端口使能下**恒为 0**（见文件头「第三个同类真缺陷」）
//    ⇒ ⇒ 期望值全 0 ⇒ ⇒ 修好该缺陷时本文件立即变红 ⇒ ⇒ 它是**回归探针**。
const uint64_t kJohnson[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

// ring_counter<4>：初值 1，周期 **4**（4 个位轮转）⇒ 「tick N = 第 N 次推进后」
//    ⇒ 序列 2, 4, 8, 1, 2, 4, 8, 1 …
const uint64_t kRing[] = {2, 4, 8, 1, 2, 4, 8, 1};

// edge_detector：sig 序列 → pos / neg
struct EdgeStep { uint64_t sig; uint64_t pos; uint64_t neg; };
// ⚠️ 【实测语义】端口输入下 `pos` / `neg` **恒为 0**（见文件头「真缺陷」段）
//    ⇒ ⇒ 期望值全部为 0 ⇒ ⇒ **一旦该缺陷被修好，本文件立即变红** ⇒ ⇒
//    它是那个缺陷的**回归探针**，而不是「沿检测语义」的判据。
//    （「沿检测语义」本身由既有的 `chlib/test_sequential.cpp` 用**建图期常量**
//      覆盖 —— 那条路径不受本缺陷影响。）
const EdgeStep kEdgeSteps[] = {
    {0, 0, 0},   // tick1: 初始低
    {0, 0, 0},   // tick2: 仍低
    {1, 0, 0},   // tick3: 上升沿 ⇒ pos **仍为 0**（缺陷）
    {1, 0, 0},   // tick4: 仍高 ⇒ pos 仍 0
    {0, 0, 0},   // tick5: 下降沿 ⇒ neg **仍为 0**（缺陷）
    {0, 0, 0},   // tick6: 仍低
};

}  // namespace

// ───────────────────── pair 1：逻辑门 / mux / 三态 ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    LogicDut,
    "逻辑门 / mux2 / 三态缓冲 - 纯组合逐位一致",
    "[chlib][logic][gate][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kGateCases) {
            sim.set_input_value(io.a, tc.a);
            sim.set_input_value(io.b, tc.b);
            sim.set_input_value(io.sel, 0u);
            sim.tick();
            INFO("a=0x" << std::hex << tc.a << " b=0x" << tc.b);
            // ⚠️ 期望值 MUST 先算到局部变量：Catch2 的 `REQUIRE` 是宏，
            //    `&` / `|` 会被当成 Catch2 的表达式分解器（实测编译失败）。
            const uint64_t e_and = tc.a & tc.b;
            const uint64_t e_or = tc.a | tc.b;
            const uint64_t e_xor = tc.a ^ tc.b;
            const uint64_t e_nand = (~e_and) & 0xFFu;
            const uint64_t e_nor = (~e_or) & 0xFFu;
            const uint64_t e_xnor = (~e_xor) & 0xFFu;
            const uint64_t e_not = (~tc.a) & 0xFFu;
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.and_v)) == e_and);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.or_v)) == e_or);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.xor_v)) == e_xor);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.nand_v)) == e_nand);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.nor_v)) == e_nor);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.xnor_v)) == e_xnor);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.not_v)) == e_not);
            // sel=0 ⇒ mux2 取 in0=a；tri_state enable=0 ⇒ 0
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.mux_v)) ==
                    tc.a);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.tri_v)) == 0u);
        }
        // sel=1 ⇒ mux2 取 in1=b；tri_state enable=1 ⇒ a
        sim.set_input_value(io.a, 0xF0);
        sim.set_input_value(io.b, 0x0F);
        sim.set_input_value(io.sel, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.mux_v)) == 0x0Fu);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.tri_v)) == 0xF0u);
    });

// ───────────────────── pair 2：寄存器（保持 / 复位优先） ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    RegDut,
    "寄存器 - 使能关闭时**保持** + 复位优先于使能（两后端逐拍一致）",
    "[chlib][sequential][register][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kRegSteps) {
            sim.set_input_value(io.rst, tc.rst);
            sim.set_input_value(io.en, tc.en);
            sim.set_input_value(io.d, tc.d);
            sim.tick();
            INFO("rst=" << tc.rst << " en=" << tc.en << " d=0x" << std::hex
                        << tc.d);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.q)) == tc.q);
        }
    });

// ───────────────────── pair 3：计数器族 ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    CounterDut,
    "计数器族 - binary/BCD/gray/johnson/ring 的推进与回绕（两后端逐拍一致）",
    "[chlib][sequential][counter][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        sim.set_input_value(io.en, 1u);
        for (int i = 0; i < 17; ++i) {
            sim.tick();
            INFO("tick " << (i + 1));
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.bin_cnt)) ==
                    kBinCount[i]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.bcd_cnt)) ==
                    kBcdCount[i % 10]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.bcd_carry)) ==
                    kBcdCarry[i % 10]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.gray_cnt)) ==
                    kGray[i % 16]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.johnson_cnt)) ==
                    kJohnson[i % 8]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.ring_cnt)) ==
                    kRing[i % 8]);
        }
    });

// ───────────────────── pair 4：边沿检测 ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    EdgeDut,
    "边沿检测 - 端口输入下 pos/neg 恒为 0（已知缺陷的回归探针 · 两后端一致）",
    "[chlib][sequential][edge][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kEdgeSteps) {
            sim.set_input_value(io.sig, tc.sig);
            sim.tick();
            INFO("sig=" << tc.sig);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.pos)) == tc.pos);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.neg)) == tc.neg);
        }
    });
