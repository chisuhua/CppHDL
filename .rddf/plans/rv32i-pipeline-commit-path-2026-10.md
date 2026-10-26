# rv32i-pipeline-commit-path-2026-10 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> ⚠️ **本 plan 是回填的**（2026-10-25）：P1 时的计划是「加观测面 → 逐拍诊断 →
> 逐条修复」，而**具体的 17 个缺陷是在诊断过程中才被逐个定位的**（每修一处就
> 暴露出下一处）⇒ 计划无法事先穷举。下文按**实际执行顺序**记录 TDD 5 步。

**Goal:** 让 RV32I SoC 能正确执行一段 41 条指令的程序（ALU + load/store + 分支），
并在 interpreter / verilator 两后端下**逐拍完全一致**。

**Architecture:** ① `Rv32iPipelineTop` 从空壳（6 个 `ch_in`）改成真 DUT
（**32 个端口** = 5 输入 + 27 输出 = 11 功能 `obs_*` + 16 诊断 `dbg_*`）；
② `Rv32iPipeline` 增 **17** 个 `dbg_*` 输出（verilator 只能读顶层端口 ⇒
内部信号必须一路引出）；③ `include/cpu/**` 内 15 处流水线修复（commit/valid
通路、地址与立即数、ALU/分支操作码、寄存器堆读路径、TCM 读路径）；
④ 护栏 C6 快照 + 零消费者豁免失效 + 计数/文档同步。

**Tech Stack:** C++23, Catch2 v3.7.0, CMake, Verilator（submodule）, bash。

---

## Task 1: 观测面（先有观测，后有修复）

- [x] **Step 1 (Red)**：写 `tests/test_rv32i_soc_dual_parity.cpp` 的端口存在性断言 ⇒ 编译失败
- [x] **Step 2**：SoC 顶层加 32 个端口 + 流水线加 17 个 `dbg_*` ⇒ 编译通过
- [x] **Step 3 (Green)**：测试能跑（此时**红** —— 逐周期 diff 在 `data_addr` /
      `data_write_data` 上分歧 52 处 ⇒ 这就是诊断入口）
- [x] **Step 4**：护栏 C6 快照同步 + 零消费者豁免移除（该头获得消费者）
- [x] **Step 5**：trace dump 工具（`RV32I_SOC_DEBUG=N`）⇒ 逐拍可读

## Task 2: 逐拍诊断 → 17 个缺陷

- [x] **Step 1 (Red)**：断言「ID 译码的指令 == 固件[pc 对应的字]」⇒ 0 处错位（对齐 OK），
      但「fetch 序列与 PC 不匹配」⇒ 抓到 D8（字节地址 vs 字索引）
- [x] **Step 2**：修 D8 ⇒ 指令流对齐；但「store 地址 = 1」⇒ 抓到 D11（funct3 语义）
- [x] **Step 3**：修 D9/D10/D11 ⇒ store 地址对；「x4 = 16 而非 10」⇒ 抓到 D10（alu_op 未接）
- [x] **Step 4**：修 D12（branch_type）⇒ 分支计数对；「load 恒读回 0」⇒ 抓到 D7 + D16
- [x] **Step 5**：「寄存器堆一个值都写不进去」⇒ 抓到 D6（`mem_valid_out` 语义）
- [x] **Step 6**：「写回了但值是上一条指令的」⇒ 抓到 D13/D14（读地址 + 读延迟）
- [x] **Step 7**：「store 的写数据是 0」⇒ 抓到 D4/D5（级间信号串级）
- [x] **Step 8**：「分支目标偏一个字」⇒ 抓到 D2/D3（pc_lag + EX 无自己的 pc）
- [x] **Step 9**：「每 11 拍程序重启」⇒ 抓到 D1（flush ⇒ PC 归零）
- [x] **Step 10**：「既有 fixture 的 imem 从没工作过」⇒ 抓到 D15

## Task 3: 验收断言 + 既有测试诚实性

- [x] **Step 1 (Red)**：把新测试的 golden 写成 ISA 语义（3 个 store 的地址+数据、
      分支计数、退休数、指令与 pc 对齐）⇒ 逐步转绿
- [x] **Step 2**：`test_verilator_complex_e2e` 的 RV32I 断言改为语义断言
      （观测 x3 的写回 == 3）⇒ 先红后绿
- [x] **Step 3**：`test_wrapper_ifstage` 的 pc 期望更新（IF/ID 的 pc 与指令同拍 ⇒ 2 拍）
- [x] **Step 4**：护栏 `CHERROR_ALLOWLIST` 期望条数下调（修复消掉了部分报错）
- [x] **Step 5**：计数同步（`CTEST_TOTAL` 200 / `EXAMPLES_TEST_SOURCES` 17）

## Task 4: 门禁 + 审稿 + 归档

- [x] 全量 `ctest -LE perf` / examples / samples / 7 个护栏
- [x] ADR-046 **D10-rev** + 三处 AGENTS.md
- [ ] 对抗审稿（critic）通过
- [ ] `openspec archive` + 手写主 spec `## Purpose`
