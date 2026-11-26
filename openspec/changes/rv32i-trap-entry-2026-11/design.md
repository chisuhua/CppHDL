# design · RV32I **trap 入口接线**（P6-3）

> **Change**：`rv32i-trap-entry-2026-11` · **母 improvement**：`rv32i-privileged-csr-2026-11`（P6-3）
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> ⇒ ⇒ **零端口面变更**（核 IP 顶层 53 端口契约表 + C6 快照**不动**）

---

## 0. 三条**硬约束**

| # | 约束 | 出处 | 对本设计的含义 |
|---|---|---|---|
| **C1** | ⚠️ **父层 MUST 读「子组件输出端口」或「本层落地网」** ⇒ ⇒ 读**子组件的输入端口**会产生**不可映射的跨模块边** | P6-2 实施期实测（ADR-060 §2.1 · ADR-066 D1） | ⇒ ⇒ 所有需要跨组件读的信号（ID 级 PC / IF 级 PC / CSR 读值）MUST 取**输出端口**侧 |
| **C2** | ⚠️ **`CsrBank` 只有一条写口** ⇒ ⇒ 第二个写源（`ExceptionUnit`）MUST 有**仲裁** | `rv32i_csr.h`（单 `csr_addr` / `csr_wdata` / `csr_write_en`） | ⇒ ⇒ 仲裁规则 MUST 写死（§4） |
| **C3** | ⚠️ **`HazardUnit` 端口面冻结在 4 个直通端口** | 根 `AGENTS.md:256` | ⇒ ⇒ 本项**不**动 `HazardUnit`（trap 判据落顶层） |

---

## 1. 载体与拓扑（**零新增端口**）

```
Rv32iPipeline（顶层）
├── csr_bank   : ch::ch_module<CsrBank>      ← **已有**（P6-2 引入）· 状态**唯一**
└── exception  : ch::ch_module<ExceptionUnit> ← **本项新增**（inline ⇒ 不新增层）
```

- ⚠️ **`ExceptionUnit` MUST 与 `CsrBank` 共享同一实例** ⇒ ⇒ 它**不能**自己再实例化一个
  `CsrBank` ⇒ ⇒ 父层（pipeline）**连线**：`ExceptionUnit` 的 CSR 读输入 ← `csr_bank` 的
  **输出端口**（`mstatus_value` / `mtvec_value` / `mepc_value` / `mie_value` / `mcause_value`
  —— **`CsrBank` 已有的 5 个输出**，**零改动**）；`ExceptionUnit` 的 CSR 写输出 → **仲裁** → `csr_bank` 写口。
- ⇒ ⇒ **零新增端口**：`ExceptionUnit` 的 `__io` **一字未动**；`CsrBank` 的 `__io` **一字未动**；
  `if_stage` **+2 输入端口**（`trap_target` / `trap_flush`）⇒ ⇒ **被顶层消费 ⇒ 不冒泡**
  ⇒ ⇒ 顶层 53 端口**不变**（AC8）。

### 1.1 `ExceptionUnit` 的**既有**语义（复用，不重写）

| 语义 | 载体 | 备注 |
|---|---|---|
| **两拍写**（拍 1 写 `mepc` = `current_pc` · 拍 2 写 `mcause` = **拍 1 锁存**的 cause） | `rv32i_exception.h:149–186` | P6-1（G5）已闭 ⇒ ⇒ **同一次异常**的成对写入 |
| trap 目标 = `mtvec & ~3`（Direct 模式） | `rv32i_exception.h:110–116` | ⚠️ `mtvec` **取值语义**归 D-1（OUT O5） |
| `mret_pc` = `csr_mepc` · `mret_valid` = `mret_instruction` | `rv32i_exception.h:193–197` | ⚠️ 本项 `mret_instruction` 接 **0**（P6-5） |

---

## 2. 触发点与 kill 语义（三条**具名不变量** · 承自母 improvement）

> 母 improvement `rv32i-privileged-csr-2026-11.md` §「P6-3 的三条具名不变量」。

1. **`pc_update_en` 新表达式**（含 `trap_redirect`）⇒ ⇒ 优先级表见 §3；
2. `trap` ⇒ ⇒ **IF/ID 均被杀**，**EX/MEM/WB 不受影响**（在途的**更老**指令继续退休）；
3. ⚠️ **`trap_flush` 也 MUST 有 `trap_flush_d1`**（imem 是 1 拍 `sread` ⇒ ⇒ 多杀一拍 ⇒ ⇒
   与 `if_stage.h` 的 `flush_d1` **同构**）· **`skid` MUST 在 `trap_flush | trap_flush_d1` 拍清空**
   （ADR-048 D5 的同构要求）。

### 2.1 触发点的**精确表达式**（承重）

```
illegal_active = (id_stage.io().illegal_reason != 0)          // 既有
exception_valid = illegal_active & id_valid & !flush            // ⚠️ 必须 !flush
```

- ⚠️ **`!flush` 是承重条款**：taken 分支的 **fall-through 槽**里可能有一条非法字
  （ADR-071 §2.3 记录的**可达轨迹**）⇒ ⇒ 若不排除 `flush`，`ExceptionUnit` 会在
  `flush` 同拍 `start_trap` ⇒ ⇒ **用错误路径的 PC 写坏 `mepc`** ⇒ ⇒ 判据 = **AC5**。
- ⚠️ `id_valid` = **ID 级的有效位**（`if_stage.io().valid`）⇒ ⇒ 复位期 / 气泡期不触发。
- ⚠️ `id_stage.io().illegal_reason` **已被 `id_stage` 用 `valid` 门控**（`id_stage.h:340`）
  ⇒ ⇒ `id_valid` 这一项在 `illegal_reason` 上**已隐含** ⇒ ⇒ 仍**显式**写出（可读性 + 防后续改动打破）。

### 2.2 `mepc` = 触发指令的 PC

- 取 **`if_stage.io().pc`**（= IF/ID 的 PC，**ID 级那条指令**的 PC）⇒ ⇒ ⚠️ **不是**
  `id_stage.io().pc_out`（那是 **EX 级**视图 ⇒ ⇒ 会指向**后一条**指令）。
- ⚠️ **C1**：MUST 读 `if_stage` 的**输出端口**（`id_stage.io().pc` 是**子组件输入端口** ⇒
  读它 = 跨模块边 ⇒ ⇒ 层次化发射失败）。
- ⇒ ⇒ `mepc` 指向**触发指令自身** ⇒ ⇒ 该指令**不前进**、**不退休**（与 P6-4 的
  「`ECALL` 的 `mepc` 指向该指令」**同型**）。

---

## 3. PC 重定向：**第二路**（trap / mret 内部 mux）

> ⚠️ 母 improvement：**不是「第三路 PC 源」** —— 新增的 redirect **源有两个**
> （trap 与 mret），它们在 `if_stage` **内部**先 mux 成**一路**，再与 branch 路做选择。

### 3.1 `if_stage` 的 PC 选择（**三路** · 优先级 = **trap > branch > PC+4**）

```
trap_target_in  = select(mret_valid, mret_pc, trap_entry_pc)   // 内部 mux（mret 本项恒 0）
next_pc_target  = select(trap_valid, trap_target_in,
                   select(branch_valid, branch_target, pc_plus_4))
```

### 3.2 同拍仲裁表（**逐行写死** · 承自母 improvement）

| `branch_redirect`（taken） | `trap` | `mret` | 谁赢 | 本项 |
|---|---|---|---|---|
| 1 | 0 | 0 | branch（现状） | ✔ 不变 |
| 0 | 1 | 0 | **trap** | ✔ 本项 |
| 0 | 0 | 1 | mret | P6-5 |
| **1** | **1** | 0 | ⚠️ **按母 improvement 的建议 = trap**（陷阱优先于错误路径上的分支） | ✔ 本项采用（**登记为假设**：母 improvement 已写明默认建议 ⇒ ⇒ 非新决策） |
| 0 | 1 | 1 | ⚠️ **不发生**（`mret` 与 trap 语义互斥）⇒ ⇒ **MUST NOT** 断言它 | — |

- ⚠️ **本项** `mret_valid` **恒 0** ⇒ ⇒ 第 3/5 行**不可达** ⇒ ⇒ 只实现第 1/2/4 行。

### 3.3 `pc_update_en`（**与既有 `flush_redirect` 同构**）

```
flush_redirect = flush & branch_valid          // 既有
trap_redirect  = trap_flush & trap_valid        // 本项（trap_flush = exception_valid）
pc_update_en   = !rst & !(stall & !(flush_redirect | trap_redirect))
```

⇒ ⇒ **trap 与 flush 同权**（都能压过 stall）⇒ ⇒ 否则「stall 压过 redirect ⇒ redirect 被丢掉」
⇒ ⇒ **重定向被丢掉** ⇒ ⇒ 非法指令的 trap 目标**永远去不了**（= P6-2 的冻结换了个地方）。

### 3.4 `trap_flush_d1` + skid 清空（承不变量 3）

```
trap_flush_d1->next = trap_flush                      // 与 flush_d1 同构
if_id_update_en     : MUST 含 `trap_flush | trap_flush_d1`
skid_valid 清空条件 : MUST 含 `trap_flush | trap_flush_d1`
```

---

## 4. `CsrBank` 单写口的**仲裁**（C2 · 承重）

### 4.1 两个写源

| 写源 | 何时 | 写什么 | 拍 |
|---|---|---|---|
| **CSR 指令**（P6-2） | EX 级 | `csr_addr` / `csr_wdata` / `csr_write_en` | 1 拍 |
| **`ExceptionUnit`** | trap 触发后 | `mepc`（拍 1）· `mcause`（拍 2） | 2 拍 |

### 4.2 仲裁规则（本项选：**`ExceptionUnit` 优先**）

```
csr_write_en = exception_write_en | (csr_instr_write_en & !exception_write_en)
```

- **理由**：trap 上下文是「**一次异常**」的**成对**写入（`mepc` + `mcause`）⇒ ⇒ 丢它
  ⇒ ⇒ **软件永远看不到原因** ⇒ ⇒ 比「一条 CSR 指令的写被吞」更严重。
- ⚠️ **冲突概率分析**（实测推导）：trap 在 **ID 级**触发 ⇒ ⇒ trap 那一拍 ID/EX 装气泡
  ⇒ ⇒ **trap 后的第 1/2 拍 EX 级是空的**（IF/ID 被杀 ⇒ ID 级无指令）⇒ ⇒
  **实际不会同拍** ⇒ ⇒ 规则仍**必须写死**（fail-closed：将来 trap 目标很近时会撞上）。
- ⚠️ **已知边界**：若真发生冲突，被抑制的是**那条 CSR 指令的写** ⇒ ⇒ 该指令**已执行**
  但其 CSR 写丢失 ⇒ ⇒ 记为**已知边界**（本版**不**加队列/重试机制 ⇒ ⇒ 归 **P6-6**
  的写口仲裁统一处理 —— 母 improvement 的 **R1** 已登记「`ExceptionUnit` 接入时 MUST 补仲裁规则」）。

### 4.3 写地址/写数据的选择（C2 的另一半）

`CsrBank` 的 `csr_addr` 端口只有**一个** ⇒ ⇒ 三个来源（CSR 指令的地址 / `mepc` / `mcause`）
MUST **先在父层 mux 成形**，再驱动 `csr_bank.io().csr_addr` / `csr_wdata` / `csr_write_en`。

---

## 5. `mret` 通道（留空但接通 · S8）

- `exception.io().mret_instruction <<= ch_bool(false)`（**P6-5** 才拉高）
- `exception.io().mret_pc` / `mret_valid` **不被消费** ⇒ ⇒ ⚠️ 它们是**子组件输出端口**
  ⇒ ⇒ 若**完全不被读**，会不会冒泡？⇒ ⇒ **会**（未消费的 `ch_out` 冒泡）⇒ ⇒
  ⇒ MUST **消费**它们（读到即可 ⇒ ⇒ 不冒泡）⇒ ⇒ 本项用 `mret_valid` 参与 PC mux（§3.1）
  ⇒ ⇒ **天然被消费** ✔（`mret_valid` 恒 0 ⇒ mux 恒取 trap 路 ⇒ ⇒ 行为零变化）。

---

## 6. 判据设计（**逐条手推** + **每条一条变异**）

### 6.1 观测面（**零新增端口**）

| 要观测的量 | 载体 | 备注 |
|---|---|---|
| `mepc` / `mcause` 的**寄存器值** | `pipeline.csr_bank.instance().io().mepc_value` / `.mcause_value` | ⚠️ `csr_bank` 是 pipeline 的 **public 成员** ⇒ ⇒ 判据可直接取 ⇒ ⇒ **不碰**顶层端口 |
| 退休计数 | `obs_perf_instructions`（既有） | |
| ID 级有效位 / PC | `dbg_id_valid` / `dbg_id_pc`（既有） | |
| PC 重定向结果 | `dbg_id_pc` 的**后续序列**（既有） | ⇒ ⇒ 判据 = trap 后 ID 级的 PC 变成 `mtvec` base |

### 6.2 判据清单（AC1–AC7 ↔ 测试用例）

| AC | 判据形态 | 变异（⇒ MUST 转红） |
|---|---|---|
| AC1 | 非法指令 ⇒ `mepc` == 该指令 PC · `mcause` == 2 | ① `mepc` 写成 `pc + 4` ② `cause` 写成 11 |
| AC2 | 触发指令**不退休** + IF/ID 被杀 | 去掉 `trap_flush`（⇒ 非法指令继续**冻结**在 ID ⇒ 不退休但 IF/ID 未被杀 ⇒ **判据必须能区分**） |
| AC3 | PC 重定向到 `mtvec & ~3` | 用 `mtvec` 原值（未清模式位）⇒ 差 0/2 字节 |
| AC4 | 在途更老指令**照常退休** | 把 `trap_flush` 误接成「杀全部级」 |
| AC5 | `flush` 同拍**不**触发 trap | 去掉 `!flush`（⇒ 错误路径的 PC 写坏 `mepc`） |
| AC6 | trap 后**连续两拍** ID 级 `valid == 0` | 去掉 `trap_flush_d1` |
| AC7 | `mepc` + `mcause` **成对**且同配对 | 改写 `ExceptionUnit` 的两拍语义（**组件级**可判） |

### 6.3 ⚠️ **固件级判据的**限制**（如实登记 · 与 D-1 相关）

- ⚠️ **`mtvec` 复位值 = `0x2000_0100`** ⇒ ⇒ 该地址**不在** I-TCM 的映射区间
  （`0x8000_0000` + 16 KB）⇒ ⇒ 但取指地址是 `instr_addr[13:2]`（**12 位** ⇒ **高位被丢弃**）
  ⇒ ⇒ **实际取到的是 I-TCM 的 word 0x040**（= 第 64 个字）⇒ ⇒
  **「trap 目标不可达」在**行为上**不成立**（它是**别名**跳转）⇒ ⇒
  ⚠️ **母 improvement 的 D-1 表述（「落在 TCM 外 ⇒ ⇒ 静默重启」）需订正**（本项实测）。
- ⇒ ⇒ **固件级判据**（真跑一条非法指令）**是可能的**：在镜像的 word 64 放一条停机指令
  ⇒ ⇒ 但**它验的是「别名跳转」**，**不是**「`mtvec` 语义正确」⇒ ⇒ 后者归 **D-1**。

---

## 7. ⚠️ **开工前置（架构面 · architect 域）**

> ⚠️ 本项**打破** ADR-048 的「**flush 唯一来源**」不变量 ⇒ ⇒ 该不变量 MUST 被**修订**
> ⇒ ⇒ **ADR 文档属 architect 写集** ⇒ ⇒ **builder MUST NOT 单方面改 ADR 决策内容**
> ⇒ ⇒ ⇒ 本项**先立项**（builder 可创建 openspec change）⇒ ⇒ **ADR dated addendum 落档**
> 是**实施的前置**（tasks T2）。

### 7.1 dated addendum **草案**（**待 architect 落档** · 供其直接采用）

> **载体**：`docs/adr/ADR-048-*.md`（**architect 写集** · builder **不写**）
> **形式**：**dated addendum**（先例 = ADR-071 v1.2 · 2026-11-15）

**D1（新增不变量 · 取代「flush 唯一来源」）**：取指级的 PC 重定向有**两类**来源：
① **控制流重定向**（taken 分支 / 跳转）⇒ ⇒ 由 `flush` 承载；
② **陷阱重定向**（trap / mret）⇒ ⇒ 由 `trap_flush` 承载。
⇒ ⇒ 两类**互斥**（同一拍只可能有一个生效 ⇒ ⇒ 仲裁表见 design §3.2）⇒ ⇒
**`flush` 唯一来源**的不变量**由「来源唯一」改为「来源互斥 + 优先级写死」**。

**D2（`pc_update_en` 的形态）**：`pc_update_en = !rst & !(stall & !(flush_redirect | trap_redirect))`
⇒ ⇒ **任何**「确实发生重定向」的情形都**压过 stall** ⇒ ⇒ 与既有 ADR-071 修正同型。

**D3（`trap_flush_d1` 与 skid）**：`trap_flush` MUST 有 `trap_flush_d1`（imem 1 拍 `sread`）
⇒ ⇒ `if_id_update_en` 与 `skid_valid` 的清空条件 MUST 含 `trap_flush | trap_flush_d1`
⇒ ⇒ 与 `flush_d1` **逐字同构**（ADR-048 D5）。

**D4（kill 域）**：trap 只杀 **IF/ID** ⇒ ⇒ **EX/MEM/WB 不受影响**（在途更老指令照常退休）⇒ ⇒
与 `flush` 的 kill 域**不同**（`flush` 杀 ID/EX + IF/ID）⇒ ⇒ **两条 flush 语义 MUST 分开命名**
（`flush` vs `trap_flush`）⇒ ⇒ **MUST NOT** 把两者合成一个信号。

---

## 8. 实施期订正（**追加段** · 实施时填）

（暂无 —— 实施开始后追加）
