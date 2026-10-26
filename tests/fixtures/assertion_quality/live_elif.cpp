// @file live_elif.cpp
// @brief fixture：`#if 0 / #elif 1` ⇒ `#elif` 体是**活的** ⇒ 计入。
// 钉住的机制：`#elif` 的常量求值（与 `dead_elif` 成对，锁住「`#elif` 不是一律死」）。
// 期望（S = 1 = 死区段里的用例宏被点名）：
// EXPECT: A=1 S=1 E=1 C=[1]
#if 0
TEST_CASE("if zero branch", "[aq]") {
    REQUIRE(11 == 11);
}
#elif 1
TEST_CASE("live elif branch", "[aq]") {
    REQUIRE(12 == 12);
}
#endif
