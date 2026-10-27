---
id: feat-rv32i-isa-completeness
kind: feature
status: active
phase_refs: [phase-5]
主题: RV32I 语义边界收口（SB/SH 字节使能 · JAL/JALR 重定向 · SRA）+ 流水线健壮性（skid buffer · 取指双装）
---

## 概述

**「parity 通」≠「ISA 完整」。** 2026-10-26 的 C3（`rv32i-soc-dual-parity-2026-10`）证明的是
「interpreter 与 verilator 两个后端对同一设计给出**逐拍相同的输出**」——即**实现与自身一致**。
它**不**证明该实现符合 RV32I。2026-10-26 的 roadmap 审计实测出三处**实现缺口**，
以及两处**流水线健壮性缺陷**，此前只登记在 root `AGENTS.md §KNOWN LIMITATIONS`，roadmap 零覆盖。

| 缺口 | 证据（2026-10-26 实测） | 后果 |
|---|---|---|
| SRA 退化为 SRL | `include/cpu/pipeline/stages/ex_stage.h:134` `ch_uint<32> sra_result = srl_result;`（随后 `:168` 按 `ALU_OP_SRA` 选中它） | 算术右移对有符号负数算错 |
| JAL / JALR 不重定向 PC | `include/cpu/pipeline/rv32i_pipeline.h:180` `if_stage.io().branch_valid <<= ex_stage.io().branch_taken;` ⇒ `if_stage` 的 PC 选择器只吃「分支成立」，JAL/JALR 的「无条件跳 + 写回 pc+4」未接 | 跳转指令不跳、写回值错 ⇒ 任何子程序调用都坏 |
| SB/SH 字节使能未生效 | `mem_stage.h:182-200` 已按 `funct3` 正确生成 lane 掩码并输出 `store_byte_enable_out`（`rv32i_pipeline.h:369` → `data_strbe`），但 `include/cpu/pipeline/rv32i_tcm.h:71-105` 的 **DataTCM 没有 strbe 端口**，SoC 装配 `examples/riscv-mini/src/rv32i_pipeline_top.h:121-126` 也只接 addr/wdata/write/valid ⇒ **`data_strbe` 全程无消费者** | 字节/半字写按整字落盘 |
| 写数据未做 lane 对齐 | `rv32i_pipeline.h:364` `io().data_write_data <<= exmem_rs2_data;`（**未**按 `addr[1:0]` 左移 `8*offset`） | ⚠️ **只加 strbe 端口不够**：SB 落在 offset 1/2/3 时会把**低字节**写进该 lane ⇒ 必须同时对齐 wdata |
| 停顿丢弃「消费者之后的第一条指令」 | imem 是 1-cycle `sread` ⇒ 停顿那一拍「已提交但未锁存进 IF/ID」的指令无处可存（`AGENTS.md §KNOWN LIMITATIONS`） | 固件 MUST 在 load-use 停顿点之后手工插 NOP —— 这是**巧合保证**，不是语义保证 |
| 取指填充期同一条指令被装两次 | `ch_mem::sread` 的 `data_proxy` 在建图期被组合求值一次 ⇒ 其初值即 `mem[addr(拍0)]`，与拍 1 的真取指重复（**框架层**） | 退休数 +1（幂等、语义无害，但计数不可信） |
| 无数据前推（ADR-046 **D5** 悬置） | `include/cpu/pipeline/hazard_unit.h` 的 `io().rs1_data = io().rs1_data_raw`；接上会在真实装配形成组合环（`git show bf78367`） | 依赖指令必须靠停顿 + 间隔保证；且 5 个预留输入端口（`hazard_unit.h:46,65-67`）无内部消费者 |

**Why now**：C1/C2/C3 已完成 ⇒ 有了**单一实现 + 可跑的 SoC + 逐拍观测面（32 端口）**，
这是修 ISA 语义的**最佳时机**（有金标可对照）。再往后每加一层（cache/AXI 接入）都让回归面更大。

## 候选 change 分解

四个 change 相互独立，建议按此顺序（互不阻塞，但 ① 会改变固件的写语义 ⇒ 先落地以免金标重算两次）：

| # | 候选 change | 范围 | 前置 | 验收（可核） |
|---|---|---|---|---|
| ① | `rv32i-store-byte-enable-2026-10` | `DataTCM` 加 `strbe` 端口 + **wdata lane 对齐**（`rs2 << 8*addr[1:0]`）+ SoC 接线 + SB/SH 固件用例 | 无 | 同一地址先 `sw 0xFFFFFFFF` 再 `sb 0x00`（offset 1/2/3）⇒ 读回逐字节正确；双后端 parity |
| ② | `rv32i-jump-sra-2026-10` | `if_stage` 的 PC 源加入「JAL/JALR 目标」选择器 + JAL 写回 `pc+4` + `sra_result` 真算术右移 | 无 | JAL/JALR 固件用例（跳转 + `ra` 回读）+ `-8 >> 1 == -4` 用例；双后端 parity |
| ③ | `rv32i-forwarding-decision-2026-10` | **ADR-046 D5 裁决**（接/不接前推）+ 按裁决清理 `hazard_unit.h` 的 5 个预留输入端口 | **需 ADR**（行为变更） | 裁决文档 + 回归（若接前推：`bf78367` 的组合环 MUST NOT 复现） |
| ④ | `rv32i-fetch-skid-buffer-2026-10` | 取指 skid buffer + PC 回退（消除「停顿丢弃指令」）+ 取指填充期双装（**框架层 `sread` 语义**，影响所有用户） | ③（无前推时的停顿语义要先定） | 固件**删除**所有手工 NOP 后仍逐拍 parity；退休数 == 指令数 |

> **① 的实施结果（2026-10-26）**：change `rv32i-store-byte-enable-2026-10` 已实施
> **并归档**（2 轮对抗审稿：第 1 轮 APPROVE_WITH_CONDITIONS ⇒ 条件项同批整改；
> 第 2 轮 **APPROVE** ⇒ `openspec/changes/archive/2026-10-26-rv32i-store-byte-enable-2026-10/`，
> 新 capability `rv32i-isa-semantics`）。要点：
> ① `DataTCM` 新增 `strbe` 端口 ⇒ **公共 API 面变化** ⇒ 4 个既有实例化点全部审计并显式驱动
>    （⚠️ 实测：未驱动的输入端口读到的是**全 1** 而非 0 ⇒ 整字写会「碰巧」通过 ⇒ 未定义行为依赖）；
> ② 合并复用 `include/axi4/byte_lane.h`（与 AXI 侧同一份字节道语义）；
> ③ lane 对齐放**流水线侧**（planner 2026-10-26 裁决）⇒ `sw` **零行为回退**；
> ④ SoC 的 32 端口契约**未变** ⇒ **无需**改 ADR-046 D10。
> 新 capability：`rv32i-isa-semantics`（字节道写语义 + 「新增输入端口 MUST 被显式驱动」）。

> **③ 的实施结果（2026-10-27）**：change `rv32i-forwarding-decision-2026-10` 已实施
> **并归档**（ADR-047 裁决「接」；对抗审稿第 1 轮 **APPROVE_WITH_CONDITIONS**、
> **无 BLOCKER** ⇒ 按评审预算不发起第 2 轮 ⇒ `openspec/changes/archive/2026-10-27-rv32i-forwarding-decision-2026-10/`，
> **新 capability `rv32i-data-forwarding`**（距离矩阵的唯一权威）+ 4 个 capability MODIFIED）。要点：
> ① mux = **既有** `chlib::ForwardingMux<32>`（**零新增组件**）⇒ 档位 select **打一拍**：
>    `1` = EX/MEM 级、`2` = MEM/WB 级的合并写回值；档位 `1` 的源选择收进 mux 内部
>    （新增 `is_jump`/`jump_result` 端口）⇒ `JAL`/`JALR` 的写回值是**返回地址**；
> ② `hazard_unit.h` 删 5 个预留输入端口 + 前推判定补 `& !ex_is_load`（档位 `1` **保守地不覆盖**
>    load ⇒ load-use 停顿契约**零回退**）；
> ③ **同时修掉一个今天就存在的缺陷**：距离 **3** 的依赖读**旧值**（ID→EX 锁存沿与 WB 写回沿同沿）
>    ⇒ 用**寄存器堆读点的 WB→ID 组合旁路**消除；⚠️ 两后端**同构**地错 ⇒ 双后端 parity **抓不到**
>    ⇒ 靠**判别性固件**（期望值手工推导 + 反向期望）；既有 108 字固件用 3 条 NOP（距离 4）恰好躲开；
> ④ **变异 6/6 全被抓到**；`CTEST_TOTAL` **202 不变**（判别性固件进既有 target）。
> ⚠️ 剩给 ④ 的两条**并列**约束未动：load-use 停顿（3 拍）与「停顿丢弃消费者之后第一条指令」。

> ③ 与 ④ 的边界：③ 决定「依赖指令怎么拿数据」，④ 决定「停顿期间指令放哪」。
> 两者都会重算时序金标 ⇒ **不得合并**（无法归因）。

> ⚠️ **rdd-arch 前置（2026-10-26 planner 审查 gap A）**：ADR-046 **D10** 冻结了 SoC 的
> **32 端口**（5 输入 + 27 输出）⇒ 若 ①/②/④ 需要**新增顶层观测端口**，
> MUST **先改 ADR-046 D10**（rdd-arch 职责）再实施 —— builder 不得在 change 内自行扩端口。
> 先例：`openspec/changes/archive/2026-10-26-rv32i-pipeline-commit-path-2026-10/design.md` §3.10。
> 执行总纲见 `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md`（T2/T3/T5）。

## 跨阶段拆分

### phase-5
- ✅ ① `rv32i-store-byte-enable-2026-10`（2026-10-26 **已归档** ⇒ capability `rv32i-isa-semantics`）
- ✅ ② `rv32i-jump-sra-2026-10`（2026-10-27 **已归档** ⇒ capability `rv32i-isa-semantics` 2 → 6 条）
- ✅ ③ `rv32i-forwarding-decision-2026-10`（2026-10-27 **已归档** ⇒ 新 capability
  `rv32i-data-forwarding`；实施结果见上方 ③ 条目）
- ⏳ ④ `rv32i-fetch-skid-buffer-2026-10`（前置 ③ ⇒ **已解锁**）

## 反例

- **不要**用「改固件绕开」冒充修复（例如继续插 NOP / 只用 `sw`）—— 本 feature 的全部验收都要求
  **固件侧不规避**。
- **不要**把 ③（前推裁决）顺手塞进 ①/② —— 前推是**行为变更**，需独立 ADR + 回归证据；
  且接法错误会直接触发 `FATAL: Cycle detected in IR graph`（`git show bf78367`）。
- **不要**只给 `DataTCM` 加 `strbe` 就宣布 SB/SH 修好 —— 还必须做 **wdata lane 对齐**，
  否则「掩码对了、数据在错误的 lane」是一个**更难发现的**静默错误。
- **不要**在 ④ 里改 `sread` 语义而不评估全仓影响 —— `sread` 被 imem / FIFO / cache 等共用。
- **不要**把「parity 绿」当作本 feature 的验收判据 —— 本 feature 的判据是**ISA golden**
  （指令语义对不对），parity 只是附加护栏。

## 验收标准

- [ ] SB/SH/LB/LH/LBU/LHU 的字节/半字语义在双后端一致且符合 ISA（含非零 offset）
- [x] JAL / JALR 真正重定向且 `ra` 正确（② 2026-10-27 归档）
- [x] SRA 对有符号负数正确（② 2026-10-27 归档）
- [x] 依赖指令在**距离 1–3** 内拿到新值（③ 2026-10-27 归档；距离 3 靠 D11 读点旁路）
- [ ] 固件**无**手工 NOP 填充仍能正确执行（④ 完成 —— ⚠️ ③ 只消除了**依赖间隔**的 NOP，
      未被 ③ 覆盖的两条并列约束「load-use 停顿」+「停顿丢弃消费者之后第一条指令」仍要求
      固件在 load-use 之后放一条 NOP）
- [ ] 退休指令数 == 实际指令数（④ 完成）
- [ ] 上述每项都有**变异验证**（改坏即转红），而非仅「跑绿」
