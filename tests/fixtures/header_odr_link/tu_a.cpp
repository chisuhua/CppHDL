// tests/fixtures/header_odr_link/tu_a.cpp
//
// ODR 链接 fixture · TU A
//
// 目的：仅 `#include "ch.hpp"`，不引用任何运行期符号。
// 若聚合头闭包内存在**非 inline 的自由函数定义**，则本 TU 与 tu_b.cpp
// 会各自发出一份外部定义 ⇒ 链接期 "multiple definition"。
//
// ⚠️ 本文件 MUST NOT 被"顺手"加上功能代码：判据是纯链接期判据，
//    加运行期依赖会让它需要链接 libcpphdl，从而把「链接失败」与
//    「未定义符号」两类失败混在一起。
#include "ch.hpp"

int odr_link_fixture_tu_a_anchor() { return 1; }
