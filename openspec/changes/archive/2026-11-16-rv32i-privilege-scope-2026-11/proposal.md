# Proposal: `rv32i-privilege` 的**范围声明**与**判据修订**（oracle 5 条 SHOULD_FIX）

## Why

新 capability `rv32i-privilege`（2026-11-16 · G5）经 **oracle 对抗复核**：**0 BLOCKER** ·
**5 条 SHOULD_FIX** ⇒ ⇒ 全部是「**范围未登记**」与「**判据写法不可判**」两类 ⇒ ⇒
**可执行性缺口**（不是缺陷）⇒ ⇒ 本 change 一次性带完（一个轻量 change）。

| # | SHOULD_FIX | 处置 |
|---|---|---|
| 1 | **排除族未登记** ⇒ ⇒ `mtval` / `mtvec` 取值语义 / `mstatus` 陷阱侧更新 / `MRET` 恢复 / 并发异常排队 ⇒ ⇒ 读者**有理由默认已覆盖** ⇒ ⇒ 归档后 proposal 的 Out of Scope **那层登记就丢了** | ✅ Requirement **正文**加**范围声明**（逐条列出「不在覆盖内」+ 理由）⇒ ⇒ 并**订正组件头的假注释**（`rv32i_exception.h` 原写「mstatus 恢复由 CSR Bank 在 MRET 时自动处理」⇒ ⇒ **实测不成立** ⇒ ⇒ `CsrBank` 的 `mstatus` 只有写口） |
| 2 | **与冻结语义的接缝未声明** ⇒ ⇒ 未来接线时「流水线冻结（终态）」与「组件级陷阱入口（有恢复）」会被当成同时活跃 ⇒ ⇒ **双头裁决** | ✅ 正文加**接缝声明**：本 capability **不改变** `rv32i-isa-semantics` 的冻结 Requirement ⇒ ⇒ 接线时 MUST 以 **MODIFIED delta** 修订它（**触发信号已登记**） |
| 3 | Scenario 4 的「不串配」条款在**恒定**激励下**恒真空** ⇒ ⇒ 判别力为零 | ✅ 激励改为「**第 2 拍起改** `exception_cause` 与 `current_pc`」⇒ ⇒ 期望改为「第 2 拍的 `mcause` MUST 是**上一拍锁存**的值」⇒ ⇒ **可判**（变异：写入源改成当前输入 ⇒ ⇒ 转红） |
| 4 | Scenario 3 的负向条款（「MUST NOT 在每一拍都指向同一个 CSR」）窗口**无界** + 只判别**旧缺陷的精确形状** | ✅ 改为**正向两拍**写法：「陷阱当拍 MUST 指向 `CSR_MEPC` · 其后那一拍 MUST 指向 `CSR_MCAUSE`」⇒ ⇒ 2 拍窗口 ⇒ ⇒ **任何**观察者可判 ⇒ ⇒ 对旧形态同样转红 |
| 5 | `## Purpose` 是归档工具留下的**占位符** | ⚠️ **CLI 产物**：delta 的合并语义**只覆盖 `## Requirements`** ⇒ ⇒ `## Purpose` **无法**经 change 修改 ⇒ ⇒ **如实登记**（不手改主 spec ⇒ 违反根 `AGENTS.md` 的「spec 变更必须走 change」纪律）⇒ ⇒ 留给下一个触碰本 capability 的 change 顺带填 |

## Out of Scope

- **实现面**：本 change **零实现改动**（只改 spec 判据 + 一处**注释**订正）
- **特权级本体**（CSR 指令 / `ECALL` / `EBREAK` / `MRET` / 中断 / trap 入口接线）⇒ ⇒ 独立立项
- **`## Purpose`** ⇒ ⇒ 见上表 #5（CLI 产物 ⇒ 如实登记）

## 验收标准（可勾选）

- [x] 范围声明覆盖 oracle 点名的 **5 项**排除（`mtval` / `mtvec` / `mstatus` / `MRET` / 并发排队）
- [x] 接缝声明写明「接线时 MUST 以 MODIFIED delta 修订冻结 Requirement」
- [x] Scenario 3 改为**正向两拍**写法（2 拍窗口 ⇒ 任何观察者可判）
- [x] Scenario 4 的激励含「**第 2 拍起改值**」⇒ ⇒ 配对条款**可判**
- [x] 组件头的**假注释**已订正（`mstatus` 恢复**当前不存在**）
- [x] 判据的**变异自证**通过：「写入源改成当前输入」⇒ ⇒ 2 个用例转红；还原 + **重建** ⇒ 复绿
- [x] `scripts/verify_openspec_delta.sh` 全绿 · `openspec validate --specs` 绿 · 归档合并成功

## 用户可见变更

| 面 | 变更 | 迁移动作 |
|---|---|---|
| **capability `rv32i-privilege`** | 唯一一条 Requirement 的**正文与判据**改写：① 加**范围声明**（`mtval` / `mtvec` 取值语义 / `mstatus` 陷阱侧更新 / `MRET` 恢复 / 并发异常排队 ⇒ ⇒ **不在覆盖内**）② 加**与冻结语义的接缝声明**（本 capability **不改变** `rv32i-isa-semantics` 的冻结 Requirement ⇒ ⇒ 接线时 MUST 以 MODIFIED delta 修订它）③ Scenario 3 改**正向两拍**写法 ④ Scenario 4 补「配对不串」的**可判**形态 | ⚠️ **Scenario 标题保留**（openspec MODIFIED 语义要求同名）⇒ ⇒ **判据以正文为准** |
| **`riscv::ExceptionUnit`** | **零行为变更**（本 change **零实现改动**）⇒ ⇒ 仅**组件头注释**订正（原写「mstatus 恢复由 CSR Bank 在 MRET 时自动处理」⇒ ⇒ **实测不成立** ⇒ ⇒ 该行为**当前不存在**） | 依赖「`mstatus` 会被自动恢复」的读者 MUST 改为「**当前不存在** ⇒ 属特权级立项」 |
| **capability 清单** | `rv32i-privilege` 仍是**种子**（1 条 Requirement）⇒ ⇒ 特权级本体未立项 | — |
