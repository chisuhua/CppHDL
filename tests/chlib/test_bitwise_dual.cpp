// ⚠️⚠️ **路线 B（新文件）—— `chlib/bitwise.h` 首次进双后端 parity**
// （2026-11-02）
//
// ## 立项理由（不是凑数 —— 为什么这是**新文件**而不是"转换"）
//
// ① **零覆盖**：实测「含 `DUAL_BACKEND_TEST_PAIR(` 调用」的源文件全集里，
//    **没有任何一个** include `chlib/bitwise.h` ⇒ ⇒ 该组件在双后端矩阵里
//    **完全未被验证**。
// ② **既有单后端测试不可转换**：`tests/chlib/test_bitwise.cpp` 的 **9 个
//    TEST_CASE 全部是裸 context**（`ch::core::context` + `ctx_swap`，无
//    `ch_device` / 无顶层端口）⇒ ⇒ 按 capability `dual-backend-test-coverage`
//    的 R6 排除清单（"bare-context tests"），**结构上不可转换**
//    —— Verilator 后端只能通过**顶层端口**驱动与观测。
//    ⇒ ⇒ 本文件是**新写**：带端口的 DUT + **手推**期望值。
// ③ **本文件覆盖的原测试从未覆盖的维度（这是真正的增量）**：原测试把输入写成
//    **编译期常量**（`ch_uint<8> input(00010010_b)`）⇒ ⇒ 那条 `select` 链在
//    **建图期**就定型 ⇒ ⇒ **运行期求值路径从未被验证**。
//    本文件把输入改成**端口** ⇒ ⇒ `select` 链在**运行期**求值
//    ⇒ ⇒ 这正是 interpreter（`data_map` + 逐节点求值）与 Verilog
//    （嵌套三元 + 连续赋值）**可能分叉**的地方。
//
// ## 为什么这些组件「容易分叉」（值得钉的具体理由）
//
// - `leading_zero_detector` / `leading_one_detector`：一条 **N 级 `select` 链**，
//   每级的条件是「当前位是 0/1 **且** 计数等于 N-i-1」⇒ ⇒ **链的方向**决定语义；
// - `trailing_zero_detector` / `trailing_one_detector`：带一个**运行期累加**的
//   `found_one` / `found_zero` 反馈（`select` 自引用链）⇒ ⇒ 反馈顺序敏感；
// - `population_count`：逐位累加（`count + select(bit,1,0)`）⇒ ⇒ 8 次串行依赖；
// - `first_set_bit_detector`：**后写覆盖**式 `select` 链（i 从高到低）⇒ ⇒
//   「先匹配者胜」vs「后写者胜」在这里**结果不同**（取**最低**置位）；
// - `bit_reversal`：逐位 `<<` + `|` 累加 ⇒ ⇒ **移位量**与**宽度截断**敏感；
// - `bit_swap`：`~mask` + `&` + `zext` + `<<` + `|` 的组合；
// - `bit_field_insert`：运行期移位的掩码 + `& ~mask | value`。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 语义前提（逐条对照 `include/chlib/bitwise.h` 的实现）：
// - `leading_zero_detector<N>`：从最高位向下数**连续 0** 的个数；
//   **全 0 ⇒ N**（实现里的 `all_zero` 分支）。
// - `leading_one_detector<N>`：从最高位向下数**连续 1**；**全 1 ⇒ N**。
// - `trailing_zero_detector<N>`：从最低位向上数**连续 0**；**全 0 ⇒ N**。
// - `trailing_one_detector<N>`：从最低位向上数**连续 1**；**全 1 ⇒ N**。
// - `population_count<N>`：1 的个数（无特殊值）。
// - `first_set_bit_detector<N>`：**最低**置位的位置（0-based）；**无置位 ⇒ N**。
// - `bit_reversal<N>`：位序反转（"10110100" ⇒ "00101101"）。
// - `bit_swap<N>(v, p1, p2)`：交换 bit p1 与 bit p2（**0-based**）。
// - `bit_field_insert<N>(v, val, start, width)`：把 `val` 的低 `width` 位插到
//   `v` 的 `[start+width-1 : start]`，其余位保留。
//   （本文件固定 `val`/`start`/`width` 为常量：两种插入形态各一个输出。）
//
// ⚠️ **向量表放在文件作用域**（陷阱 T3）：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**（只有**圆括号**才保护）。
//
// ⚠️ **MUST 驱动全部输入端口**：本 DUT 只有 1 个输入端口，每个 pair 都显式驱动它
//    （仓内 ANTI-PATTERNS 已登记「未驱动端口读到的是未定义值」）。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/bitwise.h"
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

// ── DUT 1：6 个检测器 ──
class BitwiseDetectorsDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> value;
         ch_out<ch_uint<4>> lead_zero; ch_out<ch_uint<4>> lead_one;
         ch_out<ch_uint<4>> trail_zero; ch_out<ch_uint<4>> trail_one;
         ch_out<ch_uint<4>> popcount; ch_out<ch_uint<4>> first_set;)

    BitwiseDetectorsDut(ch::Component *parent = nullptr,
                        const std::string &name = "bitwise_detectors")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // ⚠️ 输入来自**端口** ⇒ select 链在**运行期**求值
        //    （原单后端测试用的是编译期常量 ⇒ 运行期路径从未被验证）。
        ch_uint<8> in = ch_uint<8>(io().value.impl());
        io().lead_zero <<= chlib::leading_zero_detector<8>(in);
        io().lead_one <<= chlib::leading_one_detector<8>(in);
        io().trail_zero <<= chlib::trailing_zero_detector<8>(in);
        io().trail_one <<= chlib::trailing_one_detector<8>(in);
        io().popcount <<= chlib::population_count<8>(in);
        io().first_set <<= chlib::first_set_bit_detector<8>(in);
    }
};

// ── DUT 2：位反转 / 位交换 / 两种位域插入 ──
class BitwiseOpsDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> value;
         ch_out<ch_uint<8>> reversed; ch_out<ch_uint<8>> swapped;
         ch_out<ch_uint<8>> inserted3; ch_out<ch_uint<8>> inserted2;)

    BitwiseOpsDut(ch::Component *parent = nullptr,
                  const std::string &name = "bitwise_ops")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_uint<8> in = ch_uint<8>(io().value.impl());
        io().reversed <<= chlib::bit_reversal<8>(in);
        // 交换 bit 1 与 bit 4（0-based）
        io().swapped <<= chlib::bit_swap<8>(in, 1, 4);
        // 插入 0b011 到 [5:3]（start=3, width=3）
        io().inserted3 <<= chlib::bit_field_insert<8>(
            in, ch_uint<8>(3_d), 3, 3);
        // 插入 0b101 到 [4:2]（start=2, width=3）
        io().inserted2 <<= chlib::bit_field_insert<8>(
            in, ch_uint<8>(5_d), 2, 3);
    }
};

// ── 检测器的期望表（手推，见文件头「期望值」段）──
struct DetectorExpect {
    uint64_t value;
    uint64_t lead_zero;
    uint64_t lead_one;
    uint64_t trail_zero;
    uint64_t trail_one;
    uint64_t popcount;
    uint64_t first_set;
};
const DetectorExpect kDetectors[] = {
    // 0x00 = 00000000：全 0 ⇒ lz=N, tz=N；全 1 分支不触发
    {0x00, 8, 0, 8, 0, 0, 8},
    // 0xFF = 11111111：全 1 ⇒ lo=N, to=N；lz=0, tz=0
    {0xFF, 0, 8, 0, 8, 8, 0},
    // 0x12 = 00010010：置位 b1,b4
    {0x12, 3, 0, 1, 0, 2, 1},
    // 0xEA = 11101010：置位 b1,b3,b5,b6,b7
    {0xEA, 0, 3, 1, 0, 5, 1},
    // 0xA0 = 10100000：置位 b5,b7
    {0xA0, 0, 1, 5, 0, 2, 5},
    // 0x0F = 00001111：置位 b0..b3
    {0x0F, 4, 0, 0, 4, 4, 0},
    // 0x80 = 10000000：仅置位 b7
    {0x80, 0, 1, 7, 0, 1, 7},
    // 0x01 = 00000001：仅置位 b0
    {0x01, 7, 0, 0, 1, 1, 0},
    // 0x55 = 01010101：置位 b0,b2,b4,b6
    {0x55, 1, 0, 0, 1, 4, 0},
    // 0xB4 = 10110100：置位 b2,b4,b5,b7
    {0xB4, 0, 1, 2, 0, 4, 2},
};

// ── 位操作的期望表（手推）──
struct OpsExpect {
    uint64_t value;
    uint64_t reversed;
    uint64_t swapped;
    uint64_t inserted3;
    uint64_t inserted2;
};
const OpsExpect kOps[] = {
    //           value  rev   swap  ins3  ins2
    // ⚠️ ins3 = (v & ~0x38) | 0x18、ins2 = (v & ~0x1C) | 0x14
    //    （公式逐字来自 include/chlib/bitwise.h 的实现）
    // ⚠️ 曾在此**手算错 3 处**（0xEA/0xA0/0x80 的 ins2）：`E2|14` 应为 **F6**
    //    而非 E6（OR 运算笔误）⇒ ⇒ 被**双后端一致地**证伪（两侧都给 0xF6）
    //    ⇒ 这是「手推期望」必须逐位复核、而不能凭感觉写下的实例。
    {0x00, 0x00, 0x00, 0x18, 0x14},
    {0xFF, 0xFF, 0xFF, 0xDF, 0xF7},
    {0x12, 0x48, 0x12, 0x1A, 0x16},
    {0xEA, 0x57, 0xF8, 0xDA, 0xF6},
    {0xA0, 0x05, 0xA0, 0x98, 0xB4},
    {0x0F, 0xF0, 0x1D, 0x1F, 0x17},
    {0x80, 0x01, 0x80, 0x98, 0x94},
    {0x01, 0x80, 0x01, 0x19, 0x15},
    {0x55, 0xAA, 0x47, 0x5D, 0x55},
    {0xB4, 0x2D, 0xA6, 0x9C, 0xB4},
};

}  // namespace

// ───────────── pair 1：6 个检测器 ─────────────
DUAL_BACKEND_TEST_PAIR(
    BitwiseDetectorsDut,
    "chlib bitwise 检测器：前导/后导 0-1、popcount、首个置位（两后端一致）",
    "[chlib][bitwise][detector][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kDetectors) {
            sim.set_input_value(io.value, tc.value);
            sim.tick();
            INFO("value=0x" << std::hex << tc.value);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.lead_zero)) ==
                    tc.lead_zero);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.lead_one)) ==
                    tc.lead_one);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.trail_zero)) ==
                    tc.trail_zero);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.trail_one)) ==
                    tc.trail_one);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.popcount)) ==
                    tc.popcount);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.first_set)) ==
                    tc.first_set);
        }
    });

// ───────────── pair 2：位反转 / 位交换 / 位域插入 ─────────────
DUAL_BACKEND_TEST_PAIR(
    BitwiseOpsDut,
    "chlib bitwise 位操作：位反转、位交换、两种位域插入（两后端一致）",
    "[chlib][bitwise][ops][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kOps) {
            sim.set_input_value(io.value, tc.value);
            sim.tick();
            INFO("value=0x" << std::hex << tc.value);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.reversed)) ==
                    tc.reversed);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.swapped)) ==
                    tc.swapped);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.inserted3)) ==
                    tc.inserted3);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.inserted2)) ==
                    tc.inserted2);
        }
    });
