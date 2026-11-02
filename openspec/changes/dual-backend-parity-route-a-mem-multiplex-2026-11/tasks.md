# Tasks：路线 A（D 批）第 3 个 —— `ch_mem` 的两个新 codegen 形态

## C0 —— 先过「可转性 4 问」（**第一步不是写代码**）

- [x] 逐条读出 `tests/test_mem_timing.cpp` 的 7 个用例的 DUT / 断言 / 端口形态
      （由 A 组 explore 提供事实 + 自己复核）
- [x] 4 问逐个判定 ⇒ **只有 2 个形态有增量**（`:182` 的 mux 嵌读端口 · `:358` 的
      同一 mem 两个读端口）；其余 5 个**已被** `test_mem_sread_disable_dual` /
      `test_mem_comb_read_dual` / `test_mem_connection_dual` 覆盖
- [x] 确认「两个读端口」确实**零既有覆盖**：实测既有 dual 文件里每个 DUT 只有一个读端口

## C1 —— 新文件（**原文件 MUST NOT 修改**）

- [x] `tests/test_mem_multiplex_dual.cpp`
      ⇒ pair 1 `MuxOverReadPortDut`：`data_out = we ? data_in : <读端口的 data proxy>`
      （6 步序列：初值读 → 写 → 读回写值 → `we=0` 不写 → 换地址 → 再写）
      ⇒ pair 2 `MultiReadPortDut`：两个 `aread` 各自独立解码（4 组**互异**地址）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`
- [x] `tests/test_mem_timing.cpp` **未修改**

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：**28 == 2 × 14**
- [x] 变异 **4/4 全抓**：N1 读端口恒读地址 0 · N2 写口忽略 `we` · N3 写口忽略地址 ·
      N4 `aread` → `always_ff`
- [x] **N4 的机制查清（三步实证）** ⇒ 记为**框架层不变量**：
      ① 变异生成的 `top.v` 用独立 harness 驱动 ⇒ **正确** ⇒ Verilog 语义没错；
      ② `Simulator::tick()` 的 `eval()` 只在 `default_clock_instr_` 存在时才调用；
      ③ `src/simulator_init.cpp:153` 在 `sequential_instr_list_` 为空时把它置 nullptr
      ⇒ **无时序节点的设计时钟从不翻转** ⇒ `always_ff` 永不触发 ⇒ 静默读 0。
- [x] ⚠️ 如实登记：**我按 T2 预测 N4 会恒绿，实测转红** ⇒ 预测必须被实测检验
- [x] ⚠️ 收窄 T2 的表述（原文「零判别力」过强 ⇒ 改为「对 0/1-cycle **采样语义**不判别」）
- [x] ⚠️ 登记**未审计项**：是否还有其它构造也在漂移 ⇒ **独立立项**（本 change 不追）
- [x] 全部输入端口**逐个显式驱动**；期望值**手推**（一次通过）

## C3 —— 计数与验收

- [x] 实测（**全量重建后**）：`ctest -N` **230** · `-L verilator_e2e` **62** ·
      `-LE perf` **219** · `verify_dual_backend_parity.sh` **PARITY_COUNT=53 / PAIRS=251** ·
      `tests/test_*.cpp` **130**
- [x] AGENTS.md 锚点同步（4 个 anchor + 3 条 NOTES 计数 + NOTES 条目）
- [x] full rebuild 后跑 `ctest -LE perf` 全绿
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档：`--skip-specs`（`.openspec.yaml` 已显式声明）
