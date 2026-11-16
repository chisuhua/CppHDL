# `rv32i-privilege`（新 capability）+ G5 缺陷修复 —— **审稿记录**

> **日期**：2026-11-16 · **角色**：architect（build agent）
> **对象**：`openspec/specs/rv32i-privilege/spec.md`（**1** Requirement / **4** Scenario）·
> `include/cpu/riscv/rv32i_exception.h`（两拍写入 FSM）·
> `examples/riscv-mini/tests/test_csr_unit.cpp`（复合 DUT + 2 新用例 + 既有 case 6 的口径变更）
> **两个归档 change**：`2026-11-16-csr-mcause-write-2026-11`（缺陷修复 + 种子 Requirement）·
> `2026-11-16-rv32i-privilege-scope-2026-11`（范围声明 + 判据修订）
> **触发**：improvement `rv32i-isa-gap-closure-2026-11` 的 **G5**（`ExceptionUnit` 写缺陷修复）

---

## 1. 审稿链（architect 固定配对：oracle → critic）

| 轮 | 结论 | findings | 处置 |
|---|---|---|---|
| **oracle 第 1 轮** | **APPROVE**（0 BLOCKER） | **5 SHOULD_FIX** | **5/5 全部采纳** ⇒ 由第二个 change（`rv32i-privilege-scope-2026-11`）带完 |
| **critic 第 1 轮** | **APPROVE**（`mergeable: yes`） | 0 BLOCKER · 3 NIT | NIT 1（Purpose 占位符）= **CLI 产物** ⇒ 已登记；NIT 2（`当次`措辞）⇒ 测试已钉死语义 ⇒ 不改；NIT 3（接缝登记只在候选立项表里）⇒ ⇒ 登记为 **4c 的显式动作** |

---

## 2. oracle 的 5 条 SHOULD_FIX 与处置

| # | finding | 处置 | 载体 |
|---|---|---|---|
| 1 | **排除族未登记** ⇒ `mtval` / `mtvec` 取值语义 / `mstatus` 陷阱侧更新 / `MRET` 恢复 / 并发异常排队 ⇒ ⇒ 读者**有理由默认已覆盖**（且归档后 proposal 的 Out of Scope 那层登记**就丢了**） | ✅ Requirement **正文**加**范围声明**（逐条列出 + 理由）⇒ ⇒ 并**订正组件头的假注释**（「mstatus 恢复由 CsrBank 在 MRET 时自动处理」⇒ ⇒ **实测不成立** ⇒ ⇒ `CsrBank` 的 `mstatus` 只有写口 ⇒ ⇒ 该行为**当前不存在**） | `rv32i-privilege` spec 正文 · `rv32i_exception.h` 注释 |
| 2 | **与冻结语义的接缝未声明** ⇒ ⇒ 未来接线时「流水线冻结（终态）」与「组件级陷阱入口（有恢复）」会被当成同时活跃 ⇒ ⇒ **双头裁决** | ✅ 正文加**接缝声明**：本 capability **不改变** `rv32i-isa-semantics` 的冻结 Requirement ⇒ ⇒ 接线时 MUST 以 **MODIFIED delta** 修订它（**触发信号已登记**在 improvement 的候选立项表 + 路线图 §7 批 4c） | spec 正文 · `.rddf/improvements/…` · `phase-6-next-steps.md` |
| 3 | Scenario 4 的「不串配」条款在**恒定**激励下**恒真空** ⇒ ⇒ 判别力为零 | ✅ 激励改为「**第 2 拍起改** `exception_cause` 与 `current_pc`」⇒ ⇒ 期望改为「第 2 拍的 `mcause` MUST 是**上一拍锁存**的值」⇒ ⇒ **可判**（变异：写入源改成当前输入 ⇒ ⇒ **2 个用例转红**） | spec Scenario 4 · 测试 |
| 4 | Scenario 3 的负向条款（「MUST NOT 在每一拍都指向同一个 CSR」）窗口**无界** + 只判别**旧缺陷的精确形状** | ✅ 改为**正向两拍**写法：「陷阱当拍 MUST 指向 `CSR_MEPC` · 其后那一拍 MUST 指向 `CSR_MCAUSE`」⇒ ⇒ **2 拍窗口** ⇒ ⇒ **任何**观察者可判（含未来流水线集成方） | spec Scenario 3 |
| 5 | `## Purpose` 是归档工具留下的**占位符** | ⚠️ **CLI 产物**：delta 的合并语义**只覆盖 `## Requirements`** ⇒ ⇒ `## Purpose` **无法**经 change 修改 ⇒ ⇒ **如实登记**（**不**手改主 spec ⇒ 违反根 `AGENTS.md` 的「spec 变更必须走 change」纪律）⇒ ⇒ 留给下一个触碰本 capability 的 change 顺带填 | 第二个 change 的 proposal/tasks |

---

## 3. critic 的三点判定（逐条）

| 判定 | 结论 | 依据 |
|---|---|---|
| ① 跨 capability / 跨 ADR 冲突 | ✅ **通过** | 冻结 Requirement 裁的是**流水线级**行为，本 capability 通篇限定**组件级** ⇒ 作用域不重叠；接缝的**触发信号**已登记在 spec 之外的**可执行位置**（improvement 候选立项表 + 路线图批 4c）⇒ ⇒ **不是死声明**；对 `cpu-clint-timer` / `rv32i-commit-path` / `rv32i-canonical-home` 全文 grep `exception|trap|ExceptionUnit|exception_valid` ⇒ **零命中** ⇒ 无重叠、无重复裁决 |
| ② 判据与实现的一致性 | ✅ **通过**（**未发现假判据**） | critic **手工推了 `trap_phase` 状态机 5 拍** ⇒ 与测试断言**逐拍吻合**；S3 的「顺序反变异」（`mcause` 先于 `mepc`）⇒ ⇒ 转红 ⇒ ⇒ 不是「恒绿」；S4 的「配对不串」在实现里有**精确**对应物（`cause_latched` 只在 `start_trap` 拍采样） |
| ③ 两个 change 的职责切分 | ✅ **通过（不构成过度切分）** | 第二个 change 由 oracle 对第一个的复核结论驱动；**在第一个已归档的前提下**，OpenSpec 的 MODIFIED 机制**不允许** retroactive 修改 ⇒ ⇒ 「并入」在工具语义上**不可能** ⇒ ⇒ 第二个 change 是**唯一合法路径**；归档 history 里「种子 ADDED → 同日 MODIFIED 修订」的形状是**复审闭环的标准形态**（本仓有先例）⇒ ⇒ **不建议任何动作** |

---

## 4. 判别力（变异矩阵 · 两次）

| 变异 | 期望 | 实测 |
|---|---|---|
| ① 还原旧形态（`write_mcause = do_trap_entry` ⇒ ⇒ `mcause` 分支不可达） | 复合 DUT 用例**转红** | ✅ **1 个转红**（`mcause` 读回恒 0） |
| ② 写入源改成**当前输入**（不锁存） | 「配对」断言**转红** | ✅ **2 个转红**（拍 2 的 `mcause` 成了改动后的 7） |
| ③ 还原 + **重建**（二进制也是状态） | 全绿 | ✅ **14 用例 / 56 断言全绿** |
| ④ ⚠️ **无效变异（如实登记）** | — | 「`cause_latched` 改成每拍跟随当前输入」⇒ ⇒ **行为等价**（该寄存器只在 `finish_trap` 拍被读 ⇒ ⇒ 读到的是**上一拍**锁存的值）⇒ ⇒ **对写口零差异** ⇒ ⇒ **无判别力** ⇒ ⇒ 如实登记（体例同「无效变异已如实登记」） |

---

## 5. 遗留（**不在**本轮）

- **`## Purpose` 占位符** ⇒ CLI 产物 ⇒ 下一个触碰本 capability 的 change 顺带填。
- **接缝的兑现点**：特权级立项（**4c**）的 change **MUST** 同时修订
  ① `rv32i-isa-semantics` 的**冻结 Requirement** ② 本 capability 范围声明里登记的**全部 5 项**排除
  （`mtval` / `mtvec` / `mstatus` / `MRET` / 并发排队）⇒ ⇒ 建议把这条**接缝**复制进新 improvement 的
  **Acceptance**（critic NIT 3）⇒ ⇒ 避免候选立项表被后续整理时**漂移掉**。
- **`funct3` / `funct7` 非法组合维度**（G1 的 residual）⇒ ⇒ 批 4b（**行为变更** ⇒ ⇒ golden MUST 手推重算）。
