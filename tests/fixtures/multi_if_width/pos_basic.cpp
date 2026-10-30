// tests/fixtures/multi_if_width/pos_basic.cpp
//
// 正向清单（防过度实现）—— 五种写法 MUST 保持合法
//
// ⚠️ 本 fixture 由 `scripts/verify_multi_if_width.sh` 机械核验（**不是** ctest
//     用例 —— 判据是**编译期**的）。
#include "ch.hpp"
#include "chlib/if.h"
using namespace ch::core;
using namespace chlib;

void probe() {
    ch_bool c(true), c2(false);
    ch_uint<8> a(0xA5_h), b(0x5A_h);

    // ① 显式宽 `T` + 全窄分支（零扩展）
    ch_uint<16> p1 = if_then<ch_uint<16>>(c, ch_uint<8>(a));
    // ② 仅 `else_` 的两分支
    ch_uint<8> p2 = if_then(c, a).else_(b);
    // ③ **多分支无 `else_`**（尾部 = 末分支值）—— ADR-051 已刻意保留该形态
    ch_uint<8> p3 = if_then(c, a).elif(c2, b);
    // ④ 每分支显式 `ch_uint<N>(...)` 收敛（`conditional_demo` 的既有形态）
    ch_uint<8> p4 = if_then(c, ch_uint<8>(a)).elif(c2, ch_uint<8>(b)).else_(ch_uint<8>(a));
    // ⑤ 裸整数字面量分支（值域检查放行 ⇒ `1` 合法）
    ch_uint<8> p5 = if_then(c, ch_uint<8>(a)).elif(c2, 1);

    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
}
