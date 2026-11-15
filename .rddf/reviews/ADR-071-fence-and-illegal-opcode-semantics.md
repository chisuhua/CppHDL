# ADR-071 审稿记录（oracle ×1 → critic ×1）

- **ADR**: `docs/adr/ADR-071-fence-and-illegal-opcode-semantics.md`
- **日期**: 2026-11-15
- **审稿顺序**: **oracle 先行 → critic 后行**（§审查配对 的 architect 行：新建 ADR ⇒ 双审，oracle 先行）
- **最终判定**: critic = **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**

---

## 1. oracle（第一审 · 对抗式复核）

**判定**: **REJECT**（1 个**真缺陷** + 4 条 SHOULD_FIX）

### 1.1 真缺陷（已修）

**命题**：`flush`（分支/跳转重定向）与「未知 opcode 冻结」**同拍**时，原实现会**丢掉 redirect**。

**失败轨迹**（oracle 给出 · 逐条可核）：

| 步 | 事实 |
|---|---|
| ① | 冻结是本仓**第二个** stall 来源（`rv32i_pipeline.h`：`stall = hazard.stall | illegal_active`）⇒ ⇒ 旧互斥论证只覆盖 `hazard.stall` |
| ② | 原 `if_stage` 的 PC 更新门 = `!rst & !stall` ⇒ ⇒ **stall 无条件压过 redirect** |
| ③ | 碰撞拍：`flush = 1`（EX 级 taken 分支）∧ `illegal_active = 1`（ID 级 fall-through 是非法字）⇒ ①' PC 被冻住（redirect 丢）②' `flush` 当拍清 `if_id_valid_reg` ⇒ 下一拍 `valid = 0` ⇒ **冻结只活 1 拍**（不是终态） |
| ④ | 净结果：**redirect 永久丢失** → 分支目标之后的位置继续跑 → 非法字被**静默跳过** → `illegal_reason` 回落 0 → 退休数照常增长 ⇒ ⇒ **「跑飞零信号」被 G1 自己造出来**（两后端同构 ⇒ parity 结构性失明） |

**为何 V1–V7 全绿也抓不到**：新用例固件是 8 NOP + 探针 + NOP，**固件里没有分支** ⇒ 碰撞路径结构性不可达。

**修法（已落地）**：`pc_update_en = !rst & !(stall & !(flush & branch_valid))`
⇒ ⇒ **`flush`（重定向）优先于冻结 stall**，且**仅当确有重定向**。

**判别力证据（变异自证 · 实测）**：反转 `flush_redirect`（回退到 `select(io().stall, …)`）
⇒ ⇒ `REQUIRE(saw_x5)` **转红**（分支目标未执行）⇒ 还原 + 重建 ⇒ 复绿。

### 1.2 四条 SHOULD_FIX（已逐条采纳）

| # | 意见 | 处置 |
|---|---|---|
| S1 | D1 的等价性**承重假设**写错（「单核无 cache」既非充分也非必要）⇒ 需登记 4 条假设 + 重开触发器 | ✅ 已加 **A1–A4**（单 hart / in-order LSU / 无写缓冲 / **hart 不可写自己的取指空间**）+ 3 条重开触发器；并指出「接了 I-cache 本身**不**破坏等价（只读 cache 不产生排序问题）」 |
| S2 | §3 选项 D 的理由不精确（trap 组件**已在库内**） | ✅ 已订正为「`rv32i_exception.h` / `rv32i_csr.h` **已存在且有组件级测试** ⇒ 缺的是**接入本流水线**（第三路 PC 源 + `mepc`/`mcause` 副作用）= 特权级立项**本体**」 |
| S3 | V2 只在**终点**断言退休数恒定 ⇒ 放过「冻结 1 拍后静默解冻」 | ✅ **V9** 改为「`illegal_reason == 2` 恰在碰撞拍出现 + 最后回落到 0」+ 「重定向兑现」+ 「非法字未执行」三条**并行**判据 |
| S4 | ADR 未写明「`FENCE` 占一个退休槽」（V1 的 16 = 20−4 依赖它） | ✅ D1 末句已明写 |

---

## 2. critic（第二审 · 与既有 ADR / 契约的冲突面）

**判定**: **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**

### 2.1 对三个 focus 的判定（逐条）

| focus | critic 判定 |
|---|---|
| ① `flush ∥ 冻结` 优先级的三条论证是否成立 | ✅ **三条全部成立**。逐行核实：`id_stage.h:278` 的 `idex_bubble = flush \| stall` · `hazard_unit.h:273` 的 `branch_flush = is_branch & branch_taken`（条件分支与 JAL/JALR **同一条表达式** ⇒ 无第三类 flush 来源）· `if_stage.h:230` 的 IF/ID 装载要求 `!flush & !flush_d1` ⇒ 目标字最早在 flush 后第 2 拍进 ID ⇒ **正确路径的非法字不可能与 flush 同拍** |
| ② 「流水线内 `flush ⇒ branch_valid`」恒等式是否有反例 | ✅ **无反例**：`hazard.flush = ex_branch & ex_branch_taken`（`:273`）与 `branch_valid = ex_stage.branch_taken`（`:394`）**共享同一源节点**；复位拍 idex 为气泡 ⇒ `flush = 0`；`flush_d1` 拍 EX 已是气泡 ⇒ `branch_taken = 0` ⇒ ⇒ Batch 4-a 金标逐拍代入**新门**与原行为**逐拍一致** |
| ③ ADR 形态与残余风险 | ✅ D1 的 A1–A4 + 重开触发器充分 · D6（ISA 残留**不**进架构台账残差活账）的理由成立（`scripts/lib/ledger_residual.py:17` 把 `LEDGER_REL` 硬编码为层次化 RTL 台账） · 冻结终态代价已登记 |

### 2.2 SHOULD_FIX（已修）

- **V8 状态自相矛盾**：V8 行标「✅ 已做」，而 §5 末尾仍写「**V8 仍欠**」⇒ ⇒ 同一文档对同一验收项两个状态 ⇒ ✅ **已删陈旧句**，改为「V1–V10 全部已闭合」。

### 2.3 NITs（记录 · 不修）

| # | 意见 | 处置 |
|---|---|---|
| N1 | ADR 元数据头偏厚（AC 映射表 + 审稿详情内联 = 过程日志混入决策文档） | ⏸ **不修**：与仓内既有 house style 一致（ADR-070 的头部同为厚形态，含 AC 映射与审稿轮次）⇒ 按 critic 自己的例外条款豁免；后续若统一瘦身 ⇒ 作为**独立**的文档规范变更，不在本 ADR 顺手改 |
| N2 | 新门 `stall=1 ∧ flush=1 ∧ branch_valid=1 ⇒ PC 取目标` 只有 SoC 级**间接**覆盖 ⇒ 建议补 `IfStage` 组件级金标（3 拍） | ⏸ 记入 backlog（见下） |
| N3 | 建议补一个变异：「去掉 `& branch_valid` guard ⇒ Batch 4-a 应转红」 | ✅ **已实证**（但路径不同）：本轮实现过程中**第一次**的修法就是 `!stall \| flush`（= 无 guard）⇒ ⇒ `tests/test_rv32i_pipeline_behavior` Batch 4-a **当场转红**（`pc == 0x80000008` ≠ `0x80000004`）⇒ ⇒ 这证明 **4-a 确实钉住了该分支**（critic 预期的「若恒绿说明 4-a 没钉住」不成立） |

---

## 3. 结论与后续

- **ADR-071 可转 Accepted**（0 BLOCKER；唯一 SHOULD_FIX 已修；`mergeable: yes`）。
- **实现修复**已落地并全绿：`ctest -LE perf` **254/254** · 独立 Verilog TB `checks=24 fails=0`。
- **backlog（N2）**：`IfStage` 组件级的「`stall ∧ flush ∧ branch_valid` ⇒ PC 取目标」金标（3 拍金标），与 Batch 4-a 形成**正反对**。
