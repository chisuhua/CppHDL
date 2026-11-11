---
name: rv32i-isa-gap-closure-2026-11
priority: P1
phase: phase-6
category: cpu
type: feature
状态: draft
主题: RV32I ISA 缺口
依赖: rv32i-commit-path（时序基线）· memory-timing-contract-2026-11（建议先行，稳时序基线）
来源: planner 实测（未识别 opcode 静默当 NOP 穿过 5 级）+ phase-6 P6 的「CSR / ECALL / FENCE MUST 独立立项」约束
生成时间: 2026-11-10
evidence: probe:include/cpu/pipeline/stages/id_stage.h:123-131 + include/cpu/pipeline/rv32i_decoder.h:139-147 + include/cpu/riscv/rv32i_exception.h:140-151
---

# Improvement: RV32I ISA 缺口收口（**不含特权级**）—— 非法指令可观测 + FENCE 显式化 + `ExceptionUnit` 写缺陷修复

- **Date drafted**: 2026-11-10
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）
- **⚠️ 范围边界（硬约束）**: 本项 **MUST NOT** 包含 **CSR 指令 / `ECALL` / `EBREAK` / `MRET` / 中断** ⇒ ⇒ 按 `phase-6.md` **P6** 行（「CSR / `ECALL` / `FENCE` **MUST 独立立项** ⇒ 新 capability + 新 change」）⇒ ⇒ **特权级 = 另一条独立立项**（本项末尾给出候选形态）

---

**主题**: RV32I ISA 缺口

## 架构依据

### A.1 实测事实（2026-11-10）

| # | 事实 | 证据 |
|---|---|---|
| ① | ⚠️ **未识别的 opcode 静默当 NOP**
| ② | 译码器同样**没有**「非法」概念：`OP_SYSTEM`（0x73）常量**已定义但零消费者**；`type` 对未知 opcode 默认 `INSTR_R` | `include/cpu/pipeline/rv32i_decoder.h:52`（`OP_SYSTEM`）· `:139-147`（`type` 的 8 个 select，未命中则保持默认 `INSTR_R`） |
| ③ | ⇒ ⇒ **后果**：程序跑飞到非法指令时，**零报错**、**零信号** ⇒ ⇒ 表现为「程序悄悄停住 / 行为诡异」⇒ ⇒ 调试成本极高 | 由 ① 推得 |
| ④ | ⚠️ `FENCE` / `FENCE.I`（opcode `0x0F`）**完全不在译码表** | `rv32i_decoder.h:43-52` 的 opcode 常量表**无** `0x0F`；`id_stage.h:123-131` 亦无 |
| ⑤ | ⚠️ `ExceptionUnit` 的 **`mcause` 永远写不到** | `include/cpu/riscv/rv32i_exception.h:132,136`（`write_mepc = write_mcause = do_trap_entry`）· `:140-144`（`csr_addr = select(write_mepc, MEPC, select(write_mcause, MCAUSE, 0))`）⇒ ⇒ `write_mepc` 恒真 ⇒ ⇒ **MEPC 分支恒命中** ⇒ ⇒ MCAUSE 分支**不可达** |
| ⑥ | 同上，`mtval` 也**永远写不到**（`csr_addr` 的 select 链里**没有** `mtval` 分支） | `rv32i_exception.h:140-144` |
| ⑦ | `CsrBank` 只有**单写口**（`csr_addr`/`csr_wdata`/`csr_write_en`）⇒ ⇒ 一拍只能写一个 CSR ⇒ ⇒ ⑤⑥ 的根因是**结构性的**（不是优先级写错） | `include/cpu/riscv/rv32i_csr.h:41-43` · 写路径 `:161-205`（`select(we & sel_mcause, …)` 确实存在，但 `sel_mcause` 依赖 `csr_addr` 命中） |
| ⑧ | `CsrBank` / `ExceptionUnit` 都是**零流水线消费者**（仅 `test_csr_unit.cpp` / `test_interrupt_flow.cpp` 用）⇒ ⇒ ⑤⑥ 目前**不影响交付路径** ⇒ ⇒ 修它们是为**特权级接线清障** | grep：`include/cpu/pipeline/**` 对 csr/exception/mret 的引用 = **0** |
| ⑨ | 特权级组件**已存在且有测试**：`CsrBank`（9 个 CSR）· `ExceptionUnit`（trap entry + MRET）· `CLINT`（定时器） | `include/cpu/riscv/{rv32i_csr.h,rv32i_exception.h,clint.h}` · 测试 `examples/riscv-mini/tests/{test_csr_unit.cpp,test_interrupt_flow.cpp}` |

### A.2 为什么非法指令必须**先**可观测，再谈 trap

真正的 RISC-V 语义是「非法指令 ⇒ trap 到 `mtvec`」⇒ ⇒ 那需要 **CSR + `mtvec` + `mepc` + `mcause`** ⇒ ⇒ 属特权级立项（周期长、面大）。
⇒ ⇒ 但**调试价值最高的**那一步是「**知道发生了非法指令**」⇒ ⇒ 这一步**不需要**任何特权机制 ⇒ ⇒ **先做它** ⇒ ⇒ 特权级落地时，只需把「可观测」升级为「真 trap」。

⇒ ⇒ 这与本仓既有方法学一致：**先让缺陷可见**（仓内大量护栏都是这个模式）。

---

## 范围

### ✅ 在内

| 编号 | 内容 |
|---|---|
| **G1** | **未知 opcode 检测**：ID 级 MUST 产出 `illegal_instr`（= 全部已知指令标志**皆假**）⇒ ⇒ ⚠️ **metis 订正（两条硬约束）**：① 判据 MUST 包含 `io().valid` ⇒ ⇒ 否则 flush 后 `if_id_instr_reg` 的**残留旧值**（`if_stage.h:307-318` 只清 `valid` 不清指令）与复位后 `primed=0` 的**幻影字**会被误判 ⇒ ⇒ **在错误路径上把流水线冻死** ② 「非法」本版**只覆盖 opcode**（`funct3`/`funct7` 非法组合**不在本项** ⇒ ⇒ MUST 登记为 residual）⇒ ⇒ 标题与措辞由「非法指令」改为「**未知 opcode**」 |
| **G2** | **未知 opcode 的可观测面**：① `illegal_instr` **输出信号**（SoC 顶层引出）② **行为策略**：MUST **冻结**（PC 不前进 / ID-EX 装气泡 / 该指令永不退休）⇒ ⇒ **MUST NOT** 当作 NOP 继续跑 ⇒ ⇒ **MUST NOT** 计入退休数<br>⚠️ **metis 订正**：原稿写「有效链清空」有**两套互斥读法**（清空 = 装气泡 + PC 继续 ⇒ 变成「丢弃后继续」的弱化 NOP；清空 = 冻结 ⇒ 与字面相反）⇒ ⇒ MUST 写死为「**冻结**」⇒ ⇒ 实现落点 = `if_stage.stall` 与 `id_stage.stall` 均 OR 上 `illegal_active`（`rv32i_pipeline.h:252` / `:484`）+ `idex_valid` 清零 ⇒ ⇒ 附带义务：更新 `if_stage.h:110-116` 的「flush 与 stall 不同拍」不变量论证（引入冻结后该不变量被打破）<br>⚠️ **metis 建议删掉 `perf_illegal_count`**：冻结语义下非法指令只会出现 **1 条** ⇒ ⇒ 计数器上界恒为 1 ⇒ ⇒ 与 level 信号**信息量相同** ⇒ ⇒ 若保留 MUST 定义为 **sticky 位**（首次命中锁存）并给出与 level 信号的差异证据 |
| **G3** | 🔵 **已裁决（2026-11-10 用户 · 修 B5 连带）＝ 采纳 oracle 的第三种做法：装载器把尾部填成 NOP** ⇒ ⇒ 退休数 **178 不变**、`non_retired == 21` 不变（算术：`0x00000013` = `addi x0,x0,0` ⇒ 1 拍退休、`rd=0` 被 x0 保护吞掉，`wb_stage.h:102-110`；`non_retired` 与尾部无关）<br>⇒ ⇒ **原 G3 的 AC「该值不是 178」作废，改为「== 178 不变」**<br>⚠️ 填充落点 MUST 明写（三选一，实施期定）：① `firmware()` 数组尾部 ② I-TCM 初值 ③ `load_firmware` 循环扩展（现只写 `fw.size()` 字，`test_rv32i_soc_dual_parity.cpp:537-541`）。<br>（**原内容见 §What Changes 段**——本单元只留摘要，避免与正文漂移）|
| **G4** | 🔴 **B5 修复（critic 第 2 轮改向 —— 原顺序写反了）**：批内顺序 MUST 为 **`③ 尾部填 NOP` → `G1 + G4`（同批）→ 其余**。<br>**论证**：③ 单独落地时退休数 **178 不变**（G3 的算术：`0x00000013` = NOP ⇒ 与尾部 0 字同样 1 拍退休、同样不写 rd）⇒ ⇒ 之后 G1 落地时**尾部已无可冻结的 0 字** ⇒ G1 对**本固件**零影响 ⇒ golden **只重算一次**（实际零变更）。<br>⚠️ 若反序（G1 先）：G1 使 PC 在 word 108 冻结 ⇒ 退休 **178 → ~102** ⇒ 该批必须把 golden 改到 102；随后 ③ 落地 ⇒ 冻结不再触发 ⇒ 退休**回到 178** ⇒ ⇒ **golden 被改两次**。 |
| **G5** | **`ExceptionUnit` 写缺陷修复**：`mcause` MUST 能被写到 ⇒ ⇒ ⚠️ **metis 订正**：① 原稿选项 A 的描述**自相矛盾**（「第二条写口」= **同拍双写** vs 「两拍内写」= 选项 B 的语义）② **`mtval` 没有数据源**（`ExceptionUnit` 只有 `exception_cause` 输入，`rv32i_exception.h:48`）⇒ ⇒ 「`mtval` MUST 能被写到」**作废** ⇒ ⇒ 本版 `mtval` 定为「**恒 0 · 写入属特权级立项**」③ **`test_csr_unit.cpp:318-319` 已把缺陷钉住**（`REQUIRE(csr_addr == CSR_MEPC)`）⇒ ⇒ **禁止**「先写 mcause」的修法 ④ 现有 `test_csr_unit.cpp:599-615` 的 `mcause` 断言是**直接对 `CsrBank` 写**（不经 `ExceptionUnit` 的 select 链）⇒ ⇒ 现有测试**抓不到**该缺陷 ⇒ ⇒ 需**复合 DUT**（`ExceptionUnit` → `CsrBank`）才能有判别力<br>⇒ ⇒ **首选 B（`ExceptionUnit` 内部跨两拍 · `CsrBank` 零改动）**：该组件**零流水线消费者** ⇒ ⇒ 改它时序**零交付风险**；而 A/C 都要改 `CsrBank::__io` ⇒ ⇒ 触发 ~20 处测试实例化的「新增输入端口 MUST 全部显式驱动」义务（未驱动读到未定义值 ⇒ `csr2_write_en=1` ⇒ 每拍误写）⇒ ⇒ 草案原推荐 A **降级为备选** |
| **G6** | ⚠️ **metis 判定：净增量为 0 · 建议删除** —— G2 已经要求 `illegal_instr` + `illegal_reason` 由 SoC 顶层引出 ⇒ ⇒ G6 与 G2 是同一件事 ⇒ ⇒ 仓内 ZERO-DEBT 不允许「预留一个没人读的端口」⇒ ⇒ **本项删除 G6**，特权级接缝由 G2 的既有输出承担 |

### ❌ 不在本项

- **CSR 指令**（`CSRRW/CSRRS/CSRRC/CSRRWI/CSRRSI/CSRRCI`）⇒ ⇒ 独立立项
- **`ECALL` / `EBREAK` / `MRET`** ⇒ ⇒ 独立立项
- **中断**（CLINT 接入 + `mie`/`mip`/`mstatus` 语义）⇒ ⇒ 独立立项
- **trap 入口**（`mtvec` 跳转 + `mepc` 保存 + 流水线 flush）⇒ ⇒ 独立立项
- **分支预测器接入** ⇒ ⇒ 归 `rv32i-dead-component-consolidation-2026-11` 之外的独立立项

---

## Why

### W.1 「静默 NOP」是**最难查**的一类缺陷

③ ⇒ ⇒ 程序跑飞 ⇒ **零报错**、零计数器、零波形信号 ⇒ ⇒ 只能靠二分固件定位。
⇒ ⇒ 仓内已有**完全同型**的历史事故：`funct7[5]` 对 I 型误判（`addi x, x0, -8` ⇒ 算成 SUB）—— 那次能被发现，是因为**有人手工造了边界用例**；若当时有「非法指令」信号，至少能立刻知道"程序跑到了不该到的地方"。

### W.2 现在连「非法」这个**概念**都没有

①④ ⇒ ⇒ 译码表里没有"已知指令全集"这个概念 ⇒ ⇒ 未实现指令与未定义指令**混为一谈** ⇒ ⇒ 无法区分「我打算实现但还没实现」与「这根本不是指令」⇒ ⇒ **可维护性**受损。

### W.3 `ExceptionUnit` 的 `mcause` 缺陷会**污染**未来的特权级实现

⑤⑥⑦ ⇒ ⇒ `mcause` 永远读到 0 ⇒ ⇒ 特权级落地后「异常原因」永远是 0 ⇒ ⇒ 调试时又是一次"静默错误"⇒ ⇒ **现在**修（组件有测试，成本低）比**将来**修（已接进流水线 ⇒ 改时序）便宜得多。

### W.4 边界纪律：特权级必须独立立项

`phase-6.md` **P6** 行已明文裁定：CSR / `ECALL` / `FENCE` **MUST 独立立项**（新 capability + 新 change）⇒ ⇒ 本项 MUST NOT 把它们混进来 ⇒ ⇒ ⇒ 一项混杂的 change 会让 review 失焦、且与既有裁决冲突。

---

## What Changes

### G1 — 非法指令检测

- ID 级新增：`illegal_instr = !(is_op | is_opimm | is_load | is_store | is_branch | is_jalr | is_jal | is_lui | is_auipc | is_fence)`。
- ⚠️ **必须显式区分两类**：
  | 类 | 例子 | 处置 |
  |---|---|---|
  | **已知但未实现** | `OP_SYSTEM`（0x73 · ECALL/EBREAK/CSR） | 列入「已知未实现」集合 ⇒ ⇒ **显式**报 `illegal_instr`（并**附带**原因码，区分「未实现的已知 opcode」vs「未知 opcode」） |
  | **完全未知** | 任意其它 opcode | 报 `illegal_instr`（原因码 = 未知） |
- ⇒ ⇒ 输出口径：`illegal_instr`（1 bit）+ `illegal_reason`（2 bit：01 = 已知未实现 / 10 = 未知 / 11 = 保留）。
- ⚠️ **判据 MUST NOT 依赖 C++ 控制流**（仓内铁律：硬件值 MUST NOT 参与 C++ 控制流 ⇒ 条件恒假 ⇒ 分支体不发）⇒ ⇒ 全部用 `select` / `&` / `|` 实现。

### G2 — 可观测面与行为策略

- SoC 顶层新增输出：`illegal_instr`（1 bit）· `illegal_reason`（2 bit）· `perf_illegal_count`（32 bit）。
- **行为策略 = 停在原地**：非法指令进入 ID 级时 ⇒ ⇒ 有效链**清空**（该指令**不**退休）⇒ ⇒ PC **冻结**（不前进）⇒ ⇒ ⇒ 「停在原地」是**可判定**的（`dbg_id_pc` 不变 + `perf_instructions` 不增）。
- ⚠️ **不得**用「当作 NOP 继续跑」⇒ ⇒ 那正是当前行为 ⇒ ⇒ 本项的价值就在于**改掉**它。

### G3 — 退休计数口径

- `perf_instructions`（退休数）MUST 只计**有效退休** ⇒ ⇒ 非法指令不计入 ⇒ ⇒ 契约（`rv32i-commit-path` capability 的既有基线）MUST MODIFIED 订正。
- ⚠️ **既有 golden 的退休数会变**？⇒ ⇒ 当前固件**不含**非法指令 ⇒ ⇒ 既有 20 项 golden 的 `perf_instructions = 178` **不变** ⇒ ⇒ 但 MUST 有**新**用例覆盖非法指令路径 ⇒ ⇒ 期望值 MUST 手推。

### G4 — FENCE 显式化

- 译码表加 `OP_FENCE = 0x0F` ⇒ ⇒ `is_fence` 标志 ⇒ ⇒ 行为 = NOP（无副作用、不写寄存器、不访存）⇒ ⇒ **不**产生 `illegal_instr`。
- ⚠️ **单核无 cache 语境**下 FENCE 无实际语义 ⇒ ⇒ 但 MUST 显式（让"我知道它被忽略了"成为**代码里可读的事实**）⇒ ⇒ 并在 spec 记「FENCE 在本版被实现为 NOP（无 cache ⇒ 无排序语义）」。
- ⚠️ `FENCE.I`（`funct3 = 1`）同理 ⇒ ⇒ 同样 NOP（无 I-cache）⇒ ⇒ 显式记录。

### G5 — `ExceptionUnit` 写缺陷修复

- 裁决三选一（MUST 由 architect / builder 裁决并写进契约）：

| 选项 | 描述 | 代价 | 风险 |
|---|---|---|---|
| **A（建议）** | `CsrBank` 增加**第二条写口**（`csr2_addr`/`csr2_wdata`/`csr2_write_en`）⇒ ⇒ 两拍内写 `mepc` + `mcause`/`mtval` | **公共 API 变化**（`CsrBank::__io`）⇒ ⇒ 全部实例化点审计 | 低 |
| B | trap entry **跨两拍**（拍 1 写 `mepc`，拍 2 写 `mcause`/`mtval`） | 引入状态（需 `ch_reg` 记录阶段）⇒ ⇒ 改变 trap 时序 | 中（时序） |
| C | `CsrBank` 增加**专用** `mcause` / `mtval` 写口 | 公共 API 变化 ⇒ ⇒ 专用口 ⇒ ⇒ 语义清晰 | 低 |

- ⚠️ **判据 MUST 覆盖**：`mcause` 写入后**可读回**（`csr_rdata`）⇒ ⇒ 当前 `test_csr_unit.cpp` / `test_interrupt_flow.cpp` 是否已断言这一点？⇒ ⇒ MUST 核查并补（**这正是判别力来源**）。
- ⚠️ **本项修 `ExceptionUnit` 不改变交付路径**（⑧：零流水线消费者）⇒ ⇒ 风险低 ⇒ ⇒ 但它**为特权级清障** ⇒ ⇒ MUST 在 spec 记「`mcause`/`mtval` 写入契约」。

### G6 — 特权级接缝预留

- 流水线 MUST 预留**一个**信号：`illegal_instr` / `illegal_reason`（ID 级输出 ⇒ ⇒ SoC 顶层引出）⇒ ⇒ 特权级立项时，只需把它接到 trap 逻辑 ⇒ ⇒ **不新增** `csr_*` 端口 ⇒ ⇒ **零** CSR 依赖。

---

## Acceptance

### 非法指令

- [ ] 存在「未知 opcode」用例：固件含一条**未知 opcode**
- [ ] 该指令**不**写任何寄存器（写回序列无新增）
- [ ] 该指令**不**计入 `perf_instructions`；且**尾部填 NOP 后整体退休数仍 == 178**、`non_retired == 21`（🔵 2026-11-10 裁决：采纳尾部填 NOP ⇒ 原「该值不是 178」**作废**）
- [ ] `illegal_instr` 输出**拉高**，`illegal_reason` == 「未知」
- [ ] 另一用例：固件含 `ECALL`（`OP_SYSTEM`）⇒ ⇒ `illegal_instr` 拉高且 `illegal_reason` == 「已知未实现」（**与未知 opcode 可区分**）
- [ ] ⚠️ **变异自证**：把 `illegal_instr` 的判定改成恒假 ⇒ ⇒ 上述用例 MUST 转红（证明判据有判别力）<br>- [ ] ⚠️ **冻结判据 MUST 防「前 N 拍本来就相等」的恒绿**：MUST 写成四段 —— ① 非法指令进入 ID **之前**某一拍 `dbg_id_pc` 曾递增 ② 进入后**连续 ≥ K 拍**（K ≥ 10，覆盖 fill + 3 拍 load-use 窗口）`dbg_id_pc` **逐拍相等**且等于非法指令地址 ③ 同期 `perf_instructions` 逐拍相等 ④ 该期间的 `dbg_wb_write_en` **序列**与「不含该指令」的期望序列**逐项**相等（序列断言，非计数断言）

### FENCE

- [ ] `FENCE` / `FENCE.I` 进入译码表 ⇒ ⇒ 行为 = NOP（不写寄存器、不访存、不分支）
- [ ] `FENCE` **不**产生 `illegal_instr`（显式断言）
- [ ] spec 明确记载「FENCE 在本版实现为 NOP（无 cache ⇒ 无排序语义）」

### `ExceptionUnit`

- [ ] trap 后 `mcause` **可读回**为异常原因码（当前恒 0 ⇒ ⇒ 本项使其非 0）
- [ ] `mtval` 有明确契约（写 / 不写 ⇒ ⇒ 二选一并显式化；当前是**隐式不可达**）
- [ ] `CsrBank` 写口数量变化 ⇒ ⇒ 公开面登记 + CHANGELOG + 全部实例化点审计

### 边界

- [ ] ⚠️ 本项**零** `csr_*` 端口新增到流水线 / SoC 顶层（特权级 MUST NOT 混入）<br>- [ ] ⚠️ **metis 新增**：`phase-6.md` **P6 出口判据 ②**（「`OP_SYSTEM` 剥注释后引用数从 0 变正」）会被本项**预先点亮**而实质未做 ⇒ ⇒ MUST 同步把 P6 的判据改为「**执行语义**证据」（`csr_*` 在 pipeline 中的非零消费者 / 新 target / 固件级 CSR 读写断言）⇒ ⇒ 否则下一个窗口会拿一条**被洗白的绿灯**去闭 P6<br>- [ ] ⚠️ **metis 新增**：SoC 顶层新增输出的命名 MUST 遵循既有前缀惯例（`obs_*` / `dbg_*`）⇒ ⇒ 用 `obs_illegal_instr` / `obs_illegal_reason` / `obs_perf_illegal_count`（宽度与 `perf_instructions` 一致 = 48 bit）⇒ ⇒ 避免 `verify_no_duplicate_rv32i_headers.sh:164` 的 C6 快照读起来像特例<br>- [ ] ⚠️ **metis 新增（连带修改清单）**：`test_rv32i_soc_dual_parity.cpp` 的 `kObsNames`(`:74-93`) + `static_assert(kObsCount == 27)`(`:96`) + `obs_snapshot()`(`:167-196`) **三处须同步且顺序一致**；`verify_no_duplicate_rv32i_headers.sh:164` 的 `SOC_TOP_PORTS` 快照；`include/cpu/AGENTS.md` 的「30 headers / 6473 lines」自述（被该脚本 **C5** 机器校验）；根 `AGENTS.md` 的 ISA 覆盖边界段；4 个计数锚点
- [ ] 本项**零** `ECALL` / `EBREAK` / `MRET` / 中断的实现（只做"可观测"）

### 不回归

- [ ] 既有 **257** 个 ctest 全绿
- [ ] `test_rv32i_soc_dual_parity` 的 **20 项 golden** 全绿且 `perf_instructions` 仍 = **178**（既有固件**不含**非法指令 ⇒ ⇒ 退休数**不变** ⇒ ⇒ 这是"改动零影响"的证据）
- [ ] 既有 `test_csr_unit` / `test_interrupt_flow` 全绿（并**补** `mcause` 读回断言）<br>⚠️ **metis**：该断言 MUST 落在**复合 DUT**（`ExceptionUnit` → `CsrBank`）上 ⇒ ⇒ 断言「trap 后 `mepc == current_pc` **且** `mcause == exception_cause`」⇒ ⇒ 并附**变异记录**（把 `write_mepc = write_mcause = do_trap_entry` 改回 ⇒ 必红）⇒ ⇒ 光在 `CsrBank` 上断言**无判别力**
- [ ] 计数锚点同步

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| **`rv32i-illegal-instruction`**（**新**） | ADDED | 非法指令契约：① 判据（已知未实现 vs 完全未知**可区分**）② 行为（停在原地 · 不退休）③ 可观测面（`illegal_instr` / `illegal_reason` / 计数器）④ 退休计数口径 ⑤ 特权级接缝预留（只留接口，零 CSR 依赖） |
| **`rv32i-fence`**（**新**） | ADDED | FENCE / FENCE.I 显式化契约（本版 = NOP · 无 cache ⇒ 无排序语义 · MUST NOT 报非法） |
| **`rv32i-trap-context`**（**新**） | ADDED | trap 上下文写入契约：`mepc` / `mcause` / `mtval` 三者的写入**必须**都可达（MUST NOT 有隐式不可达分支）⇒ ⇒ 本版只约束**组件级**行为（`ExceptionUnit` / `CsrBank` 尚非流水线路径） |
| `rv32i-commit-path` | MODIFIED | 退休计数口径（「非法指令 MUST NOT 计入退休」）⇒ ⇒ 既有基线数字**不删** |
| `rv32i-isa-semantics` | MODIFIED | 追加「未知 opcode MUST NOT 静默通过」+「FENCE 的显式 NOP 语义」⇒ ⇒ 既有条款**不删** |
| `cpu-clint-timer` | **不动** | 中断属独立立项 |

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `include/cpu/pipeline/stages/id_stage.h` | 新增 `illegal_instr` / `illegal_reason` 判定 + 有效链清空逻辑 ⇒ ⇒ **ID 级行为变化**（对非法指令）⇒ ⇒ 对既有固件**零影响**（无非法指令） |
| `include/cpu/pipeline/rv32i_pipeline.h` | 新增 `perf_illegal_count` 计数器 + 退休口径 ⇒ ⇒ 计数器占位 ⇒ ⇒ 影响**退休数**的定义（但既有值不变） |
| `examples/riscv-mini/src/rv32i_pipeline_top.h` | **端口契约变化**（+2~3 个输出）⇒ ⇒ **ADR-046 D10** 受影响 ⇒ ⇒ 走公开面登记 + CHANGELOG ⇒ ⇒ ⚠️ 端口数变化会**影响**既有 parity 测试的观测向量长度（`static_assert(kObsCount == 27)`）⇒ ⇒ MUST 同步 |
| `include/cpu/riscv/rv32i_exception.h` | `mcause` / `mtval` 写入路径 ⇒ ⇒ 若选方案 A/C ⇒ ⇒ `CsrBank` **公共 API 变化** ⇒ ⇒ 全部实例化点审计 |
| `include/cpu/pipeline/rv32i_decoder.h` | 加 `OP_FENCE` 常量 + `is_fence` ⇒ ⇒ 组件的 `type` 选择链也要加（FENCE 的 `type` = ? ⇒ ⇒ 建议新增 `INSTR_FENCE` 或复用 `INSTR_I`） |
| 计数锚点 | `CTEST_TOTAL` +（新用例）⇒ ⇒ 同步 |

### 已知风险

| 风险 | 缓解 |
|---|---|
| **「停在原地」的实现引入组合环或时序错** | 用 `select` 实现（禁 C++ `if`）⇒ ⇒ 接入后跑拓扑排序 ⇒ ⇒ 既有 `Cycle detected` 防护 |
| **`illegal_reason` 判据把「已知未实现」和「未知」混了** | Acceptance 显式要求两者**可区分** + 两个独立用例 |
| **改 `CsrBank` 写口数** ⇒ ⇒ 跨仓下游破 | 走公开面登记 + CHANGELOG ⇒ ⇒ 护栏 `verify_public_api_visibility.sh` |
| **退休口径变化被误当成"回归"** | 既有固件无非法指令 ⇒ ⇒ `perf_instructions = 178` **必须不变** ⇒ ⇒ 若变了 ⇒ ⇒ 查 ID 级改动 |
| **特权级被"顺手"混进来** | Acceptance 显式断言「零 `csr_*` 端口新增」+ 「零 ECALL/EBREAK/MRET/中断实现」 |

### 特权级的候选形态（**独立立项**，本项不做）

| 候选 improvement | 覆盖 | 依赖 |
|---|---|---|
| `rv32i-privileged-csr-2026-11` | CSR 指令（6 条）· `CsrBank` 接入流水线 · `mepc`/`mcause`/`mtvec` 语义 · trap 入口 + 流水线 flush · `ECALL`/`EBREAK`/`MRET` | 本项的 `illegal_instr` 接缝（G6）· `memory-timing-contract` 的时序基线 |
| `rv32i-interrupt-2026-11` | CLINT 接入 · `mie`/`mip`/`mstatus` · 中断入口（可与上一项合并或独立） | `rv32i-privileged-csr-2026-11` |

⚠️ 这两条**MUST** 按 `phase-6.md` **P6** 行的约束走「新 capability + 新 change」流程 ⇒ ⇒ **MUST NOT** 并入本项。

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `memory-timing-contract-2026-11` | 建议**先行**（稳时序基线 ⇒ ⇒ 本项的退休口径订正不会算两遍） |
| `rv32i-dead-component-consolidation-2026-11` | 建议**在其后**（ID 级会被改两次 ⇒ ⇒ 一次改完更省） |
| `firmware-load-chain-2026-11` | 弱耦合：装载链路让「真程序」成为可能 ⇒ ⇒ 但本项的用例可用现有 `prog_*` 路径 ⇒ ⇒ 可并行 |
| `rtl-emit-product-tooling-2026-11` | SoC 端口契约变化 ⇒ ⇒ manifest 快照同步 |

## metis 第 1 轮 gap 分析与处置（2026-11-10）

> **轮次**：1（该 improvement 总计 1 轮）· **审查者**：metis（预规划 gap 分析）

### ✅ 已订正

| # | metis finding | 处置 |
|---|---|---|
| 1 | 🔴 **「`perf_instructions = 178` 不变」是错的**：`load_firmware` 只写 `fw.size()` 个 word ⇒ ⇒ word ≥ 108 全是 `ch_mem` 初值 **0**（= 未知 opcode）⇒ ⇒ 今天被当作 NOP **逐条退休** ⇒ ⇒ 178 里**约 76 条来自跑出固件尾部的 0 字** ⇒ ⇒ G1 生效后 PC 到 word 108 即冻结 ⇒ ⇒ 退休数**掉到 ~102** | A.1 / G3 / 验收 三处重写；给出二选一：**尾部加自环终止指令**（`JAL(0,0)`）并手推重算，或按 `rv32i-commit-path` 的「实测回填」纪律回填 |
| 2 | 🔴 G1 判据**必须含 `valid`**：flush 后 `if_id_instr_reg` 保留旧值（`if_stage.h:307-318` 只清 `valid`）⇒ ⇒ 不含 `valid` ⇒ **在错误路径上把流水线冻死** | G1 加 `io().valid` gate；标题与措辞由「非法指令」改为「**未知 opcode**」 |
| 3 | 「有效链清空」有**两套互斥读法**（装气泡+PC 继续 ⇒ 弱化 NOP / 冻结 ⇒ 与字面相反） | G2 写死为「**冻结**」+ 给出实现落点（`if_stage.stall` / `id_stage.stall` OR `illegal_active`）+ 附带更新 `if_stage.h:110-116` 的不变量论证 |
| 4 | 「本版只覆盖 opcode」被标题掩盖（`funct3`/`funct7` 非法组合**未覆盖**） | G1 追加「funct3/funct7 非法**不在本项** ⇒ 登记为 residual」 |
| 5 | G5 选项 A **描述自相矛盾**（「第二条写口」= 同拍双写 vs 「两拍内写」= B） | G5 重写三选项并明确各自语义 |
| 6 | **`mtval` 没有数据源**（`ExceptionUnit` 只有 `exception_cause`）⇒ ⇒ 「`mtval` MUST 能被写到」**作废** | G5 改为「本版 `mtval` 恒 0 · 写入属特权级立项」 |
| 7 | **`test_csr_unit.cpp:318-319` 已把缺陷钉住**（`REQUIRE(csr_addr == CSR_MEPC)`）⇒ ⇒ **禁止**「先写 mcause」 | G5 记录该硬约束 |
| 8 | 现有 `test_csr_unit.cpp:599-615` 的 `mcause` 断言是**直接对 `CsrBank` 写** ⇒ ⇒ **抓不到**该缺陷 ⇒ ⇒ 需**复合 DUT** | 验收新增「复合 DUT（`ExceptionUnit` → `CsrBank`）+ `mepc`/`mcause` 双读回 + 变异记录」 |
| 9 | G5 原推荐 A（改 `CsrBank::__io`）代价被**低估** ⇒ ⇒ 首选 B（`ExceptionUnit` 内部跨两拍 · `CsrBank` 零改动 · 该组件零流水线消费者 ⇒ 零交付风险） | G5 改为**首选 B**，A/C 降为备选并标注「~20 处实例化的新增输入端口驱动义务」 |
| 10 | G6 与 G2 **重复** ⇒ ⇒ 净增量 0 ⇒ ⇒ ZERO-DEBT 不允许「预留没人读的端口」 | G6 **删除** |
| 11 | `perf_illegal_count` 在冻结语义下**上界恒为 1** ⇒ ⇒ 与 level 信号信息量相同 | G2 建议**删除**计数器，或定义为 **sticky 位**并给出差异证据 |
| 12 | `illegal_reason` 的 2 bit 是**伪扩展性** | 登记为可讨论项（1 bit 是否够） |
| 13 | 🔴 **`phase-6.md` P6 的出口判据 ② 会被本项预先点亮**（`OP_SYSTEM` 引用数变正）而实质未做 ⇒ ⇒ 判据污染 | 验收新增：P6 判据 MUST 同步改为「**执行语义**证据」 |
| 14 | 「既有 20 项 golden」**查无实据**（该文件是 7 个 `TEST_CASE`） | 全文的「20 项」口径改为引用**具体 `TEST_CASE`** |
| 15 | 新端口名破坏 `obs_*` / `dbg_*` 前缀惯例 | 验收新增：改用 `obs_illegal_instr` / `obs_illegal_reason` / `obs_perf_illegal_count`（48 bit） |
| 16 | 缺连带修改清单 | 验收新增：`kObsNames`(`:74-93`) + `static_assert(kObsCount == 27)`(`:96`) + `obs_snapshot()`(`:167-196`) 三处 · `verify_no_duplicate_rv32i_headers.sh:164` 的 `SOC_TOP_PORTS` · `include/cpu/AGENTS.md` 的头文件计数（C5）· 根 `AGENTS.md` 的 ISA 边界段 · 4 个计数锚点 |
| 17 | 冻结判据需防「前 N 拍本来就相等」的恒绿 | 验收新增四段式判据（含 `dbg_wb_write_en` 的**序列**断言） |

### ⚠️ 待用户裁决

1. ✅ **已裁决（2026-11-10 · 用户拍板 = G3）**：**固件尾部策略** ⇒ ⇒ **采纳「装载器把尾部填成 NOP `0x00000013`」** ⇒ ⇒ 退休数 **178 不变** · `non_retired == 21` 不变（算术：`0x00000013` = `addi x0,x0,0` ⇒ 1 拍退休、`rd=0` 被 x0 保护吞掉）⇒ ⇒ ⇒ 原 G3 的 AC「该值**不是** 178」**作废** ⇒ ⇒ ⇒ 批内顺序 MUST 为 **③ 尾部填 NOP → G1 + G4（同批）→ 其余**（见 §G4 行）⇒ ⇒ ⇒ 详见 §G3 行的「已裁决」说明
2. ✅ **已裁决（2026-11-11 · 用户拍板）**：**接受修改 `phase-6.md` 的 P6 判据** ⇒ ⇒ 但 **MUST 同时记明「原判据被什么取代、为什么」** ⇒ ⇒ 理由：修好 `mcause` 后 P6 会**自动**变绿 ⇒ ⇒ 那是「**被修好**」而不是「本来就好」⇒ ⇒ 不改判据 = P6 **永久假绿**（违背「判据 MUST 可归因」纪律）⇒ ⇒ ⚠️ **代价**（如实登记）：降低了既有验收强度 ⇒ ⇒ 故 MUST 留痕以便追溯。

## critic 第 1 轮（plan, build）—— **REJECT**（随整体）· mergeable: no · scope_exceeded: false

> **轮次**：1/2 · 审查者：critic

### 🔴 BLOCKER（**plan 可修**）：oracle 的 B0–B4 **完全未编排 G1/G2/G4/G5** ⇒「golden 只重算一次」不可行

| 事实 | 证据 |
|---|---|
| G3 的 metis 订正已实测：**178 里约 76 条来自跑出固件尾部的 0 字** ⇒ G1 生效后退休数**掉到 ~102** | 本文档 `:59`、`:243` |
| oracle 的 5 个批次只把 **③（尾部策略）** 收进 B2，**G1/G2/G4/G5 一个都没排期** | 批次表 |
| G4 又自带硬约束「G1 与 G4 MUST 同批交付」 | 本文档 `:60` |

⇒ ⇒ 若 G1 在 B2/B3 的「一次 golden 重算」**之后**落地 ⇒ golden 被**第二次**改基线；若**先**落地 ⇒ ③ 的尾部填 NOP 尚未就位、退休数已掉到 ~102 ⇒ B2/B3 的「稳定 golden」前提失效。
**处置**：G1+G4 MUST **排在 B2 的 ③ 之前**（或与 ③ 同批），并**显式写入批次表**。

### ⚠️ SHOULD_FIX（已采纳）：G3 验收「该值**不是 178**」与 oracle ③「golden 数字**不变**」**直接矛盾**

- oracle ③ = 「(b) + 装载器把尾部填成 NOP `0x00000013`」⇒ 算术上**成立**（`0x00000013` = `addi x0,x0,0` ⇒ 1 拍退休、`rd=0` 被 x0 保护吞掉，`wb_stage.h:102-110`；`non_retired=21` 与尾部无关）。
- 但它既不是 G3 的 (a) 自环 `JAL(0,0)` 也不是 (b) 实测回填 ⇒ 是**第三种做法** ⇒ 会让 G3 的 AC **恒假**。

**处置**：G3 MUST **三选一写死**：
(a) 自环 `JAL(0,0)` ⇒ 手推重算 178/21；
(b) 实测回填（按 `rv32i-commit-path/spec.md:336` 「MUST NOT 预先断言」）；
(c) **尾部填 NOP** ⇒ **178/21 不变**（推荐）；并点明填充落点（`firmware()` 数组 / I-TCM 初值 / `load_firmware` 循环 —— 现只写 `fw.size()` 字，`test_rv32i_soc_dual_parity.cpp:537-541`）。
