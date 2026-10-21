# Change: sim-mem-read-timing-align-2026-10 — 让 Simulator 的 `aread` 与生成的 Verilog 0-cycle `assign` 对齐

> **状态：已实施（2026-10-21，分支 `feature/sim-mem-read-timing-align-2026-10`）。**
> 本 change 立项自 2026-10-21 会话中的一次未提交实验（"EXPERIMENT B2"）。
> 该实验已被**判定不可直接提交**（实测 5 个测试转红，根因见下），
> 其 diff 完整保存在本文的「附录 A」中，作为本 change 的**起点**而非结论。
> 实施结果见 `tasks.md` 顶部的执行结果表。

## Why

`ch_mem::aread`（async_read）在两条路径上语义**不一致**，且该差异已被登记为「已知 drift」：

| | 生成 Verilog | Simulator |
|---|---|---|
| `aread` | `assign proxy = mem[addr];`（组合读，**0-cycle**） | 在 `sequential_instr_list_` 的 posedge 求值 → **滞后 1 拍** |
| `sread` | `always_ff`（**1-cycle**） | posedge 求值（1-cycle） |

该 drift 由 capability `mem-read-timing` 显式登记（Requirement「known simulator-vs-Verilog
drift is documented」），并在 `docs/simulation/ARCHITECTURE.md`「Memory read port timing」
与根 `AGENTS.md` CONVENTIONS 中重复说明。

### drift 的精确边界（2026-10-21 实测，探针程序）

drift **只在「读地址经过寄存器」时出现**；读地址直接来自输入端口时，两条路径**本来就一致**：

| 读地址来源 | Simulator `aread` | Verilog `assign` | 是否 drift |
|---|---|---|---|
| 顶层输入端口 | 同拍可见新地址 | 同拍可见新地址 | ❌ 无 |
| **寄存器（`ch_reg`）** | **滞后 1 拍** | 立即反映新地址 | ✅ **有** |

实测（寄存器驱动 `ch_reg<ch_uint<4>> addr` → `mem.aread(addr)`，mem[3]=0xD4 / mem[4]=0xE5）：

```
【改前】tick1 (reg<-4): aread=0xA1   ← 期望 0xE5，拿到的是旧地址
       tick2 (reg<-3): aread=0xE5   ← 期望 0xD4，仍滞后一拍
       tick3 (settle): aread=0xD4

【改后】tick1 (reg<-4): aread=0xE5   ← 0-cycle，同拍反映新地址 ✅
       tick2 (reg<-3): aread=0xD4   ← 0-cycle ✅
       tick3 (settle): aread=0xD4
```

同一探针里 `sread` 的读数**改前改后完全一致**（0xA1 / 0xE5 / 0xD4）→ **Non-Goal #1（`sread` 时序不变）成立**。

**机制**：`ch_reg` 的值经由其**值代理节点**（`ch_reg` 的 `node_impl_` 是 proxy，见
`include/lnode/reg.tpp:53`）传给读端口；代理在**组合遍**刷新，而组合遍跑在
**时钟沿之前**（`src/simulator.cpp:308`）→ 沿上求值的读端口读到的是**上一拍**的地址。
`ch_mem::aread` 的消费者里，**寄存器驱动**的只有 FIFO `raddr` 模式
（`samples/fifo_example.cpp:75`、`samples/fifo_bundle.cpp:75`、
`examples/spinalhdl-ported/fifo/stream_fifo_example.cpp:60`、`tests/test_fifo_example.cpp:80`）；
其余（`branch_predictor_v2.h`、`i_cache_async.h` / `d_cache_async.h`）的地址是
**输入驱动的组合逻辑** → **不受本 change 的语义影响**（但仍受 codegen 改动影响，见 D1）。

> ⚠️ 由此推出两个对任务设计的硬约束：
> 1. **能暴露 drift 的 RED 用例必须用寄存器驱动地址**。输入驱动地址在修复前后行为
>    一致 → 用既有 `CombReadFixture`（其 `addr_async` 是顶层输入）**写不出 RED**。
>    详见 `tasks.md` Task 2。
> 2. **FIFO 读数据在 pop 拍会移位一拍**（见 D6 的实测记录）→ 回归面必须覆盖
>    FIFO 的**行为**断言，而不只是「能跑通」。

其代价是：

1. **寄存器驱动的读地址有 1 拍 skew**（Verilog 立刻反映新地址，Simulator 滞后一拍）；
2. **同拍写+读竞态的返回值不同**（Simulator 返回写前值，Verilog 返回写后值），
   被 capability `mem-read-timing` 的 Requirement
   「same-cycle write+aread race has an executable pre-write regression fence」
   以**可执行层锁**的形式钉死（`tests/test_mem_comb_read.cpp` 的 `[mem][aread][race]` 用例）。

本次立项的目标是**消除这条 drift**，让 Simulator 的 `aread` 与 Verilog 的 0-cycle `assign`
对齐 —— 即「aread 在地址变化的同一拍就反映新值」。

### 为什么不能直接提交那个实验

2026-10-21 的会话里做过一次未提交实验（附录 A），实测**不可提交**：

1. **Verilog codegen 被破坏（5 个测试转红）**：实验给 `data_output` 补了
   `add_src(read_port)` 数据流边 → `proxyimpl` 拿到 `src(0)` →
   `src/codegen_verilog.cpp::print_proxy` 额外发射 `assign <proxy> = <read_port>`；
   而 `type_mem_read_port` 节点**不在** wire 声明列表（`print_header` 的 wires 收集）→
   生成的 Verilog 引用**未声明信号** + 同一 wire **双驱动**。
   转红的测试：`test_cache_sync` / `test_branch_predictor_v2` /
   `test_verilator_memory_e2e` / `test_verilator_complex_e2e` /
   `spinalhdl_branch_predictor_v2`。
2. **违反既有 spec**：同一会话更早的一个 commit（`5fec554`，标题为 *docs(openspec)*）
   夹带了「方案 B」——把 async read port 从 `sequential_instr_list_` 挪进
   `combinational_instr_list_` —— 该改动**直接违反** capability `mem-read-timing` 的
   Requirement「Simulator classifies mem read ports as sequential」，
   并让 `test_mem_comb_read` 的 `[mem][aread][race]` 钉住用例转红
   （Simulator 变成返回写后值）→ **HEAD 自该 commit 起即为红**。
   该夹带改动已于 `7aefdfe` 回滚。

→ 因此本 change 不是「提交那个实验」，而是「**带着已知 blocker 重新立项**」。

## What Changes

### D1: codegen 必须能表达「读端口 → 数据代理」的数据流边

若要把读端口的求值排在消费者之前（消除组合阶段的陈旧读），
就需要在 AST 上建立 `read_port → data_proxy` 的数据流边（B2 实验的做法）。

⚠️ **该边会改变 Verilog codegen 的发射面**：`proxyimpl` 拿到 `src(0)` 后
`print_proxy`（`src/codegen_verilog.cpp:1019-1036`）会额外发射一条 `assign`。

> **方案 A / 方案 B 的裁决（2026-10-21 定稿，不再留给实施者）**：
>
> - **方案 B（只在 codegen 侧排序，不改 AST 形状）作废** —— interpreter 的求值顺序
>   来自 `context::get_eval_list()` 的拓扑排序（`src/core/context.cpp:131-164`），
>   而该排序**只沿 `src` 边递归**（`src/core/context.cpp:356-368`）
>   → 不加 `src` 边就无法表达「端口先于代理」→ 方案 B 对 interpreter **无效**。
> - **采用方案 A**，按下列 **A′ 四步**实施。
>
> | # | 动作 | 位置 | 目的 |
> |---|---|---|---|
> | A′.1 | 建立 `read_port → data_proxy` 的 `src` 边 | `include/ast/mem_port_impl.h` | 让拓扑排序把读端口排在代理之前 |
> | A′.2 | `print_proxy` **跳过**「其 `src(0)` 是 mem read port」的代理 | `src/codegen_verilog.cpp:1019` | 避免 `assign proxy = <read_port>` 与 `print_mem_read_port` 的 `assign proxy = mem[addr]` **双驱动** |
> | A′.3 | `print_output` 的代理链遍历**不得越过 mem read port** | `src/codegen_verilog.cpp:572-579` | 见下方「A′.3 的坑」—— **只做 A′.2 会生成非法 Verilog** |
> | A′.4 | `type_mem_read_port` **MUST NOT** 加入 `print_header` 的 wire 声明列表 | `src/codegen_verilog.cpp:425-485` | 读端口在 Verilog 里是**虚构节点**；A′.3 修好后它不会被任何 `assign` 引用 → 声明它只会产生一个**声明但无驱动**的 `logic`（违反 ZERO-DEBT POLICY 的 "No dead code"） |

> #### ⚠️ A′.3 的坑（2026-10-21 实测发现，**原提案与本 change 的既有裁决都未覆盖**）
>
> `print_output` 会沿代理链向上找「真正的驱动者」：
>
> ```cpp
> lnodeimpl *driver = output_node->src(0);              // → data proxy
> while (driver && driver->type() == type_proxy &&      // data proxy 现在 num_srcs()==1
>        driver->num_srcs() > 0) {
>     driver = driver->src(0);                          // → **mem read port**
> }
> out << "assign " << output_name << " = " << driver_name << ";\n";
> ```
>
> 加了 A′.1 的边之后，data proxy 的 `src(0)` 是 **mem read port** → 遍历会**越过
> proxy 走到 read port** → 发出 `assign top_unnamed_output_1 = top_async_port;`
> —— 而 `top_async_port` **在 Verilog 里没有任何信号**（读端口是虚构节点）→
> **生成的 Verilog 引用未声明信号 → 语法错误**。
>
> 实测（只加 A′.1 + A′.2，未加 A′.3）生成的 Verilog：
>
> ```verilog
> assign top_unnamed_output   = top_mem_sread;      // ← 未声明信号
> assign top_unnamed_output_1 = top_async_port;     // ← 未声明信号
> ```
>
> **修法（A′.3）**：遍历时**停在 mem read port 之前**，即把 data proxy 本身当作
> Verilog 侧的 driver（它确实被 `print_mem_read_port` 的 `assign proxy = mem[addr];`
> 驱动）。修好后实测生成：
>
> ```verilog
> assign top_unnamed_output   = top_mem_sread_data_proxy;
> assign top_async_port_data_proxy = top_cmp_read_mem[top_unnamed_input_1]; // Comb read
> assign top_unnamed_output_1 = top_async_port_data_proxy;
> ```
>
> **所有引用的信号均已声明，无双驱动。**
>
> > 补充：`print_input`（`codegen_verilog.cpp:620-626`）走的是**只穿未命名代理**的分支，
> > 而 mem 的 data proxy **恒为具名**（`ch_mem::aread` 命名 `<name>_data_proxy`）
> > → 遍历会停在该 proxy 上 → 不受影响。MUST 仍加同样的护栏以防未来出现未命名的情形。
> > `src/codegen_dag.cpp` 只做节点类型名映射（`:192`），**无代理链遍历** → 不受影响。
> > `src/ast/ast_nodes.cpp:183-199`（`proxyimpl::create_instruction`）的遍历在
> > mem read port 处自然终止（它不是 proxy）→ 得到的 `src_buf` 正是读端口自己的槽位 → 正确。

> **2026-10-21 补充实测（决策依据）**：在同一会话中对 B2 做了**变体实验**，
> 得到与「方案 A」倾向一致的关键证据：
>
> | 变体 | 改动 | 上游 mem 套件结果 |
> |---|---|---|
> | B2-原样（附录 A） | 加 `src` 边 + **仅 async** 写自身槽位 | `test_mem_timing` / `test_mem_connection` / `test_memory` **3 个套件转红**（sync 仍写代理槽位、而代理已改为从 sync 端口自身槽位取源 ⇒ **sync 读到 0**） |
> | B2-变体 | 加 `src` 边 + **async 与 sync 都**写自身槽位 | 3 个套件**恢复全绿**；仅剩 `test_mem_comb_read` 的竞态用例 1 处红（**预期** —— 竞态钉住值从「写前值」反转为「写后值」，属 D4 的显式契约变更） |
>
> ⇒ **裁决结论（Task 0）**：
> 1. **加 `src` 边是必要的**（无 `src` 边则 interpreter 拓扑序无法表达
>    「端口先于代理」）⇒ 排除「方案 B（只在 codegen 侧排序）」，**采用方案 A（A′）**；
> 2. **读端口写自身槽位必须 async + sync 同步改**（只改 async 会让 sync 读到 0）；
> 3. 变体实测的**上游侧净破坏面 = 1 个断言**（竞态契约反转），
>    而 B2-原样是 **3 个套件** ⇒ **变体是更小的起点**。
>
> ⚠️ **变体仍需 A′.2 / A′.3 / A′.4 的 codegen 配套**：加 `src` 边后 `print_proxy`
> 会为该代理额外发射 `assign <proxy> = <read_port>`，而 `type_mem_read_port` 不在
> wire 声明列表 ⇒ **未声明信号 + 双驱动**；`print_output` 的代理链还会**越过 proxy
> 走到 read port** ⇒ 引用未声明信号。**MUST 同时解决 A′.2 / A′.3 / A′.4 三点。**
> （2026-10-21 实测：变体 + codegen 未配套时，生成 Verilog 的 5 个套件转红。）
>
> ⚠️ **本 change 的收益已被下游实测确认**：该上游改动落地后，**ChipForge 侧
> 的 10 个 rv32ui load/store ELF 全部转绿**（`[chmem]` 从 10 failed → 0 failed，
> 47 个 vendored ELF 全 PASS，`[verilator]` 4 case / 122 assertions 不变）
> ⇒ 该 change 是 ChipForge 那 10 个红点的**唯一零数据通路改动**的解法
> （ChipForge 侧改 `sread` 单独实测反而从 10 恶化到 11 个红点）。
>
> > **2026-10-21 本仓库端到端验证**（A′ 全量 + D2 实装，临时补丁已回滚）：
> > `test_mem` / `test_mem_timing` / `test_mem_connection` / `test_fifo_example` /
> > `test_cache_sync` / `test_branch_predictor_v2` **全绿**；
> > `test_mem_comb_read` **恰好 1 处失败** = 竞态契约反转（D4 的预期变更）；
> > 探针确认 `aread` 变 0-cycle、`sread` 逐位不变。


### D2: Simulator 的 async read port 归类

`src/simulator_init.cpp`：`type_mem_read_port` 的 **async** 子类改入
`combinational_instr_list_`（sync 仍留在 `sequential_instr_list_`）。
→ 这是 capability `mem-read-timing` R3 的 **MODIFIED** 点。

**求值链条（已核对源码，实施后成立）**：

```
async read port 槽位   ← instr_mem_async_read_port::eval() 写自身槽位
        ↓ (instr_proxy, 组合表)
data proxy 槽位        ← proxyimpl::create_instruction()（src/ast/ast_nodes.cpp:183-199）
        ↓ (instr_output, 组合表)
输出端口槽位           ← outputimpl::create_instruction()（src/ast/ast_nodes.cpp:213-223）
```

三者**全部落在 `combinational_instr_list_`**（`type_proxy` / `type_output` 走
`src/simulator_init.cpp:122-126` 的 `default` 分支），且拓扑序保证
`read port → proxy → output` → **同一遍组合求值内完成传递**。

`Simulator::tick()` 内的组合遍位置（`src/simulator.cpp:308 / 372 / 328`）：

```
eval_combinational()           ← 沿**前**：读端口读到沿前状态
eval() → eval_sequential()     ← 沿上：寄存器提交 / 写端口落盘
eval() → eval_combinational()  ← 沿**后**：读端口用**新**地址 / **新**内存内容重读  ✅
```

→ 这正是「0-cycle 反映新地址」与「同拍写+读返回写后值」得以成立的机制依据。

### D3: 读端口的输出落点

**async 与 sync 读端口都改为写自身的 `data_map` 槽位**（而非直接写代理槽位），
由代理的 `instr_proxy` 完成「端口值 → 代理值」的搬运。

> **MUST async + sync 同步改**（裁决结论 #2）：`mem_read_port_impl` 的构造函数被
> async 与 sync **共用**（`include/ast/mem_port_impl.h:81-100`）→ A′.1 的边对两者
> 都生效。若只把 async 改成写自身槽位，sync 的 data proxy 会改为从
> **sync 端口自己的槽位**取源，而该槽位**永远无人写**
> → **`sread` 恒返回 0 且不报任何错**（实测：`test_mem_timing` /
> `test_mem_connection` / `test_memory` 3 个套件转红）。
>
> **对 `sread` 观测语义的影响：无**。sync 读端口仍在 `sequential_instr_list_`
> （沿上求值），其数据代理在**沿后**的组合遍把值搬过去 → tick 结束时观测值与
> 改前逐位一致（探针实测：`sread` 改前改后均为 0xA1 / 0xE5 / 0xD4）。
> 即：**实现形态变了，观测时序没变**（Non-Goal #1 成立）。

`data_map` 中读端口节点自身的槽位**已存在且宽度正确**（**无需新增工作**）：

- `src/simulator_init.cpp:41-75` 对 `eval_list_` 的**每个**节点分配
  `data_map_[node_id] = sdata_type(0, size)`；
- `ch_mem::aread` / `sread` 传给 `create_mem_read_port` 的 size 就是 `data_width`
  （`include/core/mem.h:145-147` / `:185-187`）→ 槽位宽度 == `DATA_WIDTH`。

→ 实施时只需**确认**（非新增代码）。

### D4: spec delta（`mem-read-timing` RENAMED ×2 + MODIFIED ×3）

| Requirement | 变更 |
|---|---|
| 「Simulator classifies mem read ports as sequential」 | → RENAMED + MODIFIED 为「async read 归组合、sync read 归时序」 |
| 「known simulator-vs-Verilog drift is documented」 | → MODIFIED：删掉「register-driven read address skew」场景（skew 消除），改写竞态场景 |
| 「same-cycle write+aread race has an executable pre-write regression fence」 | → RENAMED + MODIFIED：钉住值从「写前值」改为「写后值」，测试用例 MUST 同步改写 |

⚠️ 这三条 Requirement 是**可执行层锁**（它们点名了具体测试与断言），
改它们 MUST 与代码改动**同一个 commit** 落地，MUST NOT 先改 spec 后改代码。

⚠️ **delta 的 Scenario MUST 用「寄存器驱动地址」表述，不能用「输入阶段改地址」**：

- 「输入阶段改地址」在**修复前就成立**（输入驱动的 aread 本来就同拍可见）
  → 这样的 Scenario **区分不了修复前后** → 作为「可执行层锁」**锁不住任何东西**；
- 同拍写+aread 竞态的 Scenario 若也改成寄存器驱动地址，则**失去**对既有
  `[mem][aread][race]` 用例（输入驱动）的覆盖 → **必须保留输入驱动的竞态用例**。

⇒ delta 的最终形态：aread 0-cycle 场景用**寄存器驱动**、sread 场景用**寄存器驱动**
（并说明它本就与 Verilog `always_ff` 一致）、竞态场景**保持输入驱动**。

### D5: 文档同步

- `docs/simulation/ARCHITECTURE.md`「Memory read port timing」小节
  （删 skew、改竞态语义；**行号引用改为函数名**，避免改完行号漂移）
- 根 `AGENTS.md` CONVENTIONS 的「Mem read port」条目
- 根 `AGENTS.md`「已消除的过时信息」段中 2026-10-21「撤回 mem 读端口时序实验」那条
  （追加：2026-10-21 以本 change 正式立项并实施）
- `src/codegen_verilog.cpp::print_mem_read_port` 的大段注释（它详细描述了当前 drift）
- `openspec/specs/mem-read-timing/spec.md` 的 `## Purpose`
  （现文仍写 "the ChipForge dmem design tolerates via a downstream register" → 需改写）

### D6: 回归面（必须全绿）

改动触及 **Simulator 的时序语义**，影响面远超 `ch_mem` 本身：

| 回归 | 为什么必须跑 |
|---|---|
| `test_mem_comb_read` | 钉住竞态返回值（必改）+ aread codegen 发射形态（A′ 下会变） |
| `test_mem` / `test_mem_timing` / `test_mem_connection` | 核心内存语义；**sync 读到 0 的陷阱就在这三套件里** |
| `test_cache_sync` / `test_cache_complete` | cache 用 `ch_mem` + 寄存器提供 1-cycle 依赖 → **skew 消除后 cache 的时序可能整体移位** |
| `test_branch_predictor_v2` / `spinalhdl_branch_predictor_v2` | 用 `ch_mem` 存预测表（地址是**输入驱动**的 → 语义不受影响，但 **codegen 受影响**） |
| `test_verilator_memory_e2e` / `test_verilator_complex_e2e` | 双后端 parity（Verilog 侧不变 → 两侧差异会暴露） |
| `test_rv32i_pipeline` / `test_rv32i_pipeline_behavior` | **imem MUST 用 `sread`**（`pc_lag` 对齐依赖 1-cycle）→ 改 `aread` 不应影响，但必须实证 |
| `perf_tests --all` | 时序变化可能影响吞吐（perf 是 15-20 min 的独立套件） |

#### ⚠️ FIFO：语义确实移位了，但**现有护栏全是无效的**（2026-10-21 实测）

FIFO 的 `dout` 是**寄存器驱动的 `aread`** → skew 消除后 **pop 拍上的读数据移位一拍**：

| | Cycle 3 | Cycle 4 |
|---|---|---|
| 改前 | `dout=0x1`（队首） | `dout=0x2` |
| 改后 | `dout=0x2` | `dout=0x1` |

**这不是设计回归** —— Verilog 侧 `assign dout = mem[rd_ptr]` 中 `rd_ptr` 在 pop 沿
前进，连续赋值立即反映新指针 → **改后的行为才与 Verilog 一致**；改前是
「drift 恰好补偿了测试台的采样时机」。

但问题在于**没有任何自动化断言能发现它**：

| 目标 | 内容 | 能否发现移位 |
|---|---|---|
| `tests/test_fifo_example.cpp` | **全是 `REQUIRE(x.impl() != nullptr)` 结构检查**，无任何行为断言 | ❌ **无效护栏** |
| `samples/fifo_example.cpp` | 打印 `dout` 但**从不校验**（`:179-193` 只查 `empty`/`full`） | ❌ 且**不在 ctest、不在 CI** |
| `samples/fifo_bundle.cpp` | 同上 | ❌ 且**不在 ctest、不在 CI** |
| `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp`（ctest `spinalhdl_fifo`） | 打印 `dout`，只查 `empty`（`:140-145`） | ❌ **无 dout 断言** |

⇒ **MUST 新增 FIFO 读数据的**行为**断言**（见 `tasks.md` Task 2 用例 4），
否则本 change 会**静默**改变 FIFO 的可观测时序而无人察觉。

⇒ 回归清单 MUST 额外包含（**手工跑，不在 ctest 里**）：

```bash
./build/samples/fifo_example        # 打印值会变（0x1,0x2 → 0x2,0x1），需人工确认符合预期
./build/samples/fifo_bundle
```

⚠️ **Verilator 后端不受本 change 影响** —— 它跑的是生成的 Verilog（语义本就是
0-cycle），且 `VerilatorBackend::eval_combinational/eval_sequential`
（`src/core/verilator_backend.cpp:794-829`）**完全忽略指令表**，只驱动 Verilog 模型。
→ 但**生成的 Verilog 受影响**（AST 形状变了）→ **codegen MUST 同步改**（A′.2/A′.3/A′.4）。
→ 本 change 的实质是「让 interpreter 追上 Verilog」，**双后端 parity 只会变好不会变坏**。

## Non-Goals（非目标）

1. **`sread` 的观测时序**（仍 1-cycle，imem 的 `pc_lag` 对齐依赖它）
   —— ⚠️ 但 `sread` 的**实现形态会变**（读端口改写自身槽位 + 代理搬运，见 D3），
   **观测行为逐位不变**（探针实测）。
2. **窄传输 / 非对齐访问** 的内存语义
3. **cache 子系统的重新设计**（本 change 只改**读端口求值时机**，不动 cache 的结构）
4. **`ch_mem` API 变更**（`aread` / `sread` 的签名与语义入口不变）
5. **FIFO 的读数据协议**（`dout` 在 pop 拍的取值时机会变，但那是**向 Verilog 对齐**，
   不是协议变更 —— 见 D6）

## Impact

| 项 | 影响 |
|---|---|
| `src/simulator_init.cpp` | async read port 的归类（D2） |
| `include/ast/mem_port_impl.h` + `src/ast/mem_port_impl.cpp` | 数据流边 + 输出落点（D1 A′.1 / D3） |
| `src/codegen_verilog.cpp` | `print_proxy`（A′.2 跳过）/ `print_output`（A′.3 代理链不得越过 read port）/ `print_mem_read_port`（注释） |
| `openspec/specs/mem-read-timing/spec.md` | RENAMED ×2 + MODIFIED ×3（D4） |
| `docs/simulation/ARCHITECTURE.md` + 根 `AGENTS.md` + 主 spec `## Purpose` | drift 描述（D5） |
| `tests/test_mem_comb_read.cpp` | 竞态用例的期望值反转 + codegen 断言改写 + **新增寄存器驱动用例**（D4/D6） |
| **FIFO 消费者**（`samples/fifo_example.cpp`、`samples/fifo_bundle.cpp`、`examples/spinalhdl-ported/fifo/stream_fifo_example.cpp`） | `dout` 在 pop 拍移位一拍（D6）—— **现有护栏全部无效**，MUST 新增行为断言 |
| **JIT** | ✅ **不受影响**（附证据）：任何含 mem 读端口的设计 **JIT 整体编译失败** —— `src/jit/jit_compiler.cpp:295` 把 `type_mem_read_port` 归入 `block_seq`，而 `src/jit/jit_ir_gen_lnode_state.cpp:58` 对它返回 `UNSUPPORTED_OP` → `generate_ir` 在 `jit_compiler.cpp:312-314` 直接返回错误 → 整个 JIT 编译中止 → **回落到 interpreter**。⇒ 含 `ch_mem` 读端口的设计**从来不走 JIT**，本 change 不改变这一点 |
| 风险 | ⚠️ **中高**。① 改的是 **Simulator 的时序语义**（不是局部 bug 修复），全仓时序敏感的测试都要重跑；② `sread` 与 `aread` 混用（cache / FIFO / RV32I）的地方可能整体移位一拍；③ capability `mem-read-timing` 的三条 Requirement 是**可执行层锁**，改它们等于承认「旧行为是错的」——必须能说清为什么旧行为被钉住只是权宜之计；④ **A′.3 漏做会生成非法 Verilog**（引用未声明信号）→ Verilator 编译失败，且失败信息与真因相距甚远 |

---

## 附录 A：原始实验 diff（2026-10-21，未提交，**不可直接提交**）

> 来源：`git stash` "B2 experiment"（已于 `sim-mem-read-timing-align-2026-10` 立项后
> drop；此处保留完整内容以免丢失）。**该 diff 单独应用会让 5 个测试转红**（见 Why 节）。
>
> **该 diff 相对本 change 的定稿（A′）有 3 处已知缺陷，MUST NOT 直接套用**：
>
> | # | 缺陷 | 后果 | 本 change 的定稿 |
> |---|---|---|---|
> | 1 | 只把 **async** 改成写自身槽位（sync 也改了，但 `init_port` 两处都改成 `id()`，而 sync 的 `data_output()->id()` 语义被一并改变） | **sync 读到 0** | D3：async + sync **都**写自身槽位 |
> | 2 | 未配套改 codegen | **未声明信号 + 双驱动** | D1 A′.2 / A′.3 / A′.4 |
> | 3 | 分类改动（方案 B）曾被夹带进一个 `docs(openspec)` 提交 | **违反既有 spec**，HEAD 变红 | D2 单独成 Task，与 spec delta 同 commit 落地 |

---

## 附录 A：原始实验 diff（2026-10-21，未提交，**不可直接提交**）

> 来源：`git stash` "B2 experiment"（已于 `sim-mem-read-timing-align-2026-10` 立项后
> drop；此处保留完整内容以免丢失）。**该 diff 单独应用会让 5 个测试转红**（见 Why 节）。

```diff
diff --git a/include/ast/mem_port_impl.h b/include/ast/mem_port_impl.h
index 0631c5e..a52f54a 100644
--- a/include/ast/mem_port_impl.h
+++ b/include/ast/mem_port_impl.h
@@ -88,9 +88,11 @@ public:
           data_output_(data_output) {

         // 添加数据输出节点作为源
-        // if (data_output) {
-        //     data_output->add_src(this);
-        // }
+        // EXPERIMENT B2: establish the dataflow edge read_port -> data_proxy
+        // so the topological sort orders the port before its consumers.
+        if (data_output) {
+            data_output->add_src(this);
+        }

         // 添加父内存节点
         add_src(parent);
diff --git a/src/ast/mem_port_impl.cpp b/src/ast/mem_port_impl.cpp
index 9262a68..cfc1e75 100644
--- a/src/ast/mem_port_impl.cpp
+++ b/src/ast/mem_port_impl.cpp
@@ -46,13 +46,13 @@ mem_read_port_impl::create_instruction(ch::data_map_t &data_map) const {
                 static_cast<ch::instr_mem_async_read_port *>(instr.get());
             uint32_t enable_node_id = has_enable() ? enable()->id() : 0;
             async_read_port->init_port(addr()->id(), enable_node_id,
-                                       data_output()->id(), data_map);
+                                       id(), data_map);  // EXPERIMENT B2
         } else if (port_type_ == mem_port_type::sync_read) {
             auto *sync_read_port =
                 static_cast<ch::instr_mem_sync_read_port *>(instr.get());
             uint32_t enable_node_id = has_enable() ? enable()->id() : 0;
             sync_read_port->init_port(cd()->id(), addr()->id(), enable_node_id,
-                                      data_output()->id(), data_map);
+                                      id(), data_map);  // EXPERIMENT B2 (sync)
         }

         // 设置data_map引用
```

### 附录 A.2：被夹带的「方案 B」（已在 `7aefdfe` 回滚）

```diff
--- a/src/simulator_init.cpp
+++ b/src/simulator_init.cpp
@@ -114,11 +114,27 @@ void Simulator::initialize() {
                 break;

             case ch::core::lnodetype::type_reg:
-            case ch::core::lnodetype::type_mem_read_port:
             case ch::core::lnodetype::type_mem_write_port:
                 sequential_instr_list_.emplace_back(node_id,
                                                     instr_map_[node_id]);
                 break;
+            case ch::core::lnodetype::type_mem_read_port: {
+                // EXPERIMENT (方案 B): classify async_read ports as
+                // combinational so the Simulator matches the generated
+                // Verilog's continuous `assign` (0-cycle).
+                const auto* rp = dynamic_cast<const ch::core::mem_read_port_impl*>(
+                    node);
+                const bool is_async =
+                    rp && rp->port_type() == ch::core::mem_port_type::async_read;
+                if (is_async) {
+                    combinational_instr_list_.emplace_back(node_id,
+                                                           instr_map_[node_id]);
+                } else {
+                    sequential_instr_list_.emplace_back(node_id,
+                                                        instr_map_[node_id]);
+                }
+                break;
+            }
```
