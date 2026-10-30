// tests/fixtures/multi_if_width/reject_wide_literal.cpp
//
// 形态 A' —— sized 字面量分支过宽 ⇒ MUST 编译期拒绝
//
// ⚠️ 本 fixture 由 `scripts/verify_multi_if_width.sh` 机械核验（**不是** ctest
//     用例 —— 判据是**编译期**的）。
#include "ch.hpp"
#include "chlib/if.h"
using namespace ch::core;
using namespace chlib;

void probe() {
    ch_bool c(true), c2(false);
    ch_uint<8> x(0x11_h);
    // ⚠️ `0x1FF_h` 的宽度 W = **9**（sized literal ⇒ 编译期可知）⇒ MUST 编译失败
    ch_uint<8> v = if_then(c, ch_uint<8>(x)).elif(c2, 0x1FF_h);
    (void)v;
}
