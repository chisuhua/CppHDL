# Tasks：路线 B（新文件）—— `chlib/arithmetic_advance.h` 进双后端 parity

## C0 —— 先过「可转性 4 问」

- [x] ① 语义是否已被别的 dual 文件覆盖？ ⇒ **否**（实测：含 `DUAL_BACKEND_TEST_PAIR(`
      调用的源文件全集里**无任何一个** include 该头）
- [x] ② 断言形态有判别力吗？ ⇒ 变异实测 12/12 全抓（见 C2）
- [x] ③ 与后端无关吗？ ⇒ **否**（本文件全部跑仿真 + 读顶层端口）
- [x] ④ 形态不可转换 ⇒ 值得新写吗？ ⇒ 既有测试是**裸 context**（R6 排除项）
      ⇒ **必须新写**；且它**不在任何既有批次清单里**（计划漏项）
- [x] 读实现时先做**缺陷甄别**（不是先写测试）：确认三处可疑点（wallace≡booth /
      CSA 截断 / 除零守卫死代码）**均在源码层可判定**，其中第三处**不喂触发输入**

## C1 —— 新文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_arithmetic_advance_dual.cpp`
      ⇒ pair 1 `AddersDut`（CLA 含 `carry_in` + CSA 三操作数）
      ⇒ pair 2 `MulDivDut`（wallace / booth / 非恢复除法，**含 b == 0**）
      ⇒ pair 3 `FixedPointDut`（N=16/Q=8 四则，**除法不喂 b == 0**）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`
- [x] `tests/chlib/test_arithmetic_advance.cpp` **未修改**

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：**208 == 2 × 104**
- [x] 变异 **12/12 全抓**，且**每个断言组有专属变异**（转红行互不相同）：
      A1 `:254` · A2 `:256` · A3 `:258` · A4 `:260` · A5 `:277` · A6 `:281` ·
      A7 `:283` · A8 `:285` · A11 `:301` · A12 `:303` · A9 `:305` · A10 `:307`
- [x] ⚠️ 如实登记一处**手算笔误**（CSA 列误按 CLA 的入参组合填 ⇒ 2 行错
      ⇒ 被双后端一致证伪 ⇒ 按公式重算）
- [x] ⚠️ 三个既有缺陷**如实登记 + 明确不修**（各自的修复路径写在 proposal 里）
- [x] 全部输入端口**逐个显式驱动**

## C3 —— 计数与验收

- [x] 实测（**全量重建后**）：`ctest -N` **231** · `-L verilator_e2e` **63** ·
      `-LE perf` **220** · `verify_dual_backend_parity.sh` **PARITY_COUNT=54 / PAIRS=254** ·
      `tests/chlib/test_*.cpp` **45**
- [x] AGENTS.md 锚点同步（4 个 anchor + 3 条 NOTES 计数 + NOTES 条目）
- [x] 全量重建后跑 `ctest -LE perf` 全绿
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档：`--skip-specs`（`.openspec.yaml` 已显式声明）
