// tests/fixtures/conditional_assign_literal/neg_control.cpp
//
// ⚠️ 本 fixture MUST **编译失败** —— 它是「判据**有区分度**」的对照：
//     证明「`pos_literal.cpp` 编译通过」不是因为编译器**根本没检查**这段代码。
//
// 机制：这里**故意**用一个**不存在**的 API ⇒ ⇒ 编译 MUST 失败
//     ⇒ ⇒ 若 `pos_literal.cpp` 也能「通过」，就说明判据形同虚设。
//
// ⚠️ 失败点 MUST 指向**本文件**（而非别处）⇒ ⇒ 护栏会 grep 错误文本。

#include "ch.hpp"
#include "chlib/if_stmt.h"

using namespace ch::core;
using namespace chlib;

void probe() {
    ch_bool c(true, "c");
    ch_reg<ch_uint<8>> reg(0_d, "reg");
    _if(c).then([&] { conditional_assign(reg).next() = 0xAA_h; }).else_()
        .then([&] { conditional_assign_NO_SUCH_API(reg).next() = 0xBB_h; }).end();
}
