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
| 0 = REG | 寄存器堆（`id_stage.rs1_data`） | 与今天的直通值**同源** ⇒ 行为零变化<br>⚠️ **距离 3** 的依赖需要 **D11 的读点旁路**才拿到新值 |
| 1 = EX/MEM | EX/MEM 流水线寄存器 | `select(exmem_is_jump, exmem_link_value, exmem_alu_result)`<br>⚠️ 该 select MUST **收进 `ForwardingMux`**（见下） |
| 2 = MEM/WB | MEM/WB 流水线寄存器 | `wb_stage.io().write_data`（**已合并** load/jump/alu 三路） |

⚠️ **「数据源 MUST 取自流水线寄存器」的准确表述**：数据源的**叶子** MUST 是流水线
寄存器 —— `wb_stage.io().write_data` **不是**字面的 `ch_reg`，它是对 `memwb_alu_result`
/ `memwb_mem_data` / `memwb_link_value` 三个寄存器的**组合 select**（`wb_stage.h:84-87`）
⇒ 「取自寄存器」是**传递性**成立 ⇒ 断环论证（D3）**不受影响**（叶子是寄存器 ⇒ 环断开）。

⚠️ **档位 1 MUST 含 `link_value` 分支**：`JAL`/`JALR` 的写回值是 `pc + 4`（不是 `alu_result`）
⇒ 漏掉它 ⇒ 档位 1 会把 **ALU 垃圾**当成 jump 的结果前推。
（`exmem_link_value` 寄存器已存在，`rv32i_pipeline.h:122`）

⚠️ **该 select MUST 收进 `ForwardingMux`（端口面变化）—— 2026-10-27 缺口分析追加**：

- **动机**：若该 select 留在 pipeline 装配里（内联），则**不可单测** ——
  `ForwardingMux` 的端口面**没有** `is_jump` / `link` 输入
  （`include/chlib/forwarding.h:188-206`），而 `exmem_is_jump` 是 pipeline 的
  **成员 `ch_reg`**（`rv32i_pipeline.h:135`）⇒ **TB 无法驱动**
  ⇒ D9 要求的「定向接线级单测」**照现文本无法落地**
  ⇒ 实施者很可能交一个**复刻 DUT**（自己重建 select）⇒ 测的是**副本**
  ⇒ 对**真实接线零信号** ⇒ **假绿**；
- **裁决**：`chlib::ForwardingMux` 新增 **2 个输入端口** `is_jump` / `jump_result`
  ⇒ 档位 `1` 的数据 = `select(is_jump, jump_result, ex_mem_alu_result)`
  ⇒ **仍是既有组件**（**零新增组件**）⇒ 档位**编号**不变（`0`/`1`/`2`）
  ⇒ **不违反** `rv32i-canonical-home` 的「逐档等价」（该条约束**档位编号**，
  **不是**端口面）⇒ 该分支可由**组件级单测**判别；
- **爆炸半径（实测）**：`ForwardingMux` 的**组件**实例化点**只有 1 个**
  （`examples/riscv-mini/tests/forwarding_dut_fixture.h:91`）
  ⇒ 该 fixture 的 2 个使用者（`test_pipeline_exec.cpp` / `test_rv32i_pipeline.cpp`）
  MUST 显式驱动新端口；
  ⚠️ 同头文件的**函数式接口** `forward_select<>`（`forwarding.h:326-344`）是
  **同一份逻辑的第二实现** ⇒ MUST **同步修改**（否则两个 API 行为分叉）
  ⇒ 其调用点**只有** `tests/chlib/test_forwarding.cpp`（6 处）⇒ 半径可控。

⚠️ **档位 1 的 `link_value` 分支降级为「防御性」要求**：

- 该分支的**失效模式在固件级不可达**：`JAL`/`JALR` 是**无条件**跳
  ⇒ 它的**距离-1 消费者必然落在错误路径上**（fall-through）⇒ 当拍就被 flush
  成气泡 ⇒ **永远到不了真实消费者**
  ⇒ 「`jal ra,…; addi x,ra,…` 紧邻序列会静默错值」这个描述的序列**不会执行**；
- ⚠️ **更正（2026-10-27 缺口分析，机制归因错误）**：
  **没有任何前推档位能命中 jump 生产者** —— 跳转在 **EX 级**就重定向 PC
  ⇒ 目标指令最早在**若干拍之后**才到 ID 级 ⇒ 届时生产者**已离开**可命中的级
  ⇒ 它的返回地址**经寄存器堆写回路径**到达消费者
  ⇒ ⇒ **不属**前推档位的覆盖范围
  （⚠️ 初版本 ADR 写「真正覆盖 jump 生产者的是**档位 2**」是**错的** ⇒ 已更正；
  ⇒ 对应的 D9 第 6 类用例因此**不判别** ⇒ 已从判别性清单移除并降级为回归用例）
- ⇒ ⇒ 该分支 MUST **保留**（防御性，无害）但其验收 **MUST NOT** 靠固件
  ⇒ MUST 由**组件级单测**验收（D9）。

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
- ⚠️ **pipeline 装配内 MUST **保留** `:484` / `:485` 两处 `rs1_data_raw` / `rs2_data_raw` 的驱动**
  —— 初版本 ADR 要求删掉它（「喂了没人读」⇒ ZERO-DEBT），但**该要求与仓库反模式
  「新增组件输入端口 ⇒ 全部实例化点 MUST 显式驱动」正面相撞**
  （未驱动端口读到的是**未定义值** ⇒ 静默错值）
  ⇒ **正确性优先于整洁** ⇒ **保留驱动**（驱动来源 `id_stage.io().rs1_data` 是**合法**的）
  ⇒ 但 MUST 在注释里**如实说明**「该驱动的消费者已迁到 mux，本端口保留是因为它是
  **合法输入**」⇒ 例外必须**显式登记**，不能靠沉默
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

⇒ 实施 MUST 新增**判别性固件**，至少覆盖 **8 类**：

| # | 用例 | 覆盖 | 验收形态 |
|---|---|---|---|
| 1 | **相邻** ALU→ALU，**无 NOP** | 档位 `1`（前推的核心收益） | 固件 |
| 2 | **距离 2** 的 load→use | 档位 `2` | 固件 |
| 3 | **距离 3** 的 load→use | **D11 的读点旁路** | 固件 |
| 4 | **`JALR` 的 `rs1` 紧邻被写** | 档位 `1`（消除 `rv32i-isa-semantics` 的已知限制） | 固件 |
| 5 | **分支条件用紧邻新值** | 档位 `1`（前推给 BEQ/BNE 提供新值） | 固件 |
| 6 | **store 的写数据（rs2）** 紧邻被写 | mux 的 **b 路**（此前零覆盖） | 固件 |
| 7 | **双生产者同 rd** | 档位优先级（`hazard_unit.h:116-126` 注释里的陷阱形态） | 固件 |
| 8 | **`x0` 的负向用例**（生产者写 x0 / 消费者读 x0） | `rd_nz` 门控 | 固件 |

⚠️ **「jump 生产者的前推」NOT 在此列** —— 初版本 ADR 列它（第 6 类）是
**机制归因错误**（D4 已更正：跳转的返回地址**不经**前推档位到达消费者）
⇒ 该用例**不判别**（实为「`link_value` 经寄存器堆通路」的回归测试）
⇒ **降级为回归用例**，MUST NOT 被当作前推的**判别性证据**。

⇒ **每个**判别性用例 MUST 附带**反向期望**（「未接前推时此处读到旧值 `X_old`」）
且 `X_old != X_new` ⇒ 否则该用例**可能已退化**（零信号）⇒ MUST 重写。

⇒ 验收 MUST 包含：

- **双后端逐拍 parity**（无掩码）
  ⇒ ⚠️ 「无掩码」MUST 有**显式的端口清单**（顶层观测面是**有限**的
  ⇒ 不写清单 ⇒ 该判据**不可执行**）；
- **变异验证**（每条都先确认**编译成功** —— 编译失败的变异会跑**旧二进制**
  ⇒ **假绿**）：

  | 变异 | 期望 | ⚠️ 验收形态 |
  |---|---|---|
  | 去掉打拍 | 红（档位错一级） | **固件级**可判别 |
  | 交换档位 1 / 2 的数据源 | 红 | **固件级**可判别 |
  | 前推判定漏 `& !ex_is_load` | 红 | ⚠️ **固件级不可达**（误选只打进气泡 ⇒ 结果被丢弃）⇒ MUST 用**组件级真值表**（独立驱动 `ex_is_load=1` + `ex_reg_write=1` + rd 匹配 ⇒ 断言档位 **≠** `1`） |
  | 档位 1 忘接 `link_value` | 红 | ⚠️ **固件级不可达**（D4）⇒ MUST 用**组件级单测**（直接驱动 mux 的 `is_jump=1` + `jump_result` + 档位 `1` ⇒ 断言 mux 输出 == `jump_result`） |

- **负向对照**：实施期第一屏 MUST 先跑 **bf78367 形态**的最小复现
  ⇒ 确认环检测**仍然 FATAL**（证明护栏有效），再接新形态
  ⇒ 且 MUST **固化为常驻测试**（一次性人工检查 ⇒ 护栏失效无人守）；
- **金标纪律**：所有重算的期望值（内存值 / 停顿拍数 / 退休数 / 控制停顿数）MUST
  **手工推导**并在**代码注释**里留痕 ⇒ MUST NOT 从仿真输出**抄值**
  （那是**循环论证**）；
- `rv32i-canonical-home` spec 已要求的「**装配后无环**」的可执行验证。

### D10. 规格锁同步（MUST 走 MODIFIED delta，不得直接改主 spec）

接前推会**过时**以下既有契约：

| spec | 动作 | 冲突点 |
|---|---|---|
| **`rv32i-data-forwarding`（新建）** | ADDED | 前推的**微架构契约**（档位 / 数据源 / 时间对齐 / **距离矩阵** / 装配无环 / 判别性证据）——⚠️ 它 MUST 是**距离矩阵的唯一权威**（D11）⇒ 其它 capability **MUST NOT** 自述矩阵 |
| `rv32i-isa-semantics` | MODIFIED | 「`JALR` 的 `rs1` MUST 取 EX 级的寄存器值（**MUST NOT 依赖数据前推**）」——本 ADR 裁决「接」⇒ 该条必须 MODIFIED |
| `rv32i-commit-path` | MODIFIED ×2 | ① load-use 停顿的「**无数据前推**」前提（改为「档位 1 **保守地**不覆盖 load」⇒ 停顿契约本身不变）；② 「load-use 自保只覆盖紧邻」的**取值结论**（读旧值 ⇒ 读**新值**）；③ 观测面 / 逐拍 parity 契约（固件变了 ⇒ golden 变了） |
| `verilator-rv32i-e2e` | MODIFIED | 为「无前推」留的 NOP 间隔约定（间隔**不再是**读对新值的前提 ⇒ 且 MUST NOT 被当作前推正确性的证据） |
| `rv32i-canonical-home` | MODIFIED | mux 归属（已要求「装配后无环」）—— 需登记新的验收证据；⚠️ 「逐档等价」条款 MUST 澄清：**档位编号**不变，档位 `1` 在**跳转**指令上的数据是**返回地址** |

⇒ 实施 change MUST 用 **MODIFIED delta** 更新既有契约，并**新建** `rv32i-data-forwarding`
⇒ 在 change 的 tasks 里逐条列出。

⚠️ **文档同步（不在 spec 层，但同样 MUST 做）**：

| 位置 | 过时之处 |
|---|---|
| 根 `AGENTS.md §KNOWN LIMITATIONS` | **3 条**会过时：① 「无数据前推」约束；② 「距离 2 检测不到」；③ 「`JALR` 的 `rs1` 无前推」<br>⇒ 且 **「距离 3」的缺陷当前根本没写** ⇒ MUST **补写**（或随 D11 的旁路一并消除后写明「已消除」） |
| `include/cpu/AGENTS.md` | 「`HazardUnit` 的『预留输入』端口无内部消费者」条目（D7 删端口 ⇒ 该条目消失） |
| `include/cpu/pipeline/stages/ex_stage.h:200-202` | 「`JALR` 的 `rs1` 取 `io().rs1_data` ⇒ **无数据前推**（ADR-046 D5 悬置）」⇒ 本 ADR 裁决「接」⇒ 过时（⚠️ 且 `JALR` 的 PC 重定向路径从此**经过 mux** ⇒ 组合深度 +1） |
| `include/cpu/pipeline/hazard_unit.h:83-86` | 「前推数据输出（**经过前推MUX 后的数据**）」⇒ **今天就是假的**（`:198-199` 是**直通**）⇒ MUST 改为如实描述 |
| `examples/riscv-mini/tests/forwarding_dut_fixture.h` 头注释 | 该 fixture **不打拍**、**不含** link-select ⇒ 落地后它**不再镜像**真实流水线接法 ⇒ MUST **显式声明**它**不是**装配级证据（否则「装配错了」对它零信号） |

### D11. **距离 3** 的同沿竞态 MUST 由**寄存器堆读点旁路**解决（不是由 hazard 解决）

⚠️ **本条是 2026-10-27 的缺口分析（metis）追加的**，初版 ADR **遗漏**了它
⇒ 它**推翻** §3「固件不再需要插 NOP」的承诺 ⇒ **先读本条**再读后果。

#### 缺陷：ID/EX 锁存沿与 WB 写回沿**同沿**

| 事实 | 位置 |
|---|---|
| ID/EX **锁存**了寄存器堆的**组合读** | `id_stage.h:255`（`idex_rs1_data->next = select(idex_update_en, io().reg_rs1_data, idex_rs1_data)`） |
| 寄存器堆读点**无 WB 旁路** | `rv32i_pipeline.h:171-178`（`reg_file.aread(...)` 直连 `id_stage.io().reg_rs1_data`） |
| 写回发生在**同一条 posedge** | `rv32i_pipeline.h:180`（`reg_file.write(...)`）⇒ `ch_mem` 的写是 posedge |

⇒ 在「ID→EX 锁存」的那条沿上，寄存器堆里还是**写回前**的值
⇒ ID/EX 锁存到的是**旧值**。

⚠️ **Verilog 侧同构**（两个 `always_ff` 同沿 + 非阻塞赋值 ⇒ 都读旧值）
⇒ **两后端一致地错** ⇒ **双后端 parity 抓不到它** ⇒ MUST 靠**判别性固件**抓
（D9 第 3 类）。

#### 后果：距离矩阵出现一个洞（1 ✓ / 2 ✓ / **3 ✗** / ≥4 ✓）

| 距离 | 生产者所在级（消费者在 ID 时） | 可命中的前推档位 | 结论 |
|---|---|---|---|
| 1 | EX | 档位 `1` | ✅ |
| 2 | MEM | 档位 `2` | ✅ |
| **3** | **WB** | **无** —— hazard 只比较 EX / MEM **两级** ⇒ 生产者**已离开**可命中的级；且档位 `2` 的数据源 `wb_stage.write_data` 此时**也已不是**该生产者 | ❌ **读旧值** |
| ≥ 4 | 已退休 | 无（前推不需要） | ✅（走寄存器堆路径 ⇒ 写回早已落地） |

⚠️ **距离 3 的洞「今天就存在」**（无前推时同样读旧值）⇒ **不是**接前推引入的
⇒ 现有固件用 **3 条 NOP（距离 4）恰好躲开** ⇒ **从未被文档化**。
⚠️ 后果：「隔 2 条不行、隔 1 或 3 条才行」是**无法向固件作者交代**的约束
⇒ MUST **修**，不能只登记为限制。

#### 裁决：在**寄存器堆读点**加 WB→ID **组合旁路**

形态（pipeline 级，约 10 行，`rv32i_pipeline.h:171-178`）：

```
auto wb_bypass_hit_a = wb_write_en
                     & (wb_stage.io().rd_addr_out == id_stage.io().rs1_addr_id);
id_stage.io().reg_rs1_data <<= select(wb_bypass_hit_a,
                                      wb_stage.io().write_data,
                                      rs1_data_result);
```

⇒ **不成环**：旁路的**数据叶**是 `memwb_*` **流水线寄存器**
（`wb_stage.io().write_data` = 对 `memwb_alu_result` / `memwb_mem_data` /
`memwb_link_value` 的组合 select；`rd_addr_out` ← `memwb_rd_addr`；
`write_en` ← `memwb_valid`）⇒ 叶子是**寄存器** ⇒ 环断开（D3 的机制）
⇒ 且 `type_mem_read_port` **不豁免** ⇒ 旁路的**地址链**也止于寄存器之后
⇒ 不触发 `CHFATAL`；
⇒ **与 D7 正交**：**不需要** hazard 的 `wb_reg_write` / `wb_rd_addr` / `wb_write_data`
⇒ 那 3 个端口仍然**可以删**（见 §4「否决」）；
⇒ **`x0` 无需特判**：`wb_write_en` 已含 `rd_addr_out != 0`（`rv32i_pipeline.h:179`）
⇒ 命中时 `rs1_addr_id` 必然非 0；
⇒ **两侧对称**：`rs1` / `rs2` 各一条（`rs2_data_result` 同理）。

⚠️ **时序纪律**：旁路是**组合**的 ⇒ 它在「写回那一拍」就把**新值**喂给 ID 级
⇒ ID/EX 在**同一条沿**上锁存到**新值** ⇒ 距离 3 的依赖被修复。
⚠️ **不改变任何既有档位的语义**：旁路只影响**档位 0**（寄存器堆读）的**取值**
⇒ 档位 1 / 2 命中时旁路值被 mux 丢弃 ⇒ **互不干扰**。

#### **距离矩阵 = 本 ADR 的契约唯一权威**

⚠️ 「距离 N 的依赖拿到什么值」是**固件作者唯一需要知道**的契约
⇒ 它 MUST 有**唯一**出处 ⇒ **本表即该出处**
⇒ 实施 change 新建的 `rv32i-data-forwarding` capability MUST **逐字**承载本表
⇒ 其它 capability / 文档 **MUST NOT** 另写一份（否则必然漂移）
⇒ ⚠️ `rv32i-commit-path` 里那条「load-use 自保只覆盖紧邻」的 Scenario
MUST **引用**本矩阵，**MUST NOT** 自述矩阵。

## 3. Consequences

### 正面

- 相邻 ALU→ALU 依赖**零停顿** ⇒ 固件不再需要为它插 NOP
- 距离 2 的 load→use 被**档位 2** 覆盖 ⇒ 「距离 2 读旧值」的缺口关闭
- 距离 3 的 load→use 被 **D11 的读点旁路**覆盖 ⇒ 「距离 3 读旧值」的缺口关闭
  （⚠️ 该缺口**今天就存在**，本 ADR 顺带修掉 ⇒ **不接前推也得修**）
- ⇒ **配合 D11**，固件对 ALU 依赖 / load 依赖**不再需要插任何 NOP 间隔**
- `JALR` 的 `rs1` 限制**消除** ⇒ `rv32i-isa-semantics` 的那条约束可放宽
- 分支条件可用**紧邻**的新值 ⇒ 分支逻辑更短
- 顺带清掉 5 个预留端口（含**环饵** `ex_alu_result`）⇒ ZERO-DEBT 合规
- 顺带收敛 `ex_stage.rs1_data` 的双驱动 ⇒ 消除双后端分叉隐患

### 负面 / 代价

- **load-use 停顿仍在**（3 拍 + 「丢弃消费者后第一条指令」）⇒ **T5（skid buffer）仍然必需**
  ⇒ 本 ADR **不**让 T5 变成冗余
- ⚠️ **D11 的读点旁路是「组合旁路」** ⇒ 它在写回那一拍把新值喂给 ID 级
  ⇒ **固件的依赖间隔约束被取消**（不再需要「隔 2 条」这类**不可解释**的规则）
  ⇒ 代价：ID 级的组合逻辑多一个 `select`（深度 +1）
- ⚠️ **`ForwardingMux` 的端口面变化**（新增 `is_jump` / `jump_result`）⇒
  `chlib` 的**公共 API** 破坏性变更（爆炸半径小：组件实例化点**只有 1 个**，
  函数式接口 `forward_select<>` 的调用点**只有 1 个测试文件**）
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
| **距离 3 用 hazard 的**第三级**前推解决**（把 `wb_reg_write` / `wb_rd_addr` / `wb_write_data` 用起来） | ① 它要求 mux 有**第 4 个档位**（`ForwardSource::RESERVED = 3` 目前是**未用**的）；② 它把 D7 要删的 3 个端口**重新变成必需** ⇒ 与 D7 冲突；③ 档位 3 的语义与「档位 0 = 寄存器堆读」**重叠**（WB 的写回值**就是**下一拍寄存器堆里要写的值）⇒ 用旁路表达**更直接**、更少状态 |
| **距离 3 登记为残留限制**（不修） | 「隔 2 条不行、隔 1 或 3 条才行」是**无法向固件作者交代**的约束 ⇒ 与 §3「固件不再需要插 NOP」**直接矛盾** ⇒ 只能作为**次选**（若实现期发现旁路有未预见的副作用）|

## 5. 实施追踪

### 5.1 预期改动面（供 change 立项时对照）

| 文件 | 改动 |
|---|---|
| `include/cpu/pipeline/rv32i_pipeline.h` | 加 `ForwardingMux` 实例；加 `fwd_a_d1`/`fwd_b_d1` 两个 `ch_reg`；接线 3 个前推源；**删 `:200`/`:201` 的旧驱动**（D8）；删 5 个预留端口的驱动（D7）；**加 D11 的寄存器堆读点旁路**（rs1 / rs2 各一条） |
| `include/cpu/pipeline/hazard_unit.h` | 删 5 个预留端口（D7）；前推判定加 `& !ex_is_load`（D6）；修 `:83-86` 的**假注释**（「前推数据输出（经过前推MUX 后的数据）」—— 今天 `:198-199` 是**直通** ⇒ 该注释**今天就是假的**） |
| `include/chlib/forwarding.h` | `ForwardingMux` 新增 2 个输入端口 `is_jump` / `jump_result`（D4）；**同步**函数式接口 `forward_select<>`（同一份逻辑的第二实现 ⇒ 否则两个 API 行为分叉） |
| `include/cpu/pipeline/stages/ex_stage.h` | （无端口变化；`rs1_data` 的**驱动源**变）；修 `:200-202` 的**过时注释**（「`JALR` 的 `rs1` 取 `io().rs1_data` ⇒ **无数据前推**（ADR-046 D5 悬置）」⇒ 本 ADR 裁决「接」⇒ 该注释过时） |
| `examples/riscv-mini/tests/forwarding_dut_fixture.h` | 补 `ex_is_load` 驱动（D6 的连带约束）；**新增 `is_jump` / `jump_result` 端口并显式驱动**（D4 的端口面变化 ⇒ 该 fixture 是 `ForwardingMux` 的**唯一**组件实例化点）；头注释补「本 fixture **不打拍**、**不含**打拍后的对齐 ⇒ **不是**装配镜像 ⇒ MUST NOT 当作装配级证据」 |
| `tests/chlib/test_forwarding.cpp` | 同步函数式接口 `forward_select<>` 的新参数（6 处调用） |
| `examples/riscv-mini/tests/test_rv32i_pipeline.cpp` | **删 4 个区块 × 5 端口 = 20 处** `set_input_value`；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_pipeline_exec.cpp` | **删 8 个区块 × 5 端口 = 40 处** `set_input_value`；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_pipeline_debug.cpp` / `test_wrapper_simple.cpp` | 各删 5 处驱动；补 `ex_is_load` 驱动 |
| `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` | 判别性固件 + golden 全量重算（D9） |

### 5.2 阶段拆分建议

| 阶段 | 内容 | 可独立验收 |
|---|---|---|
| **C1** | 前推**接线**（mux + 打拍 + 源）+ **D11 的读点旁路** + 判别性固件 + 变异验证 | ✅ 双后端 parity + **8 类**判别性用例 + **2 条组件级**验收（`& !ex_is_load` 门控 / 档位 1 的 `link_value`）+ **档位对齐的实测**（距离由实测钉住，MUST NOT 预先断言） |
| **C2** | 端口清理（D7）+ 双驱动收敛（D8）+ `ForwardingMux` 端口面（D4）+ 文档/spec 同步（D10） | ✅ 护栏 + spec delta |

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
- `include/cpu/pipeline/stages/id_stage.h:255`（ID/EX **锁存**寄存器堆组合读 ⇒ D11 的竞态根源）
- `include/cpu/pipeline/rv32i_pipeline.h:171-180`（寄存器堆读点 / 写回点 ⇒ D11 的旁路位置）
- `include/cpu/pipeline/stages/wb_stage.h:84-87`（`write_data` 是对 `memwb_*` 的**组合 select**
  ⇒ 「数据源的**叶子**」是寄存器 ⇒ 断环论证成立）

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

### 修订记录（2026-10-27，缺口分析后的修订）

本 ADR 经过 **2 轮 critic 对抗审稿**（第 1 轮 APPROVE_WITH_CONDITIONS ⇒ 10 条处置
全部 CLOSED；第 2 轮 **APPROVE**）。此后 planner 起草实施 change 时跑的**缺口分析
（metis）**找出 **3 处架构级缺陷** ⇒ 本次修订：

| # | 缺陷 | 修订 |
|---|---|---|
| 1 | **距离 3 的同沿竞态**（ID/EX 锁存沿 == WB 写回沿）⇒ ADR 的形态**修不了它** ⇒ §3「不再需要插 NOP」的承诺**不成立** | **新增 D11**（寄存器堆读点的 WB→ID 组合旁路）+ **距离矩阵成为契约的唯一权威** |
| 2 | 档位 1 的 `link_value` 变异**没有可落地的 harness**（mux 无 `is_jump`/`link` 端口；`exmem_is_jump` 是 pipeline 的成员 `ch_reg` ⇒ TB 无法驱动）⇒ 实施者会交**复刻 DUT**（**假绿**） | **D4 改写**：该 select **收进 `ForwardingMux`**（端口面变化）⇒ 降级为**防御性**要求 + **组件级**验收；`& !ex_is_load` 变异同样改**组件级真值表** |
| 3 | D4 / D9 第 6 类的**机制归因错误**（跳转在 EX 级就重定向 PC ⇒ 目标指令若干拍后才到 ID ⇒ **没有任何前推档位能命中 jump 生产者**） | **D4 更正**：跳转的返回地址**经寄存器堆写回路径**到达消费者 ⇒ **不属**前推档位的覆盖范围；D9 第 6 类**降级为回归用例**（不判别） |

⚠️ 另修 **D7**（pipeline 内 `:484`/`:485` 的驱动**保留**而非删除）—— 与仓库反模式
「新增组件输入端口 ⇒ 全部实例化点 MUST 显式驱动」正面相撞 ⇒ **正确性优先于整洁**。

⚠️ **一条方法论教训**：本 ADR 过了 2 轮**架构**审稿却漏了这 3 条 ⇒ 因为它们全是
**实现层时序细节**（锁存 vs 写回的同沿竞态 / mux 的端口面 / 跳转的到达路径）
⇒ **架构审稿 ≠ 时序审稿** ⇒ 后者只能靠**组件级真值表 + 判别性固件**兜住
⇒ 已写进 D9 的验收判据。
