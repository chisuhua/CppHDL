# ADR-047: RV32I 流水线的数据前推 —— 裁决「接」、形态与验收判据（ADR-046 D5 的落地裁决）

- Status: Accepted（proposed 2026-10-27）
- Date: 2026-10-27
- Author: build-agent（architect role）
- Parent: ADR-046（RV32I canonical home，**D5 = 本 ADR 的裁决对象**）、ADR-038（forwarding namespace orphan，§2.3 的「合并留未来」）、ADR-037（stub 物理删除，删除死代码的先例）
- 关联 OpenSpec change：`rv32i-forwarding-decision-2026-10`（**待立项**，实施本 ADR）
- 关联 objective / feature：`feat-rv32i-isa-completeness` ③、`.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` T4
- 前置审查：本 ADR 的分析经 **oracle 独立复核**（4 处判断被修正，见 §6）

## 1. Context

### 1.1 触发

ADR-046 §D5 把「流水线前推语义」显式**挂起**：

> 前推语义（启用 / 禁用 / 重构为真正的转发通路）MUST 单独立项 —— 需独立 ADR +
> 回归证据（证明「启用前推不会产生 IR 图循环」或「确实会，需换实现」）。
> **C2 MUST NOT 在收敛过程中顺手改前推语义。**

2026-10-24 的补充表述把待裁决范围收窄为：**前推的数据源**（应为 EX/MEM 流水线寄存器）
与**接线**。本 ADR 就是那个独立 ADR。

### 1.2 现状：lib 流水线**不接**前推

`include/cpu/pipeline/hazard_unit.h:198`：

```cpp
io().rs1_data = io().rs1_data_raw;   // 直通，无前推
```

后果（均为**实测**）：

| 现象 | 证据 |
|---|---|
| 相邻 ALU→ALU 依赖读到**旧值** | 固件 MUST 在依赖指令之间插 NOP/间隔 |
| 距离 2 的 load→use **检测不到** | 「`lw` → `nop` → `add`」读旧值（load-use 检测只覆盖**紧邻**） |
| load-use 停顿 **3 拍** + **丢弃「消费者之后的第一条指令」** | 根 `AGENTS.md §KNOWN LIMITATIONS`（imem 是 1-cycle `sread` ⇒ 已提交未锁存的指令无处可存） |
| `JALR` 的 `rs1` 读到**旧值** | `rv32i-isa-semantics` spec 的「`JALR` 的 rs1 MUST NOT 依赖数据前推 —— 前推未裁决」 |

⇒ 固件的「手工 NOP」是**巧合保证**，不是语义保证（ADR-046 §D5 补充表述已指出）。

### 1.3 组合环的事故记录（`git show bf78367`，2026-04-15）

事故前的形态（`git show bf78367^:include/cpu/pipeline/rv32i_pipeline.h`）：

```cpp
hazard.io().ex_alu_result  <<= ex_stage.io().alu_result;    // ← EX 级**组合**输出
hazard.io().mem_alu_result <<= mem_stage.io().alu_result;  // ← MEM 级**组合**输出
hazard.io().rs1_data_raw   <<= id_stage.io().rs1_data;      // ← reg_file 组合读
ex_stage.io().rs1_data     <<= hazard.io().rs1_data;        // ← mux 输出回到 EX
```

⇒ 环：`ex_stage.alu_result → hazard.ex_alu_result → hazard.rs1_data(mux) → ex_stage.rs1_data → ex_stage.alu_result`
—— **全链组合、无寄存器/存储器节点** ⇒ `src/core/context.cpp` 的拓扑排序报
`FATAL: Cycle detected in IR graph during topological sort`。

`bf78367` 的处置是**移除 mux**（`hazard.rs1_data` 改为直通），并把 `forward_a`/`forward_b`
的选择信号**留着**（"still computed for future use"）⇒ 这就是今天的状态。

### 1.4 框架层的「断路器」规则（决定成环与否的机制）

`src/core/context.cpp` 的拓扑排序对循环节点的处理：

| 节点类型 | 处理 | 效果 |
|---|---|---|
| `type_reg` | **只遍历 init_val**，不遍历 `next`/`srcs` | ✅ **寄存器是断路器** |
| `type_mem` | 放行 | ✅ 存储器是断路器 |
| `type_mem_read_port` | **不在豁免清单** ⇒ 走 `CHFATAL` | ⚠️ 环穿过读端口**照样炸** |
| 其他（组合节点） | `CHFATAL` | ❌ |

⇒ **任何穿过 `ch_reg` 的路径在该寄存器处终止** ⇒ 「数据源取自流水线寄存器」是断环的
**充分条件**（且是本设计**唯一需要**的形态）。

⚠️ **它不是必要条件**：`type_mem`（存储器）同样是断路器（`context.cpp:304` / `:326`；
`bf78367` commit message 第 1 条正是「Allow `type_mem` in cycles」）⇒ 源取自 `ch_mem`
的读端口**也可能**断环（但那是**另一种**设计，本 ADR 不采用）。
⇒ 另注：`type_mem_read_port`（`ch_mem` 的**读端口**节点，`include/ast/mem_port_impl.h:32`）
**不在**豁免清单 ⇒ 环穿过**读端口**照样 FATAL。

## 2. Decision

### D1. 裁决：**接**数据前推

不接 ⇒ 把「固件手工 NOP + 距离-2 读旧值」**固化为 ISA 语义**，与「5 级流水线」的宣称不符；
且 `JALR` 的 `rs1` 限制与 load-use 停顿的「丢指令」面都**留在**后续 change 里。

⇒ 本 ADR 裁决**接**，并给出形态（D2–D8）与**可判定的验收判据**（D9）与**规格同步清单**（D10）。

### D2. 数据 mux 归属 = **既有**的 `chlib::ForwardingMux`（零新增组件）

`include/chlib/forwarding.h` 的 `ForwardingMux<W>` 已是 ADR-046 §D3 #14 指定的
**唯一**数据 mux 实现（档位语义与当年被移出的 mux **逐档等价**：0→REG / 1→EX / 2→MEM）。

⇒ **不得**把 mux 放回 `HazardUnit`（bf78367 的教训 + `rv32i-canonical-home` spec 已禁止）。
⇒ **零新增组件**。

### D3. 前推的**数据源 MUST 取自流水线寄存器**（不成环的必要条件）

| 形态 | 数据源 | 环？ |
|---|---|---|
| ❌ 事故形态 | EX/MEM 级的**组合输出端口**（`ex_stage.alu_result` / `mem_stage.alu_result`） | **成环**（全链组合） |
| ✅ 合法形态 | `ch_reg`（`exmem_*` / `memwb_*` / `wb_stage.write_data`） | **断**（`type_reg` 是断路器） |

⚠️ 附带约束：`type_mem_read_port` **不豁免** ⇒ reg_file / dmem 的 `aread` **地址**
MUST 保持在寄存器之后（当前已满足：reg_file 的地址 ← 指令字 ← `if_stage` 寄存器；
dtcm 的地址 ← `exmem_alu_result` 寄存器）。

### D4. 档位定义（**打拍后** = 教科书编号）

| 档位 | 来源 | 数据 |
|---|---|---|
| 0 = REG | 寄存器堆（`id_stage.rs1_data`） | 与今天的直通值**同源** ⇒ 行为零变化 |
| 1 = EX/MEM | EX/MEM 流水线寄存器 | `select(exmem_is_jump, exmem_link_value, exmem_alu_result)` |
| 2 = MEM/WB | MEM/WB 流水线寄存器 | `wb_stage.io().write_data`（**已合并** load/jump/alu 三路） |

⚠️ **档位 1 MUST 含 `link_value` 分支**：`JAL`/`JALR` 的写回值是 `pc + 4`（不是 `alu_result`）
⇒ 漏掉它 ⇒ 档位 1 会把 **ALU 垃圾**当成 jump 的结果前推。
（`exmem_link_value` 寄存器已存在，`rv32i_pipeline.h:122`）

⚠️ **但这条失效模式在固件级「不可达成」（2026-10-27 critic 指出，已核实）**：
`JAL`/`JALR` 是**无条件**跳 ⇒ 它的**距离-1 消费者必然落在错误路径上**（fall-through）
⇒ 当拍就被 flush 成气泡 ⇒ **永远到不了真实消费者** ⇒
「`jal ra,…; addi x,ra,…` 紧邻序列会静默错值」这个描述的序列**不会执行**。
⇒ 真正覆盖 jump 生产者的是**档位 2**（`wb_stage.write_data` 已含 `link_value`；
目标指令最早**距离 3** 到达 EX ⇒ 此时它已在 MEM/WB ⇒ 档位 2 命中）。
⚠️ 「距离 3」是**分析性下界**（按 D5 的打拍语义推得）⇒ **验收以实测为准**，
MUST NOT 当作已证事实（与 D9 第 6 类用例的「距离 MUST 由实施期实测确定」一致）。
⇒ ⇒ **保留**档位 1 的 `link_value` 分支作为**防御**（无害），
但它的验收**不得**靠固件（见 D9 的变异项改写）。

⚠️ 档位 2 用 `wb_stage.io().write_data` 是**免费**的：它已经把 load 数据 / ALU 结果 /
`link_value` 合并好了，且三个输入**全是 MEM/WB 寄存器** ⇒ 不成环。

### D5. 档位 select **MUST 打一拍**（`fwd_a_d1` / `fwd_b_d1` 两个 `ch_reg`）

**根因**：`hazard` 的比较点是「**EX 级** rd vs **ID 级** rs」（那是 **load-use 检测点**），
而 mux 喂的是 **EX 级**操作数 ⇒ 拍 T 算出的 `forward_a=1` 描述的是「**T+1** 拍消费者进 EX
时该选 EX/MEM」，但 mux 在**拍 T** 就把它用在「T 拍 EX 里那条（生产者自己）」身上
⇒ **错一级**。

⚠️ 错一级的后果**不是「不前推」，而是把错误数据塞进当前 EX 指令** ——
若生产者的 rd 碰巧匹配再下一条指令的 rs，就会静默算错 ⇒ **真实的静默错值路径**。

⇒ 修复：**pipeline 级加两个 `ch_reg` 打拍** ⇒ 拍 T 的档位在 T+1 生效 ⇒ 语义恢复标准。
⇒ **打拍本身对 `hazard` 零改动** ⇒ `hazard` 的组合逻辑（档位判定 + `forward_a/b` 输出）
**一字不改** ⇒ 既有真值表测试（`tests/chlib/test_forwarding.cpp` +
`examples/riscv-mini/tests/test_rv32i_pipeline.cpp`）**因打拍而**全部保留。
⚠️ **限定**：D6 要求给前推判定加 `& !ex_is_load` 门控 ⇒ 那**是**对 `hazard` 的改动
⇒ 会改变若干实例化点的行为 ⇒ D6 的连带约束（补驱动 + 复核断言）适用。
（这两句不矛盾：**打拍**不改 hazard；**D6 的门控**改 hazard。）

⚠️ 气泡安全性已推演：stall/flush 拍 `idex_rd_addr` 被清零（`id_stage.h:280`）⇒ 误匹配被
`rd_nz` 门掉；打进气泡的选择无害（`idex_valid=0` ⇒ 结果被丢弃）。

⚠️ **否决**「改检测点为 EX 视图」：`id_rs*_addr` 同时服务 load-use 检测（**MUST** 是 ID 视图，
`rv32i_pipeline.h:430-436`）⇒ 改它要加新端口 + 第三路比较 ⇒ 改动面更大且收益相同。

### D6. 保守边界：档位 1 **不覆盖 load**（保留 3 拍 load-use 停顿）

`exmem_alu_result` 对 load 指令是**访存地址**而非数据 ⇒ 档位 1 若含 load 会前推错值。

⚠️ 形态上「档位 1 = `select(exmem_is_load, mem_stage.io().mem_data, exmem_alu_result)`」
是**可行的**（dmem 是 `aread` 0-cycle ⇒ MEM 级当拍可得；`mem_stage.io().mem_data` 已含
字节/半字抽取与符号扩展；其地址链止于 `exmem_alu_result` 寄存器 ⇒ 不成环）
—— 但它会**消除 load-use 停顿**，而停顿是「丢弃消费者后第一条指令」的触发面
⇒ 爆炸半径独立 ⇒ **留作独立 change（V2）**，不在本 ADR。

⇒ 但 hazard 的前推判定 **MUST 加 `& !ex_is_load` 防御门控**：
当前 `forward_ex_to_a = ex_reg_wr & ex_rd_nz & rs1_match_ex`，而 `ex_reg_wr`（= `ex_reg_write`）
**包含 load** ⇒ 保守形态下 EX 级 load 命中前推的当拍，stall 必然同时生效（两者共享匹配条件）
⇒ 消费者不进 EX ⇒ 误选只打进气泡 ⇒ 结果被丢弃。**但这是隐式耦合** ⇒ ADR 显式要求防御门控。

⚠️ **连带（实测，本条比初稿更严格）**：`ex_is_load` 在**全仓的测试里从未被驱动过**
（唯一出现是 `test_rv32i_soc_dual_parity.cpp:556` 的**注释**）
⇒ **除流水线装配（`rv32i_pipeline.h:459` 有驱动）外，全部 6 个独立实例化点**
（`test_pipeline_debug.cpp` / `test_wrapper_simple.cpp` / `test_pipeline_exec.cpp` /
`examples/riscv-mini/tests/test_rv32i_pipeline.cpp` ×2 / `forwarding_dut_fixture.h`）
**都未驱动**它 ⇒ 它们的读值是**未定义值**。
（口径：**按实例**计 = 6（总 7 − 装配 1）；**按文件**计 = 5 个测试文件 + 装配 = 6。
⚠️ `forwarding_dut_fixture.h` 的实例化写的是 `ch::ch_module<riscv::HazardUnit>`
⇒ **不带 namespace 前缀的 grep 会漏掉它** ⇒ 计数时须按 §5.3 的 7 个实例逐个核对。）

⚠️ **措辞更正（critic NIT #1）**：根 `AGENTS.md` 记录的是「未驱动端口读到的是
**未定义值**」——`strbe` 个案读到**全 1**、`rst_n` 个案读到 **0** ⇒ **两种读值都是坏行为**，
不可假设为某个特定值 ⇒ 本 ADR **不**依赖「读到全 1 ⇒ `ex_load_use` 恒真」这个推论。

⇒ 加 `& !ex_is_load` 门控会**改变**那些实例的行为（`ex_load_use` 从「未定义」
变成「按驱动值」）⇒ MUST **逐点补驱动**并**复核既有断言**（不是「顺手补一下」）。

### D7. 删除 `HazardUnit` 的 5 个**预留输入端口**

| 端口 | 现状 | 裁决 |
|---|---|---|
| `wb_reg_write` / `wb_rd_addr` | 无内部消费者（`ForwardingMux` 只有 3 档 ⇒ WB 档位用不上） | **删** |
| `ex_alu_result` | 无内部消费者，**但已接线到 `ex_stage.io().alu_result`（EX 级组合输出）** | **删**（见下） |
| `mem_alu_result` | 无内部消费者 | **删** |
| `wb_write_data` | 无内部消费者 | **删** |

⚠️ **`ex_alu_result` 是环饵**：它已接线到 EX 级的**组合输出** ⇒ 留着就**诱使**后人
「顺手消费它」⇒ **精确复现 bf78367**。删掉让本 ADR **自执行**。

⇒ 爆炸半径封闭可枚举（§5.3）；本仓先例（ADR-037 的 stub 物理删除）最终归宿都是**删**。

⚠️ **但「直通路径」的 4 个端口 MUST 保留**（`rs1_data_raw` / `rs2_data_raw` 输入 +
`rs1_data` / `rs2_data` 输出）—— `HazardUnit` 作为**独立组件**仍有**真实消费者**：

| 消费者 | 证据 |
|---|---|
| `forwarding_dut_fixture.h:105-106` | 组合 DUT **喂** `rs1_data_raw` / `rs2_data_raw` |
| `test_pipeline_exec.cpp:531/560/…` | **读** `hazard.io().rs1_data`（`get_port_value`）⇒ 删了会断既有断言 |

⇒ 故 D7 的准确形态是：
- **删 5 个预留输入端口**（公共 API 破坏性变更 ⇒ 爆炸半径见 §5.3）
- **保留** 4 个直通端口（独立组件仍可用）
- ⚠️ **但 pipeline 装配内 MUST 删掉 `:484` / `:485` 两处 `rs1_data_raw` / `rs2_data_raw` 的驱动**
  —— 接 mux 后 `hazard.rs1_data` 在 pipeline 里**无消费者**（mux 直喂 `ex_stage.rs1_data`）
  ⇒ 继续驱动 `rs1_data_raw` 就是「**喂了没人读**」⇒ ZERO-DEBT 违规
- ⇒ **不是「删 9 个」**：4 个直通端口有独立消费者（证据见上表）

### D8. 收敛 `ex_stage.io().rs1_data` / `rs2_data` 的**双驱动**

`rv32i_pipeline.h` 当前对同一端口 `<<=` 了**两次**：

```
:200   ex_stage.io().rs1_data <<= id_stage.io().rs1_data;
:488   ex_stage.io().rs1_data <<= hazard.io().rs1_data;
```

`inputimpl::set_driver` 只替换 `driver_`，但 **`add_src(drv)` 会累积**
（`include/ast/ast_nodes.h:156-159`）⇒ IR 图里 `ex_stage.rs1_data` 残留**两条 src 边**。

⚠️ **今天的实际风险被夸大过（2026-10-27 critic 证伪，已核实）**：
`src/codegen_verilog.cpp:613-625` 的 `type_input` 分支**只取单数**
`input_node->driver()` ⇒ 两个后端都取「后写者」⇒ **今天不会双后端分叉**。
（初稿把它写成「本仓最痛的 bug 类别」是**过强**的措辞，已更正。）

⚠️ **但接 mux 后风险会变成真的**：`:488` 改接 mux 输出 ⇒ `:200`（`id_stage` 直通）与
`:488`（mux 输出）指向**不同的值** ⇒ 残留边会让拓扑排序**多走一条依赖链**
（该链今日止于 `if_stage` 寄存器 ⇒ 不断环；但它是一条**纯组合链**
`id_stage.rs1_data ← regfile.aread ← 指令字 ← if_stage` ⇒ 未来若该链上出现组合回边
⇒ **误报成环** ⇒ 又是一次 `CHFATAL` 事故）。

⇒ **MUST** 在同一个 change 里删掉 `:200` / `:201`（保留单一驱动）。
⇒ 正确理由 = **ZERO-DEBT / 拓扑排序 hygiene**（残留的 stale src 边），
**不是**「今天已在双后端分叉」。

### D9. 验收判据（写死；**判别性固件是必需项**）

⚠️ **当前固件刻意无冒险**（`rv32i-soc-dual-parity` design.md §「固件」+ `verilator-rv32i-e2e`
spec 的 NOP 间隔都是为「无前推」留的）⇒ **前推接错 / 接反 / 没接，全仓照绿**。

⇒ 实施 MUST 新增**判别性固件**，至少覆盖：

1. **相邻 ALU→ALU，无 NOP**（前推的核心收益）
2. **距离 2 的 load→use**（档位 2 覆盖 load 的判据）
3. **`JALR` 的 `rs1` 紧邻被写**（消除 `rv32i-isa-semantics` 的已知限制）
4. **分支条件用紧邻新值**（前推给 BEQ/BNE 提供新值）
5. **双生产者同 rd**（`hazard_unit.h:116-126` 注释里的陷阱形态）
6. **jump 生产者的前推**（`JAL` → 若干条无关指令 → 消费者）
   ⇒ 覆盖**档位 2** 对 jump 生产者的前推（`wb_stage.write_data` 已含 `link_value`）
   ⇒ 这是固件级**唯一**能覆盖 jump 前推的形态（D4：距离 1 不可达）
   ⚠️ **具体距离 MUST 由实施期实测确定**，MUST NOT 写死
   （档位与数据源的时间对齐由 D5 的**打拍**保证 ⇒ 打拍后的对齐是唯一权威；
   本 ADR 不预先断言「距离 N」—— 那正是 D5 要用实测钉住的东西）

⇒ 验收 MUST 包含：

- **双后端逐拍 parity**（无掩码）
- **变异验证**（每条都先确认编译成功）：
  - 去掉打拍 ⇒ 红（档位错一级）
  - 交换档位 1 / 2 的数据源 ⇒ 红
  - 前推判定漏 `& !ex_is_load` ⇒ 红（配合新增的 load 判别性用例）
  - ⚠️ **「档位 1 忘接 `link_value`」** ⇒ **不得**用固件验收
    （D4 已说明：`JAL`/`JALR` 的距离-1 消费者必在错误路径上 ⇒ 该失效模式固件级不可达）
    ⇒ **MUST** 改用**定向接线级单测**：直接驱动 `exmem_is_jump = 1` + 档位 1，
    断言 mux 输出 == `link_value`（而非 `alu_result`）
- **负向对照**：实施期第一屏 MUST 先跑 **bf78367 形态**的最小复现
  ⇒ 确认环检测**仍然 FATAL**（证明护栏有效），再接新形态
- `rv32i-canonical-home` spec 已要求的「**装配后无环**」的可执行验证

### D10. 规格锁同步（MUST 走 MODIFIED delta，不得直接改主 spec）

接前推会**过时**以下既有契约：

| spec | 冲突点 |
|---|---|
| `rv32i-isa-semantics` | 「`JALR` 的 `rs1` MUST 取 EX 级的寄存器值（**MUST NOT 依赖数据前推**）」——本 ADR 裁决「接」⇒ 该条必须 MODIFIED |
| `rv32i-commit-path` | 观测面 / 逐拍 parity 契约（固件变了 ⇒ golden 变了） |
| `verilator-rv32i-e2e` | 为「无前推」留的 NOP 间隔约定 |
| `rv32i-canonical-home` | mux 归属（已要求「装配后无环」）—— 需登记新的验收证据 |

⇒ 实施 change MUST 用 **MODIFIED delta** 更新它们，并在 change 的 tasks 里逐条列出。

⚠️ **文档同步（不在 spec 层，但同样 MUST 做）**：根 `AGENTS.md §KNOWN LIMITATIONS` 里的
① 「距离 2 读旧值」与 ② 「`JALR` 的 `rs1` 无前推」两条会**过时** ⇒ MUST 同步更新；
`include/cpu/AGENTS.md` 的「`HazardUnit` 的『预留输入』端口无内部消费者」条目也会过时
（D7 删端口 ⇒ 该条目消失）。

## 3. Consequences

### 正面

- 相邻 ALU→ALU 依赖**零停顿** ⇒ 固件不再需要为它插 NOP
- 距离 2 的 load→use 被**档位 2** 覆盖 ⇒ 「距离 2 读旧值」的缺口关闭
- `JALR` 的 `rs1` 限制**消除** ⇒ `rv32i-isa-semantics` 的那条约束可放宽
- 分支条件可用**紧邻**的新值 ⇒ 分支逻辑更短
- 顺带清掉 5 个预留端口（含**环饵** `ex_alu_result`）⇒ ZERO-DEBT 合规
- 顺带收敛 `ex_stage.rs1_data` 的双驱动 ⇒ 消除双后端分叉隐患

### 负面 / 代价

- **load-use 停顿仍在**（3 拍 + 「丢弃消费者后第一条指令」）⇒ **T5（skid buffer）仍然必需**
  ⇒ 本 ADR **不**让 T5 变成冗余
- 固件 golden 大改（判别性固件 + 依赖关系变化）⇒ 时序类断言全部重算
- 3 处「预留端口」删除是**破坏性公共 API 变更**（爆炸半径见 §5.3）
- 档位语义依赖**打拍** ⇒ 若后人把打拍删掉 ⇒ 静默错值（⇒ 变异验证 D9 覆盖）

### 不在本 ADR 范围

- **V2：档位 1 或上 `mem_stage.mem_data`** ⇒ 消除 load-use 停顿（独立 change，
  爆炸半径含 `perf_load_use_stalls` 语义与停顿契约）
- **`riscv::ForwardingUnit`（`rv32i_forwarding.h`）与 `chlib::ForwardingMux` 的合并**
  （ADR-038 §2.3 裁决留未来）—— ⚠️ 但必须**点名警告**：它的 `ex_alu_result` 输入语义
  就是「**活的 EX 输出**」⇒ **拿它接进流水线会成环**（环饵）
- 取指 skid buffer / PC 回退（= T5 `rv32i-fetch-skid-buffer-2026-10`）
- 分支预测（`rv32i_branch_predict.h` 未接入流水线）

## 4. 备选方案（否决）

| 方案 | 否决理由 |
|---|---|
| **不接**（永久 V0） | 把「固件手工 NOP + 距离-2 读旧值」固化为 ISA 语义，与「5 级流水线」宣称不符；若判别性固件的回归证据做不出来，**回落到它并如实登记** |
| **把 mux 放回 `HazardUnit`** | bf78367 的教训 + `rv32i-canonical-home` spec 禁止 |
| **用 `riscv::ForwardingUnit`** | 其 `ex_alu_result` 输入就是「活 EX 输出」= 环饵；且 mux 归属独立组件是 spec 明文要求。它仍有消费者（`hazard_detection_unit.h` + 两个根测试）⇒ 本 ADR **不动**它 |
| **前推组合 `alu_result`（「同拍 EX→EX」）** | 生产者与消费者不可能同拍同处 EX ⇒ 语义上无意义，且就是事故环 |
| **档位 1 覆盖 load（V2 形态）** | 会消除 load-use 停顿 ⇒ 与 T5 的边界重叠 ⇒ 独立立项 |
| **改检测点为 EX 视图**（免打拍） | `id_rs*_addr` 同时服务 load-use 检测（**MUST** 是 ID 视图）⇒ 需加端口 + 第三路比较 ⇒ 改动面更大 |
| **保留 5 个预留端口** | `ex_alu_result` 是**环饵**（已接线到 EX 级组合输出）⇒ 留着就诱使后人复现 bf78367；ZERO-DEBT POLICY 倾向删 |

## 5. 实施追踪

### 5.1 预期改动面（供 change 立项时对照）

| 文件 | 改动 |
|---|---|
| `include/cpu/pipeline/rv32i_pipeline.h` | 加 `ForwardingMux` 实例；加 `fwd_a_d1`/`fwd_b_d1` 两个 `ch_reg`；接线 3 个前推源；**删 `:200`/`:201` 的旧驱动**（D8）；删 5 个预留端口的驱动（D7） |
| `include/cpu/pipeline/hazard_unit.h` | 删 5 个预留端口（D7）；前推判定加 `& !ex_is_load`（D6） |
| `include/cpu/pipeline/stages/ex_stage.h` | （无端口变化；`rs1_data` 的**驱动源**变） |
| `examples/riscv-mini/tests/forwarding_dut_fixture.h` | 补 `ex_is_load` 驱动（D6 的连带约束） |
| `examples/riscv-mini/tests/test_rv32i_pipeline.cpp` | **删 4 个区块 × 5 端口 = 20 处** `set_input_value`；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_pipeline_exec.cpp` | **删 8 个区块 × 5 端口 = 40 处** `set_input_value`；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_pipeline_debug.cpp` / `test_wrapper_simple.cpp` | 各删 5 处驱动；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | 判别性固件 + golden 全量重算（D9） |

### 5.2 阶段拆分建议

| 阶段 | 内容 | 可独立验收 |
|---|---|---|
| **C1** | 前推**接线**（mux + 打拍 + 源）+ 判别性固件 + 变异验证 | ✅ 双后端 parity + 6 类判别性用例 + **档位对齐的实测**（距离由实测钉住，MUST NOT 预先断言） |
| **C2** | 端口清理（D7）+ 双驱动收敛（D8）+ 文档/spec 同步（D10） | ✅ 护栏 + spec delta |

⚠️ C1 与 C2 **可以分开提**，但 C1 的验收**依赖**判别性固件 ⇒ **不得**先合 C1 再补固件。

### 5.3 爆炸半径（删端口的可枚举清单）

⚠️ 以下清单为 **2026-10-27 实测**（初稿曾把它标为「实测」却**漏了 `set_input_value`
这个驱动模式** —— 已更正）。`HazardUnit` 共 **7 个实例化点**（分布在 **6 个文件**），
**7 个全部驱动**了这 5 个端口：

⚠️ **计数口径（2026-10-27 critic NIT #1）**：**按实例** = 7（`rv32i_pipeline.h:114` 装配 1 +
`forwarding_dut_fixture.h:90` 组合 DUT 1 + `test_pipeline_debug.cpp:62` 1 +
`test_wrapper_simple.cpp:39` 1 + `test_pipeline_exec.cpp:595` 1 +
`examples/riscv-mini/tests/test_rv32i_pipeline.cpp:179` 与 `:239` **2**）；
**按文件** = 6。⚠️ `forwarding_dut_fixture.h:90` 写的是 `ch::ch_module<riscv::HazardUnit>`
（**带 namespace 前缀**）⇒ 用 `ch_module<HazardUnit>` 这类模式 grep 会**漏掉它**
（⇒ 本条曾被两轮误数）。

| 文件 | 位置 | 数量 |
|---|---|---|
| `include/cpu/pipeline/hazard_unit.h` | 端口声明 | 5 个 `ch_in` |
| `include/cpu/pipeline/rv32i_pipeline.h` | `:447` / `:451` / `:469` / `:477` / `:478` | 5 处 `<<=`（**`ex_alu_result` ← EX 级组合输出 ⇒ 环饵**） |
| `examples/riscv-mini/tests/forwarding_dut_fixture.h` | `:98` / `:101` / `:107` / `:108` / `:109` | 5 处 `<<=` |
| `examples/riscv-mini/tests/test_pipeline_debug.cpp` | `:65` / `:66` / `:67` / `:72` / `:75` | 5 处 `<<=` |
| `examples/riscv-mini/tests/test_wrapper_simple.cpp` | `:42` / `:43` / `:44` / `:49` / `:52` | 5 处 `<<=` |
| `examples/riscv-mini/tests/test_pipeline_exec.cpp` | 8 个区块（`:514-520` / `:543-549` / `:572-578` / `:609-616` / `:636-641` / `:669-675` / `:698-704` / `:727-733`） | **40 处** `set_input_value` |
| `examples/riscv-mini/tests/test_rv32i_pipeline.cpp` | 4 个区块（`:111-117` / `:149-157` / `:195-202` / `:250-257`） | **20 处** `set_input_value` |

⇒ **删这 5 个端口 ⇒ 上述 7 个文件全部编译失败**（⇒ 爆炸半径**封闭**且**可枚举**）。
⚠️ `examples/riscv-mini/tests/test_rv32i_pipeline.cpp:82` 的
`reg.instance().io().mem_alu_result` 是**另一个组件**（`reg`）⇒ 不在清单内。
⚠️ 根 `tests/test_rv32i_pipeline.cpp` 实例化的是 `riscv::ForwardingUnit` / `ControlHazardUnit`
（`ForwardingUnit` **3** 处：`:151` / `:188` / `:225`；`ControlHazardUnit` 1 处：`:425`）
⇒ **不受** `HazardUnit` 改动影响（初稿误列了它，已更正）。

⇒ 另：`scripts/verify_example_assertion_quality.sh` 钉住断言计数 ⇒ 删驱动后
**MUST 重跑该护栏**（删驱动语句不动断言数，但计数护栏会验证这一点）。
⇒ 仓外无契约面。

## 6. 参考与审查记录

- `docs/adr/ADR-046-rv32i-canonical-home.md` §D5 + §D3 #14 + 「D5 的补充表述（2026-10-24）」
- `git show bf78367`（2026-04-15，组合环事故记录：移除 mux + 把 reg_file 提到流水线级）
- `git show bf78367^:include/cpu/pipeline/rv32i_pipeline.h`（事故前的成环接线）
- `src/core/context.cpp:294-311`（`temp_mark` 分支：只豁免 `type_reg` / `type_mem`）与 `:318-352`
  （`cyclic_nodes` 分支 + `type_reg` 只遍历 `init_val`）—— 拓扑排序的差异化处理
- `include/ast/ast_nodes.h:156-159`（`set_driver` 替换 `driver_` 但 `add_src` 累积 ⇒ 双驱动）
- `include/chlib/forwarding.h:184-243`（`ForwardingMux`：既有 mux，档位 0/1/2）
- `include/cpu/pipeline/rv32i_forwarding.h`（`riscv::ForwardingUnit`：4 档、源选择+数据选择合一、**环饵**）
- `openspec/specs/rv32i-isa-semantics/spec.md`（「`JALR` 的 `rs1` MUST NOT 依赖数据前推」⇒ D10 需 MODIFIED）
- `openspec/specs/rv32i-canonical-home/spec.md`（mux 归属 + 「装配后无环」的验证要求）
- 根 `AGENTS.md §KNOWN LIMITATIONS`（load-use 停顿的「丢指令」面 ⇒ T5 仍必需）

### 独立复核记录（oracle，2026-10-27）

本 ADR 的初版分析经 oracle **独立复核**，其 4 处判断被修正并已并入本文：

1. **C 的机制**：初版写「数据晚 1 拍」⇒ 实为「**select 早算一级**」⇒ 修正为 D5 的表述
   （并追加「不打拍会**静默错值**」而非仅「不前推」）
2. **B 过强**：初版称「前推不能覆盖 load」⇒ 实为档位 2（`wb_stage.write_data`）**已覆盖**
   相隔两级的 load ⇒ 修正为 D4 的档位表 + D6 的保守边界（档位 1 不覆盖 load，V2 独立立项）
3. **档位 1 源漏 `link_value`**（初版遗漏）⇒ 并入 D4 的 ⚠️
4. **`ex_stage.rs1_data` 双驱动**（初版完全遗漏）⇒ 并入 D8

⇒ 且 oracle 独立确认：残留成环路径**无**；`type_mem_read_port` **不豁免**（环穿过它照样 FATAL）；
现有测试对「装配级前推」**零覆盖**（⇒ D9 的判别性固件是必需项）。
