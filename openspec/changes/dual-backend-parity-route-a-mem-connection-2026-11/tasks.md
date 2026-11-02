# Tasks：路线 A（D 批）—— `ch_mem` 读端口连接形态双后端

## C1 —— 新增 dual 文件（**原文件 MUST NOT 修改**）

- [x] `tests/test_mem_connection_dual.cpp`（**4** pair × 5 地址）
      ⇒ pair 1 `sread` → `ch_out`（2 tick）· pair 2 `aread` → `ch_out`（1 tick）
      ⇒ pair 3 `sread` → `ch_uint` 中转 → `ch_out`（16 位，1 tick）
      ⇒ pair 4 `sread` → `ch_reg` → `ch_out`（2 tick）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`
- [x] 原文件 `tests/test_mem_connection.cpp` **未修改**

## C2 —— 判据（N2′）与判别力

- [x] **40 == 2 × 20**（有/无 verilator 实测），且 4 个 pair **一次通过**
- [x] 变异 **2/2 全抓**（每条先确认仍可编译）：
      M1' sync 读端口恒读地址 0 ⇒ **3 failed**
      · M2 aread 恒读地址 0 ⇒ **1 failed**
- [x] ⚠️ **如实登记一条无效变异**：原计划 M1 = 把 `always_ff` 里的 `<=` 改成 `=`
      ⇒ 实测**恒绿** ⇒ 因为「只在 `tick()` 后读」的模型下两者**语义等价**
      （`always_ff` 内的阻塞赋值同样只在沿上生效）⇒ 该变异**不改变可观测行为**，
      不是判别力缺口。
- [x] 还原用 `cp`（刷新 mtime ⇒ 强制重建）⇒ 复验回到 40/20 全绿

## C3 —— 计数与验收

- [x] 实测：`ctest -N` **225** · `-L verilator_e2e` **57** · `-LE perf` **214** ·
      `verify_dual_backend_parity.sh` **PARITY_COUNT=48 / PAIRS=241** ·
      `tests/test_*.cpp` **128**
- [ ] AGENTS.md 锚点同步（4 个 anchor + 2 条源文件计数 + NOTES 条目）
- [ ] ⚠️ **全量重建后**跑 `ctest -LE perf`（AGENTS.md COMMANDS 段的强制要求：
      `ctest` 不会自动重建 ⇒ 只重建被点名的 target 会跑**旧二进制** ⇒ 假绿）
- [ ] 全部 `scripts/verify_*.sh` exit 0
- [ ] 归档：`--skip-specs`（`.openspec.yaml` 已显式声明）
