// @file word_boundary.cpp
// @brief fixture：`MYREQUIRE(` / `XCHECK(` 这类**同前缀**标识符不是断言宏 ⇒ 不计入。
// 钉住的机制：词边界（断言宏前面 MUST 是非标识符字符）。
// 期望：A = 0，S = 0，E = 1，逐用例断言数 = [0]
TEST_CASE("word boundary", "[aq]") {
    MYREQUIRE(13 == 13);
    XCHECK(14 == 14);
    SUCCEED();
}
