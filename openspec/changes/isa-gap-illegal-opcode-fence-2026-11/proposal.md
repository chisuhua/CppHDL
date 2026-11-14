# Proposal: RV32I 未知 opcode 可观测 + 冻结 · FENCE 显式化（B4 · G1+G3+G4）

## Why

实测（2026-11-10 登记 · 2026-11-15 复核仍成立）：

1. **未识别的 opcode 被静默当作 NOP 逐条退休** ⇒ ⇒ 程序跑飞到非法指令时**零报错、
   零计数器、零波形信号** ⇒ ⇒ 只能靠二分固件定位（调试成本极高）。
2. `FENCE` / `FENCE.I`（opcode `0x0F`）**完全不在译码表**里 ⇒ ⇒ 与「未知 opcode」
   **混为一谈** ⇒ ⇒ 无法区分「我打算实现但还没实现」与「这根本不是指令」。
3. I-TCM 在程序末尾之后读到的是 `ch_mem` 初值 **0** ⇒ ⇒ 那本身是**未知 opcode**
   ⇒ ⇒ 「跑出程序尾部」这一事实**完全不可见**（实测：既有 golden 的 176 条退休里
   约 76 条来自跑出固件尾部的 0 字）。

## What Changes

| 单元 | 内容 |
|---|---|
| **G3（先）** | **装载器把程序尾部填成 NOP**（`0x00000013`）⇒ ⇒ 尾部不再产生未知 opcode |
| **G1** | ID 级产出 `illegal_reason`（2 bit：`0` 合法 · `1` **已知未实现** = `OP_SYSTEM` · `2` **未知 opcode**）⇒ ⇒ 判据 MUST 含 `io().valid` |
| **G2（最小面）** | ① SoC 顶层引出 `dbg_illegal_reason`；② **行为策略 = 冻结**（IF/ID 同时 stall ⇒ PC 不前进、ID/EX 装气泡 ⇒ 该指令**永不退休**） |
| **G4** | 译码表加 `OP_FENCE = 0x0F` ⇒ ⇒ **已知 = NOP**（单核无 cache ⇒ 无排序语义；`FENCE.I` 同理）⇒ ⇒ **不**产生 `illegal_reason` |

### 为什么 G3 必须**先**（顺序由 critic 第 2 轮改向确定）

G1 单独落地 ⇒ 流水线在固件尾部冻结 ⇒ 退休数 **176 → ~102** ⇒ golden 改一次；
随后补尾部填充 ⇒ 冻结不再触发 ⇒ 退休数回到 176 ⇒ **golden 改两次**。
⇒ ⇒ 故 **G3 先、G1+G4 后**（`git log` 可验）。

## Out of Scope（MUST 独立立项）

- **CSR 指令**（`CSRRW/CSRRS/...`）· **`ECALL` / `EBREAK` / `MRET`** · **中断** ·
  **trap 入口**（`mtvec` / `mepc` / 流水线 flush）⇒ ⇒ 本项只把它们**分类**为
  「已知未实现」（`illegal_reason = 1`），**不实现**其语义
- `funct3` / `funct7` 的非法组合（本版**只覆盖 opcode**）⇒ ⇒ 已登记为 residual
- `ExceptionUnit` 的 `mcause` 写缺陷（G5）⇒ ⇒ 本项不做

## 验收标准（可勾选）

- [x] **退休数不变**：`test_rv32i_soc_dual_parity` 的 `non_retired == 23` 与
      `retire == kCycles - 24` 两条断言**原样通过**（G3 的算术：NOP ≡ 0 字）
- [x] **未知 opcode ⇒ 冻结**：新用例三个场景（未知 / `OP_SYSTEM` / `FENCE`）× 双后端
      ⇒ ⇒ 未知与 `OP_SYSTEM` 场景 `retire` **恒为 8**（探针指令永不退休）·
      `dbg_id_pc` 停在探针 PC（手推 `0x80000020`）
- [x] **FENCE 是负向对照**：`illegal_reason == 0` 且 **不冻结**（`retire == 16`）
- [x] **`valid` 门控**：判据含 `io().valid`（flush 后的残留旧值不得触发冻结）
- [x] 无组合环（`illegal_reason` 的来源是 ID 级**输入**；`if_stage.valid` 是**寄存**值）
- [x] `ctest -LE perf` **254/254** · 全部 `scripts/verify_*.sh` PASS
- [x] 独立 Verilog TB `checks=24 fails=0`
