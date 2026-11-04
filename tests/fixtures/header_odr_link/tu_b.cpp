// tests/fixtures/header_odr_link/tu_b.cpp
//
// ODR 链接 fixture · TU B（含 main）
//
// 与 tu_a.cpp 一样只 `#include "ch.hpp"`。两个 TU **一起链接**即为判据。
//
// ⚠️ main 只调 tu_a 的 anchor —— 不调 `ch::` 的任何符号
//    （调用会让 fixture 依赖 libcpphdl ⇒ 见 tu_a.cpp 的说明）。
#include "ch.hpp"

int odr_link_fixture_tu_a_anchor();

int main() { return odr_link_fixture_tu_a_anchor() == 1 ? 0 : 1; }
