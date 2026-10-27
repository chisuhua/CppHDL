---
id: feat-rv32i-isa-completeness
kind: feature
status: active
phase_refs: [phase-5]
主题: RV32I 语义边界收口（SB/SH 写字节道 · LB/LH 读字节道 · JAL/JALR 重定向 · SRA）+ 流水线健壮性（skid buffer · 取指双装）
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
| ④ | `rv32i-fetch-skid-buffer-2026-10` | 取指 skid buffer（消除「停顿丢弃指令」，**不需要 PC 回退** —— ADR-048 D3）+ 取指填充期双装（**`primed` 门控**，ADR-048 D6；**不动框架层 `sread` 语义** —— D7 不启动） | ③（无前推时的停顿语义要先定）⇒ **已解锁** | 固件**删除**所有手工 NOP 后仍逐拍 parity；每条指令退休**恰好一次** |
| ⑤ | `rv32i-load-byte-lane-2026-10` | **load 侧**字节道语义（`LB`/`LH`/`LBU`/`LHU` 的**车道选择 + 符号/零扩展**）+ 修 `LB` 的符号扩展缺陷 + 独立固件级判别性用例 | 无（**不依赖** ①–④） | 12 项手推写回金标逐项比对 + 双后端逐拍全等 + **变异 7/7 全转红**（含 `LB` 掩码回归锁） |

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
- ✅ ④ `rv32i-fetch-skid-buffer-2026-10`（2026-10-27 **已归档** ⇒ `rv32i-commit-path` 的
  **2 条 Requirement MODIFIED**；架构依据 = ADR-048，**一个 change、两个 commit**：
  C1 缺陷 A（skid buffer）/ C2 缺陷 B（`primed` 门控）⇒ 实施结果见上方 ④ 条目）
- ✅ ⑤ `rv32i-load-byte-lane-2026-10`（2026-10-28 **已归档** ⇒ `rv32i-isa-semantics`
  **ADDED ×1**；实施结果见下方 ⑤ 条目）

> **④ 的实施结果（2026-10-27）**：change `rv32i-fetch-skid-buffer-2026-10` 已实施
> **并归档**（ADR-048 已 Accepted，2 轮 critic；归档**禁** `--skip-specs` ⇒ 走 MODIFIED delta）。
> 要点：
> ① **C1 = 1 级 skid buffer**（`IfStage` 内 3 个寄存器）—— 填充在**停顿首拍**抓
>    `(fetched_instr, fetch_pc_reg)` 这一对（⚠️ `skid_pc` MUST 取 `fetch_pc_reg`），
>    排空与 `if_id_update_en` **同拍原子**；**不做 PC 回退**（ADR-048 D3）；
> ② **C2 = `primed` 标志**（复位后首个非复位周期置 1）**只门控 IF/ID 的 `valid`**
>    ⇒ 幻影装载不再执行 ⇒ 退休数 **179 → 178**、`non_retired` **20 → 21**
>    （fill 段 2 → 3 拍；停顿 / 冲刷段**逐段不变**）；
> ③ ⚠️ **实施时实测到 ADR 未记载的事实**：停顿之后还有一次**重复装载**（同一 word 连装两拍）
>    ⇒ C1 的「救回 1 条」与「消掉 1 条重复」在计数上**相抵** ⇒ C1 单独**零观测面扰动**
>    （既有金标：退休数 179 / `non_retired` 20 / 写回序列 24 项全部不变）；
> ④ **变异 6/6 全被抓到**（M1 `skid_pc` 取错 / M2 关排空 / M3 排空写反 /
>    M4 删 flush 清空 skid —— **只有组件级能抓** / M5 去 `primed` / M6 门控全部装载）；
> ⑤ **删掉 3 条失效护栏**（`stall_drop_slots_are_nop` / `fwd_stall_drop_slots_are_nop` /
>    `fwd_first_instruction_not_self_dependent`）+ **删掉「连续重复去重」掩码**
>    （缺陷 B 修复后，任何连续重复都是真回归）；
> ⑥ `CTEST_TOTAL` **202 不变** / `EXAMPLES_TEST_SOURCES` **17 不变**（用例进既有 target）。

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

- [x] SB/SH/LB/LH/LBU/LHU 的字节/半字语义在双后端一致且符合 ISA（含非零 offset）
      —— 写侧（`SB`/`SH`）由 ① `rv32i-store-byte-enable-2026-10`（2026-10-26 归档）覆盖；
      **读侧**（`LB`/`LH`/`LBU`/`LHU`）由 ⑤ `rv32i-load-byte-lane-2026-10`（2026-10-28）
      覆盖 ⇒ ⚠️ 读侧在 ⑤ 之前是**错的**（`LB` 的符号扩展掩码 `0xFF000000` ⇒ `0xFF0000C3`）
- [x] JAL / JALR 真正重定向且 `ra` 正确（② 2026-10-27 归档）
- [x] SRA 对有符号负数正确（② 2026-10-27 归档）
- [x] 依赖指令在**距离 1–3** 内拿到新值（③ 2026-10-27 归档；距离 3 靠 D11 读点旁路）
- [x] 固件**无**手工 NOP 填充仍能正确执行（④ 2026-10-27 归档 ⇒ 判别性固件
      `skid_firmware()` 在停顿点之后放**非 NOP**（`ADDI x19` + `SW x19`）且 MUST 执行到；
      ⚠️ 既有 108 字固件**仍**在那两处放 NOP —— 那是**允许**的，不再**必需**）
- [x] 每条指令退休**恰好一次**（④ 2026-10-27 归档 ⇒ `x16` 写回恰好 1 次且值 != 2、
      `x20` 写回恰好 1 次、ID word 序列去重后逐 4 递增 = 无跳号/无倒退）
- [x] 上述每项都有**变异验证**（改坏即转红），而非仅「跑绿」
      —— ① 2 条 / ② **6/6** / ③ **6/6** / ④ **6/6** / ⑤ **7/7**（每条都**先确认编译成功**
      ⇒ 否则跑的是旧二进制 ⇒ 假绿）

> **⑤ 的实施结果（2026-10-28）**：change `rv32i-load-byte-lane-2026-10` 已实施
> （**含 2 行实现修复** —— 不是纯验证 change）。要点：
> ① **修掉一个从未被文档化的缺陷**：`mem_stage.h` 的 `LB` 符号扩展掩码是
>    `0xFF000000`（注释却写 `0xFFFFFF00`）⇒ `lb` 返回 `0xFF0000C3`
>    ⇒ 改用框架已有的 `sext<32>` 原语（**三后端都支持** ⇒ 不会双后端分叉）；
>    `LH` 一并改（**逐位等价** ⇒ 既有 golden 零变化）⇒ 一次消掉「手写填充掩码」整类错误；
> ② ⚠️ **缺陷为何从未暴露**：`LB`/`LBU`/`LH`/`LHU` 的唯一仓内用例依赖**仓外**
>    riscv-tests 二进制（`lb.bin`）⇒ CI 中**全部 SKIP** ⇒ 有效覆盖为 0；
> ③ **独立 20 字判别性固件**（4× `SB` 造字 + 8 条 load）+ **12 项手推写回金标**
>    + **编译期判别力护栏**（`static_assert`）⇒ 判别力前提被钉死，不会静默归零；
> ④ ⚠️ **实测推翻**提案里「4 次 `SW` 能拼出 `0xA1B2C3D4`」：`SW` 的字节使能是 `0xF`
>    且数据**不**按 `addr[1:0]` 移位 ⇒ 实测 `[0x100] = 0x000000A1` ⇒ **造字 MUST 用 `SB`**；
> ⑤ **变异 7/7 全被抓**（M1–M6 运行期转红；**M7 被编译期护栏先抓**，另单独验证
>    护栏中性化后运行期亦转红）；
> ⑥ **顺带收口一个既有红**：`tests/test_verilator_complex_e2e.cpp` 的退休数金标
>    **17 → 16**（ADR-048 D6 的 `primed` 门控落地时漏改 ⇒ 自那时起一直红；
>    ⚠️ 已实测确认在**本 change 之前**（HEAD `6d28264`）**同样失败** ⇒ 与本 change 无关）；
> ⑦ 计数**三者均不变**（用例进既有 target）：`CTEST_TOTAL` **202** /
>    `EXAMPLES_TEST_SOURCES` **17** / `ctest -L verilator_e2e` **37**；
>    验收 = 单测 **13062 断言 / 6 用例**全绿 + `ctest -LE perf` **191/191** +
>    `run_all_ported_tests.sh` **28/28** + `run_all_samples.sh` **35/35** +
>    8 个护栏脚本全 exit 0。
>    ⚠️ `ctest -L perf` **10/11**：唯一失败 `perf_regression`
>    （`TC-08|regs=1000|verilator` 比率 1.403 > 阈 1.3）⇒ **环境漂移**（五项证据：
>    perf target 零依赖 `include/cpu/**` · interpreter/jit 同档正常 · verilator
>    绝对值不随工作量变化 · 同二进制三次测量超阈档位每次不同 · 与上次失败同型）
>    ⇒ 与本 change 无关；baseline 重采集已独立立项。
> ⇒ **本 feature 的 ISA 语义边界（不含 CSR / ECALL / FENCE / 预测取指）全部落地**
> ⇒ 剩余三项（CSR / `ECALL`/`EBREAK` / `FENCE`）**不在**任何仓内 capability 覆盖内
> ⇒ 需**独立立项**（不可顺手塞进本 feature）。
