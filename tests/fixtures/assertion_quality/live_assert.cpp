// @file live_assert.cpp
// @brief fixture：正常形态 —— 断言**在活代码里** ⇒ MUST 被计入。
// 钉住的机制：断言宏识别 + 词边界（前面是行首/空白 ⇒ 合法边界）。
// 期望：
// EXPECT: A=1 S=0 E=1 C=[1]
//
// ⚠️ 本文件是**静态分析 fixture**，**不参与编译**（tests/CMakeLists.txt 用显式源文件列表，
//    无 GLOB_RECURSE）⇒ 语法只需「能被分析器读懂」，无需可编译。
// ⚠️ 本目录的文件 MUST NOT 出现「组件实例化」相关的三个 token
//    （`scripts/verify_rst_n_wiring.sh` 按它们扫 tests/ ⇒ 出现即把本文件
//    收进那条护栏的扫描集 ⇒ 引入未预期的耦合）。
//    ⚠️ **注释里也不许出现** —— 否则**这个注释自己**就会把本文件收进去。
//    ⇒ 该约定由 `scripts/verify_assertion_quality_fixtures.sh` 的 C5 检查强制。
TEST_CASE("live assert", "[aq]") {
    REQUIRE(1 == 1);
}
