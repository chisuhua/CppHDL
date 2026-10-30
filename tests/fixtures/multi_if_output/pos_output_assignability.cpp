// tests/fixtures/multi_if_output/pos_output_assignability.cpp
//
// ⚠️ 本 fixture MUST **编译通过** —— 它是 E1 + E3 两条契约的**可执行**判据：
//
//   **E1**：`multi_if` MUST 能**直接赋给 `ch_out`**（此前 **编译不过**：
//        `include/core/io.h:115: error: no matching function for call to
//        'get_lnode(const chlib::multi_if<ch::core::ch_uint<9> >&)'`）
//        ⇒ 而 `select` 对同类操作数**可用** ⇒ ⇒ 框架内不一致。
//
//   **E3**：`if_then` 的条件 MUST 能是 **`ch_in<ch_bool>` 端口**（此前
//        **编译不过**：`no matching function for call to
//        'if_then(ch::core::ch_in<ch::core::ch_bool>&, ...)'`）
//        ⇒ 而 `_if`（语句块形式）对端口**可用** ⇒ ⇒ 框架内不一致。
//
// ⚠️ **两条**修复都在 `chlib` 层 ⇒ ⇒ **`core/` 零改动**。
//
// ⚠️ 覆盖 4 种组合：显式 `T` / 推导 `T` × 端口条件 / 硬件条件。

#include "ch.hpp"
#include "chlib/if.h"

using namespace ch::core;
using namespace chlib;

void probe(ch_in<ch_bool> &en, ch_out<ch_uint<9>> &out9,
           ch_out<ch_uint<8>> &out8) {
    ch_bool c(true, "c");
    ch_uint<8> a(0xA5_h);

    // ① E1：显式 `T` + 直接赋给 `ch_out`
    out9 = if_then<ch_uint<9>>(c, ch_uint<8>(0xA5_h)).else_(ch_uint<4>(0xF_h));

    // ② E1：`T` 推导 + 直接赋给 `ch_out`
    out8 = if_then(c, a).else_(a);

    // ③ E3：条件是 `ch_in<ch_bool>` 端口
    out9 = if_then<ch_uint<9>>(en, ch_uint<8>(0xA5_h)).else_(ch_uint<4>(0xF_h));

    // ④ E3 + E1：端口条件 + 直接赋给 `ch_out`
    out8 = if_then(en, a).else_(a);

    // ⑤ E3 的**延伸**：`elif` 分支的条件**也** MUST 能是端口
    //    （⚠️ 此前 `multi_if::elif` 的条件形参仍是 `const ch_bool &`
    //    ⇒ ⇒ 端口绑不上 ⇒ ⇒ 「`if_then` 的条件可以是端口、`.elif` 的不行」
    //    = **同一能力面**上的**框架内不一致** ⇒ ⇒ 2026-10-30 补修）
    out9 = if_then<ch_uint<9>>(en, ch_uint<8>(0xA5_h))
               .elif(en, ch_uint<8>(0x5A_h))
               .else_(ch_uint<8>(0x11_h));

    // ⑥ 同上，但走 `multi_if` 的**直接**构造（`if_then` 之外）
    out8 = multi_if<ch_uint<8>>(en, a).elif(en, a).else_(a);
}
