// @file throws_only.cpp
// @brief fixture：只有 `REQUIRE_THROWS` ⇒ 它**是**可执行断言 ⇒ 计入且满足 C4b。
// 钉住的机制：Catch2 的 throws 系断言宏在宏表内（且长名优先于短名 `REQUIRE`）。
// 期望：
// EXPECT: A=1 S=0 E=1 C=[1]
TEST_CASE("throws only", "[aq]") {
    REQUIRE_THROWS(throw 1);
}
