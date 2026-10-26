// @file dead_elif.cpp
// @brief fixture：`#if 1` 之后的 `#elif 1` 分支是**死的** ⇒ 其断言 MUST 不计入。
// 钉住的机制：条件编译状态机对 `#elif` 的判定（`gtaken[depth]` 为真 ⇒ 后续分支全死）。
// 期望：A = 0，S = 1（死分支里的用例宏被点名，可见性计数），E = 1，逐用例断言数 = [0]
#if 1
TEST_CASE("dead elif - live branch", "[aq]") {
    SUCCEED();
}
#elif 1
TEST_CASE("dead elif - dead branch", "[aq]") {
    REQUIRE(6 == 6);
}
#endif
