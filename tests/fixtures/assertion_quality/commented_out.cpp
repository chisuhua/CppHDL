// @file commented_out.cpp
// @brief fixture：断言被**注释**掉（`//` 行注释 + `/* */` 块注释）⇒ MUST 不计入。
// 钉住的机制：整行/行尾/跨行块注释的剥离。
// 期望（断言总数 / 可见性计数 / 字面用例数 / 逐用例断言数）：
// EXPECT: A=0 S=0 E=1 C=[0]
TEST_CASE("commented out", "[aq]") {
    // REQUIRE(2 == 2);
    /* REQUIRE(3 == 3); */
    /* multi
       line
       REQUIRE(4 == 4); */
    SUCCEED();
}
