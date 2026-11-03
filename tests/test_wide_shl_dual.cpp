// ⚠️⚠️ **多 word 左移 MUST 与 Verilog 的 `<<` 逐位一致**（2026-11-03 · B1 根因修复的判据）
//
// ## 缺陷（实测定位，见 `.rddf/improvements/interpreter-eq-wide-operand-miscompare-2026-11.md`）
//
// 现象最初登记为「`eq` 在宽操作数下判错」：
//
//     ch_uint<64> a == (ch_uint<64>(1) << pos)     // shl 节点宽 127 位
//     ⇒ ⇒ pos = 0,1 正确；pos = 2..5 **返回 0（应为 1）**
//
// ⇒ ⇒ 但 `eq` 本身是**对的**：`ch::core::operator==` → `bv_eq_truncate` →
// `bv_eq` 会先做零扩展再逐 word 比较（已用独立的 `sdata_type` 探针逐值验证）。
// 真正的缺陷在 **`include/bv/bv_shift.h` 的 `bv_shl_vector`**：
//
//     out[i] = in[i - shift_words];                                   // ← 少了 << shift_bits
//     out[i] |= in[i - shift_words - 1] << (WORD_SIZE - shift_bits);  // ← 方向反了（应为 >>）
//
// ⇒ ⇒ 两个后果：
//   ① **虚假高位**：`in[0] << (64-d)` 把**源**的左移结果塞进**高** word
//      （实测 `1 << 2` 在 127 位槽位里得到 `2^66 + 2^2`，而 Verilog 给 `2^2`）；
//   ② **丢失跨 word 进位**：真正该进位的 `in[0] >> (64-d)` 一位都没进来
//      （实测 `0x8000000000000000 << 1` 得到 0，应为 `2^64`）；
//   ③ 旧代码只判 `i - s - 1 >= 0`、**不判上界** ⇒ ⇒ 当 `out_num_words > in_num_words + s`
//      时**越界读** `in[]`（`out=192 位 / in=64 位` 会读到源数组之外）。
//
// ⇒ ⇒ 修法：`out[i] = (in[i-s] << d) | (in[i-s-1] >> (64-d))`（两边都判上下界）。
// ⇒ Verilog 的 `<<` 本来就是对的 ⇒ ⇒ **这是 interpreter ↔ Verilog 的分歧** ⇒ ⇒
//   只有双后端判据能把它钉住（单后端只看数值会以为是「eq 的语义问题」）。
//
// ## 为什么单后端 / 窄宽度测试一直没抓到
//
// `bv_eq` 正确、`Shl` 的 `dst` 宽度由 codegen 与 interpreter **各自**决定；
// 而 `out_size <= 64 && in_size <= 64` 走的是 `bv_shl_scalar`（那一路是对的）
// ⇒ ⇒ 既有 bv 测试（`tests/test_bitvector.cpp`）的多 word 用例**整段被注释掉了**
// ⇒ ⇒ 「多 word 左移」这一格**零覆盖**。
//
// ## 本文件的判据
//
// 两条路径 × 11 组激励，每组断言 **7** 个 64 位切片（A: 3 片 · B: 4 片）：
//   * 路径 A：`in` = 1 个 word（64 位字面量 `1`）⇒ 结果 191 位（3 个 word）
//     ⇒ ⇒ 覆盖「源比目标**少**一个 word」（旧实现的 ① 与 ③ 都在这里）
//   * 路径 B：`in` = 2 个 word（`concat(hi, lo)` = 128 位）⇒ 结果 255 位（4 个 word）
//     ⇒ ⇒ 覆盖「跨 word 进位」（旧实现的 ②）、`shift_words = 1` 的 word 对齐与
//     word 偏移 + 位偏移组合
//
// 期望值由**独立模型**给出（`ref_shl()`：把 256 位容器按**定义**逐位置位
// ⇒ 与 `bv_shl_vector` 的「循环 + word 进位」实现**无共同代码路径**）。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == 2 × 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "simulator.h"

#include <array>
#include <cstdint>

using namespace ch;
using namespace ch::core;

namespace {

// ===========================================================================
// DUT：多 word 左移
// ===========================================================================
class WideShlDut : public ch::Component {
public:
    __io(ch_in<ch_uint<64>> lo;    // 路径 B 的低 word
         ch_in<ch_uint<64>> hi;    // 路径 B 的高 word
         ch_in<ch_uint<7>> k;      // 移位量 0..127
         ch_out<ch_uint<64>> a0;   // 路径 A（源 = 64 位字面量 1）的四段切片
         ch_out<ch_uint<64>> a1;
         ch_out<ch_uint<64>> a2;
         ch_out<ch_uint<64>> b0;   // 路径 B（源 = 128 位）的四段切片
         ch_out<ch_uint<64>> b1;
         ch_out<ch_uint<64>> b2;
         ch_out<ch_uint<64>> b3;)

    WideShlDut(ch::Component *parent = nullptr,
               const std::string &name = "wide_shl_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 路径 A：源只有 1 个 word ⇒ 结果宽度 64 + 127 = 191（3 个 word）
        ch_uint<64> one(1_d);
        auto shl_a = one << io().k;
        io().a0 <<= ch_uint<64>(shl_a);
        io().a1 <<= ch_uint<64>(bits<127, 64>(shl_a));
        io().a2 <<= ch_uint<64>(bits<190, 128>(shl_a));

        // 路径 B：源 2 个 word ⇒ 结果宽度 128 + 127 = 255（4 个 word）
        ch_uint<128> v = concat(io().hi, io().lo);
        auto shl_b = v << io().k;
        io().b0 <<= ch_uint<64>(shl_b);
        io().b1 <<= ch_uint<64>(bits<127, 64>(shl_b));
        io().b2 <<= ch_uint<64>(bits<191, 128>(shl_b));
        io().b3 <<= ch_uint<64>(bits<254, 192>(shl_b));
    }
};

// ===========================================================================
// 独立模型：256 位容器 + 逐位左移（**定义式**，不借用任何 bv_* 实现）
// ===========================================================================
struct Wide256 {
    std::array<uint64_t, 4> w{};

    void set_bit(unsigned b) {
        if (b < 256) {
            w[b / 64] |= (1ULL << (b % 64));
        }
    }
};

Wide256 ref_shl(const Wide256 &in, unsigned dist) {
    Wide256 out{};
    for (unsigned b = 0; b < 256; ++b) {
        if (((in.w[b / 64] >> (b % 64)) & 1ULL) != 0) {
            out.set_bit(b + dist);
        }
    }
    return out;
}

// ===========================================================================
// 激励表（**文件作用域** —— 宏 body 内花括号里的逗号会被预处理器当参数分隔）
//   {lo, hi, k}
// ===========================================================================
struct ShlCase {
    uint64_t lo, hi;
    unsigned k;
};
const ShlCase kShlCases[] = {
    {0x0000000000000000ULL, 0x0000000000000000ULL, 0},    // 全 0
    {0x0000000000000001ULL, 0x0000000000000000ULL, 0},    // 不移
    {0x0000000000000001ULL, 0x0000000000000000ULL, 2},    // ★ 旧实现：虚假高位
    {0x0000000000000001ULL, 0x0000000000000000ULL, 63},
    {0x0000000000000001ULL, 0x0000000000000000ULL, 64},   // shift_words=1, bits=0
    {0x0000000000000001ULL, 0x0000000000000000ULL, 65},   // shift_words=1, bits=1
    {0x8000000000000000ULL, 0x0000000000000000ULL, 1},    // ★ 旧实现：丢失跨 word 进位
    {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 1},    // 全 1 左移 1
    {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 63},
    {0x8000000000000000ULL, 0x8000000000000000ULL, 127},  // 最高位 → bit 255
    {0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL, 17},
};

}  // namespace

DUAL_BACKEND_TEST_PAIR(
    WideShlDut,
    "多 word 左移：结果 MUST 与 Verilog 的 << 逐位一致（无虚假高位 / 不丢进位）",
    "[core][bv][shift][multiword][dual][b1]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kShlCases) {
            sim.set_input_value(io.lo, tc.lo);
            sim.set_input_value(io.hi, tc.hi);
            sim.set_input_value(io.k, tc.k);
            sim.tick();

            // 路径 A：源 = 2^0
            Wide256 a_src{};
            a_src.set_bit(0);
            const Wide256 a_exp = ref_shl(a_src, tc.k);

            // 路径 B：源 = (hi : lo)
            Wide256 b_src{};
            b_src.w[0] = tc.lo;
            b_src.w[1] = tc.hi;
            const Wide256 b_exp = ref_shl(b_src, tc.k);

            INFO("lo=0x" << std::hex << tc.lo << " hi=0x" << tc.hi
                         << " k=" << std::dec << tc.k);

            // 路径 A 三片
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.a0)) ==
                    a_exp.w[0]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.a1)) ==
                    a_exp.w[1]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.a2)) ==
                    a_exp.w[2]);

            // 路径 B 四片
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.b0)) ==
                    b_exp.w[0]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.b1)) ==
                    b_exp.w[1]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.b2)) ==
                    b_exp.w[2]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.b3)) ==
                    b_exp.w[3]);
        }
    });
