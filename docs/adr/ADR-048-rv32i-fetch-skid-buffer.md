# ADR-048: RV32I 取指的 skid buffer 与填充期语义 —— 裁决「停顿不丢指令」与「填充期不双装」

- Status: **Accepted**（proposed 2026-10-27；**2 轮 critic 对抗审稿** ⇒ 第 1 轮 REJECT
  （1 BLOCKER + 3 SHOULD_FIX + 2 NIT）⇒ 整改 ⇒ 第 2 轮 **APPROVE** ⇒ 见 §7.3）
- Date: 2026-10-27
- Author: build-agent（architect role）
- Parent: ADR-046（RV32I canonical home；**D10 = SoC 32 端口冻结**，本 ADR 的一处硬约束）、ADR-047（数据前推；**它让缺陷 B 从「计数偏差」升级为「正确性地雷」**，见 §1.3.2）、ADR-041（**imem MUST 用 `sread`**，1 拍读）、ADR-035（verilator 后端的 reset/clock 语义）
- 关联 OpenSpec change：`rv32i-fetch-skid-buffer-2026-10`（**待立项**，本 ADR 是其架构依据）
- 关联 objective / feature：`feat-rv32i-isa-completeness` ④、`.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` T5、`.rddf/improvements/rv32i-fetch-skid-buffer-2026-10.md`（5 段草案；**其 §2.1 第 2 条与 §2.2 的根因表述经本 ADR 纠正**，见 §6）
- 前置审查：本 ADR 的时序推导经 **oracle 独立复核**（9 处判断被修正，其中 2 处推翻了我的初稿 ⇒ 见 §7）
- ⚠️ **配置缺口**：`.rddf/config/arch-docs.json` **不存在** ⇒ 本 ADR 按 fallback 目录（`docs/adr` / `docs/architecture`）判定归属 ⇒ 该配置的建立**独立立项**（不阻塞本 ADR）

## 1. Context

### 1.1 触发

`feat-rv32i-isa-completeness` ④（`rv32i-fetch-skid-buffer-2026-10`）要收口 ISA 语义边界的**最后一项**：
取指侧的两个残留 —— 停顿丢指令（缺陷 A）与填充期双装（缺陷 B）。
两者都在 `openspec/changes/archive/2026-10-26-rv32i-pipeline-commit-path-2026-10/design.md`
§3.10 / §3.6 有实测记录，并在根 `AGENTS.md §KNOWN LIMITATIONS` 与
`openspec/specs/rv32i-commit-path/spec.md` 中如实登记。

③（数据前推，ADR-047）已落地 ⇒ 固件**不再需要**为「依赖间隔」插 NOP ⇒ 固件里残留的 NOP
**只剩**「load-use 停顿点之后那一条」⇒ 缺陷 A 是这条残留的**唯一**解法。

### 1.2 缺陷 A：load-use 停顿会**丢弃「消费者之后的第一条指令」**

**机制**（design.md §3.10，第 17 轮 critic 实测 + 逐拍 trace 双向确认）：

- imem 是 **1-cycle `sread`**（ADR-041 强制）⇒ 取指地址在第 t 拍提交、第 t+1 拍数据返回；
- 检测拍 T（EX 级是 load、ID 级是 W 的消费者）中，`pc_reg` 指向 `W+2`，
  而 `W+1` 的地址在 **T−1** 拍已提交 ⇒ `W+1` 的数据**恰好在 T 拍返回** ——
  正是 `if_id_update_en` 变 0 的第一拍（`if_stage.h:192-200`）⇒ **无处可存**；
- ⇒ 停顿解除后 IF/ID 锁存的是 `W+2` ⇒ **`W+1` 从指令流里消失**
  （其地址 `addr(W+1)` 不会再被提交 ⇒ 永不重读）。

**实测证据**（该 change 自己的 trace）：ID 级序列在停顿 #1 处是 word 21（保持）→ **word 23**
（word 22 从未进 ID）；停顿 #2 处 word 44 → **word 46**。
⚠️ **两处被丢的都是 NOP** ⇒ 全部 golden 仍绿 ⇒ 典型的「**绿 ≠ 对**」。

**当前边界（如实登记）**：固件 MUST 保证「load-use 停顿点之后的那条指令」是 NOP
（`rv32i-commit-path` capability 的 Scenario「停顿 MUST 只**延迟**退休 —— ⚠️ 当前有一处例外」
已把这个例外**写进主 spec**；根 `AGENTS.md` 亦登记）。

### 1.3 缺陷 B：取指填充期「同一条指令被装两次」

#### 1.3.1 现象

固件 word 0 **被写回两次**（连续两拍 `wb_write_en=1`，`rd` / `data` 相同）⇒ 幂等无害，
但 **退休数 +1** ⇒ 测试的退休数金标**含**这 +1。

#### 1.3.2 ⚠️ 严重性已被 ADR-047 **升级**（本 ADR 的新发现）

ADR-047 接上前推后，**第二条** word 0 会通过前推路径读到**第一条**的写回值
⇒ 若 word 0 **自依赖**（`rd == rs1`）⇒ 第二条**不是**幂等的 ⇒ 这是**正确性**地雷，
不再是计数偏差。

现有防线是测试里的 compile-time 护栏 `fwd_first_instruction_not_self_dependent`
（`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:1154-1166`）
⇒ **固件级巧合在挡一个正确性缺陷** ⇒ 这是「缺陷 B 必须随本 change 一并修」的**决定性理由**。

#### 1.3.3 ⚠️ 根因表述必须纠正（design.md §3.6 的措辞不准）

design.md §3.6 写「`ch_mem::sread` 的输出端口在**建图期**就被组合求值了一次 ⇒ 初值 =
`mem[addr(拍 0)]`」。**核实结果**：`Simulator` 构造只调 `initialize()`，它把 proxy 槽位
**分配为 0**（`src/simulator_init.cpp:63-66`）⇒ **没有**任何建图期求值；
sync 读端口只在 clk 高时求值（`src/ast/instr_mem.cpp:240`）。

真正的预填机制是 **`load_firmware` 的加载拍**（= 固件 word 数，每 word 一拍；
`test_rv32i_soc_dual_parity.cpp:509-523`：`ext_rst=1` 下逐拍 tick）：
每个 posedge 上 sread 端口照常工作 —— 解释器**无 reset 意识**；
Verilog 的 reset 分支只对框架 `default_reset` 生效，而 `ext_rst` 是 **fabric 端口**
（`examples/riscv-mini/src/rv32i_pipeline_top.h:136`），加载期间 `default_reset=0`
⇒ ⇒ **两后端在当前流程下对称地预填** proxy（= word 0）。

⇒ 结论不变（word 0 进两次 ⇒ 退休数 +1），但**机制**必须改写，否则实施者会去改**错的东西**
（例如去查建图期求值）。

### 1.4 两处必须先纠正的既有表述

| 既有表述 | 出处 | 纠正 |
|---|---|---|
| 「正解需要取指 skid buffer（1 级缓冲）+ **PC 回退**」 | design.md §3.10、plan T5、5 段草案 §2.1 | **PC 回退不需要**（见 D3 的完整论证）⇒ 三处 MUST 同步订正 |
| 「`ch_mem::sread` 的输出端口在**建图期**被组合求值」 | design.md §3.6、5 段草案 §2.2 | 见 §1.3.3（真实机制 = 加载拍的预填） |

## 2. Decision

### D1. 裁决：取指 MUST 有 **skid buffer**（缺陷 A 的正解）

`IfStage` MUST 在 IF/ID 之前挂一条 **1 级**的取指缓冲（skid buffer），用于保存
「停顿期间已到达、但 IF/ID 无法装载」的那条指令。

- 理由：缺陷 A 的根因是「**已提交未锁存**的数据无处可存」⇒ 必须有一个存放点；
- 形态：新增 3 个寄存器 + 1 个 valid 位（`skid_instr` / `skid_pc` / `skid_valid`）；
- **零新增组件**（与 ADR-047 的 mux 同一原则：复用既有流水线级，不引入新层级）。

### D2. skid 的填充 / 排空条件（精确）

- **填充**（把流上数据存进 skid）：
  `stall && instr_ready && !skid_valid && !flush && !flush_d1 && !rst`
  ⇒ 填充时 `skid_instr ← fetched_instr`、**`skid_pc ← fetch_pc_reg`**（**不是** `pc_reg`！见 D2.1）
- **排空**（把 skid 内容送进 IF/ID）：
  `!rst && !stall && skid_valid && !flush && !flush_d1`
  ⇒ **必须与 IF/ID 的实际装载同拍原子发生**（即 `if_id_update_en` 为真时才排空；
  该使能**含** `!rst`（`if_stage.h:192-200`）⇒ 复位拍不排空）
- **排空拍丢弃流上数据**（语义 MUST 写死，最易实现错）：
  排空拍 IF/ID 取的是 skid 内容 ⇒ 同拍到达的流上数据被**丢弃** ⇒ 这是**安全**的：
  `pc_reg` 在该拍**仍指向**那条指令（推进发生在边沿）⇒ 下一拍会被**重读**一次。
- **IF/ID 的取数源**：`skid_valid ? skid : 流上数据`（pc 同理）。

#### D2.1 `skid_pc` MUST 取 `fetch_pc_reg`（不是 `pc_reg`）

`fetch_pc_reg` 比 `pc_reg` 晚 1 拍（`if_stage.h:203`）⇒ 二者差一个 word（4 字节）。
若锁存 `pc_reg` ⇒ skid 里的指令与它的 pc **错 4 字节** ⇒ 分支目标算错
⇒ **与 `flush_d1` 缺陷同型** ⇒ MUST 进变异验证清单（D9.3）。

### D3. 明确**否决**「PC 回退」

**论证**（取代 design.md §3.10 与 plan T5 的「+ PC 回退」）：

1. 停顿 ⇒ `pc_reg` **冻结**（`if_stage.h:120-124` 的 `pc_update_en = !rst && !stall`）
   ⇒ 取指地址流在停顿期间**自我重复**（同一地址被反复读出，见 D3.1）；
2. ⇒ 落在 PC **之后**（= 已提交但 PC 已越过）的在途数据**至多一条**（D4）；
3. ⇒ skid 接住那一条 ⇒ 停顿解除后：IF/ID 先取 skid（= `W+1`），
   下一拍从正常流取（= 当前 PC 对应的那条 = `W+2`，因为 PC 仍冻结在 `addr(W+2)` 且数据被重读）
   ⇒ 序列 `W+1`、`W+2` 连续 ⇒ **无丢失、无重复**。

⇒ **PC 回退不需要**。**两种误实现形态 MUST 记录在案**（供实施者与审稿者识别）：

| 误实现 | 后果 |
|---|---|
| `PC ← skid_pc` | 把已从 skid 排空的 `W+1` **再取一次** ⇒ 制造出本 change 正要修的**双装**缺陷 |
| `PC ← skid_pc + 4` | 恒等于冻结的 `pc_reg` ⇒ **空操作** |

**附带收益**：不做回退 ⇒ PC 选择器仍是**两路**（`branch_valid` / `branch_target`）
⇒ `if_stage.h:84-103` 的不变量「flush 的唯一来源 ⇒ `pc_update_en` 不看 flush」
**原样保留**；反之，回退会给 PC 引入**第三路**源 ⇒ 恰好触发该注释自己预警的重审义务
（`if_stage.h:101-103`：「引入第三个 flush 来源前 MUST 改为『flush 时也更新 PC』」）。

#### D3.1 「同一地址被重复读出」在**两后端**都成立且等价

- **解释器**：`instr_mem_sync_read_port::eval()` 只在 clk 高时执行，用**边沿前**的地址
  （地址经**组合节点**传递 —— `rv32i_pipeline_top.h:112` 的 `bits<...>(instr_addr)` ——
  组合槽位在 clk 高相位不重算，`src/simulator.cpp:356-371`）⇒ 冻结地址 ⇒ 每拍同一值；
- **Verilog**：`always_ff @(posedge …) proxy <= mem[addr];`
  （`src/codegen_verilog.cpp:1255-1263`）⇒ 非阻塞语义 ⇒ 同样采边沿前地址 ⇒ 同一值。

⚠️ **已知差异（如实登记）**：Verilog 侧有 reset 分支（`else proxy <= 1'b0;`）而
**解释器的 sync 读端口根本没有 reset 输入**（`src/ast/instr_mem.cpp:194-198` 的 `init_port`
只收 clk/addr/enable/output）⇒ **潜在**的填充期分歧真实存在；
但它在**当前 SoC 流程下不可达**（`default_reset` 脉冲只发生在仿真前，此后 `default_reset=0`，
加载拍在两后端都重新预填 proxy）⇒ 对 skid 设计影响为零，但 MUST 保留为可观测的差异。

### D4. 深度 1 够用的不变式（**三条承重前提**）

**不变式**：任意时刻，落在 PC 之后的在途数据**至多一条** ⇒ 1 级缓冲足以覆盖。

**三条前提**（缺一则充分性不成立 ⇒ MUST 在实施中逐条核对）：

1. **装配只接 EX 级检测**：`rv32i_pipeline.h:506-511` 里 `mem_is_load` **故意不连接**
   ⇒ 停顿检测只在 EX 级发生；
2. **停顿保持拍与排空拍的 EX 都是气泡**（`id_stage.h:269,287`）⇒ 排空拍**不可能**有新的检测
   ⇒ 「排空 ⇒ 至少隔一拍才可能再填充」是**结构性保证**；
3. **排空优先于填充且同拍互斥**。

⇒ **余量为零但成立**：背靠背两次 load-use 停顿（含「消费者本身是 load」的 load→load 冒险）下，
第二次填充恒在第一次排空**之后** ⇒ skid 不会溢出。

⚠️ **作用域声明**：该保证**不覆盖**「独立使用 `HazardUnit` 并驱动 `mem_is_load`」的场景
（那条路只有单拍组合 stall、无 hold 链，`hazard_unit.h:266-273`）⇒ **本不变式的作用域
= 当前流水线装配**。

### D5. flush × skid：**当前不可达**，但 MUST 写成**防御性契约**

**不可达性推导**：skid 非空 ⟺ 处于停顿窗口（停顿保持拍或排空拍）⇒ 那些拍 EX 级全是气泡
⇒ **无分支** ⇒ **无 flush** ⇒ 「flush ∥ skid_valid」在当前微架构下**不可达**。
同理，填充条件排除 `flush | flush_d1` 在当前微架构下也**不可达**
（停顿首拍的 EX 是 load，不是分支）。

**但 MUST 写成防御性契约**（理由：可达性论证依赖多重耦合前提，而 `if_stage.h:100-103`
自己已预警「引入**第三个** flush 来源前 MUST 重审」⇒ 未来加第二个 flush 源时，
「flush 时清空 skid」是唯一的兜底）：

- **flush（含 `flush_d1`）拍 MUST 清空 `skid_valid`**；
- **填充条件 MUST 排除 `flush | flush_d1`**（否则会把**错误路径**的指令存进 skid，
  之后塞进 ID）；
- ⚠️ **验收 MUST 落到组件级**（D9.4）：流水线级不可达 ⇒ SoC 级变异验证**抓不到**
  「删掉清空逻辑」的变异 ⇒ 该防御逻辑否则是「看起来有保护」。

### D6. 缺陷 B 的修法：**`primed` 标志门控 IF/ID 装载**

**判据（决策门 1）**：能否在**不**改框架层的前提下表达「首个取指数据已就绪」
⇒ **能** ⇒ 缺陷 B 随本 change **一并消除**（退休数 −1，且消除 §1.3.2 的正确性地雷）。

**方案**：`IfStage` 内部新增 `primed` 标志（复位后首拍置 1），IF/ID 的装载 MUST 以
「`primed` 已置位」为前提：

- ⚠️ **判据 MUST 内生于 `IfStage`**：`instr_ready` **恒为 1**
  （`rv32i_tcm.h:58`）⇒ **不能**从 `instr_ready` 导出「已就绪」（它不携带信息）；
- **两种门控面**（MUST 在实施时选定并写进 change）：

| 方案 | 效果 | 对 `dbg_id_pc` 观测序列的影响 |
|---|---|---|
| **只门控 `valid`** | 幻影装载的**内容**仍在 IF/ID 寄存器里，但 `valid=0` ⇒ 不进流水线 ⇒ `idex_update_en = io().valid`（`id_stage.h:248`）⇒ ID/EX 不前进 ⇒ **无漏装** | 幻影装载的 pc 是 **0x80000000**（与真值相同 —— `fetch_pc_reg` 无条件跟 `pc_reg`，而 `pc_reg` 被 rst 冻结在 0x80000000）⇒ `dbg_id_pc` 仍显示 0x80000000（**与今天一致**），只是 `valid=0` |
| **门控全部三个装载** | IF/ID 三个寄存器都保持复位值 | cycle 1 的 `dbg_id_pc` 显示复位值 **0** ⇒ **既有逐拍观测序列会变** ⇒ 观测面 golden 扰动更大 |

⇒ **默认选「只门控 `valid`」**（观测面零扰动）；若实施时发现「只门控 valid」不足
（例如 `dbg_id_instr` 仍被下游用作有效信号），MUST 在 change 里记录并改选另一方案。

- **无新假 valid**：被压掉的幻影装载与下一拍的真实取指是**同一地址同一数据** ⇒ word 0 恰好进一次。

### D7. 框架层：**加 API**，不改 `sread` 现有语义（若 D6 判定需要）

- **形态**：为 `ch_mem` 的 sync 读端口提供**「尚未取指」的可观测判据**（例如
  `sread_valid()` 或等价的 valid 端口）⇒ **增量**（不改现有初值语义 ⇒ 不影响既有用户）；
- ⚠️ **MUST NOT** 改 `aread` 语义（0-cycle 组合读已与 Verilog `assign` 对齐；
  改它会波及 FIFO 的 `dout` 时序契约 —— `rv32i-commit-path` 与 `mem-read-timing` capability）；
- **影响面清单**（`sread` 的全部消费者，MUST 逐个回归）：
  imem（`rv32i_tcm.h` 的 `InstrTCM`）、`chlib::sync_fifo`、cache Sync 变体
  （`i_cache_sync.h` / `d_cache_sync.h`）、AXI 互连相关读口、perf 基准路径；
- ⚠️ **默认路径是 D6 的局部修法** ⇒ D7 仅在 D6 判定「局部不可行」时才启动。

### D8. 取指地址路径 MUST 保持**经组合节点**

⚠️ 解释器「采样边沿前地址」是**涌现性质**（组合槽位在 clk 高相位不重算），
**不是**显式锁存 ⇒ 若改造时把取指地址**直接从某个寄存器节点**连到 `sread`
（不经中间组合节点）⇒ 解释器会采到**边沿后**的值，而 Verilog 采边沿前值
⇒ **双后端 parity 分叉**（且是那种「interpreter 与 verilator 不一致」的最难查形态）。

⇒ ADR 写死：取指地址路径 MUST 保持经组合节点（如 `bits<…>()`）；
若实施中确需直连，MUST 补一条 parity 回归用例。

### D9. 验收判据（写死）

#### D9.1 判别性固件（**必需项**）

- load-use 停顿点之后 MUST 放一条**非 NOP**（例如 `addi`）⇒
  **未修前该指令消失**（写回序列少一条 / 后续 pc 错位）⇒ **修后必须出现**；
- 每个用例 MUST 带**反向期望**（`X_old != X_new`）且期望值**手工推导 + 注释留痕**。

#### D9.2 退休数与金标：**从 179 / 20 起算**（不是 188）

⚠️ **既有漂移（R4）**：`rv32i-commit-path` capability 的「退休指令数的推导」Scenario
（`openspec/specs/rv32i-commit-path/spec.md:270-278`）仍写 **188**（199 − 11），
而**测试**断言的是 **179**（199 − 20，`test_rv32i_soc_dual_parity.cpp:905,917`）
⇒ 该 Scenario 自己的 MUST「与测试断言**同义**」**当前就不满足**
（自 2026-10-26 的 JAL/JALR change 起就已脱节）⇒ **本 change 的 MODIFIED MUST 先修正基线**。

加 skid 后的变化**不是简单 ±1**：

- `+k`：被救回的指令**真实退休**（当前固件 k = 2 个被丢的 NOP）；
- `−1`：缺陷 B 若修（D6）⇒ 填充期的重复退休消失；
- 200 拍**固定窗口**的尾部截断 ⇒ 被救回的指令会把下游退休**整体后移**。

⇒ **更稳的锚点 = `non_retired` 的分段分解**（fill / stall / flush 各几拍）：
skid **不改**停顿拍数（仍 3）与冲刷拍数（仍 3）⇒ 停顿 / 冲刷段**不变**，
只有 fill 段随 D6 的修法变 ⇒ ADR 钉住**分段不变式**，而不只是总数。

#### D9.3 变异验证清单（每条先确认**变异后仍编译成功**）

| # | 变异 | 预期转红 |
|---|---|---|
| M1 | 去掉打拍式对齐：填充时锁 `pc_reg` 而非 `fetch_pc_reg`（D2.1） | 分支目标算错 ⇒ 转红 |
| M2 | 关掉排空 | 复现「丢指令」⇒ 转红 |
| M3 | 排空条件写反（`stall` 时排空） | 转红 |
| M4 | 排空**不**与 IF/ID 装载同拍原子（flush 拍也排空） | 组件级转红（D9.4） |
| M5 | 去掉 D6 的 `primed` 门控 | 退休数 +1 ⇒ 转红 |
| M6 | `primed` 门控错误地门控全部三个装载 | `dbg_id_pc` 序列变化 ⇒ 转红（⚠️ **以 D6 默认方案「只门控 `valid`」为前提**；若实施改选方案 2，M6 的 golden 前提失效 ⇒ 须改写该变异） |

#### D9.4 组件级负向对照（**必需**）

`ch_device<IfStage>` 组件级测试 MUST 覆盖**流水线级不可达**的组合：
**同时驱动 `stall` 与 `flush`** ⇒ 断言「flush 时 skid 被清空 / 不被填充」。

⚠️ 理由（D5）：SoC 级变异验证**抓不到**「删掉清空逻辑」的变异 ⇒ 该防御逻辑否则是
「看起来有保护」⇒ **组件级**是唯一的判别力来源。

- **宿主 target**：**`tests/test_rv32i_pipeline_behavior.cpp`**（既有 dual-backend target）
  ⇒ 该文件本就是「RV32I 微架构子组件的 SECTION-free dual 用例」容器（`IfStage` 属其 scope）
  ⇒ **`CTEST_TOTAL` 202 不变**、**不新建测试文件**；
- ⚠️ **MUST NOT** 改 `tests/test_rv32i_pipeline.cpp`（既有 interpreter-only baseline，见该
  `_behavior` 文件头的约定）。

#### D9.5 回归

`ctest -LE perf` 零回归 / 双后端逐拍全等（无掩码）/ 护栏脚本全 exit 0 /
`CTEST_TOTAL` 与 `EXAMPLES_TEST_SOURCES` **不变**（判别性固件进**既有** target）。

### D10. 规格锁同步（MUST 走 MODIFIED delta，**禁** `--skip-specs`）

- **`rv32i-commit-path`** —— MODIFIED：
  1. Scenario「停顿 MUST 只**延迟**退休（MUST NOT 丢弃指令）—— ⚠️ 当前有一处例外」
     ⇒ **去掉例外**，改为「停顿 MUST NOT 丢弃任何指令」+ skid 的契约；
  2. `## Purpose` 段的「⚠️ 已知边界」（`:33-36`）⇒ 改写；
  3. Scenario「退休指令数的推导 MUST 与测试断言同义」（`:270-278`）⇒ **先修 188 → 179 基线**
     （D9.2 的 R4），再叠加本 change 的新数字；
  4. ⚠️ **MODIFIED 会整体替换该 Requirement** ⇒ MUST 保留**原标题**与全部条目
     （`openspec validate` 规则：删改既有 Scenario 标题会被拒）；
- **`mem-read-timing`** —— **可能** MODIFIED（仅当 D7 启动）；
- **`verilator-rv32i-e2e`** —— **仅当** SoC 观测端口变化 ⇒ ⚠️ **ADR-046 D10 冻结 32 端口**
  ⇒ 需**先**改 D10（rdd-arch 职责）再实施（见 D12）。

### D11. 既有 compile-time 护栏与**去重掩码**的处置

⚠️ 决策门 4 要定位的护栏**不在 `scripts/`**，在**测试文件内**：

| 护栏 | 位置 | 处置 |
|---|---|---|
| `stall_drop_slots_are_nop` | `test_rv32i_soc_dual_parity.cpp:573-592` | ⚠️ 判别性固件要**故意**在停顿点后放非 NOP ⇒ 该 `static_assert` 会**编译失败** ⇒ MUST **删除或反转**（反转为「该位置 MUST **不是** NOP 且必须退休」）—— 与判别性固件**同 commit** |
| `fwd_stall_drop_slots_are_nop` | 同上 `:1134-1147` | 同上 |
| `fwd_first_instruction_not_self_dependent` | 同上 `:1154-1166` | D6 修缺陷 B ⇒ **删除**；若最终不修 ⇒ MUST **升格为 spec 义务**并扩大固件面（不能只守在 forwarding 判别固件上，见 §1.3.2） |
| **连续重复去重掩码** | 同上 `:941-945`（「连续重复 ⇒ 已知残留（幂等写回）」） | ⚠️ 该掩码是为**缺陷 B** 设计的（design.md §3.9 记录它曾掩盖 `(8,1)` 双写回）⇒ 缺陷 B 修复的**同一 change** MUST **移除它**，否则修复后它转而掩盖**真实**的双写回回归 |

### D12. 观测面：skid **不加任何顶层端口**

SoC 观测端口被 **ADR-046 D10 冻结**（32 端口）⇒ skid 的验收只能靠**既有**观测面：
`dbg_id_*` 序列 + 写回序列 + 退休数分段。
⇒ **若**实施中认为需要新增 skid 观测端口 ⇒ **先改 D10**（rdd-arch 职责），**不得**在 change 内自行扩端口。

### D13. 实施顺序与回退点

1. **先做缺陷 A**（skid buffer）：局部、不动框架层 ⇒ 风险可控；
2. **再做缺陷 B**（`primed` 门控）：同样局部，但**会改退休数与观测面序列**
   ⇒ **单独 commit**，但**同一 change**（见 D13.1）；
3. **框架层（D7）仅在 D6 判定必要时启动**，且 MUST 独立立项。

⇒ 两步可**分别回退**（commit 粒度）：若缺陷 B 的修法引发意外观测面扰动 ⇒ 可只回退第 2 步，
保留 skid（缺陷 A 的修复独立有价值）。

#### D13.1 缺陷 A 与缺陷 B 属**同一 change**（两个 commit），**不拆成两个 change**

**裁决**：同一 change（`rv32i-fetch-skid-buffer-2026-10`）内分 **两个 commit**。

**理由**（为什么不能拆成两个 change）：

1. **spec 的退休数口径同时受两者影响**：缺陷 A 让被救回的指令**真实退休**（`+k`），
   缺陷 B 让填充期的重复退休消失（`−1`）⇒ 最终数字是**两者之和**
   ⇒ 若拆成两个 change，则 `rv32i-commit-path` 的「退休指令数的推导」Scenario
   在中间状态**无法给出自洽的数字**（第一次合并后是 `179 + k`，第二次才是 `179 + k − 1`）
   ⇒ 每个 change 归档时都要改一次主 spec 的同一个 Scenario ⇒ 两次「整体替换」叠加
   ⇒ 反而更容易与「MODIFIED MUST 保留原标题与全部条目」的纪律打架；
2. **D11 的去重掩码拆除**（`:941-945`）是为缺陷 B 设计的 ⇒ 它必须与缺陷 B 的修复
   **同 commit** ⇒ 而 D10 的 spec MODIFIED 必须与掩码拆除**同批**
   ⇒ 两者同 change 才能保证「归档时不出现『掩码已拆但缺陷未修』的中间态」；
3. 缺陷 B 的**判别力依赖缺陷 A 的固件**：D6 的 `primed` 门控修的是「填充期双装」，
   而 D9.1 的判别性固件（停顿点后放非 NOP）验证的是缺陷 A ⇒ 两者在同一套固件上
   ⇒ 拆成两个 change 会让「判别性固件」在两个 change 之间**重复出现或分裂**。

⇒ **D10 / D11 的归属随之确定**：全部落在**同一 change**；
D13.1 是本 ADR 对「立项时写一个还是两个 proposal」的**唯一答案**。

## 3. Consequences

### 正面

- 缺陷 A 消除 ⇒ 固件 MUST 在每个 load-use 停顿点后放 NOP 的约束**消失**
  ⇒ `rv32i-commit-path` 主 spec 里那条**例外**可以去掉；
- 缺陷 B 消除 ⇒ 退休数口径变干净（不再含 +1）⇒ 消除 §1.3.2 的**正确性地雷**
  ⇒ 固件的「第一条指令不自依赖」巧合护栏可以退役；
- 「绿 ≠ 对」的一类问题被消灭：被丢的指令此前**都是 NOP** ⇒ golden 全绿。

### 负面 / 代价

- **时序金标全量重算**（停顿 / 冲刷 / 写回序列 / 退休数）⇒ 观测面 golden 全部要重推；
- 固定 200 拍窗口 ⇒ 尾部截断会把下游退休整体后移 ⇒ 数字不是简单 ±1；
- `primed` 门控是**「靠时序假设」的正确性依赖**（依赖 imem 的 1 拍延迟）
  ⇒ MUST 写进 ADR 并在 spec 里固化（否则未来改 imem 变体时会静默失效）；
- 引入 3 个新寄存器 + 1 个 valid 位 ⇒ **skid 是 IF/ID 旁的旁路缓冲**（IF/ID 的取数源多一个
  mux，**不串入**流水线级数）⇒ 停顿/冲刷的**拍数不变**（D9.2 的分段不变式）；
  时序金标的变化来自「**被救回的指令**真实退休」（`+k`）与「填充期重复退休消失」（`−1`），
  以及**固定 200 拍窗口的尾部截断**（被救回的指令把下游退休整体后移），
  而**不是**新增一级延迟。

### 不在本 ADR 范围

- 固件的 NOP 填充策略本身（固件侧决定，不属架构）；
- imem 换 `aread`（0 拍）—— ADR-041 强制 imem 用 `sread`；
- 预测取指 / 取指队列（会改变 D3 的前提 ⇒ 若将来引入 ⇒ MUST 重审 D3/D4）；
- 「档位 1 覆盖 load」（消除 load-use 停顿）—— 独立立项（V2）；
- 缺陷 B 的**框架层根治**（D7）—— 仅在 D6 判定必要时启动。

## 4. 备选方案（否决）

| 方案 | 否决理由 |
|---|---|
| **PC 回退**（`PC ← skid_pc` 或 `← skid_pc+4`） | 前者**制造**双装；后者**空操作**（D3） |
| 深度 2 的 skid | 不变式保证至多一条在途数据落在 PC 之后（D4）⇒ 深度 2 是无谓复杂度 |
| 停顿时**冻结取指地址**（让 PC 不前进） | 已经是现状（`pc_update_en` 看 `stall`）⇒ 无增益 |
| 停顿时**重复发射**上一条指令（把它再取一次） | 需要额外状态且会与 `fetch_pc_reg` 的对齐耦合 ⇒ 复杂度更高、收益相同 |
| 用 **aread** 取代 sread | ADR-041 强制；且 aread 会把 1 拍对齐变成 0 拍 ⇒ 打破 `pc_lag` 契约 |
| 改**框架层** `sread` 初值语义来修缺陷 B | 影响**所有** `sread` 用户的填充期行为 ⇒ 增量（加 API）优先（D7） |
| 只改**固件**（在停顿点后放 NOP，即现状） | 不解决「绿 ≠ 对」，且不解决 §1.3.2 的正确性地雷 |

## 5. 实施追踪

### 5.1 预期改动面（供 change 立项时对照）

| 文件 | 预期改动 |
|---|---|
| `include/cpu/pipeline/stages/if_stage.h` | 新增 `skid_instr` / `skid_pc` / `skid_valid` / `primed`；改 `if_id_*_reg->next` 的取数源与门控；补 `flush` 清空 skid；**顺带删掉 `:105-112` 与 `:113-119` 的逐字重复注释**（复制粘贴事故，ZERO-DEBT） |
| `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | 判别性固件（停顿点后放非 NOP）；退休数与 `non_retired` 分段金标重算；删/反转两个 NOP 护栏；删去重掩码；`fwd_first_instruction_not_self_dependent` 处置 |
| 新增组件级**用例** | `ch_device<IfStage>` 覆盖 stall×flush 组合（D9.4）—— **宿主 = `tests/test_rv32i_pipeline_behavior.cpp`（既有 target ⇒ 计数不变，不新建文件）** |
| `openspec/specs/rv32i-commit-path/spec.md` | MODIFIED（D10）—— 经 change 合并，**禁**直接改主 spec |
| 根 `AGENTS.md` | §KNOWN LIMITATIONS 两条残留消解 + §NOTES 记录 |

### 5.2 阶段拆分（**C1 与 C2 属同一 change**，见 D13.1）

1. **C1 缺陷 A**（skid，**commit 1**）：局部；风险中（时序重算）；验收 = 判别性固件 + 组件级负向对照；
2. **C2 缺陷 B**（`primed`，**commit 2**，**同一 change**）：局部；风险中（观测面 golden 扰动）；
   验收 = 退休数 −1 + 幻影消失；
3. **C3（可选）框架层**（D7）：仅在 C1/C2 后仍不满足时启动 ⇒ **独立立项**。

⇒ 立项时写**一个** change（`rv32i-fetch-skid-buffer-2026-10`）+ **两个** commit。

### 5.3 爆炸半径

- **流水线时序**：skid 是 IF/ID 旁的**旁路缓冲**（不串入级数）⇒ 停顿 / 冲刷**拍数不变**
  ⇒ 变化来自**被救回的指令真实退休**（`+k`）与**填充期重复退休消失**（`−1`）
  ⇒ 退休数与写回序列重算（D9.2）；
- **观测面**：`dbg_id_*` 逐拍序列变化 ⇒ SoC parity 测试的 golden 重推；
- **框架层**：仅 C3 触及 ⇒ 影响面清单见 D7；
- **计数**：`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` **不变**
  （判别性固件与组件级用例都进**既有** target：`test_rv32i_soc_dual_parity` /
  `test_rv32i_pipeline_behavior`）。

## 6. 与既有文档的差异（实施时 MUST 同步）

| 文档 | 现写法 | 本 ADR 之后 |
|---|---|---|
| design.md §3.10 | 「正解需要取指 skid buffer（1 级缓冲）+ **PC 回退**」 | **否决 PC 回退**（D3）⇒ 订正为「skid buffer（1 级）**且不需要** PC 回退」 |
| design.md §3.6 | 「`ch_mem::sread` 的输出端口在**建图期**被组合求值」 | 根因改为「**加载拍的预填**」（§1.3.3）⇒ 订正 |
| plan T5 | 「先做 skid buffer，双装单独评估」 | 沿用；D6 已给出缺陷 B 的**局部修法**（`primed`）⇒ 「评估」环节的结论 = 可局部修 ⇒ D7 仅在必要时启动 |
| 5 段草案 §2.1 第 2 条 | 「PC 回退：避免 `W+2` 被取两次」 | **理由错误**：`W+2` 的数据从未被消费（IF/ID 被门控）⇒ 订正为 D3 的论证（由 planner 更新草案） |
| 5 段草案 §2.2 | 「根因在框架层 `sread` 的建图期组合求值」 | 订正为加载拍预填（§1.3.3） |
| 5 段草案 §3 | 退休数金标引用「188」 | 订正为 **179 / 20** 起算（D9.2） |
| `rv32i-commit-path` spec `:270-278` | 退休数 188 / 11 | **188 → 179 / 20**（既有漂移，R4）⇒ 本 change 的 MODIFIED 先修基线 |
| 根 `AGENTS.md §KNOWN LIMITATIONS` | 两条残留：① 「正解需要取指 skid buffer + **PC 回退**」（`:192`）② 「根因在**框架层**：`sread` 的输出端口在**建图期**被组合求值」（`:193`） | ① **PC 回退已否决**（D3）⇒ 订正；② 根因改为「**加载拍预填**」（§1.3.3）⇒ 订正；两条残留本身在实现后消解（或按 D6 结果改写） |

## 7. 参考与审查记录

### 7.1 依据

- `openspec/changes/archive/2026-10-26-rv32i-pipeline-commit-path-2026-10/design.md` §3.6 / §3.10
- `openspec/specs/rv32i-commit-path/spec.md`（`:33-36`、`:259-268`、`:270-278`）
- `openspec/specs/mem-read-timing/spec.md`、`openspec/specs/dual-backend-test-coverage/spec.md`
- `docs/adr/ADR-035` / `ADR-041` / `ADR-046` / `ADR-047`
- 根 `AGENTS.md §KNOWN LIMITATIONS`（两条残留）+ `include/cpu/AGENTS.md`

### 7.2 独立复核记录（oracle，2026-10-27）

本 ADR 的时序推导经 **oracle 独立复核**（只读，逐条给出 `文件:行号` 依据）。
复核**确认**了 A / B / D / F 与 G 的方向，并**修正或补强**了 9 处：

| # | 复核结论 | 对本 ADR 的影响 |
|---|---|---|
| 1 | **确认**：丢指令机制 = 「停顿首拍到达、且地址已落在 PC 之后」的那一条 | D2 / D4 的基础 |
| 2 | **确认**：停顿期间地址自我重复 ⇒ 只有首拍那条会永久丢失 | D4 不变式 |
| 3 | **补强**：深度 1 的充分性需要**三条承重前提**（EX-only 检测装配 / 停顿期 EX 气泡 ⇒ 排空拍无新检测 / 排空优先且同拍互斥）+ **作用域声明** | D4 增补前提 ①②③ 与作用域 |
| 4 | **确认并纠正我的理由**：PC 回退**不需要**；草案 §2.1 的「避免 W+2 被取两次」**理由错误** | D3 + §6 订正 |
| 5 | **确认**：两后端都重复读同一地址且**等价**；但**我引用的 Verilog else 归属写错**（`en=0` ⇒ **保持**，非清 0）；且**解释器的 sync 读端口无 reset 输入** ⇒ 潜在分歧真实存在但当前流程不可达 | D3.1 增补已知差异 |
| 6 | **纠正根因**：不是「建图期组合求值」，而是**加载拍的预填**（`load_firmware` 的 48+ 拍，`ext_rst=1`）；⇒ 两后端当前**对称** | §1.3.3 改写 + D6 |
| 7 | **纠正**：幻影装载的 pc 是 **0x80000000**（与真值相同）⇒ 「只门控 valid」时 `dbg_id_pc` **不变**；「门控全部三个装载」才会变 ⇒ 且 `instr_ready ≡ 1` ⇒ 判据**不能**由 `instr_ready` 导出 ⇒ 必须用内部 `primed` | D6 的两方案对比表 + 内部标志 |
| 8 | **升级严重性**：ADR-047 后双装**不再恒幂等**（第二条会读到第一条的前推值 ⇒ 自依赖时是正确性地雷）⇒ 「固件级巧合护栏」在挡正确性缺陷 ⇒ 强化「缺陷 B 一并修」 | §1.3.2 + D11 |
| 9 | **发现既有漂移（R4）**：spec 的退休数 Scenario（188/11）与测试（179/20）**今天就脱节** ⇒ 该 Scenario 自己的 MUST 当前不满足 ⇒ MUST 先修基线 | D9.2 + D10.3 |

另新增 3 条**实施期风险**（已并入 D5 / D8 / D11 / D9.4）：

- **R1** 解释器「采边沿前地址」是**涌现性质** ⇒ 取指地址路径 MUST 保持经组合节点（D8）；
- **R2** `skid_pc` MUST 取 `fetch_pc_reg`（否则指令与 pc 错 4 字节，与 `flush_d1` 缺陷同型）⇒ 进变异清单（D2.1 / D9.3 M1）；
- **R3** 「flush 清空 skid」在流水线级**不可达** ⇒ SoC 级变异抓不到 ⇒ 验收 MUST 落**组件级**（D5 / D9.4）。

### 7.3 对抗审稿记录

#### 第 1 轮（2026-10-27，critic，`review_profile: arch`）⇒ **REJECT**

`mergeable: no` / `scope_exceeded: false`；**1 BLOCKER + 3 SHOULD_FIX + 2 NIT**。
复核确认：13 条决策的**技术推导全部成立**（D2 / D3 / D4 / D5 / D6 逐拍验证通过）、
ADR 内**全部 `文件:行号` 引用零失配**、D10 与仓库 OpenSpec 纪律一致、D12 与 ADR-046 D10 合规。

| # | 级别 | 问题 | 处置 |
|---|------|------|------|
| B1 | **BLOCKER** | **D13 与 §1.3.2 / D6 自相矛盾**：§1.3.2 与 D6 说缺陷 B「必须随本 change 一并修」，而 D13 步骤 2 写「单独 commit / **单独 change**」⇒ 立项时无法判断写**一个还是两个** proposal；D10 的 spec MODIFIED 与 D11 的去重掩码拆除的归属也随之漂移 | ✅ **已修**：新增 **D13.1** —— 裁决为「**同一 change、两个 commit**」，并给出**三条不可拆的理由**（spec 退休数口径同时受 `+k` 与 `−1` 影响 ⇒ 中间态无法自洽 / D11 掩码拆除必须与缺陷 B 同批 / D9.1 的判别性固件同时验证两者）⇒ D10、D11 的归属随之确定；§5.2 / §5.3 同步改写 |
| SF1 | SHOULD_FIX | §3「引入 3 个新寄存器 ⇒ **流水线级数 +1**」表述不准：skid 是 IF/ID 旁的**旁路缓冲**（取数源多一个 mux，不串入级数）⇒ 会误导实施者去找「多一拍」的延迟 | ✅ **已修**：改写为「不串入级数 ⇒ 停顿/冲刷拍数**不变**；金标变化来自**被救回的指令真实退休**（`+k`）与填充期重复退休消失（`−1`）」；§5.3 同步 |
| SF2 | SHOULD_FIX | D2 排空条件公式 `!stall && skid_valid && !flush && !flush_d1` **漏了 `!rst`**，与 prose（「以 `if_id_update_en` 为真」—— 该使能含 `!rst`）不一致 | ✅ **已修**：公式补 `!rst`，并显式标注「该使能含 `!rst` ⇒ 复位拍不排空」 |
| SF3 | SHOULD_FIX | D9.4 要求**新增**组件级测试，而 D9.5 / §5.3 钉死 `CTEST_TOTAL` 不变 ⇒ 若新建 target 则违反 D9.5，且未指明宿主 | ✅ **已修**：指明宿主 = **`tests/test_rv32i_pipeline_behavior.cpp`**（既有 dual-backend target，本就是子组件 SECTION-free 用例的容器）⇒ 计数不变、不新建文件；并补「MUST NOT 改 `tests/test_rv32i_pipeline.cpp`（interpreter-only baseline）」 |
| NIT1 | NIT | D9.3 的 M6（`primed` 门控全部三个装载）以「只门控 `valid`」为前提，而 D6 把该形态列为合法备选 ⇒ 若实施选方案 2，M6 的 golden 前提失效 | ✅ **已修**：M6 行加「⚠️ **以 D6 默认方案为前提**；若改选方案 2 须改写该变异」 |
| NIT2 | NIT | §1.3.3 写死「48+ 个加载拍」⇒ 固件一变即漂移 | ✅ **已修**：改为「= 固件 word 数，每 word 一拍（`load_firmware` 每 word 一拍）」 |

⇒ 按评审预算（**只有 BLOCKER 触发返工与复审**）⇒ 第 2 轮复审**只提交**
「B1 的闭合证据 + 本轮新增 diff」。

#### 第 2 轮（2026-10-27，critic，`review_profile: arch`）⇒ **APPROVE**

`mergeable: yes` / `scope_exceeded: false`；**无 BLOCKER、无 SHOULD_FIX**、2 NIT。

- **B1 闭合确认**：§1.3.2 / D6 / D13 / **D13.1** / §5.2 / §5.3+D10+D11 共 **6 处**逐一核对
  ⇒ 全文一致表达「**一个 change、两个 commit**」；残留扫描（`单独 change` / `两个 change` /
  `独立立项`）的命中项均为**有意**（D13.1 在论证为何不能拆 / §7.3 引用第 1 轮原文 /
  D7 与 V2 的独立立项）⇒ **无二义残留**；
- 复核另确认：D13.1 的三条理由与 D9.2 / D10 / D11 **自洽**（无虚构依据）；
  D9.4 的宿主选择与该文件头自述**逐条对得上**（既有 dual target + 「原文件 MUST NOT 修改」）；
  §5.3 的「拍数不变」与 D9.3 一致；SF2 修补后的 `!rst` 与 `if_stage.h:191-199` 吻合。

| # | 级别 | 问题 | 处置 |
|---|------|------|------|
| NIT3 | NIT | §3「时序金标的**全部**变化来自 `+k` 与 `−1`」overstated（**固定 200 拍窗口的尾部截断**也是变化来源之一） | ✅ **已修**：改为「变化来自 `+k`、`−1`，以及固定 200 拍窗口的尾部截断」 |
| NIT4 | NIT | §5.1 表格行首「**新增**组件级测试」+ 行尾「不新建文件」字面可读作「新增测试文件」 | ✅ **已修**：行首改为「新增组件级**用例**」 |

⇒ **审稿预算用尽（2/2）** ⇒ 本 ADR 状态转 **Accepted**；
⇒ 后续：rdd-planner 立项 **一个** proposal（`rv32i-fetch-skid-buffer-2026-10`）+
**两个** commit（C1 缺陷 A / C2 缺陷 B）；C3（D7 框架层）按 D13 步骤 3 **独立立项**。
