// @file dead_macro.cpp
// @brief fixture：断言在**永不展开的宏体**里 ⇒ MUST 不计入。
// 钉住的机制：`#define` 宏体**整行不计**（宏体可能永不展开 ⇒ 计入即后门；
//             C1 第五轮对抗审稿 BLOCKER-1：往被掏空的文件尾追加一个含 52 条
//             `REQUIRE` 的宏体就能把计数灌回去）。
// 期望：
// EXPECT: A=0 S=0 E=1 C=[0]
#define DEAD_NEVER_EXPANDED() REQUIRE(5 == 5)

TEST_CASE("dead macro", "[aq]") {
    SUCCEED();
}
