// tests/fixtures/multi_if_width/pos_bool1.cpp
//
// `T = ch_bool` / `T = ch_uint<1>` ⇒ MUST 保持合法（D5：结果类型恒为 `T`）
//
// ⚠️ 本 fixture 由 `scripts/verify_multi_if_width.sh` 机械核验（**不是** ctest
//     用例 —— 判据是**编译期**的）。
#include "ch.hpp"
#include "chlib/if.h"
using namespace ch::core;
using namespace chlib;

void probe() {
    ch_bool c(true), c2(false);

    // `T = ch_bool`
    ch_bool v1 = if_then<ch_bool>(c, ch_bool(true)).else_(ch_bool(false));
    // `T = ch_uint<1>`（1 位分支）
    ch_uint<1> v2 = if_then<ch_uint<1>>(c, ch_bool(true)).else_(ch_bool(false));

    (void)v1; (void)v2;
}
