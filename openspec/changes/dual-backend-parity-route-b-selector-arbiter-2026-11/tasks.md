# Tasks：路线 B（H 批）—— 两个 selector 进双后端 parity

## C0 —— 可转性 4 问

- [x] ① 语义是否已被别的 dual 文件覆盖？ ⇒ **否**（两个符号在 54 个 dual 文件里各 0 次）
- [x] ② 断言形态有判别力吗？ ⇒ 变异 6/6 全抓
- [x] ③ 与后端无关吗？ ⇒ **否**（真跑仿真 + 读顶层端口）
- [x] ④ 形态值得新写吗？ ⇒ 既有测试**全是裸 context** ⇒ R6 排除项 ⇒ **不可转换** ⇒ 新写

## C1 —— 新文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_selector_arbiter_dual.cpp`
      ⇒ pair 1 `priority_selector<4>`（7 组向量：最低位优先 / 多位置位）
      ⇒ pair 2 `round_robin_selector<4>`（10 组向量：起点推进 / 环绕扫描）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`
- [x] `tests/chlib/test_selector_priority.cpp` / `test_selector_round_robin.cpp` **未修改**

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：**68 == 2 × 34**
- [x] 变异 **6/6 全抓**（方向反转 / valid 恒值 / 起点少 1 / grant 恒 0 / 扫描反向）
- [x] 期望值**手推**（独立 Python 模型 + 模型自检 ⇒ **一次通过**）
- [x] ⚠️ 判别力要害已登记：**单位置位时两种语义给出同一个 grant** ⇒ 激励 MUST 含多位置位

## C3 —— 命名诚实性（B8 的 R3）与护栏自检修复

- [x] proposal 带 `<!-- touched-components: priority_selector, round_robin_selector -->`
- [x] 登记表 **+2 条**（本批）+ **+3 条**（G 批的三个 pipe，追溯补登记）
- [x] G 批归档 proposal **补上** `touched-components` 标记
- [x] 修护栏自检的投毒定位脆弱性（定位到「最近 change 触及」的那一行 ⇒ R3 必红）
- [x] 护栏 + **自检 3/3** 全绿

## C4 —— 计数与验收

- [x] 实测（全量重建后）：`ctest -N` **235** · `-L verilator_e2e` **67** · `-LE perf` **224** ·
      `verify_dual_backend_parity.sh` **58 / 263** · `tests/chlib/test_*.cpp` **49**
- [x] AGENTS.md 锚点同步（4 anchor + 3 NOTES 计数 + NOTES 条目含护栏变更登记）
- [x] 全量重建后 `ctest -LE perf` **224/224** 全绿
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
