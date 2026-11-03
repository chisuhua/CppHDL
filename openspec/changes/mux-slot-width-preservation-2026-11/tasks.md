# Tasks：`mux` 槽位宽度保持

## T1 —— 根因定位（**实测**，全部通过临时插桩，事后已完整还原）

- [x] 复现矩阵：完整跑 ⇒ 6 条；单 DUT ⇒ 0 条
- [x] 插桩 `node_id` + 宽度 ⇒ `src1_width=32` ⇒ ⇒ 是**编译期索引**变体
- [x] 打印节点身份 ⇒ 图里正确（`mux_select` 宽 3）⇒ ⇒ 问题在**槽位**不在**图**
- [x] 检测「槽位宽度被 eval 改变」⇒ ⇒ `node 367 'top.mux_select' 3 -> 1`
- [x] 读实现 ⇒ `instr_mux::eval` 用 `*dst_ = *分支`（拷贝赋值 ⇒ RESIZE）
- [x] **临时插桩已全部还原**（`include/ast/instr_op.h` / `src/simulator.cpp` /
      `src/core/context.cpp` 三处 ⇒ `git status` 确认干净）

## T2 —— 修复

- [x] `include/ast/instr_mux.h`：两处 ⇒ `dst_->assign_truncate(*分支)`
- [x] 加注释说明「为什么 MUST NOT 用 `operator=`」+ 同一惯例的另外三处

## T3 —— 判据

- [x] `tests/test_mux_width_dual.cpp`（**新**，1 pair / **42 == 2 × 21**）
      - 「宽分支 → 窄分支」后**高位归零**（抓「不清高位」）
      - `bit2` / `bit1` 在窄分支被选中时**不越界**且为 0
- [x] `tests/CMakeLists.txt`：注册 + `FAIL_REGULAR_EXPRESSION "Bit index out of range"`

## T4 —— 变异 / 判别力（**如实登记边界**）

- [x] M1（改回 `*dst_ = *分支`）⇒ ⇒ **`FAIL_REGULAR_EXPRESSION` 判红** ✓
- [x] ⚠️ **M1 在【值层面】恒绿**（已实测）—— 压窄只丢「已经为 0」的高位
      ⇒ ⇒ `REQUIRE` 抓不到 ⇒ ⇒ **唯一可观测量是越界日志**
      ⇒ ⇒ 这就是那条 `FAIL_REGULAR_EXPRESSION` 的**存在理由**
- [x] 还原后复绿

## T5 —— 契约与验收

- [x] capability `dual-backend-test-coverage` **ADDED ×1**（1 Requirement + 3 Scenario）
      ⇒ 归档**禁** `--skip-specs`
- [x] `.rddf/improvements/cross-dut-bit-select-out-of-range-2026-11.md`
      ⇒ 状态改为**已修复** + 补 §6（定位过程 / 根因 / 为什么只在多 DUT / 修复与判据）
- [x] 全量重建 0 error · `ctest -LE perf` **231/231**
- [x] 计数锚点同步（`CTEST_TOTAL` 241 → **242** · `VERILATOR_E2E_TOTAL` 73 → **74** ·
      parity 64 → **65** · PAIRS 287 → **288** · non-perf 230 → **231** ·
      `tests/test_*.cpp` 133 → **134**）
