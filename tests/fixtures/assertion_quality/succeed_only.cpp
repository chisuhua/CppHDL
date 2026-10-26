// @file succeed_only.cpp
// @brief fixture：只有 `SUCCEED()` ⇒ **不是**可执行断言 ⇒ A = 0 且 C4b MUST 报违规。
// 钉住的机制：`SUCCEED` 不在断言宏表内（它是「账面覆盖」的典型形态）。
// ⚠️ 本 fixture 刻意**违反** C4b（每个用例 MUST ≥ 1 条可执行断言）⇒
//    它是「护栏 MUST 能抓到的反例」，不是「合规样例」。
// 期望：A = 0，S = 0，E = 1，逐用例断言数 = [0]
TEST_CASE("succeed only", "[aq]") {
    SUCCEED();
}
