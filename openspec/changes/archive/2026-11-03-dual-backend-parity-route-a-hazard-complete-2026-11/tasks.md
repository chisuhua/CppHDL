# Tasks：路线 A（F 批）—— `HazardUnitComplete` 进双后端 parity

## C0 —— 可转性 4 问

- [x] ① 语义是否已被别的 dual 文件覆盖？ ⇒ **否**（全仓 dual 文件无 `HazardUnitComplete`）
- [x] ② 断言形态有判别力吗？ ⇒ 变异 7/7 全抓
- [x] ③ 与后端无关吗？ ⇒ **否**（真跑仿真 + 读顶层端口）
- [x] ④ 形态值得新写吗？ ⇒ 无需新写 DUT（库组件即有顶层端口）⇒ 只写 16 个向量表
- [x] **先读实现**：逐条核对 `describe()` 的 `match`/`src`/`data`/`stall`/`flush` 公式

## C1 —— 新文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_hazard_complete_dual.cpp`（**零 DUT 复制**，直接用 `riscv::HazardUnitComplete`）
      ⇒ 16 个向量 × 7 个输出 = **112 断言/后端**
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`
- [x] `tests/chlib/test_hazard_complete.cpp` **未修改**

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：**224 == 2 × 112**
- [x] 变异 **7/7 全抓**（优先级 / x0 前推 / we 门控 / load-use rs2 / load-use x0 / flush 门控 / rs2 数据源）
- [x] ⚠️ 如实登记：**7 条全部两半边同时红** ⇒ 变异在两后端同构生效
- [x] ⚠️ 如实登记**两个死端口**（`id_is_load` / `id_is_branch`）与「本批不修」的理由

## C3 —— 计数与验收

- [x] 实测（全量重建后）：`ctest -N` **233** · `-L verilator_e2e` **65** · `-LE perf` **222** ·
      `verify_dual_backend_parity.sh` **56 / 258** · `tests/chlib/test_*.cpp` **47**
- [x] AGENTS.md 锚点同步（4 anchor + 3 NOTES 计数 + NOTES 条目）
- [x] 全量重建后 `ctest -LE perf` **222/222** 全绿
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
