// @file live_assert.cpp
// @brief fixture：正常形态 —— 断言**在活代码里** ⇒ MUST 被计入。
// 钉住的机制：断言宏识别 + 词边界（前面是行首/空白 ⇒ 合法边界）。
// 期望：A = 1，S = 0，E = 1，逐用例断言数 = [1]
//
// ⚠️ 本文件是**静态分析 fixture**，**不参与编译**（tests/CMakeLists.txt 用显式源文件列表，
//    无 GLOB_RECURSE）⇒ 语法只需「能被分析器读懂」，无需可编译。
// ⚠️ 不要在本目录引入 `ch_device` / `ch_module` / `DUAL_BACKEND_TEST` 字样
//    （`scripts/verify_rst_n_wiring.sh` 会按这三个标记扫 tests/）。
TEST_CASE("live assert", "[aq]") {
    REQUIRE(1 == 1);
}
