// ⚠️⚠️ **路线 B（A5 的 H 批）第 4 批**（2026-11-02 · change
// `dual-backend-parity-route-b-arithmetic-encoding-2026-11`）
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `chlib` 的**编码器 / 译码器 / 优先级选择器**是仓内最基础的一批算术语义，
// 而它们的 **dual 覆盖率是 0**（盘点实测：`priority_encoder` ·
// `binary_encoder` · `binary_decoder` · `binary_to_onehot` · `onehot_to_binary` ·
// `bcd_to_binary` · `binary_to_bcd` · `gray_to_binary` 在 41 个 dual 文件里
// **一个都没出现**）⇒ ⇒ 它们的 Verilog codegen 路径**从未被验证过**。
//
// ⚠️ **为什么这类语义特别危险**：它们全由 **`select` 链**实现
// （`result = select(cond, v, result)` 逐层套），⇒ ⇒ **链的方向**决定语义
// ⇒ ⇒ 而「方向」在 interpreter 侧**几乎不可观测**：
//   · `priority_encoder` / `binary_encoder`：既有测试**只覆盖单 bit**
//     （`0x10`→4 · `0x20`→5）⇒ ⇒ **单 bit 无法区分方向**；
//   · `binary_decoder`：输入是**互斥**的 ⇒ ⇒ 方向同样不可观测。
// ⇒ ⇒ **「多 bit 同时置位时选哪个」这条真正的语义，两条后端都没被钉过**。
//
// ## 期望值（**穷举手推**，禁抄仿真输出）
//
// 实现（`include/chlib/combinational.h` / `converter.h`）的链方向：
//   `for (i = 0 .. N-1) { result = select(cond_i, v_i, result); }`
// ⇒ 第 i 轮把 `result` **包在里层** ⇒ ⇒ **后写的外层优先**
// ⇒ ⇒ **高编号 i 优先**（与 `chlib::compose_guard_chain` 的「先收集者在外层」
// 方向**相反** —— 两者都写 `select` 链但遍历方向不同 ⇒ ⇒ 这正是值得双后端
// 验证的地方）。
//
// ⚠️ **`priority_encoder` 的头注释说「优先处理低位」，与实现相反**（实现是高编号
// 优先）。既有测试 `chlib/test_combinational.cpp` 的多 bit 断言
// （`0x32` ⇒ 期望 5）**与实现一致** ⇒ ⇒ 注释是**错的**，实现是既定的。
// ⇒ ⇒ 本文件按**实现**（= 既有断言）手推 ⇒ ⇒ 并在注释里登记这处不一致
// （⇒ 独立议题：注释订正 / 实现改向，**不在本 change 范围**）。
//
// 手推表（N = 8 ⇒ 输出宽度 `compute_idx_width(8)` = 3）：
//
// **priority_encoder**（高编号优先）：
//   0x00 → 0（无 bit 置位 ⇒ 保持初值 0）
//   0x01 → 0 · 0x02 → 1 · 0x04 → 2 · 0x08 → 3
//   0x10 → 4 · 0x20 → 5 · 0x40 → 6 · 0x80 → 7
//   0x32（bits 1,4,5）→ **5**（最高位优先）← 与既有断言一致
//   0x83（bits 0,1,7）→ **7**
//
// **binary_encoder**（同一实现 ⇒ 同一方向 ⇒ 高编号优先）：
//   单 bit：0x01→0 · 0x02→1 · 0x04→2 · 0x08→3 · 0x10→4 · 0x20→5 · 0x40→6 · 0x80→7
//   多 bit 0x32（bits 1,4,5）→ **5**
//
// **binary_decoder**（输入互斥 ⇒ 方向不可观测）：
//   0→0x01 · 1→0x02 · 2→0x04 · 3→0x08 · 4→0x10 · 5→0x20 · 6→0x40 · 7→0x80
//
// **binary_to_onehot**（输入互斥 ⇒ 同上）：0→0x01 · 3→0x08 · 7→0x80
//
// **onehot_to_binary**（同一 select 链 ⇒ 高编号优先）：
//   单 bit：0x01→0 · 0x02→1 · 0x04→2 · 0x08→3 · 0x10→4 · 0x20→5 · 0x40→6 · 0x80→7
//   多 bit 0x32（bits 1,4,5）→ **5**
//
// **bcd_to_binary**（N=8 ⇒ `tens = input>>4` · `ones = input&0xF` · `tens*10+ones`）：
//   0x00→0 · 0x05→5 · 0x09→9 · 0x10→10 · 0x12→12 · 0x25→25 · 0x99→99
//   ⚠️ 0x12 ⇒ tens=1 · ones=2 ⇒ 1*10+2 = **12** ✔
//
// **binary_to_bcd**（N=8 ⇒ `tens = input/10` · `ones = input%10` ⇒ `(tens<<4)|ones`）：
//   0→0x00 · 5→0x05 · 9→0x09 · 10→0x10 · 12→0x12 · 25→0x25 · 99→0x99
//   ⚠️ 12 ⇒ tens=1 · ones=2 ⇒ (1<<4)|2 = **0x12** ✔
//   ⚠️ 99 ⇒ tens=9 · ones=9 ⇒ (9<<4)|9 = **0x99** ✔
//
// **gray_to_binary**（N=8）：**标准**格雷码 ⇒ 二进制（定义：gray = b ^ (b>>1)，
//   反解 b = 折叠异或：b = gray ^ (gray>>1) ^ (gray>>2) ^ … ）
//   0x00→0x00 · 0x01→0x01 · 0x02→0x03 · 0x03→0x02 · 0x04→**0x07** · 0x05→**0x06**
//   0x06→**0x04** · 0x07→**0x05** · 0x08→**0x0F** · 0x0F→**0x0A** · 0x80→**0xFF**
//   0xFF→**0xAA**
//   ⚠️ **本表首版被我手推错**（写成 0x04→0x06 / 0x08→0x0C / 0xFF→0xFF）⇒ ⇒
//   实测**两后端一致**给出标准值（0x07 / 0x0F / 0xAA）⇒ ⇒ **不是 parity 分歧，
//   是推导错** ⇒ ⇒ 按**标准定义**重算后订正（不抄仿真输出）。
//
// **binary_to_gray**（`(input>>1) ^ input` ⇒ 与标准定义一致）：
//   0→0x00 · 1→0x01 · 2→0x03 · 3→0x02 · 4→0x06 · 5→0x07 · 6→0x05 · 7→0x04
//   8→0x0C · 15→0x08 · **128→0xC0** · **255→0x80**
//   ⚠️ 128 ⇒ (0x40)^0x80 = **0xC0**；255 ⇒ (0x7F)^0xFF = **0x80**
//   ⇒ ⚠️ 本表首版把这两项写成 0x80 / 0xFF（**丢了 `>>1` 那一半**）⇒ ⇒
//   实测两后端一致给出标准值 ⇒ ⇒ 同上：**推导错**，不是 parity 分歧。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/combinational.h"
#include "chlib/converter.h"
#include "component.h"
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

/// 编码器 DUT：`priority_encoder` + `binary_encoder`（同一实现 ⇒ 同一方向）。
///
/// ⚠️ **两个输出都是 `ch_out`** ⇒ 观测面在两条后端上完全一致。
class EncoderDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in; ch_out<ch_uint<3>> prio; ch_out<ch_uint<3>> bin;)

    EncoderDut(ch::Component *parent = nullptr,
               const std::string &name = "encoder_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().prio = priority_encoder<8>(io().in);
        io().bin = binary_encoder<8>(io().in);
    }
};

/// 译码器 DUT：`binary_decoder` + `binary_to_onehot` + `onehot_to_binary`。
class DecoderDut : public ch::Component {
public:
    __io(ch_in<ch_uint<3>> idx; ch_in<ch_uint<8>> onehot;
         ch_out<ch_uint<8>> dec; ch_out<ch_uint<8>> to_onehot;
         ch_out<ch_uint<3>> to_binary;)

    DecoderDut(ch::Component *parent = nullptr,
               const std::string &name = "decoder_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().dec = binary_decoder<8>(io().idx);
        io().to_onehot = binary_to_onehot<8>(io().idx);
        io().to_binary = onehot_to_binary<8>(io().onehot);
    }
};

/// BCD DUT：`bcd_to_binary` + `binary_to_bcd`（N=8 ⇒ 走 `if constexpr (N >= 8)` 分支）。
class BcdDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> bcd; ch_in<ch_uint<8>> bin;
         ch_out<ch_uint<8>> to_binary; ch_out<ch_uint<8>> to_bcd;)

    BcdDut(ch::Component *parent = nullptr,
           const std::string &name = "bcd_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().to_binary = bcd_to_binary<8>(io().bcd);
        io().to_bcd = binary_to_bcd<8>(io().bin);
    }
};

/// 格雷码 DUT：`gray_to_binary` + `binary_to_gray`。
class GrayDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> gray; ch_in<ch_uint<8>> bin;
         ch_out<ch_uint<8>> to_binary; ch_out<ch_uint<8>> to_gray;)

    GrayDut(ch::Component *parent = nullptr,
             const std::string &name = "gray_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().to_binary = gray_to_binary<8>(io().gray);
        io().to_gray = binary_to_gray<8>(io().bin);
    }
};

// ── 手推金标 ────────────────────────────────────────────────────────────
// priority_encoder / binary_encoder（**高编号优先**）
struct EncCase { uint64_t in; uint64_t prio; uint64_t bin; };
const EncCase kEncCases[] = {
    {0x00, 0, 0},    // 无 bit 置位 ⇒ 保持初值 0
    {0x01, 0, 0},    // bit0
    {0x02, 1, 1},    // bit1
    {0x04, 2, 2},    // bit2
    {0x08, 3, 3},    // bit3
    {0x10, 4, 4},    // bit4
    {0x20, 5, 5},    // bit5
    {0x40, 6, 6},    // bit6
    {0x80, 7, 7},    // bit7
    {0x32, 5, 5},    // bits 1,4,5 ⇒ **最高位优先** ⇒ 5（与既有断言一致）
    {0x83, 7, 7},    // bits 0,1,7 ⇒ **最高位优先** ⇒ 7
};

// binary_decoder / binary_to_onehot（输入互斥）
struct DecCase { uint64_t idx; uint64_t dec; uint64_t onehot; };
const DecCase kDecCases[] = {
    {0, 0x01, 0x01}, {1, 0x02, 0x02}, {2, 0x04, 0x04}, {3, 0x08, 0x08},
    {4, 0x10, 0x10}, {5, 0x20, 0x20}, {6, 0x40, 0x40}, {7, 0x80, 0x80},
};

// onehot_to_binary（高编号优先）
struct OnehotCase { uint64_t in; uint64_t out; };
const OnehotCase kOnehotCases[] = {
    {0x01, 0}, {0x02, 1}, {0x04, 2}, {0x08, 3},
    {0x10, 4}, {0x20, 5}, {0x40, 6}, {0x80, 7},
    {0x32, 5},  // bits 1,4,5 ⇒ 5
    {0x83, 7},  // bits 0,1,7 ⇒ 7
};

// BCD（tens*10+ones / (tens<<4)|ones）
struct BcdCase { uint64_t bcd; uint64_t to_bin; uint64_t bin; uint64_t to_bcd; };
const BcdCase kBcdCases[] = {
    {0x00, 0, 0, 0x00},   {0x05, 5, 5, 0x05},   {0x09, 9, 9, 0x09},
    {0x10, 10, 10, 0x10}, {0x12, 12, 12, 0x12}, {0x25, 25, 25, 0x25},
    {0x99, 99, 99, 0x99},
};

// 格雷码（gray→binary 是标准表；binary→gray = (x>>1)^x）
struct GrayCase { uint64_t gray; uint64_t to_bin; uint64_t bin; uint64_t to_gray; };
const GrayCase kGrayCases[] = {
    // ⚠️ `to_bin` = **标准** gray→binary（折叠异或）；`to_gray` = (b>>1)^b
    {0x00, 0x00, 0, 0x00}, {0x01, 0x01, 1, 0x01}, {0x02, 0x03, 2, 0x03},
    {0x03, 0x02, 3, 0x02}, {0x04, 0x07, 4, 0x06}, {0x05, 0x06, 5, 0x07},
    {0x06, 0x04, 6, 0x05}, {0x07, 0x05, 7, 0x04}, {0x08, 0x0F, 8, 0x0C},
    {0x0F, 0x0A, 15, 0x08}, {0x80, 0xFF, 128, 0xC0}, {0xFF, 0xAA, 255, 0x80},
};

}  // namespace

// ───────────────────── pair 1：编码器（优先级方向） ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    EncoderDut,
    "编码器 - priority/binary encoder 多 bit 时**高编号优先**（两后端逐位一致）",
    "[chlib][combinational][encoder][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kEncCases) {
            sim.set_input_value(io.in, tc.in);
            sim.tick();
            INFO("in=0x" << std::hex << tc.in);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.prio)) ==
                    tc.prio);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.bin)) == tc.bin);
        }
    });

// ───────────────────── pair 2：译码器 + onehot→binary ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    DecoderDut,
    "译码器 - binary_decoder / binary_to_onehot / onehot_to_binary（两后端逐位一致）",
    "[chlib][combinational][converter][decoder][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kDecCases) {
            sim.set_input_value(io.idx, tc.idx);
            sim.tick();
            INFO("idx=" << tc.idx);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dec)) == tc.dec);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.to_onehot)) ==
                    tc.onehot);
        }
        for (const auto &tc : kOnehotCases) {
            sim.set_input_value(io.onehot, tc.in);
            sim.tick();
            INFO("onehot=0x" << std::hex << tc.in);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.to_binary)) ==
                    tc.out);
        }
    });

// ───────────────────── pair 3：BCD ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    BcdDut,
    "BCD - bcd_to_binary / binary_to_bcd（两后端逐位一致）",
    "[chlib][converter][bcd][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kBcdCases) {
            sim.set_input_value(io.bcd, tc.bcd);
            sim.set_input_value(io.bin, tc.bin);
            sim.tick();
            INFO("bcd=0x" << std::hex << tc.bcd);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.to_binary)) ==
                    tc.to_bin);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.to_bcd)) ==
                    tc.to_bcd);
        }
    });

// ───────────────────── pair 4：格雷码 ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    GrayDut,
    "格雷码 - gray_to_binary / binary_to_gray（两后端逐位一致）",
    "[chlib][converter][gray][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kGrayCases) {
            sim.set_input_value(io.gray, tc.gray);
            sim.set_input_value(io.bin, tc.bin);
            sim.tick();
            INFO("gray=0x" << std::hex << tc.gray);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.to_binary)) ==
                    tc.to_bin);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.to_gray)) ==
                    tc.to_gray);
        }
    });
