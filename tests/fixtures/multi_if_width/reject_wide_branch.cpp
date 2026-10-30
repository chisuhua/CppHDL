// tests/fixtures/multi_if_width/reject_wide_branch.cpp
//
// 形态 A —— 首分支窄、后续宽 ⇒ MUST 编译期拒绝
//
// ⚠️ 本 fixture 由 `scripts/verify_multi_if_width.sh` 机械核验（**不是** ctest
//     用例 —— 判据是**编译期**的）。
#include "ch.hpp"
#include "chlib/if.h"
using namespace ch::core;
using namespace chlib;

void probe() {
    ch_bool f(false), t(true);
    ch_uint<8> a(12_d), b(10_d);
    // ⚠️ 首分支 `a - b` 是 **8 位** ⇒ T = ch_uint<8>；后续 `a + b` 是 **9 位**
    //    ⇒ MUST 编译失败（今天的行为是**静默截断** ⇒ 丢 bit8）
    ch_uint<8> v = if_then(f, a - b).elif(t, a + b);
    (void)v;
}
