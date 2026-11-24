# 审稿记录 · `rv32i-commit-event-pulse-2026-11`（S4a-0）

> **被审 commit**：`27dae01`（+ 本记录的「审稿轮次 1 修复」同批）
> **review_profile**：`build, plan`（代码面 = `include/**`+`examples/**`+`tests/**` ⇒ build；
> change 材料面 = `openspec/changes/**` ⇒ plan ⇒ **跨档** ⇒ 两档同结论）
> **审稿者**：`critic`（subagent · 只读）
> **轮次**：**1 / 2**（预算 2 轮 · 本轮 APPROVE ⇒ 无需复审）
> **日期**：2026-11-24

## 1. 结论

**APPROVE** · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**（build / plan 两档同结论）。

## 2. critic 的核对面（据其自述）

两根端口的**全部消费者**（全仓 grep）· `wb_stage.h` 的 `write_en` 定义 · MEM/WB 与 EX/MEM 的更新规则 ·
`d_cache_sync.h` 的 `ready` / `aw_fire` 逻辑 · spec delta 与既有 3 个 capability 的相容 ·
S4a 下游订正块。**未核**：运行期行为（归 verifier）。

## 3. 逐条 finding 与处置

| # | 级别 | finding | 处置 |
|---|---|---|---|
| — | ✅ | **「恰好一次」论证成立**：MEM/WB 的更新规则 `select(mem_stall, hold, load)` ⇒ ⇒ 一次驻留内恰有一拍非停顿 ⇒ ⇒ 脉冲恰好一次，**不丢不重**；「整条驻留落在停顿内被吞」的场景被更新规则**结构性排除** | **接受**（结论保留）⇒ ⇒ 但 critic 的 NIT 指出**原措辞不严谨** ⇒ ⇒ 见 NIT-1 |
| — | ✅ | **不成环论证成立**：`dbg_*` 零消费者 ⇒ 门控 `mem_stall` 无回路；`data_write_en` 在功能路径上 ⇒ 不可门控（实测 `FATAL`）且已 dated 登记 | **接受**（无需改） |
| — | ✅ | **store 事件无第二套口径**：`exmem_is_store` 与 `exmem_valid` 同构更新 ⇒ `is_store=1 ⟺ valid store`；代入 D-L1 ⇒ `obs_data_write_en ≡ aw_fire` ⇒ **它就是 AXI 写地址握手本身**，与功能面 `we` 电平**不存在错配** | **接受**（无需改） |
| — | ✅ | **S4a 后派生式仍成立**：`data_ready_calc` 两处共用同一节点；S4a 的 T2.3（`master.ready` 单拍）+ T3.3（路径 mux 后同一表达式）已把前提钉进下游 tasks ⇒ 前提失守会被 S4a 的 AC3 当场抓出 | **接受**（无需改） |
| — | ✅ | **既有 capability 相容**：spec delta 只 MODIFIED `rv32i-commit-path`，既有 3 个 Scenario 逐字保留；`rv32i-memory-timing` 是**前提**而非被违反者；`cpu-cache-subsystem` 零触碰 | **接受**（无需改） |
| — | ✅ | **「零行为变化」自陈成立**：`data_write_en` 未动 · `reg_file.write` 口径未动（残留已登记）· 既有 golden 的「折连续相同」workaround 在脉冲下幂等（逐项不变）· riscv-soc 侧 7 字程序不受影响 | **接受**（无需改） |
| — | ✅ | **plan 档**：AC1–AC7 均有可执行判据 · AC1–AC3 的判别力有实测反例（旧实现 12 / 9 / 8 ⇒ 必红）· 恒绿的 store 事件判据已按纪律撤除并归 S4a（AC7）· 订正块保留被推翻的原文与证据 · 范围与母 improvement 一致 | **接受**（无需改） |
| **SHOULD_FIX-1** | 🟠 | **跨产品消费者的注释仍是旧推导**：`examples/riscv-soc/src/soc_trace_counter.h:11-15` 的注释块写「`store_event` ← `obs_data_write_en`（= `exmem_is_store`）」「`retire_event` ← `wb.write_en`」⇒ ⇒ 推导式**已失实**；且**含数据反压的固件**下 `writeback_count` 的语义**已变**（脉冲固件 12 → 4）⇒ ⇒ 该消费者应有一条 **dated 脚注**，否则下一轮换人会按旧电平口径手推 | ✅ **已修**：该文件注释块加 **dated 订正**（两个源现为**单拍脉冲** · 计数器数的是「**事件次数**」· 今日**零可观测变化**（SoC 级 7 字程序只有 `sw` ⇒ write-through 单拍 ⇒ 无反压）· **含反压的固件会变** ⇒ ⇒ S4a 的 MMIO 固件 MUST 按事件次数手推）⇒ ⇒ 并**实测复核**：`tb_riscv_soc` ⇒ `RESULT: checks=7 fails=0` ✓ |
| **NIT-1** | 🔵 | 「每条指令的 MEM/WB 驻留的**首个拍必是非停顿拍**」的措辞**不严格**：MEM/WB 在非停顿沿装载后，`mem_stall` 可在紧随的拍**组合式重新拉高**（`data_ready` 是组合信号）⇒ ⇒ 驻留首拍**可以**是停顿拍；**结论（恰好一次）不受影响** —— 真正的机制是「**驻留内非停顿拍恰有一拍**」（停顿沿不更新内容） | ✅ **已修**（措辞精确化 · **实现不变**）：① `include/cpu/pipeline/rv32i_pipeline.h` 的口径注释改为「内容**只在非停顿沿换入** ⇒ ⇒ 一次驻留内**恰有一拍** `mem_stall = 0`（即该驻留的**最后一拍**）⇒ ⇒ 脉冲**恰好一次**」+ 显式声明「**不是**『驻留首拍必非停顿』」；② change 的 `design.md §3.1` 同步（标 **critic NIT 订正**） |

## 4. 本轮修复的验证

| 项 | 结果 |
|---|---|
| 全量重建 `cmake --build build -j$(nproc)` | ✅ 无 error |
| `test_rv32i_soc_dual_parity`（双后端） | ✅ **23164 断言 / 15 用例全绿** |
| 核级 TB `tb_rv32i_soc` | ✅ `RESULT: checks=24 fails=0`（AC5 零变化） |
| SoC 级 TB `tb_riscv_soc`（**跨产品消费者**） | ✅ `RESULT: checks=7 fails=0`（**零变化** ⇒ 佐证 SHOULD_FIX-1 的「今日零可观测变化」自陈） |
| `scripts/verify_*.sh`（55 个） | ✅ 全绿（含 C5 行数自述同步 6993 → **6996** · C6 端口快照 32 不变） |
| `openspec validate --all` | ✅ 54/54 |

## 5. 未采纳 / 记 backlog

| 项 | 处置 |
|---|---|
| `Rv32iCoreTop` 的**逐拍 flat≡hier 等价**（层次化语料）**未覆盖** | **backlog**（已登记在 `tests/hier_corpus_registry.md` 豁免表 · 含原因）⇒ ⇒ **不在本 change 内** |
| **store 事件**的脉冲化判别力 | **归 S4a**（今日 store 路径不反压 ⇒ 恒绿 ⇒ 恒绿判据已撤除 ⇒ 已在 proposal AC7 / design R4 登记） |
| `reg_file.write` 在停顿期仍每拍写同一 `(rd,data)` | **残留**（幂等 ⇒ 零可观测影响）⇒ ⇒ 已登记 design §4 R1 |
