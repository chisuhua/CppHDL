// tests/fixtures/multi_if_width/reject_narrow_explicit_T.cpp
//
// 形态 E —— 显式 `T` 过窄 ⇒ MUST 编译期拒绝
//
// ⚠️ 本 fixture 由 `scripts/verify_multi_if_width.sh` 机械核验（**不是** ctest
//     用例 —— 判据是**编译期**的）。
#include "ch.hpp"
#include "chlib/if.h"
using namespace ch::core;
using namespace chlib;

void probe() {
    ch_bool c(true);
    ch_uint<8> a(0xA5_h);
    // ⚠️ 显式 `T = ch_uint<1>` 而分支是 **8 位** ⇒ MUST 编译失败
    //    （今天的行为是**静默截断到 1 位** ⇒ 丢 7 位）
    ch_uint<1> v = if_then<ch_uint<1>>(c, a);
    (void)v;
}
