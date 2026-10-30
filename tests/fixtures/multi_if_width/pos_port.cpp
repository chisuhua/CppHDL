// tests/fixtures/multi_if_width/pos_port.cpp
//
// 形态 C 的**正面**一侧 —— 首分支为 port ⇒ MUST 归一化并**可用**
//
// ⚠️ 本 fixture 由 `scripts/verify_multi_if_width.sh` 机械核验（**不是** ctest
//     用例 —— 判据是**编译期**的）。
#include "ch.hpp"
#include "chlib/if.h"
using namespace ch::core;
using namespace chlib;

void probe() {
    ch_bool c(true);
    // ⚠️ 顶层 port（默认构造 ⇒ 本 fixture **只编译**，不运行）
    ch_in<ch_uint<8>> p8;
    ch_in<ch_bool> p1;

    // 首分支是 port ⇒ `T` 归一化为它的 `value_type` ⇒ MUST 编译通过
    ch_uint<8> v8 = if_then(c, p8);
    // `ch_bool` 端口没有 `operator ch_bool()` ⇒ MUST 经 `lnode` 兜底
    ch_bool v1 = if_then(c, p1);

    (void)v8; (void)v1;
}
