// tests/fixtures/conditional_assign_literal/pos_literal.cpp
//
// ⚠️ 本 fixture MUST **编译通过** —— 它是「`conditional_assign` 接受**裸
//     literal**」这条契约的**可执行**判据。
//
// 缺陷形态（修复前）：`conditional_assignment::assign()` 的 else 分支（块外直写）
// 写的是 `(*out_) <<= value;` ⇒ 它走 `operator<<=` ⇒ 用 `get_lnode(driver)`
// ⇒ ⚠️ `ch_literal_impl` **没有** `get_lnode` 重载 ⇒ ⇒ **裸 literal 编译不过**
// ⇒ ⇒ 而 `if_stmt.h` **自己的头注释示例**就是这个形态
//     （`conditional_assign(count).next() = 0_d;`）⇒ ⇒ **文档示例不可编译**。
//
// ⚠️ **关键**：该分支**总被实例化**（`if` / `else` 都是模板代码）⇒ ⇒ 即使
//     `reg_` 非空（运行期走不到）⇒ ⇒ 该行仍会编译 ⇒ ⇒ 一旦有 literal 实参
//     ⇒ **整个调用点编译失败**。
//
// ⚠️ **判据覆盖四种组合**：寄存器/输出端口 × 块内/块外。

#include "ch.hpp"
#include "chlib/if_stmt.h"

using namespace ch::core;
using namespace chlib;

void probe(ch_out<ch_uint<8>> &out8) {
    ch_bool c(true, "c");
    ch_reg<ch_uint<8>> reg(0_d, "reg");

    // ① 寄存器 + 块内 + 裸 literal
    _if(c).then([&] { conditional_assign(reg).next() = 0xAA_h; }).else_()
        .then([&] { conditional_assign(reg).next() = 0xBB_h; }).end();

    // ② 寄存器 + 块外 + 裸 literal（走 else 分支 ⇒ 修复点所在）
    conditional_assign(reg).next() = 0xCC_h;

    // ③ 输出端口 + 块内 + 裸 literal
    _if(c).then([&] { conditional_assign(out8).next() = 0xDD_h; }).else_()
        .then([&] { conditional_assign(out8).next() = 0xEE_h; }).end();

    // ④ 输出端口 + 块外 + 裸 literal
    conditional_assign(out8).next() = 0xFF_h;
}
