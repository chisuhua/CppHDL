// @file dead_ifdef_else.cpp
// @brief fixture：`#ifdef X` 整组（含 `#else`）**不可判定** ⇒ 保守整体跳过。
// 钉住的机制：`#ifdef` / `#ifndef` / 非常量 `#if` ⇒ `gres = 2` ⇒ 连同 `#elif`/`#else`
//             一起跳过（宁可少算）。C1 第六轮 BLOCKER-2：此前只跳过 `#else` 之前，
//             恰好把**死**的那一半算进来（与「不会多算」的声明矛盾）。
// 期望（S = 2 = 两个用例宏都在被跳过的区段）：
// EXPECT: A=0 S=2 E=0 C=[]
#ifdef NOT_DEFINED_ANYWHERE
TEST_CASE("ifdef branch", "[aq]") {
    REQUIRE(7 == 7);
}
#else
TEST_CASE("ifdef else branch", "[aq]") {
    REQUIRE(8 == 8);
}
#endif
