// tests/fixtures/multi_if_output/neg_control.cpp
//
// ⚠️ 本 fixture MUST **编译失败** —— 它是「判据**有区分度**」的对照：
//     证明「`pos_output_assignability.cpp` 编译通过」不是因为编译器**根本没
//     检查**这段代码。
//
// 机制：这里**故意**用一个**不存在**的 API ⇒ ⇒ 编译 MUST 失败
//     ⇒ ⇒ 失败点 MUST 指向**本文件**（⇒ 护栏 grep 错误文本）。

#include "ch.hpp"
#include "chlib/if.h"

using namespace ch::core;
using namespace chlib;

void probe(ch_in<ch_bool> &en, ch_out<ch_uint<9>> &out9) {
    ch_bool c(true, "c");
    out9 = if_then_NO_SUCH_API<ch_uint<9>>(c, ch_uint<8>(0xA5_h)).else_(ch_uint<4>(0xF_h));
}
