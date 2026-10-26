// @file live_else.cpp
// @brief fixture：`#if 0` 区段是死的，但其 `#else` 之后的活代码**计入**。
// 钉住的机制：`#if 0` 区段跳过 + `#else` 分支复活。
// 期望：A = 1，S = 1（死区段里的用例宏被点名），E = 1，逐用例断言数 = [1]
#if 0
TEST_CASE("if zero branch", "[aq]") {
    REQUIRE(9 == 9);
}
#else
TEST_CASE("live else branch", "[aq]") {
    REQUIRE(10 == 10);
}
#endif
