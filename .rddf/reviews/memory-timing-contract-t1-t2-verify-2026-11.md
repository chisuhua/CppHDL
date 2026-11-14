---
id: memory-timing-contract-t1-t2-verify-2026-11
日期: 2026-11-14
对象: change `memory-timing-contract-2026-11` 的 **T1 + T2 + T3**（ADR-068 T1/T3 · ADR-069 决策 A0）
状态: 🟡 **部分通过**（T1 / T2 / T3 落地且全绿 · **V12 已闭** · **V13–V15 的专向判别用例未写** · **T4 / T6 未做** · **L1–L6 未做**）
角色: builder 角色内建验收（**verifier 契约** · 只验不改）
判据来源: ① `openspec/changes/memory-timing-contract-2026-11/tasks.md`（⚠️ **2026-11-14 已归档** ⇒ ⇒ 现行路径 = `openspec/changes/archive/2026-11-14-memory-timing-contract-2026-11/tasks.md`）（S1–S6）② `design.md` §10（**先推后写**的手推金标）③ ADR-069 §5 **V1–V16** + §7.1–§7.6 ④ ADR-068 §5 **V7**
---

# 验收报告：`memory-timing-contract-2026-11` · T1 + T2 + T3（ADR-069 决策 A0）

> **范围** = D-TCM 读口 `aread → sread`（T1）· MEM 区裂为 `EX/MEM → M2 → MEM/WB`（T2）
> · `data_ready` 语义**登记**（T3 · **非**反压实现）
> ⇒ ⇒ **不含** T4 / T5 的全部产物判据 / T6 / L1–L6（固件加载链路）
> ⇒ ⇒ ⇒ **本报告不构成「B3 批闭合」的结论**

---

## 1. ⚠️ 实施期挖到的**架构级缺陷**（先于验收）

### 1.1 现象

`test_rv32i_soc_dual_parity` 的 `LoadByteLaneDiscriminating` 用例 ⇒ ⇒ load 的写回值**全为 0**；
`FetchSkidBufferDiscriminating` ⇒ ⇒ `rd=19` 期望 **42** 实得 **0**。
⇒ ⇒ **零报错**（两后端一致地错 ⇒ ⇒ 双后端 parity 对该族缺陷**结构性失明** ⇒ ⇒ 只能靠判别性固件抓）。

### 1.2 根因（拍级推导）

初版实现把 load 的**原始字**（raw word）**寄存进 M2**（`m2_word->next = io().data_read_data`）⇒ ⇒
`sread` 的输出寄存器在**同一条沿**上被更新 ⇒ ⇒ 在 M2 的装载沿（posedge N+2）采到的仍是**上一拍**的值
⇒ ⇒ 采到的是 **data(A_{i-1})**（**前一条指令**的地址）⇒ ⇒ 与 M2 的元数据（指令 i）**错配一拍**
⇒ ⇒ `LB`/`LH` **静默错车道**、`LW` **静默读回别人的字**。

⇒ ⇒ **该形态与 ADR-069 §7.3 + V1 互斥**（V1 禁止加地址寄存器 ⇒ ⇒ 原始字**只能**晚 1 拍可用）
⇒ ⇒ 已登记为 **ADR-069 §7.6**（dated addendum · **实施形态订正** · **决策零变更**）。

### 1.3 修法与判别力

- **M2 = 8 个元数据成员**（**无** `m2_word`）· 原始字**组合**取自 `io().data_read_data`
  （= `sread` 的**输出寄存器** ⇒ ⇒ 是寄存器 ⇒ ⇒ 满足 **V15**「数据叶 = M2 寄存器 + `sread` 输出寄存器」）
- **车道选择**仍在 **M2 的下游**（`riscv::LoadLaneSelector`）⇒ ⇒ 满足 **V5**
- ⚠️ **ADR-069 §7.6 是 builder 侧自指审查** ⇒ ⇒ **不构成独立证据** ⇒ ⇒ **待 architect 追认**

---

## 2. 逐条判定

### 2.1 T1 —— D-TCM 改寄存读（`include/cpu/pipeline/rv32i_tcm.h`）

| 判据 | 结果 | 证据 |
|---|:--:|---|
| 读口 = `sread(addr, ch_bool(true))`（enable **恒真** ⇒ ADR-068 §2.2-R6） | ✅ | `grep -n "sread" include/cpu/pipeline/rv32i_tcm.h` → `auto load_data = memory.sread(io().addr, ch_bool(true));` |
| 字节写 RMW **仍用 `aread`**（`ch_mem` 无字节使能写 API） | ✅ | 同文件 `:113` `old_word <<= memory.aread(io().addr);` |
| **组件级**双后端判据按新契约改写（**两拍一用例**：第 1 拍 = **沿级 read-old** · 第 2 拍 = **写后内容**） | ✅ | `tests/test_rv32i_timing_dual.cpp` 的 `DataTCM` pair（拍表新增 `rdata_prev` 列）⇒ ⇒ `ctest -R test_rv32i_timing_dual` → **Passed** |
| 「1 拍延迟」**可判别**（退回 `aread` ⇒ 第 1 拍即见新值 ⇒ **必红**） | ✅ | 断言形态：`REQUIRE(rdata == tc.rdata_prev)` 在**第 1 拍**（写前内容）⇒ ⇒ 0-cycle 实现**必红** |
| ⚠️ **产物级**判据（「D-TCM 的读 MUST NOT 是组合 mux」） | ❌ **未写** | tasks.md S1 第 1 项 ⇒ ⇒ **登记为未完成**（归属 S6 收口前补） |

### 2.2 T2 —— A0 的三处强制结构变更（**同批同 commit**）

| 判据 | 结果 | 证据 |
|---|:--:|---|
| ① 前推**档位 2** 的数据源 → **M2** | ✅ | `rv32i_pipeline.h`：`fwd_mux.io().mem_wb_result <<= m2_write_data;` |
| ①′ load 侧车道 mux 用 **M2 锁存的地址 / funct3** | ✅ | `load_lane.io().addr <<= m2_alu_result;` · `load_lane.io().funct3 <<= m2_funct3;` |
| ② D11 = **两级优先旁路**（M2 档 > WB 档 · **WB 档逐字保留**） | ✅ | `rv32i_pipeline.h` 的嵌套 `select(m2_bypass_hit_a, m2_write_data, select(wb_bypass_hit_a, wb_stage.io().write_data, rs1_data_result))` ⇒ ⇒ **WB 档仍在** |
| ③ 停顿链**加一级**（`stall_hold_d3`）⇒ ⇒ 停顿 = **4 拍** | ✅ | `hazard_unit.h`：`ch_reg<ch_bool> stall_hold_d3` + `stall_hold_d3->next = stall_hold_d2;` ⇒ ⇒ 实测 `load_use_stalls` = **8**（= 2 处 × 4 拍） |
| 对外端口契约**不变**（`stall` 仍是布尔输出） | ✅ | `HazardUnit::__io` 零改动 |
| **V12**（组件级停顿链真值表） | ✅ | **2026-11-14 新建** `tests/chlib/test_hazard_unit_stall.cpp`（36 断言 / 3 用例）⇒ ⇒ 钉住「检测条件持续 N 拍 ⇒ 停顿 **N+2** 拍」⇒ ⇒ 变异（删 `stall_hold_d3`）⇒ ⇒ **必红** ⇒ ⇒ ⚠️ V12 原文点名的 `chlib/test_hazard_complete.cpp` 已随 B0 批退役 ⇒ ⇒ 判据按原文不可满足 ⇒ ⇒ 落到**新建**文件 |
| ⚠️ **V13–V15 的「专向」判别用例**（手推 + 变异自证 · **新建 pipeline 级 fixture**） | ❌ **未写** | tasks.md S2「每处各配一条判别性用例」⇒ ⇒ **登记为未完成**；⚠️ **间接**判别力**已有**（24 条写回序列 + `load_use_stalls` = 8 ⇒ ⇒ 档位 2 源改回 `memwb_*` ⇒ ⇒ **必红**） |
| **V15** M2 档的数据叶 = **流水线寄存器** | ✅ | 叶 = M2 寄存器 + `sread` 输出寄存器（`io().data_read_data`）⇒ ⇒ **不成环** |
| **V16** `commit_count` 的退休时点**未动** | ✅ | 增量源仍 = `mem_stage.io().mem_valid_out` ⇒ ⇒ **fill 段仍是 3** ⇒ ⇒ `non_retired` = 3 + 8 + 12 = **23** ✓ |

### 2.3 T3 —— `data_ready` 语义**登记**（**非**反压实现）

| 判据 | 结果 | 证据 |
|---|:--:|---|
| driver **恒 1**（ADR-068 **V7** 胜出） | ✅ | `rv32i_pipeline_top.h:143`：`pipeline.io().data_ready <<= dtcm.io().ready;` ⇒ ⇒ `DataTCM` 的 `ready` 恒 `true` ⇒ ⇒ **零改动** |
| 契约登记（`include/cpu/AGENTS.md`） | ✅ | `include/cpu/AGENTS.md` 的「`Rv32iPipeline::data_ready` 的语义」条（**固定 1 拍延迟** · **不是握手** · **禁**真反压） |
| 公开面登记（`surface.md` 行为变更节 + `CHANGELOG.md`） | ✅ | 两处各一行（预告 → **已实施**） |
| ⚠️ 「MEM 级在 `data_ready == false` 时冻结」等**三条** | ⛔ **不在本批** | ADR-068 §6 + V7 明写「驱动 0 ⇒ MEM 冻结」= **反压实现** ⇒ ⇒ 属「**接 cache**」独立立项 ⇒ ⇒ tasks.md S3 已**订正**为「不在本批」（**不是未做**） |

### 2.4 时序金标（**只改 3 个数** · 手推值 vs 实测值）

| 断言 | 手推（`design.md` §10.4） | 实测 | 结果 |
|---|:--:|:--:|:--:|
| `load_use_stalls` | **8** | **8** | ✅ |
| `non_retired` | **23** | **23** | ✅ |
| 退休数（`kCycles - 24u`） | **176** | **176** | ✅ |

⇒ ⇒ **三数同时**对 ⇒ ⇒ 单改一个 ⇒ ⇒ 另两个必红 ⇒ ⇒ 判别力成立（**非对称 · 非回文**）✓
⇒ ⇒ **24 条写回序列**：值与顺序**逐条不变**（仅拍号平移 ⇒ ⇒ 归因 = `SHIFT` · **无 `STALE`**）✓

---

## 3. 不回归 + 遗留清扫

| 项 | 结果 | 证据 |
|---|:--:|---|
| 全量重建后 `ctest -LE perf` | ✅ **253/253** | ⚠️ **全量重建后**跑的（`cmake --build build -j$(nproc)` ⇒ ⇒ 再 `ctest`）⇒ ⇒ 非旧二进制 |
| `test_rv32i_soc_dual_parity` | ✅ **13064 断言 / 6 用例全绿** | 写回序列 + `LoadByteLane` + `FetchSkidBuffer` 三族同时绿 |
| 全部 **44** 个 `scripts/verify_*.sh` 护栏 | ✅ **全绿** | 逐条跑完（`verify_port_naming` 曾红 ⇒ ⇒ 已修：具名 + 登记表同步；`verify_no_duplicate_rv32i_headers` 曾红 ⇒ ⇒ 已订正头文件自述数） |
| RTL 产物护栏（flat / hierarchical / examples RTL sim） | ✅ | `verify_flat_rtl_emit` ✅（F1–F8）· `verify_hierarchical_rtl` ✅（R1–R9）· `verify_examples_rtl_sim` ✅（**10/10** · 含 RV32I SoC 的独立 Verilog TB **checks=24 fails=0**） |
| 公开面登记 | ✅ | `verify_public_api_visibility` ✅（新增 **2** 条探针 + 锚点 19→20 / 32→34） |
| 计数锚点 | ✅ | `scripts/update_test_count_anchors.sh` ⇒ ⇒ `DUAL_BACKEND_PARITY_PAIRS` **284 → 285** |
| 头文件自述数 | ✅ | `include/cpu/AGENTS.md`：**27 → 28 headers** · **6064 → 6305 lines** ⇒ ⇒ `verify_no_duplicate_rv32i_headers` C5 绿 |
| 遗留清扫（本次范围内） | ✅ | 无 TODO / 无半截实现 / 无临时日志 / 无被 skip 的测试 / 无未替换的 mock；`m2_word` 已在**代码注释**里逐字登记禁令（「补回即必错」） |

### 3.1 遗留清扫里**如实登记**的项（**不静默留尾巴**）

| 项 | 性质 | 归属 |
|---|---|---|
| 「产物级」判据：D-TCM 读口 MUST NOT 是组合 mux | **未写** | tasks.md S6 前补 |
| **V12**（组件级停顿链真值表） | ✅ **已闭**（2026-11-14 · `tests/chlib/test_hazard_unit_stall.cpp`）|
| V13–V15 的专向判别用例（手推 + 变异自证） | **未写** | tasks.md S6 前补（载体 = **新建** pipeline 级 fixture） |
| T4（写相位 + `verify_dtcm_read_timing.sh`）/ T6（紧邻同址 store→load） | **未做** | B3 批剩余 |
| L1–L6（固件加载链路 · ELF） | **未做** | B3 批剩余（依赖 T1 ⇒ ⇒ 顺序在后） |
| `rv32i_pipeline.h` 对 `rv32i_pipeline_regs.h` 的 include（**仓内零使用**） | **既有**（非本次引入） | 记录 · 不在本批改（测试直接 include 该头 ⇒ ⇒ 删它需同步确认） |
| ⚠️ `hazard_unit.h` 的两条**注释 vs 行为**差异（**既有** · 非本次引入） | **只登记** | ① 注释称移位链是「**固定延迟**」且「**不依赖 rd**」⇒ ⇒ **实测不成立**（检测条件掉一拍 ⇒ 停顿**提前结束**；流水线里成立是因为那里的条件**持续**）；② 注释称 `load_use_detected` 是「**单拍脉冲**」⇒ ⇒ **实测**它是检测条件的**组合**输出（条件持续 ⇒ 持续为高）⇒ ⇒ ⚠️ **登记需同时改架构台账的 §7 + 残差登记表（architect 域 · 双向 1:1）⇒ ⇒ 本轮**只登记在 builder 侧**（本报告 + `phase-6-next-steps.md`）⇒ ⇒ **待 architect 批次落地** |

---

## 4. 结论

- **T1 / T2 / T3 = 落地且全绿**（253/253 ctest + 44/44 护栏）
- ⚠️ **ADR-069 §7.2 的实施形态**在实施期被证伪 ⇒ ⇒ 已由 **§7.6**（dated addendum）订正
  ⇒ ⇒ **决策（A0 / 3 级 / 4 拍）零变更** ⇒ ⇒ 但 §7.6 **待 architect 追认**（builder 侧自指审查）
- ⛔ **本 change 未闭合**（T4 / T6 / L1–L6 / V13–V15 专向判据未完成）
  ⇒ ⇒ **不得**据此宣布「B3 批完成」⇒ ⇒ 也**不得**归档
