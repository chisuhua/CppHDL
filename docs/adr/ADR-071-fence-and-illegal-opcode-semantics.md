# ADR-071: RV32I `FENCE` 显式化 + 未知/未实现 opcode 的**冻结**语义

- **Status**: **Accepted**（2026-11-15 · architect role（build agent）· **oracle ×1**（对抗式复核 ⇒ **REJECT** · 1 真缺陷 ⇒ 已修 + 变异自证）· **critic ×1**（**APPROVE_WITH_CONDITIONS** · 0 BLOCKER · 1 SHOULD_FIX ⇒ 已修）· ⇒ ⇒ **追认** B4 已落地的实现 + **修正**其一处真缺陷）
- **Date**: 2026-11-15
- **Author**: architect role（build agent）
- **Parent**: **ADR-046**（RV32I 收敛 · 唯一权威）· **ADR-063**（B 型立即数 `imm[11]` 误置位）· **ADR-069**（内存数据对齐 · 4 拍 load-use）· **ADR-035**（verilator 后端 · `init_words` 装载链路的先例）
- **关联 change**: `openspec/changes/archive/2026-11-15-isa-gap-illegal-opcode-fence-2026-11`（**已归档** ⇒ ⇒ capability `rv32i-isa-semantics` **+2 Requirement** · 11 Requirement / 47 items 全绿）
- **AC 映射**: 本 ADR §5 的 **V1–V7** → 该 change 的 `## 验收标准`（7 条 checkbox · **全勾**）⇒ ⇒ 逐条对应：
  V1 ↔ 「`FENCE` 是负向对照」· V2 ↔ 「未知 opcode ⇒ 冻结」· V3 ↔ 「已知未实现 vs 未知可区分」·
  V4 ↔ 「`valid` 门控」· V5 ↔ 「无组合环」· V6 ↔ 「退休数不变」· V7 ↔ 「顶层端口 31 → 32」
- **审稿记录**: `.rddf/reviews/ADR-071-fence-and-illegal-opcode-semantics.md`（**oracle ×1**（2026-11-15 · 对抗式复核 ⇒ **REJECT** · 1 个**真缺陷**：`flush ∥ 冻结 stall` 同拍时 **redirect 被丢掉 + 冻结只活 1 拍** ⇒ 已修 + 已补回归用例 + 变异自证；另有 4 条 SHOULD_FIX 已逐条采纳）· **critic ×1** ⇒ 见该文件）
- **修订 v1.2（2026-11-16 · architect · B4 · G1 扩展）**：**supersede §2.6 D6**（原「本版**只覆盖 opcode**」）⇒ ⇒ 本版**已覆盖** `funct3` / `funct7` 的**保留组合**（新 reason 码 **3**）⇒ ⇒ 判定表见 **§2.6.1** ⇒ ⇒ 判定**移到库组件 `Rv32iDecoder`**（**单一来源** · 与 C4 同精神）⇒ ⇒ ID 级只转发 ⇒ ⇒ 实测：**金标零变化**（`ctest -LE perf` 255/255 · 独立 Verilog TB `checks=24 fails=0`）⇒ ⇒ 且**挖出 1 条测试夹具缺陷**（差分表的 SUB 编码 `funct7=0x40` = 保留组合 ⇒ 已按手推修正）
- ⚠️ **本 ADR 是「追认」**：实现已先行落地（commit `514fc26` · B4 · G1+G4）⇒ ⇒ 本 ADR 的作用是
  **补上缺失的决策与审稿**（`phase-6-next-steps.md` 的 **R11**：ISA 语义裁决归 architect，
  planner 无权拍板）⇒ ⇒ **裁决内容 MUST 与已落地实现一致**；若审稿判定实现有错 ⇒ ⇒ 走
  `design_drift` 流程（**不**在本 ADR 内静默改实现）。

---

## 1. 背景与问题

### 1.1 诉求与缺口

`phase-6-next-steps.md` 的 **R11** = 「`FENCE` 语义裁决（**architect 域** · planner 无权拍板）」，
其前置是 **R7（P6-0 侦察）**。⚠️ B4 的实施顺序（C1→C2→C3/C4→**G3→G1+G4**）把 **G4**
（`FENCE` 显式化）**提前落地**了 ⇒ ⇒ **实现先于决策** ⇒ ⇒ 本 ADR 补做该裁决（**追认**）。

### 1.2 实测事实（2026-11-10 登记 · 2026-11-15 复核仍成立 · 可复算）

| # | 事实 | 证据 |
|---|---|---|
| ① | ⚠️ **未知 opcode 被静默当 NOP 逐条退休**：ID 级的 `is_op`/`is_load`/… **全假** ⇒ ⇒ 无任何控制信号 ⇒ ⇒ EX 级照常推进 ⇒ ⇒ 该指令**照常退休** | `include/cpu/pipeline/stages/id_stage.h`（`is_*` 均为 `opcode` 比较）· 后果 = 「程序跑飞到非法指令时**零报错、零信号**」 |
| ② | ⚠️ **「跑出固件尾部」完全不可见**：I-TCM 的 `ch_mem` 初值是 `0` ⇒ ⇒ `0x00000000` 的 opcode = `0x00` = **未知** ⇒ ⇒ 既有 golden 的 **176** 条退休里约 **76** 条来自跑出固件尾部的 0 字 | `.rddf/improvements/rv32i-isa-gap-closure-2026-11.md`（G3 的算术）· 实测佐证：`tests/test_verilator_complex_e2e.cpp` 的 imem 数组只写了 **15** 个初值 ⇒ 第 16 字是 `0` ⇒ G1 落地后流水线在 `pc = 0x8000003C` **冻结** ⇒ 退休数 16 → 15 ⇒ ⇒ **「静默 NOP」被点亮** |
| ③ | ⚠️ **`FENCE`/`FENCE.I`（`0x0F`）此前完全不在译码表** ⇒ ⇒ 与「未知 opcode」**混为一谈** ⇒ ⇒ 无法区分「我打算实现但还没实现」与「这根本不是指令」 | 旧 `include/cpu/pipeline/rv32i_decoder.h` 的 opcode 常量表（无 `0x0F`）· 现补于 `:57` |
| ④ | ⚠️ **`OP_SYSTEM`（`0x73`）常量已定义但零消费者** ⇒ ⇒ `ECALL`/`EBREAK`/CSR/`MRET` 与真未知**无法区分** | `rv32i_decoder.h`（常量在 · 判据不在） |
| ⑤ | ⚠️ **「静默 NOP」是仓内已证的高成本缺陷族**：`funct7[5]` 对 I 型误判（`addi x, x0, -8` ⇒ 算成 SUB）靠**人手工造边界用例**才被发现 ⇒ ⇒ 若当时有「非法指令」信号，至少能立刻知道「跑到了不该跑的地方」 | `include/cpu/AGENTS.md` CONVENTIONS（`alu_op` 的 SUB/SRA 判定） |

### 1.3 本 ADR 裁决**三件**同族但不同面的事

| 问题 | 内容 | 面 |
|---|---|---|
| **P-A** | `FENCE`/`FENCE.I` 的**语义**（是「已知」还是「未知」？行为是什么？） | ISA 语义 |
| **P-B** | 未知 / 已知未实现 opcode 的**行为策略**（冻结？陷阱？继续跑？） | 行为契约 |
| **P-C** | 该策略的**可观测面**（哪些信号、几位、暴露到哪一层？） | 端口契约 |

⇒ ⇒ 三者**互相依赖**（P-C 的分类必须能表达 P-B 的处置，P-A 的分类必须与 P-B 一致）
⇒ ⇒ 故**同批裁决**（拆开会产出「一半实现先于另一半决策」的同类漂移）。

---

## 2. 决策

### 2.1 **D1**（P-A）= `FENCE` / `FENCE.I` 是**已知指令**，行为 = **NOP**

`OP_FENCE = 0x0F` MUST 进「已知 opcode」集合；本版行为 = **NOP**：无副作用、不写寄存器、
不访存、**不**产生 `illegal_reason`。

**理由（语义等价性论证）**：`FENCE` 的排序语义**无可实现对象** ⇒ ⇒ 「执行 `FENCE`」与
「什么都不做」在**可观察行为上等价** ⇒ ⇒ NOP 是**语义等价的最小实现**，而非**偷懒的占位**。

⚠️⚠️ **等价性的承重假设 MUST 逐条登记**（oracle 订正：原稿写「单核无 cache」= **既非充分也非必要**）：

| # | 假设 | 证据 / 边界 |
|---|---|---|
| A1 | **单 hart** | 本仓无多核装配 |
| A2 | **in-order LSU** | `MemStage` 单出口、无乱序队列 |
| A3 | **无写缓冲** | `DCacheWriteBuffer` 是**直通 shim**（无缓冲状态）⇒ 没有可被重排的写 |
| A4 | **hart 不可写自己的取指空间** | 数据口只接 D-TCM；I-TCM 的写口只有**外部** `prog_*`（SoC 顶层）⇒ ⇒ 不存在「hart 自写指令流」⇒ ⇒ **`FENCE.I` 无失效对象** |

⇒ ⇒ **重开触发器（任一成立 ⇒ MUST 重审本条）**：① 真写缓冲落地 ② 引入 MMIO 有序性需求
（外设可见性）③ hart 获得可写的 icache 区域（A4 被打破）⇒ ⇒ ⚠️ **「接了 I-cache」本身不破坏等价**
（只读 cache 不产生排序问题）⇒ ⇒ 破坏者是**可写的**取指空间。
⚠️ `FENCE.I`（`funct3 = 1`）同理（A4 成立 ⇒ 无失效对象）。
⚠️ **`FENCE` 仍占一个退休槽**（`mem_valid_out = mem_valid` ⇒ 它是 valid 指令）⇒ ⇒ V1 的「20 拍 ⇒ 16 条退休」依赖此。

### 2.2 **D2**（P-B 的输入）= opcode 分类 = **2 bit**，`0`/`1`/`2` 三态

| 值 | 含义 | 判据 |
|---|---|---|
| `0` | 合法 | 命中已知 opcode 集合（含 `FENCE`） |
| `1` | **已知未实现** | `OP_SYSTEM`（`0x73`）= `CSR` / `ECALL` / `EBREAK` / `MRET` |
| `2` | **未知 opcode** | 其余一切 |

⇒ ⇒ **「已知未实现」与「未知」MUST 可区分** ⇒ ⇒ 理由：`OP_SYSTEM` 是**本仓计划实现**的
（特权级独立立项）⇒ ⇒ 与「这根本不是指令」是**不同的工程状态** ⇒ ⇒ 混为一谈会让
「特权级还没做」看起来像「程序跑飞了」。

### 2.3 **D3**（P-B）= 行为策略 = **冻结**（PC 不前进 · ID/EX 装气泡 · 永不退休）

非法指令进入 ID 级时 ⇒ ⇒ **IF 与 ID 同时 `stall`** ⇒ ⇒
① PC 不前进（`if_stage` 侧冻结 ⇒ 取指与 skid 一并停）· ② ID/EX 装**气泡**
⇒ ⇒ 该指令 **MUST NOT** 进入 EX、**MUST NOT** 退休、**MUST NOT** 产生任何副作用。

⇒ ⇒ **MUST NOT** 当作 NOP 继续跑（那是旧行为，正是本项要消灭的）⇒ ⇒
⇒ ⇒ **MUST NOT** 陷阱到 `mtvec`（真 ISA 语义）—— 理由见 §3 选项 D。

#### D3-b（**优先级** · oracle 复核后新增 · 承重）

⚠️ **冻结 stall 与 `flush` 可以同拍**（冻结是本仓**第二个** stall 来源
⇒ ⇒ 旧互斥论证只覆盖 `hazard.stall`）⇒ ⇒ **优先级 MUST 为**：

> **`flush`（重定向）优先于冻结 stall** —— 判据：`pc_update_en = !rst & !(stall & !(flush & branch_valid))`

**为什么（三条，缺一条都会造出静默错误）**：

1. **与 ID 级对齐**：`id_stage` 的 `idex_bubble = flush | stall` ⇒ ⇒ `flush` **本来就优先**
   造气泡（ID 级指令被 `flush` 杀掉）⇒ ⇒ PC MUST 跟随 ⇒ ⇒ 否则「ID 已被杀、PC 却不动」
   ⇒ ⇒ 指令流与 PC **失步**。
2. **语义正确性**：`flush` 只来自 **EX 级**的分支/跳转 ⇒ ⇒ 同拍 ID 级持有的是该分支的
   **fall-through**（= **错误路径**）指令 ⇒ ⇒ 「错误路径上的非法字」被 `flush` 杀掉是
   **正确**的（那本就不该被执行）⇒ ⇒ **不是**「放过非法指令」。
3. **冻结域不受影响**：**正确路径**上的非法字 **不可能**与 `flush` 同拍
   （`flush` ⇒ ID 必在错误路径上）⇒ ⇒ 冻结在**正确路径**上仍是**终态**。

⚠️ **必须有重定向才让 `flush` 优先**（`flush & branch_valid`）：本仓 `flush` 的唯一来源 =
`ex_branch & ex_branch_taken`，而 `branch_valid` 接的正是同一个 `ex_stage.branch_taken`
⇒ ⇒ **流水线内 `flush ⇒ branch_valid` 恒成立** ⇒ ⇒ 该门在流水线内恒等于 `flush`；
而「`flush` 但无重定向」在流水线内**不可达** ⇒ ⇒ 组件级场景
（`tests/test_rv32i_pipeline_behavior.cpp` Batch 4-a：`stall=1 ∧ flush=1 ∧ branch_valid=0`）
仍保持「PC 冻结」的旧契约 ⇒ ⇒ **既有钉住的判据零改动**。

#### D3-c（**实现缺陷史** · 诚实记录）

⚠️ **初版实现是错的**（`514fc26` 落地时用 `pc_update_en = !rst & !stall`）：
`flush ∥ 冻结 stall` 同拍 ⇒ ① **redirect 被丢掉**（PC 被 stall 冻住）② 下一拍 `valid=0`
⇒ **冻结只活 1 拍** ③ 非法字被静默跳过 + `illegal_reason` 回落到 0
⇒ ⇒ **「跑飞零信号」被 G1 自己造出来**（且两后端同构 ⇒ parity 结构性失明）。
⇒ ⇒ 由 oracle 对抗式复核发现 ⇒ 按 §4.2 ⑤ 的 `design_drift` 路径**修实现 + 补回归用例 + 变异自证**：
反转 `flush_redirect` ⇒ `REQUIRE(saw_x5)` **转红**（实测）。

### 2.4 **D4**（P-C）= 可观测面 = **一个** 2 bit 端口，逐层上引

- ID 级：`illegal_reason`（2 bit，**显式具名** ⇒ 不进 `unnamed_output` 编号池 ⇒ 既有端口编号零位移）
- pipeline：`dbg_illegal_reason`（2 bit，**显式具名**）
- SoC 顶层：`dbg_illegal_reason`（2 bit）⇒ ⇒ **顶层端口 31 → 32** · 产物端口 **126 → 129**

⇒ ⇒ **不新增**独立的 1 bit「非法」位（它恒等于 `reason != 0` ⇒ **冗余端口** ⇒ ZERO-DEBT 不允许）。

### 2.5 **D5** = 判据 MUST 含 **`valid`**（ID 级的 `io().valid`）

⚠️ 否则：冲刷后 IF/ID 寄存器**保留旧指令字**（只清 `valid`）⇒ ⇒ 会在**错误路径**上把流水线冻死；
复位后的**幻影字**同理。⇒ ⇒ 判据 = `io().valid & f(opcode)`。

### 2.6 **D6** = 本版**只覆盖 opcode** ⇒ `funct3`/`funct7` 非法组合 = **residual**

> ⚠️ **v1.2（2026-11-16）已 supersede 本条**：`funct3` / `funct7` 的**保留组合**已纳入本版
> ⇒ ⇒ 判定表见 **§2.6.1** ⇒ ⇒ 本节的「residual」结论**仅作历史留痕**（不再生效）。

载体 = capability `rv32i-isa-semantics` 的「**本版只覆盖 opcode**」场景（已随 change 归档进主 spec）。
⚠️ **不**进「架构台账残差活账」（`.rddf/checks/arch-residual-registry.md`）—— 该活账是
**层次化 RTL 台账 §7** 的派生（`scripts/lib/ledger_residual.py:17` 的 `LEDGER_REL` 硬编码）
⇒ ⇒ ISA 主题的残留**进那本表 = 主题污染** ⇒ ⇒ 若将来要纳入统一活账 ⇒ ⇒ **独立立项**
（新增第二本台账 + 护栏支持多台账）⇒ ⇒ **不在本项范围**。

### 2.6.1 **保留组合判定表**（v1.2 · 2026-11-16 · 取代 §2.6 的 residual 结论）

⚠️⚠️ **关键事实（手推 · 实测踩过）**：**U 型（`LUI` / `AUIPC`）与 J 型（`JAL`）没有 `funct3` 字段**
—— 它们的 bits 14:12 是**立即数位**（U 型 = `imm[14:12]` · J 型 = `imm[19:17]`）
⇒ ⇒ 这两类的 bits 14:12 **任何取值都合法** ⇒ ⇒ 把「`funct3` MUST 为 `000`」套到它们上
⇒ ⇒ **误判合法指令为非法**（实测：`lui x10, 0x1` = `0x00001537` 的 bits 14:12 = `001` ⇒ **合法**）。

| opcode | 有 `funct3` 字段？ | 合法组合 | 备注 |
|---|---|---|---|
| `OP`（`0x33` · R 型） | ✅ | `funct3` **全 8 个**合法 | `funct7` ∈ {`0x00`, `0x20`} ⇒ ⇒ `0x20` **仅当** `funct3` ∈ {`000`（SUB）, `101`（SRA）} |
| `OP-IMM`（`0x13`） | ✅ | 全 8 个 | **不查 `funct7`** ⇒ ⇒ bits 31:25 是 `imm[11:5]`（`imm[10]` 是 `SRAI` 的选择位 ⇒ 两者皆合法） |
| `LOAD`（`0x03`） | ✅ | {`000`,`001`,`010`,`100`,`101`} | `011` / `110` / `111` = **保留** |
| `STORE`（`0x23`） | ✅ | {`000`,`001`,`010`} | `011`–`111` = **保留** |
| `BRANCH`（`0x63`） | ✅ | {`000`,`001`,`100`,`101`,`110`,`111`} | `010` / `011` = **保留** |
| `JALR`（`0x67`） | ✅ | {`000`} | 其余 = **保留** |
| `JAL`（`0x6F`） | ❌（bits 14:12 = `imm[19:17]`） | **全部** | — |
| `LUI`（`0x37`）· `AUIPC`（`0x17`） | ❌（bits 14:12 = `imm[14:12]`） | **全部** | — |
| `SYSTEM`（`0x73`） | — | 归 **reason 1**（已知未实现）⇒ ⇒ 不做 `funct3` 检查 | 见 §2.2 D2 |
| `FENCE`（`0x0F`） | ❌（bits 14:12 = `fm[2:0]`） | **全部** | 见 §2.1 D1（= NOP）⇒ ⇒ **不**参与保留组合判定 |

⇒ ⇒ **reason 码扩到 4 态**：`0` 合法 · `1` 已知未实现（`OP_SYSTEM`）· `2` 未知 opcode
· **`3` = 已知 opcode 但 `funct3` / `funct7` 是保留组合**。
⇒ ⇒ **优先级**：`1`（`OP_SYSTEM`）> `3`（保留组合）> `2`（未知）> `0`（合法）
⇒ ⇒ ⚠️ `OP_SYSTEM` 已是「已知未实现」⇒ ⇒ 不重复判其 `funct3`（否则 reason 会随 `funct3` 抖动）。
⇒ ⇒ **判定 MUST 在库组件 `Rv32iDecoder`**（**单一来源**）⇒ ⇒ ID 级**只转发** + 保留 `valid` 门控
⇒ ⇒ MUST NOT 在两处各写一份判据（漂移风险）。

⇒ ⇒ **行为**（与 §2.3 D3 一致）：reason ≠ 0 ⇒ ⇒ **冻结**（PC 不前进 · ID/EX 装气泡 · 永不退休）
⇒ ⇒ ⚠️ 独立 Verilog TB 的 `checks=24` 与既有 golden **实测不变**（仓内固件**未踩**保留组合）。

### 2.7 **D7** = 实现 MUST **无组合环**

`illegal_reason` 只依赖 ID 级的**输入**（`io().valid` + `io().instruction`）⇒ ⇒ 而
`if_stage.io().valid` 是**寄存**值（`include/cpu/pipeline/stages/if_stage.h:329`
`io().valid = if_id_valid_reg;`）⇒ ⇒ `stall → valid → illegal_reason → stall` 的反馈
**被寄存打断** ⇒ ⇒ 拓扑排序 MUST NOT 报 `Cycle detected`。

### 2.8 **D8** = **顺序硬约束**：`G3`（尾部填 NOP）MUST **先于** `G1`

理由（critic 第 2 轮改向）：`G1` 单独落地 ⇒ 流水线在固件尾部冻结 ⇒ 退休数 **176 → ~102**
⇒ ⇒ golden 被改**一次**；随后补尾部填充 ⇒ 冻结不再触发 ⇒ 退休数回到 176 ⇒ ⇒ golden 被改**两次**。
⇒ ⇒ 故顺序 MUST 为 **G3 → G1+G4**，且**退休数 MUST 引用「B3 后的基线」**（禁写死旧值）。

---

## 3. 选项对比表

| # | 选项 | 描述 | 结论 | 理由 |
|---|---|---|---|---|
| A | **`FENCE` = 已知 · NOP** | 进已知集合 · 行为 = NOP | ✅ **采纳** | 单 hart + in-order + 无写缓冲 ⇒ 无排序语义 ⇒ NOP **语义等价** + 最小实现（§2.1） |
| B | `FENCE` = 未知 ⇒ 冻结 | 留在「未知」集合 | ❌ **否决** | 合法的 `fence` 会让核**永久冻死** ⇒ 比「静默」更糟（静默至少还能跑） |
| C | `FENCE` = 「已知未实现」（`reason = 1`） | 与 `OP_SYSTEM` 同类 | ❌ **否决** | `FENCE` **可实现**（= NOP）⇒ 与「本版不做」混为一谈 ⇒ 分类失去信息量 ⇒ 且会让「特权级未做」的计数被 `fence` 污染 |
| D | 未知 opcode = **陷阱到 `mtvec`** | 真 RISC-V 语义 | ⏸ **独立立项** | 真语义最正确 ⇒ 但 ⚠️ **订正**：trap 组件**已在库内**（`include/cpu/riscv/rv32i_exception.h` 的 trap 入口 + MRET · `rv32i_csr.h` 的 CSR bank，均有组件级测试）⇒ ⇒ **缺的是「接入本流水线」**（第三路 PC 源 + `mepc`/`mcause` 副作用 + 流水线 flush）⇒ ⇒ 那正是特权级立项的**本体** ⇒ ⇒ 现在做 = **半个陷阱**（有组件、无接入 ⇒ 连现场都抓不住）⇒ ⇒ **冻结是诚实的中间态**（至少保住 `dbg_id_pc` 现场 + 可复位） |
| E | 未知 opcode = **当 NOP 继续**（旧行为） | 静默 | ❌ **否决** | 零信号 ⇒ 调试成本极高（§1.2 ⑤ 的仓内同型事故）⇒ ⇒ 本项的价值恰在于改掉它 |
| F | 可观测面 = 1 bit「非法」**+** 2 bit「原因」**两个**端口 | 两个独立信号 | ❌ **否决** | 二者**冗余**（`非法 == reason != 0`）⇒ ⇒ ZERO-DEBT「禁预留没人读的端口」⇒ ⇒ 只留 2 bit |
| G | 冻结只做「ID 级装气泡」，**PC 继续** | 丢弃后继续 | ❌ **否决** | 那正是 §2.3 引用的「弱化 NOP」读法 ⇒ 后续指令继续退休 ⇒ 「跑飞」仍不可见 ⇒ ⇒ **PC 必须一起冻** |

---

## 4. 后果 / 代价

### 4.1 正面

- 「跑飞」从**不可见**变为**可判定**：`dbg_illegal_reason != 0`（分类）+ PC 不前进 + 退休数停 ⇒
  三条**互相独立**的观测量 ⇒ ⇒ 可写进「停机原因」的诊断文案。
- `FENCE` 从「未知」升为「已知且合法」⇒ ⇒ 合法的 `fence` 不再让核冻死。
- 「已知未实现」与「未知」可区分 ⇒ ⇒ 特权级立项的进度可被观测（`reason = 1` 的计数）。

### 4.2 负面（**必须承担**）

| # | 代价 | 处置 |
|---|---|---|
| ① | **下游负担**：依赖「跑出程序尾部仍继续退休」的下游 MUST 把尾部填成 NOP | 载体 = 装载器纪律（G3：`load_firmware` + 独立 Verilog TB 的装载任务）+ `docs/CHANGELOG.md` + 归档 change 的「## 用户可见变更」段 |
| ② | **冻结是终态**：没有恢复路径（无 trap）⇒ 只能复位 | 已如实登记（§4.3 风险 ①） |
| ③ | **分类只覆盖 opcode**：`funct3`/`funct7` 非法组合**仍静默** | residual（D6） |
| ④ | **端口面增长**：顶层 31 → 32 · 产物 126 → 129 | 已登记（C6 快照 +1 · TB +3 根） |
| ⑤ | **实现先于决策**（本 ADR 是追认） | 由本 ADR + oracle→critic 审稿**补齐**；⚠️ **该风险已实际发生**：oracle 复核查出 D3-c 的实现缺陷 ⇒ 已按 `design_drift` 修实现 + 补回归用例 + **变异自证** |

### 4.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| ① | **冻结与 flush 同拍**（**新增可达组合**）：冻结是本仓**第二个** stall 来源 ⇒ ⇒ 旧互斥论证（只覆盖 `hazard.stall`）**不再充分** | ✅ **已处置**（D3-b）：优先级 = **`flush`（重定向）优先**，且**只在确有重定向**时（`flush & branch_valid`）⇒ ⇒ 组件级既有的「`stall=1 ∧ flush=1 ∧ branch_valid=0` ⇒ PC 冻结」契约**零改动** ⇒ ⇒ 旧不变量注释（「flush 与 stall 不会同拍」= **事实错误**）已在 `if_stage.h` 重写 |
| ② | **组合环** | D7 已钉 + V5 判据 |
| ③ | **顺序错误**（G1 先于 G3）⇒ 退休数被改两次 | D8 已钉 + 落地顺序已可验（`git log`：`a144eef` = G3 早于 `514fc26` = G1+G4） |

---

## 5. Verification（可勾选验收判据 · **前瞻**方法）

| # | 判据 | 方法 | 对应载体 |
|---|---|---|---|
| **V1** | `FENCE`（`0x0FF0000F`）⇒ `illegal_reason == 0` **且不冻结**（退休数继续增长） | 组件级 + SoC 级用例（双后端） | `test_rv32i_soc_dual_parity` 的 `Rv32iSoc - UnknownOpcodeFreezes + FenceIsNop` |
| **V2** | 未知 opcode（`0xFFFFFFFF`）⇒ `illegal_reason == 2` **且**退休数**恒定**（探针指令永不退休）**且** `dbg_id_pc` 停在探针 PC | 同上（期望值**手推**） | 同上 |
| **V3** | `OP_SYSTEM`（`0x00000073`）⇒ `illegal_reason == 1` | 同上 | 同上 |
| **V4** | 冲刷残留 / 复位幻影**不得**触发冻结 | 用例（`valid` 门控） | 同上 + `id_stage.h` 的 `io().valid &` |
| **V5** | IR 拓扑排序 MUST NOT 报 `Cycle detected` | 任意一次建图 | 构图期 |
| **V6** | 退休数基线**不变**：`non_retired == 23` · `retire == kCycles - 24` | 既有 golden 断言（**原样**通过） | `test_rv32i_soc_dual_parity` |
| **V7** | 顶层端口 **32** · 产物端口 **129** | C6 快照 + 产物实测 | `verify_no_duplicate_rv32i_headers.sh` C6 |
| **V8** | `if_stage.h` 的**旧互斥论证**（「flush 与 stall 不会同拍」）MUST 重写为「**两个 stall 来源** + `flush` 优先」 | 文档 | ✅ **已做**（本轮） |
| **V9** | **`flush` ∥ 非法字** ⇒ ⇒ ① 重定向**兑现**（分支目标写回出现）② 非法字（`rd=31`）**未被执行**（无 x31 写回）③ `illegal_reason == 2` **恰在碰撞拍**出现并随后回落 0 | 双后端用例 + **变异自证**（反转 `flush_redirect` ⇒ `REQUIRE(saw_x5)` **转红**） | `test_rv32i_soc_dual_parity` 的 `Rv32iSoc - flush wins over illegal-freeze stall` |
| **V10** | 组件级既有契约**零改动**：`stall=1 ∧ flush=1 ∧ branch_valid=0` ⇒ PC 冻结 | 既有用例（**原样通过**） | `tests/test_rv32i_pipeline_behavior.cpp` Batch 4-a |

⇒ ⇒ **V1–V10 全部已闭合**（critic 2026-11-15 指出此处曾残留「V8 仍欠」的陈旧句 ⇒ 已删）：
V1–V7 在归档 change 中实现并通过 · **V8**（注释重写）本轮完成 ·
**V9/V10** 由 `illegal-freeze-flush-priority-2026-11` 落地（双后端 9 断言 + 变异自证）。
⇒ ⇒ 判据：`ctest -LE perf` **254/254** · 独立 Verilog TB `checks=24 fails=0`。

---

## 6. 关联与边界

- **不覆盖**：CSR 指令 / `ECALL` / `EBREAK` / `MRET` 的**语义实现**（特权级 ⇒ 独立立项）·
  中断 · trap 入口 · `funct3`/`funct7` 非法组合（D6 residual）。
- **不改变**：退休计数口径（`perf_instructions` 仍只计**有效退休**）· 任何既有 golden 的期望值。
- **与 ADR-046 的关系**：D1/D2 使 `rv32i_decoder.h` 的 opcode 常量表**成为**「已知指令全集」的
  单一权威 ⇒ ⇒ 与 ADR-046 的「唯一权威」方向**一致**，非冲突。
