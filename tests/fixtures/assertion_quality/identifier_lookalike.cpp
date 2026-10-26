// @file identifier_lookalike.cpp
// @brief fixture：`MY_TEST_CASE(` 这类**含 `TEST_CASE` 子串的标识符**会被逐用例
//        扫描器当成一个用例 ⇒ 它**吃掉一个名字槽** ⇒ 后续真实用例的名字被误报为 `?`。
//        （**计数不受影响**：断言数与用例数仍正确。）
// 钉住的机制：**已登记的局限**（不是「期望的正确行为」）——
//   分析器的逐用例扫描用**子串**匹配 `TEST_CASE`（而非「行首的用例宏」）⇒
//   任何在标识符里含该子串的写法都会错位。
//   ⚠️ 归位条件：分析器改为「按**登记序**取名」或「按行首匹配」后，本 fixture 的
//   用例名期望会变 ⇒ 那时**必须**同步更新期望表（而不是让 fixture 悄悄转红）。
// 期望：A = 2，S = 0，E = 2，逐用例断言数 = [1,1]
//   （E = 2 = **登记**的用例数；第二个 C 行的**名字**会是 `?` —— 名字错位，计数不错）
TEST_CASE("first real case", "[aq]") {
    REQUIRE(1 == 1);
}
static void helper() { MY_TEST_CASE("lookalike", "[aq]"); }
TEST_CASE("second real case", "[aq]") {
    REQUIRE(2 == 2);
}
