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
// ⚠️ **触发集 = `illegal_reason ∈ {2,3}`**（ADR-048 §8.2 DA5 · oracle + critic 复核后钉死）
//    ⇒ `reason = 1`（`OP_SYSTEM` 的 `funct3 = 0` ⇒ ECALL/EBREAK/MRET/WFI）**维持冻结**
//    ⇒ ⇒ `illegal_active`（!= 0）**继续只服务 stall**（ID/EX 装气泡）
exception_valid = ((id_stage.io().illegal_reason == 2) | (id_stage.io().illegal_reason == 3))
                   & id_valid & !flush                          // ⚠️ 必须 !flush
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
- ⚠️ **冲突窗口 = 拍 T 本身**（⚠️ **订正**：原判「trap 后的第 1/2 拍 EX 级是空的 ⇒ 实际不会同拍」
  **是错的** —— `ExceptionUnit` 的 `csr_write_en` 在 trap **同拍**就拉高（`start_trap` 是**组合**量
  ⇒ ⇒ 沿上即写）⇒ ⇒ **拍 T** 的 EX 级**仍持有更老指令 J** ⇒ ⇒ 若 J 是 **CSR 写指令**，
  它的 CSR 写与 `mepc` 写**同拍冲突** ⇒ ⇒ ⇒ **仲裁规则（`ExceptionUnit` 优先）是承重的**，
  不是「理论上不会发生」⇒ ⇒ （trap **之后**的第 1/2 拍 EX 级才是空的 ⇒ 那两拍无冲突。）
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
| **AC12 / V9** | `reason = 1`（`ECALL`）⇒ ⇒ **仍冻结**（**不** trap） | 把触发集写成 `illegal_active`（`!= 0`）⇒ ⇒ `ECALL` 也 trap |

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

### 7.1 ADR-048 dated addendum（**已落档** · 权威在 ADR）

⚠️ **权威 = `docs/adr/ADR-048-rv32i-fetch-skid-buffer.md` 的 **§8（dated addendum v1.1 · 2026-11-26）****
⇒ ⇒ 本节**不再复制**其正文（**避免两份漂移**）⇒ ⇒ 实现时 MUST 以 ADR 的
**DA1–DA5**（决策）+ **V1–V9**（判据）+ **S1–S5**（同步义务）为准。

- **审稿**：oracle（REJECT · 5 条 finding 全处置）→ critic（**APPROVE_WITH_CONDITIONS** · 0 BLOCKER）
  ⇒ ⇒ 报告 = `.rddf/reviews/ADR-048-pc-redirect-second-path-review-2026-11.md`
- **连带**：`docs/adr/ADR-071-fence-and-illegal-opcode-semantics.md` 的 **v1.3** 标注
  （D3「冻结是终态」对 `reason ∈ {2,3}` 失效 · D3-b 的优先级公式被 DA2 扩展）

## 8. 实施期订正（**追加段** · 2026-11-26 填）

### 8.1 判据的 DUT 从「流水线级」改为「**核级**」（**实测订正**）

§6.2 原设想直接实例化 `riscv::Rv32iPipeline` 并把镜像**组合**喂给 `instr_data`。
**实测不成立**：imem 是 **1 拍 `sread`**（ADR-041）⇒ ⇒ 取指数据比取指地址**晚 1 拍**
⇒ ⇒ 组合喂数会让**取指与 PC 错开一个字** ⇒ ⇒ 译码到的指令与 `dbg_id_pc` **不对应**
⇒ ⇒ 判据全红，且**测的不是产品形态**。
⇒ ⇒ **改为核级 DUT**（`rv32i_core_top.h` + I-TCM 构造参数装载）⇒ ⇒ 取指对齐由**产品**保证
（与 P6-2 的 `test_rv32i_csr_instructions.cpp` 同型）。

### 8.2 ⚠️ ADR-048 §8 DA3 的 **② / ③** 两个 skid 站点**互相掩护**

- 「填充排除」（③）在 trap 拍**是否生效**，只在 **skid 队满**时被 `!skid_valid` 挡住
  ⇒ ⇒ 若 trap 拍 skid **非空**，③ 的变异**不可观测**；
- 「清空」（②）的变异会被 **`trap_flush_d1` 的清空**兜住（第二拍必清）。

⇒ ⇒ **判据的排程 MUST 让 skid 在 trap 拍为空**（先填 → trap 拍清 → trap+2 才重新填），
两条变异的判别力才各自可观测 ⇒ ⇒ 落点 = `tests/test_rv32i_pipeline_behavior.cpp` 的
**Batch 5-f**（变异 **M9** = ③ · **M9b** = ②，二者**各自**转红）。

### 8.3 ⚠️ `trap_flush_d1` 的漏杀在**核级不可观测**（如实登记）

漏掉 `trap_flush_d1`（kill 站点 ①）⇒ ⇒ 在途指令会进 IF/ID ⇒ ⇒ **pc↔指令的配对**错一个字。
但 `mepc`（= 触发指令的 PC）**仍正确**、handler **仍执行** ⇒ ⇒ 核级观测面（写回序列 +
`dbg_id_*`）**看不出配对错误**（本核**没有**「读当前 PC」的 CSR：`auipc` 可行但会新增耦合）。
⇒ ⇒ 判别力**归组件级** Batch 5-e（**M8**）—— 与 ADR-048 §8 **D9.4** 的定位一致。

### 8.4 ⚠️ **语义变更的爆炸半径**（3 条既有判据的期望值过时 ⇒ 已订正）

「冻结 → 陷入」是**语义变更**（capability `rv32i-isa-semantics` 的 **MODIFIED** delta 已写死）
⇒ ⇒ 三条**既有**判据的期望值因此过时（**判据变更，不是行为回归**）：

| # | 用例 | 订正 |
|---|---|---|
| 1 | `test_rv32i_csr_instructions.cpp` 的 `funct3=4 ⇒ reason 3` | 只留**译码级**判据；「陷入」归 `test_rv32i_trap_entry.cpp`；镜像 MUST 在别名落点放**自环** |
| 2 | `test_pipeline_wrapper.cpp` 的 `PC advances after reset release` | 填 **8** 个 NOP（未初始化 = 0 = 非法 ⇒ 会 trap ⇒ 实测 `instr_addr = 0x20000108`） |
| 3 | `test_rv32i_soc_dual_parity.cpp` 的 `UnknownOpcodeFreezes + FenceIsNop` | 拆 `freezes` 两类（`reason = 1` 仍冻结 · `{2,3}` 陷入）· `retire(20)` 8 → **13** |

⚠️ **第 3 条的手推曾被实测打红**：初版推 `retire` 恒 8（假设落点 word `0x040` 读到 0 ⇒ 再陷入）
⇒ ⇒ 实测 **13** ⇒ ⇒ 根因 = `load_firmware` 补 **256** 个尾部 NOP ⇒ 落点是 NOP ⇒ `8 + 5 = 13`。
⇒ ⇒ **手推 MUST 以「固件装载的真实内容」为前提**（含 `kTailFillWords`），不能只看镜像数组。

### 8.5 追加判据：**单写口仲裁的同拍冲突**（AC7 的直接判据）

原 §6.2 的 AC7 只由「`mepc` / `mcause` 成对」间接覆盖 ⇒ ⇒ **同拍冲突**（EX 级是
CSR 写指令 + ID 级触发 trap）**没有直接判据** ⇒ ⇒ 追加用例：
`addi` → `csrrw x0, mscratch, x1` → 非法字 ⇒ ⇒ 处理器读 `mscratch` MUST 得**复位值 0**
（CSR 指令的写被抑制）· `mepc` MUST = 非法字的 PC。
⇒ ⇒ 变异 **M12**（让 CSR 指令优先）⇒ 该用例转红 ✔
