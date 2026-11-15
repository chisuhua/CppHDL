# Proposal: 冻结与 `flush` 同拍时 MUST 以**重定向优先**（ADR-071 审稿修正）

## Why

`rv32i-isa-semantics` 的「未知 opcode ⇒ 冻结」Requirement 落地后（2026-11-15 · B4 · G1+G4），
**ADR-071 的 oracle 对抗式复核查出一个真缺陷**：

- 冻结是本仓**第二个** stall 来源 ⇒ ⇒ 旧互斥论证（「`flush` 由 EX 产生 ⇒ 那拍 EX 不是 load
  ⇒ load-use 不可能同拍」）**只覆盖 `hazard.stall`** ⇒ ⇒ **`flush` 与冻结可以同拍**。
- 同拍时原实现 `pc_update_en = !rst & !stall` ⇒ ⇒ **stall 无条件压过 redirect** ⇒ ⇒
  ① **重定向被丢掉**（PC 被冻住）② 下一拍 `valid = 0` ⇒ **冻结只活 1 拍**
  ③ 非法字被静默跳过 + `illegal_reason` 回落到 0
  ⇒ ⇒ **「跑飞零信号」被 G1 自己造出来**（且两后端同构 ⇒ parity 结构性失明）。

## What Changes

`if_stage` 的 PC 更新门：`!rst & !stall` ⇒ ⇒ **`!rst & !(stall & !(flush & branch_valid))`**
⇒ ⇒ **`flush`（重定向）优先于冻结 stall**，但**仅当确有重定向**（`flush & branch_valid`）。

- 与 `id_stage` 的 `idex_bubble = flush | stall` **对齐**（`flush` 本来就优先造气泡）。
- **语义正确性**：`flush` 只来自 EX 级的分支/跳转 ⇒ 同拍 ID 级持的是该分支的 **fall-through**
  （= 错误路径）⇒ 杀掉错误路径上的非法字**正确**；**正确路径**上的非法字**不可能**与 `flush` 同拍
  ⇒ 冻结在正确路径上仍是**终态**。
- **组件级既有契约零改动**：「`flush` 但无重定向」（`branch_valid = 0`）在流水线内**不可达**
  ⇒ 仅当 `flush & branch_valid` 才让 `flush` 优先 ⇒ `tests/test_rv32i_pipeline_behavior.cpp`
  Batch 4-a 的「`stall=1 ∧ flush=1 ∧ branch_valid=0` ⇒ PC 冻结」原样通过。

## Out of Scope

- 真 trap 语义（`mtvec` 接入）= 特权级独立立项（ADR-071 §3 选项 D）。
- `funct3`/`funct7` 非法组合 = residual（ADR-071 D6）。

## 验收标准（可勾选）

- [x] **V9**：`flush` ∥ 非法字 ⇒ ① 重定向兑现（分支目标 `addi x5,x0,1` 的写回出现）
      ② 非法字（`rd=31`）**未被执行**（无 x31 写回）③ `illegal_reason == 2` 恰在碰撞拍出现并回落 0
      ⇒ ⇒ 载体 = `test_rv32i_soc_dual_parity` 的 `Rv32iSoc - flush wins over illegal-freeze stall`
- [x] **变异自证**：反转 `flush_redirect`（回退到 stall 优先）⇒ `REQUIRE(saw_x5)` **转红**（实测）
- [x] **V10**：组件级既有契约（`stall=1 ∧ flush=1 ∧ branch_valid=0` ⇒ PC 冻结）**原样通过**
- [x] `ctest -LE perf` **254/254** · 独立 Verilog TB `checks=24 fails=0`
- [x] `if_stage.h` 的旧互斥论证注释（事实错误）已重写

## 用户可见变更

| 面 | 变更 | 迁移动作 |
|---|---|---|
| **行为（控制流）** | `flush`（分支/跳转重定向）与「未知 opcode 冻结」**同拍**时 ⇒ ⇒ **重定向优先**（此前 redirect 被丢掉） | 无需动作（这是**修复**：此前该组合下分支目标被跳过） |
| **行为（冻结域）** | 冻结只覆盖**正确路径**上的非法字；被 `flush` 杀掉的**错误路径**非法字照常被丢弃 | 无需动作 |
| **端口面** | **不变**（无新增/删除端口） | 无需动作 |
