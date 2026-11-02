# Tasks：路线 A（D 批）—— `ch_mem` 写口 × `aread` 交互双后端

## C0 —— 先判「可转性」（**第一步不是写代码**）

- [x] 读原文件 6 个运行期用例的**断言实现**，逐条判「转 dual 后是否真有判别力」
      ⇒ ⚠️ **推翻了交接文档的「6 可转」初判**，实际只有 **2 个**有新覆盖：
      · `:203` / `:247`（输入地址 + `tick()` 后读）⇒ **对 0/1-cycle 零判别力**（T2）·
        其数据通路已被 `test_mem_connection_dual` 覆盖
      · `:289` / `:323`（时序）⇒ **已被** `test_mem_read_timing_dual.cpp` 以
        **同一判别形态**（寄存器驱动地址）+ 同一组初值/期望覆盖 ⇒ 重复零增量
      · `:368` / `:439` ⇒ codegen 文本断言，与后端无关
- [x] 核实**写口**在双后端确实零覆盖：三个 mem dual 文件的 `.write(` 计数 = **0**；
      `tests/chlib/test_memory_dual.cpp` 的写口属 `chlib::memory` RAM 族
- [x] ⚠️ 自查后**删除**了两个「重复」pair 与一个「零判别力」pair（初稿 5 pair
      ⇒ 终稿 2 pair）⇒ 依据：仓内既有先例把「已被覆盖的用例」判为重复而不转换

## C1 —— 新增 dual 文件（**原文件 MUST NOT 修改**）

- [x] `tests/test_mem_comb_read_dual.cpp`（**新**）
      ⇒ pair 1 跨拍写后读 + `we=0` 不写的负向对照（**3** 断言）
      ⇒ pair 2 同拍写 + aread 的写后值 + 持久化（**4** 断言）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`（含可转性判定的注释）
- [x] 原文件 `tests/test_mem_comb_read.cpp` **未修改**
- [x] 4 个输入端口**逐个显式驱动**（`addr_sync` / `addr_async` / `wdata` / `we`）

## C2 —— 判据（N2′）与判别力

- [x] **14 == 2 × 7**（有/无 verilator 实测）
- [x] 变异 **4/4 全抓**（每条先确认仍可编译）：
      M1 codegen `aread`→`always_ff` ⇒ pair 2 红 ·
      M2 interpreter `aread` 归入 sequential ⇒ pair 2 红 ·
      M3 codegen 写口忽略 `we` ⇒ pair 1 红（`:185` 读回 0x00）+ pair 2 ·
      M4 codegen 写口忽略地址 ⇒ pair 1 + pair 2 红
- [x] 每条 pair 都有 **≥2** 条变异能抓它 ⇒ 无填充 pair
- [x] 还原用 `cp`（刷新 mtime ⇒ 强制重建）⇒ 复验回到 14/7 全绿

## C3 —— 计数与验收

- [x] 实测（**全量重建后**）：`ctest -N` **226** · `-L verilator_e2e` **58** ·
      `-LE perf` **215** · `verify_dual_backend_parity.sh` **PARITY_COUNT=49 /
      PAIRS=243** · `tests/test_*.cpp` **129**
- [x] AGENTS.md 锚点同步（4 个 anchor + 3 条 NOTES 计数 + NOTES 条目）
- [x] ⚠️ **全量重建后**跑 `ctest -LE perf`（AGENTS.md COMMANDS 段的强制要求：
      `ctest` 不会自动重建 ⇒ 只重建被点名的 target 会跑**旧二进制** ⇒ 假绿）
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档：`--skip-specs`（`.openspec.yaml` 已显式声明）
