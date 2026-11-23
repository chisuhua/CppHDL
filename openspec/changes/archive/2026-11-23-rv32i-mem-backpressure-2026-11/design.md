# Design: RV32I MEM 级反压

## 1. 现状（实测拍级事实）

`Rv32iPipeline` 的流水线寄存器链（ADR-069 A0 之后）：

```
IF/ID ──► ID/EX ──► EX ──► EX/MEM ──► M2 ──► MEM/WB ──► WB
  ▲        ▲                 ▲         ▲       ▲
if_stage  id_stage       exmem_*    m2_*   memwb_*
(stall)   (stall=气泡)
```

| 寄存器组 | 装载条件（现状） | 出处 |
|---|---|---|
| IF/ID | `if_id_update_en`（含 `!stall` / `!flush`） | `stages/if_stage.h` |
| ID/EX | `idex_update_en = io().valid`；`idex_bubble = flush \| stall` ⇒ **装气泡** | `stages/id_stage.h:253/274` |
| EX/MEM | `exmem_en = id_stage.io().valid_out`（**不因 stall 冻结** · ADR-069 V4） | `rv32i_pipeline.h:541` |
| M2 | **无条件**（ADR-069 V4） | `rv32i_pipeline.h:293` |
| MEM/WB | **无条件**（ADR-069 V4） | `rv32i_pipeline.h:605-619` |

**`data_ready` 的现状**：`__io` 有该输入（`:69`），但 `describe()` 的三个 helper
（`build_datapath` / `build_control_signals` / `build_forwarding_paths`）**从不读它**
⇒ 它是**接了线但没落地**的死输入。

## 2. 设计

### 2.1 反压源

```cpp
auto mem_stall = !io().data_ready;   // 无条件：spec 的原文是「上游拉低它时」
```

**为什么无条件（不看 `mem_valid`）**：spec 的 Requirement 是「上游拉低**就绪信号**时，
MEM 级 MUST 冻结」，没有「仅当 MEM 级有访存」这个前提。若加 `& mem_valid` 就会引入
「无访存时不反压」的隐含豁免 ⇒ 那是**新语义**，超出 spec。而且无条件版本更简单、
更可测（任意拍拉低都可观测冻结）。

### 2.2 `freeze`：与 `stall` 分离（**核心**）

**为什么不能复用 `IdStage.io().stall`**：

| 语义 | 行为 | 谁需要 |
|---|---|---|
| `stall` | ID/EX **装气泡**（`idex_valid←0`）+ IF/ID 冻结 | load-use（消费者留在 ID 级，load 正常前进） |
| `freeze` | ID/EX **保持**（含 valid） | MEM 反压（EX 级那条指令不能丢） |

若反压复用 `stall`：EX 级指令在 ID/EX 装气泡时被**丢弃**，而 EX/MEM 已冻结
⇒ 该指令**永远消失**（既不执行也不退休）⇒ **静默丢指令**。
⇒ ⇒ 必须新增 `freeze` 输入。

改动（`id_stage.h`，**3 行**）：

```cpp
auto idex_update_en = io().valid & !io().freeze;         // freeze ⇒ 不更新
auto idex_bubble    = (io().flush | io().stall) & !io().freeze;  // freeze 优先级最高
idex_valid->next = select(io().freeze, idex_valid,
                          select(io().flush | io().stall, ch_bool(false),
                                 select(idex_update_en, io().valid, idex_valid)));
```

数据字段的 `select(idex_update_en, 新值, 保持)` 在 `freeze` 时 `update_en=0`
⇒ 自动**保持**，**无需逐行改**；控制字段的 `select(idex_bubble, 清零, …)` 在 `freeze`
时 `bubble=0` 且 `update_en=0` ⇒ 同样**保持**。⇒ ⇒ 只改 3 行。

### 2.3 冻结链（`rv32i_pipeline.h`）

| 组 | 改法 |
|---|---|
| EX/MEM「清零型」字段（`valid` / `is_store` / `is_load`） | `select(mem_stall, 原值, select(exmem_en, 新值, false))` ⇒ 反压**保持**，气泡仍清零 |
| EX/MEM 其余字段 | `exmem_en` 改为 `… & !mem_stall` ⇒ 现有 `select(exmem_en, 新, 保持)` 自动冻结 |
| M2（8 个） | `select(mem_stall, m2_X, exmem_X)` |
| MEM/WB（8 个） | `select(mem_stall, memwb_X, m2_X)` |
| IF/ID | `if_stage.io().stall <<= hazard.io().stall \| … \| mem_stall` |
| ID/EX | `id_stage.io().freeze <<= mem_stall`（新增驱动） |

⚠️ **`EX/MEM` 的「清零型」字段必须显式分两路**：现有 else 分支是 `ch_bool(false)`
（气泡时清零，防「残留 store 标记 ⇒ 静默内存误写」）。
若只把 `exmem_en` 置 0 而不加 `mem_stall` 外层 ⇒ 反压时 `is_store` 会被**清零** ⇒
解除反压后该 store 变成 NOP（**静默丢写**）。⇒ ⇒ 这类字段 MUST 用
`select(mem_stall, 保持, select(exmem_en, 新, false))` 两层。

### 2.4 手推 stall 拍数

反压语义是「**冻结**」（不是脉冲）⇒ 反压 N 拍 ⇒ 流水线停 N 拍：

**口径（两种 run，结论相反，别混）**：
- **跑完**（拍数足够）⇒ 退休数**相同**（反压只让序列晚到）
- **固定拍数**（本次测试用）⇒ 退休数**差 N**（少跑 N 拍）

| 量（固定拍数 T） | 无反压 | 反压 N 拍 | 差值 |
|---|---|---|---|
| `perf_cycles`（`cycle_count` 每拍 +1） | T | T + N | **+N** |
| 退休数 `perf_instructions` | R | R（解除后继续，最终同为 R） | **0**（同一程序） |
| `perf_load_use_stalls` | S | S | **0**（分档，AC6） |

⇒ ⇒ AC5 的判据 = `perf_cycles` 增量 **恰为 N**（手推）+ 去掉驱动则失败（覆盖自证）。

### 2.5 组合环路检查

`mem_stall` 驱动 `if_stage.io().stall` / `id_stage.io().freeze` / `exmem_en` /
`m2_*` / `memwb_*`。这些全部是**寄存器装载条件**（`->next` / 沿上更新），
**不进入**任何当拍的组合输出通路（`data_addr` / `data_write_en` 取自 `exmem_*`
寄存器 ⇒ 反压时保持不变）。
⇒ ⇒ **无组合环**。⚠️ 唯一需要确认的是：`data_ready` 是**纯输入**（来自 TCM 的
`sread` 输出寄存器）⇒ 它的驱动源是**寄存器**，不是本组件的组合输出 ⇒ 环被打断。

## 3. 为什么**不**加 `perf_mem_stalls` 端口

ADR-069 `:176` 的原文是「T3 的反压**不得**混进该计数器 ⇒ ⇒ T3 独立立项时 MUST
**另立计数器或显式分档**」⇒ ⇒ 两个选项中本 change 选 **显式分档**：

- `perf_load_use_stalls` 的驱动**一字不改**（仍 = `hazard.io().stall` ⇒ 只计 load-use）
- 反压拍数**不计入任何**现有计数器
- **分档的机械判据** = AC6（反压期间 `perf_load_use_stalls` 不增）⇒ ⇒ 不是口头约定

**代价（显式登记）**：反压拍数**不可观测** ⇒ ⇒ 接总线后若出现性能问题，
需要临时加端口或走 `--trace`。⇒ ⇒ 接受理由：**加端口会改核的 34 端口面**
（⇒ C6 快照 / 34 端口契约表 / `verify_riscv_soc_core_ports.sh` / SoC 产物
全部连带变更）⇒ ⇒ 与 S3「总线口一次成型」撞车 ⇒ ⇒ **推迟到 S3**（那时核的端口面
本来就要大改）。

## 4. ADR-069 V4 的口径限定（**需要登记**）

V4（`:214`）：「**全部**新寄存器（M2 + MEM/WB′）的装载使能恒定（不随停顿冻结）
⇒ 变异：让它们随停顿冻结 ⇒ 必红（否则死锁）」

本 change 的实现**在 `mem_stall` 时冻结 M2 / MEM/WB** ⇒ ⇒ **看起来撞 V4**。三者关系：

1. ADR-069 `:134` 明文 **T3 不在本 ADR 范围**（「本 ADR 零 T3 内容」）
2. 所以 V4 的「停顿」= **load-use 停顿**（ADR 范围内唯一的停顿源）
3. 本 change **没有**改变 load-use 下的行为（`mem_stall=0` ⇒ 冻结条件不成立
   ⇒ M2 / MEM/WB 照常前进）⇒ **V4 的意图完整保留**

⇒ ⇒ **不构成决策变更**，但 **V4 的判据表述**（「不随**停顿**冻结」）在新实现下
有歧义 ⇒ ⇒ 建议 **ADR-069 加 dated addendum**（architect 的活，本 change 不单方面改）：
「V4 的『停顿』限定为 **load-use 停顿**；T3 反压（`rv32i-mem-backpressure-2026-11`）是
显式例外 ⇒ ⇒ 变异判据 MUST 用 load-use 场景触发」。
⇒ ⇒ 本 change 的 `tasks.md` 含「检查 V4 判据是否被新实现打红」这一条（实测：
V4 是 ADR 的手动 checkbox 判据，**仓内无自动护栏** ⇒ 不会误红）。

## 5. 风险

| 风险 | 处置 |
|---|---|
| **丢指令**（反压时 ID/EX 装气泡而非冻结） | 2.2 节的设计直接消解（`freeze` 分离）；AC3/AC4 抓 |
| **静默丢写**（EX/MEM 的 `is_store` 在反压时被清零） | 2.3 节的两层 `select`；AC4 的写回序列比对抓 |
| **死锁**（load-use 期间也冻结 M2 ⇒ load 永不写回） | `mem_stall` 与 `hazard.stall` 是两个独立源；AC7 的 24 判据 + dual parity 抓 |
| **组合环** | 2.5 节；`data_ready` 源是寄存器 |
| **`IdStage` 新增端口未驱动**（读到未定义值） | 全部 5 个实例化点显式驱动（`rv32i_pipeline.h:141` + 4 个测试）；`verify_input_port_additions.sh` 会列出 |

## 6. 产物影响（实施期实测 · 已回写 proposal AC7）

**核侧产物字节会变**（`flat` 实测 400 行 diff）：

- `mem_stall = !io().data_ready` 的节点挂在**真实端口** `data_ready` 上
  ⇒ ⇒ Verilator **不做常量折叠**（「核里接恒 1」是**接线**，不是常量表达式）
  ⇒ ⇒ 新增的 `not` / 多个 `select` mux 会留在产物里（当前恒 0）。
- `top_pipeline_unnamed_input_*` 等内部 wire 的编号会整体位移 ⇒ diff 看起来很大，
  但**顶层端口表零变化**（34 根 · 名 / 方向 / 位宽 / 顺序逐字不变 —— 已用端口名集 diff 验证）。

⇒ ⇒ **这不是死代码**：`data_ready` 是真输入，S3 接 AXI4 主口后它会真的动。
⇒ ⇒ 因此 AC7 从「sha256 逐字节不变」**订正**为「端口表不变 + 行为零回归」。

## 7. 同族计数器（critic SHOULD_FIX #1 的实质缺陷 · 已修）

`perf_instructions` 的重复计数（`mem_valid_out` 冻结为 1 ⇒ 每拍 +1）**不是孤立**的：

| 计数器 | 驱动源 | 反压时为何会污染 |
|---|---|---|
| `perf_branch_count` | `id_stage.is_branch`（ID/EX 寄存） | freeze 保持 1 ⇒ 每拍 +1 |
| `perf_control_stalls` | `ex_stage.branch_taken & if_stage.valid & hazard.flush` | 三者冻结后稳定为 1 ⇒ 每拍 +1 |
| `perf_branch_mispredict` | 同上 & `id_stage.is_branch` | 同上 |
| `perf_load_use_stalls` | `hazard.stall` | load 卡在 EX ⇒ stall 持续 1 ⇒ 反压拍被记成 load-use |

⇒ ⇒ 四处**全部**加 `& !mem_stall`（纯 load-use 场景 `mem_stall=0` ⇒ 行为不变）。

**判据的判别力**（变异自证，已实测）：把三处门控去掉 ⇒
`test_rv32i_mem_backpressure` 的「同族计数器」用例转红
（`branch=1 ctrl=1 mispred=1` 相对 base 0）⇒ 还原后复绿。

⚠️ **反压起点 MUST 选在「EX 级确有分支」的拍**：若反压开始时 EX 级是气泡，
气泡驻留 ⇒ 三个计数器本来就不增 ⇒ 判据**平凡成立**（首次实现时踩到，变异验证曾仍全绿）。
