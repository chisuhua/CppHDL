// tests/fixtures/multi_if_width/reject_arithmetic_T.cpp
//
// 形态 D —— `T` 推导为纯算术 ⇒ MUST 编译期拒绝（「接受但不可用」死路）
//
// ⚠️ 本 fixture 由 `scripts/verify_multi_if_width.sh` 机械核验（**不是** ctest
//     用例 —— 判据是**编译期**的）。
//
// ⚠️ **为什么只构造、不转换**：形态 D 在**三处**都会被拒 ——
//   ① 本次新增的 `static_assert(HardwareType<T>)`（ctor）；
//   ② `operator T()` 里的 `static_cast<T>(select(...))`（`int` ← `ch_uint<32>`）；
//   ③ 赋给硬件类型时的转换失败（`multi_if<int>` → `ch_uint<8>`）。
//   ⇒ ⇒ 若 fixture 触发 ②③ ⇒ 中性化 ① 之后**仍然编译失败** ⇒ ⇒ R2 的
//   「中性化对照」就无法证明「拒绝来自 ①」⇒ ⇒ 判据会失去判别力。
//   ⇒ ⇒ 这里**只构造**（`operator T()` 不被实例化）⇒ ⇒ 拒绝源**唯一**。

#include "ch.hpp"
#include "chlib/if.h"
using namespace ch::core;
using namespace chlib;

void probe() {
    ch_bool c(true);
    // ⚠️ `T` 是纯算术 ⇒ MUST 编译失败
    //    （今天的行为：ctor **放行** ⇒ 结果**无法用** ⇒ 「接受但不可用」的死路）
    //    逃生口：`if_then<ch_uint<8>>(c, 1)`
    multi_if<int> m(c, 1);
    (void)m;
}
