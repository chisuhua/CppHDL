## MODIFIED Requirements

### Requirement: 未知 opcode MUST 可观测，且 MUST NOT 被当作 NOP 退休

- **MODIFIED**（2026-11-26 · P6-3）：本 Requirement 的
  **行为策略**因 **trap 入口接线**而**分流** —— 既有内容**一条都不删** ⇒ ⇒ 本 delta 只**改写 3 处**：
  ① 「**行为策略 = 冻结**」段 ⇒ ⇒ 改为**按 `illegal_reason` 分流**（`{2,3}` ⇒ **陷入** · `1` ⇒ **仍冻结**）
  ② Scenario「冻结 —— 该指令永不退休」⇒ ⇒ 拆成**两条**（「`{2,3}` ⇒ 陷入」·「`1` ⇒ 冻结」）
  ③ Scenario「`flush` 的残留不得触发冻结」⇒ ⇒ 追加「MUST NOT 触发 trap」+「同拍既有 taken 分支
  ⇒ MUST 不触发 trap」
  ⇒ ⇒ 架构依据 = `docs/adr/ADR-048-rv32i-fetch-skid-buffer.md` **§8（dated addendum v1.1）**
  + `docs/adr/ADR-071-fence-and-illegal-opcode-semantics.md` **v1.3**
  ⇒ ⇒ 母 improvement 的「接缝兑现 #1」（接线 change MUST 以 MODIFIED delta 修订本冻结条款）
  ⇒ ⇒ ⚠️ **禁**只改实现不改 spec。

ID 级 MUST 产出操作码分类（`0` = 合法 · `1` = **已知未实现**（`OP_SYSTEM`，
即 `ECALL`/`EBREAK`/CSR/`MRET`）· `2` = **未知 opcode**
· **`3` = 已知 opcode 但 `funct3` / `funct7` 是保留组合**（见下）。

⇒ ⇒ **保留组合判定（v1.2 · 2026-11-16 扩展）**：判定 MUST 覆盖 `funct3` / `funct7` 的
**ISA 保留组合** ⇒ ⇒ 判定表 = `docs/adr/ADR-071-fence-and-illegal-opcode-semantics.md` **§2.6.1**
⇒ ⇒ **唯一权威在那份表**（本 Requirement 只要求「MUST 判」+ reason 值）。
⇒ ⇒ ⚠️ **关键边界**：**U 型（`LUI` / `AUIPC`）与 J 型（`JAL`）没有 `funct3` 字段**
（bits 14:12 是**立即数位**）⇒ ⇒ 它们的 bits 14:12 **任何取值都合法**
⇒ ⇒ **MUST NOT** 把「`funct3` MUST 为 `000`」套到它们上（否则**误判合法指令为非法**）。
⇒ ⇒ **优先级**：`1`（`OP_SYSTEM`）> `3`（保留组合）> `2`（未知）> `0`（合法）。
⇒ ⇒ **判定 MUST 在库组件 `Rv32iDecoder`**（**单一来源**）⇒ ⇒ ID 级**只转发** + 保留 `valid` 门控
⇒ ⇒ **MUST NOT** 在两处各写一份判据（漂移风险）⇒ ⇒ 两份判据会**静默分叉**。

⇒ ⇒ 判据 MUST 以「ID 级**有效**」为门控（`valid`）—— 否则 flush 后 IF/ID 寄存器
保留的**残留旧指令**会被误判 ⇒ ⇒ 在**错误路径**上冻结流水线。

⇒ ⇒ **行为策略 = 冻结**（MUST）：非法指令进入 ID 级时 ⇒ ⇒ IF 与 ID **同时停顿**
（PC 不前进 · ID/EX 装**气泡**）⇒ ⇒ 该指令 **MUST NOT** 进入 EX、**MUST NOT** 退休。

⇒ ⇒ **行为策略**（MUST）—— ⚠️ **2026-11-26 · P6-3 起按 `illegal_reason` 分流**
（既有「一律冻结」**不再成立**）：
- **`illegal_reason ∈ {2, 3}`**（**未知 opcode** · **ISA 保留组合**）⇒ ⇒ **陷入**（trap）：
  ⇒ ⇒ **MUST** 写异常上下文（`mepc` = **该指令自身的 PC** · `mcause` = illegal instruction（**2**））
  ⇒ ⇒ **MUST** 把取指级 PC **重定向**到 `mtvec` 的**基址**
  ⇒ ⇒ 该指令 **MUST NOT** 进入 EX、**MUST NOT** 退休、**MUST NOT** 产生副作用
  ⇒ ⇒ 依据 = capability `rv32i-privilege` 的「trap 入口 MUST 保存异常上下文并重定向 PC」
  ⇒ ⇒ 架构依据 = `docs/adr/ADR-048-rv32i-fetch-skid-buffer.md` **§8（dated addendum v1.1）**
  ⇒ ⇒ ⚠️ **「陷入」的判据 MUST 直接观测 `valid` 清零**（否则「trap 每拍重触发 ⇒ 活锁」不被抓到）
- **`illegal_reason = 1`**（`OP_SYSTEM` 的 `funct3 = 0` ⇒ `ECALL`/`EBREAK`/`MRET`/`WFI`）
  ⇒ ⇒ **行为策略仍是冻结**（MUST）：IF 与 ID **同时停顿**（PC 不前进 · ID/EX 装**气泡**）
  ⇒ ⇒ 该指令 **MUST NOT** 进入 EX、**MUST NOT** 退休。
- ⇒ ⇒ 两者**都** **MUST NOT** 把它当作 NOP 继续执行。

#### Scenario: 未知 opcode 在 ID 级被点亮

- **WHEN** ID 级持有一条有效指令，其 opcode 不属于已知指令集合
- **THEN** 操作码分类 MUST 为 `2`（未知）
- **AND** MUST NOT 为 `0`

#### Scenario: 已知但未实现的 opcode 与未知 opcode MUST 可区分

- **WHEN** ID 级持有一条 `OP_SYSTEM`（`0x73`）指令
- **THEN** 操作码分类 MUST 为 `1`（已知未实现）
- **AND** MUST NOT 为 `2`（未知）

#### Scenario: 冻结 —— 该指令永不退休

- **WHEN** 一条未知 opcode 指令进入 ID 级
#### Scenario: 非法指令（`reason ∈ {2,3}`）—— 陷入而非冻结

- **WHEN** 一条**未知 opcode**（`illegal_reason = 2`）或 **ISA 保留组合**（`= 3`）指令进入 ID 级
- **THEN** `mepc` MUST = **该指令自身的 PC**（**不是** PC+4）
- **AND** `mcause` MUST = **2**（illegal instruction）
- **AND** 取指级 PC MUST **重定向**到 `mtvec` 的**基址**
- **AND** 该指令 MUST NOT 出现在任何一级的**有效**链上（`MUST NOT` 退休）
- **AND** IF/ID MUST 被杀（下一拍 ID 级 MUST 无效）⇒ ⇒ 在途的**更老**指令 MUST 照常退休

#### Scenario: 已知未实现（`reason = 1`）—— 仍是冻结，该指令永不退休

- **WHEN** 一条 `OP_SYSTEM` 的 `funct3 = 0` 指令（`ECALL`/`EBREAK`/`MRET`/`WFI`）进入 ID 级
- **THEN** PC MUST 停在原地（不前进）
- **AND** 该指令 MUST NOT 出现在任何一级的**有效**链上（`MUST NOT` 退休）
- **AND** 后续指令 MUST NOT 被取入并退休

#### Scenario: `flush` 的残留不得触发冻结

- **WHEN** 一条指令在**错误路径**上被冲刷，且 IF/ID 寄存器保留其旧指令字
#### Scenario: `flush` 的残留不得触发冻结（也 MUST NOT 触发 trap）

- **WHEN** 一条指令在**错误路径**上被冲刷，且 IF/ID 寄存器保留其旧指令字
- **THEN** 因为该拍 `valid` 为假 ⇒ MUST NOT 触发冻结
- **AND** MUST NOT 触发 trap（`mepc` / `mcause` MUST 不变）
- **AND** ⚠️ **同拍既有 taken 分支又有非法字**（错误路径的 fall-through 槽）⇒ ⇒ MUST **不**触发 trap
  ⇒ ⇒ 理由：否则会用**错误路径**的 PC 写坏 `mepc`

#### Scenario: 本版只覆盖 opcode

- ⚠️ **标题保留为可追溯性**（openspec MODIFIED 语义要求 Scenario **同名**）⇒ ⇒
  **判据以本 Scenario 正文为准**（2026-11-16 · v1.2：**本版已覆盖该维度**）
- **WHEN** 一条指令的 opcode **合法**但 `funct3` / `funct7` 是 **ISA 保留组合**
      （判定表 = **ADR-071 §2.6.1**；例：`LOAD` 的 `funct3 = 011` · `OP` 的 `funct7 = 0x40`）
- **THEN** 操作码分类 MUST 为 **`3`**（保留组合）
- **AND** 该指令 MUST **冻结**（同「未知 opcode」的处置）
- **AND** ⚠️ **负向对照**：`LUI` / `AUIPC` / `JAL` / `FENCE` 的 bits 14:12 **不是 `funct3`**
      ⇒ ⇒ 它们的**任何**取值 MUST 判为**合法**（`0`）

### Requirement: 冻结与 `flush` 同拍时 MUST 以**重定向优先**

- **MODIFIED**（2026-11-26 · P6-3）：本 Requirement 的**判据公式**因 **trap 入口接线**而**扩展** ——
  既有内容**一条都不删** ⇒ ⇒ 本 delta 只**改写 2 处**：① 判据公式的 override 集新增 `trap_redirect`
  ② Scenario「正确路径的冻结仍是终态」⇒ ⇒ 限定为「**仅对 `illegal_reason = 1` 成立**」
  （`{2,3}` 改为陷入）⇒ ⇒ 依据同 ADR-048 §8。
⇒ ⇒ **MUST**：冻结（`illegal_reason = 1`）的 stall 与**重定向**（`flush` 的分支重定向或
`trap_redirect` 的陷阱重定向）同拍时，**MUST 以重定向优先**。

「未知 opcode ⇒ 冻结」引入了本仓**第二个** stall 来源 ⇒ ⇒ 它与 `flush`（分支/跳转重定向）
**可以同拍** ⇒ ⇒ 此时 **MUST 以重定向优先**：

- 判据 MUST 形如 `pc_update_en = !rst & !(stall & !(flush & branch_valid))`
- 判据 MUST 形如
  `pc_update_en = !rst & !(stall & !((flush & branch_valid) | trap_redirect))`
  ⇒ ⇒ **仅当确有重定向**（`flush & branch_valid` 或 `trap_redirect`）才让重定向优先。
  ⇒ ⇒ ⚠️ **2026-11-26 · P6-3 扩展**：override 集新增 `trap_redirect`（陷阱重定向）
  ⇒ ⇒ 依据 = ADR-048 **§8 DA2**；且 `trap_redirect` MUST 与 `flush` 的重定向**互斥**
  （流水线级由「trap 触发 MUST 排除 `flush`」保证 ⇒ 组件级 MUST 写死优先级）。
- ID 级的 `flush` 优先级 MUST 与 PC 侧**一致**（`idex_bubble = flush | stall`）。
- **MUST NOT** 让 stall 把重定向冻住（那会丢 redirect，且下一拍 `valid = 0`
  ⇒ 冻结只活 1 拍 ⇒ 非法字被**静默**跳过）。
- 「`flush` 但无重定向」（`branch_valid = 0`）时 PC MUST 仍冻结（组件级既有契约）。

#### Scenario: 重定向 MUST 被兑现（taken 分支 + fall-through 非法字）

- **WHEN** EX 级有一条 taken 分支，同拍 ID 级持有其 fall-through（**错误路径**）上的非法字
- **THEN** PC MUST 更新为分支目标
- **AND** 分支目标处的指令 MUST 执行（其写回 MUST 出现）
- **AND** 非法字 MUST NOT 被执行（其 `rd` MUST NOT 收到写回）

#### Scenario: 非法字 MUST 在碰撞拍被点亮

- **WHEN** 上述碰撞发生
- **THEN** 操作码分类 MUST 在 ID 级持有该非法字的那一拍为 `2`（未知）
- **AND** `flush` 生效后 MUST 回落到 `0`（冻结域只覆盖正确路径）

#### Scenario: 正确路径的冻结仍是终态

- **WHEN** ID 级持有一条未知 opcode 指令，且**同拍无** `flush`
#### Scenario: 正确路径的「冻结是终态」—— 仅对 `reason = 1` 成立

- **WHEN** ID 级持有一条 `illegal_reason = 1` 的指令（`ECALL`/`EBREAK`/`MRET`/`WFI`），且**同拍无** `flush`
- **THEN** PC MUST 停在原地（不前进）
- **AND** ⚠️ **`illegal_reason ∈ {2,3}` 的「冻结是终态」已失效**（2026-11-26 · P6-3）：
  ⇒ ⇒ 它们改为**陷入**（见「未知 opcode MUST 可观测…」那条 Requirement 的
  「非法指令（`reason ∈ {2,3}`）—— 陷入而非冻结」）
- **AND** 该指令 MUST NOT 进入 EX、MUST NOT 退休

#### Scenario: 组件级既有契约 MUST 保持

- **WHEN** `stall = 1` · `flush = 1` · `branch_valid = 0`（流水线内不可达的组件级场景）
- **THEN** PC MUST 冻结（**不改动**既有钉住的判据）
