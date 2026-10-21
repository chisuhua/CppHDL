# Change: sim-mem-read-timing-align-2026-10 — 让 Simulator 的 `aread` 与生成的 Verilog 0-cycle `assign` 对齐

> **状态：草案（proposal only）——尚未实施。**
> 本 change 立项自 2026-10-21 会话中的一次未提交实验（"EXPERIMENT B2"）。
> 该实验已被**判定不可直接提交**（实测 5 个测试转红，根因见下），
> 其 diff 完整保存在本文的「附录 A」中，作为本 change 的**起点**而非结论。

## Why

`ch_mem::aread`（async_read）在两条路径上语义**不一致**，且该差异已被登记为「已知 drift」：

| | 生成 Verilog | Simulator |
|---|---|---|
| `aread` | `assign proxy = mem[addr];`（组合读，**0-cycle**） | 在 `sequential_instr_list_` 的 posedge 求值 → **滞后 1 拍** |
| `sread` | `always_ff`（**1-cycle**） | posedge 求值（1-cycle） |

该 drift 由 capability `mem-read-timing` 显式登记（Requirement「known simulator-vs-Verilog
drift is documented」），并在 `docs/simulation/ARCHITECTURE.md`「Memory read port timing」
与根 `AGENTS.md` CONVENTIONS 中重复说明。其代价是：

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
`print_proxy` 会额外发射一条 `assign`。因此 codegen MUST 显式处理这一情形，
二选一（**实施时裁决其一**，MUST NOT 由实施者临场随机选）：

- **方案 A**：把 `type_mem_read_port` 节点加入 wire 声明列表
  （`print_header` 的 wires 收集），并让 `print_mem_read_port` 只在
  `async_read` 时发射 `assign proxy = mem[addr];`（`sread` 仍走 `always_ff`）；
  同时 `print_proxy` MUST 跳过「其 src 是 mem read port」的代理（避免双驱动）。
- **方案 B**：不在 AST 上加边，改为在 codegen 的**拓扑排序**里显式把
  mem read port 排在其数据代理之前（codegen 侧的顺序约束，不改 AST 形状）。

> ⚠️ **方案 B 对 interpreter 无效**：interpreter 的求值顺序来自
> `context::get_eval_list()` 的拓扑排序，而该排序**只沿 `src` 边递归**
> → 不加 `src` 边就无法表达「端口先于代理」。→ **倾向方案 A**。

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
>    「端口先于代理」）⇒ 排除「方案 B（只在 codegen 侧排序）」，**采用方案 A**；
> 2. **读端口写自身槽位必须 async + sync 同步改**（只改 async 会让 sync 读到 0）；
> 3. 变体实测的**上游侧净破坏面 = 1 个断言**（竞态契约反转），
>    而 B2-原样是 **3 个套件** ⇒ **变体是更小的起点**。
>
> ⚠️ **变体仍需 Task 4 的 codegen 配套**：加 `src` 边后 `print_proxy` 会为
> 该代理额外发射 `assign <proxy> = <read_port>`，而 `type_mem_read_port` 不在
> wire 声明列表 ⇒ **未声明信号 + 双驱动**。Task 4 MUST 同时解决这两点。
> （2026-10-21 实测：变体 + codegen 未配套时，生成 Verilog 的 5 个套件转红。）
>
> ⚠️ **本 change 的收益已被下游实测确认**：该上游改动落地后，**ChipForge 侧
> 的 10 个 rv32ui load/store ELF 全部转绿**（`[chmem]` 从 10 failed → 0 failed，
> 47 个 vendored ELF 全 PASS，`[verilator]` 4 case / 122 assertions 不变）
> ⇒ 该 change 是 ChipForge 那 10 个红点的**唯一零数据通路改动**的解法
> （ChipForge 侧改 `sread` 单独实测反而从 10 恶化到 11 个红点）。


### D2: Simulator 的 async read port 归类

`src/simulator_init.cpp`：`type_mem_read_port` 的 **async** 子类改入
`combinational_instr_list_`（sync 仍留在 `sequential_instr_list_`）。
→ 这是 capability `mem-read-timing` R3 的 **MODIFIED** 点。

### D3: 读端口的输出落点

读端口指令改为写**自身**的 `data_map` 槽位（而非直接写代理槽位），
由代理的 `instr_proxy` 完成「端口值 → 代理值」的搬运（B2 实验的做法）。
→ 需要 `data_map` 中端口节点自身的槽位存在且宽度正确（`size_` = `DATA_WIDTH`）。

### D4: spec delta（`mem-read-timing` MODIFIED ×3）

| Requirement | 变更 |
|---|---|
| 「Simulator classifies mem read ports as sequential」 | → 改为「async read 归组合、sync read 归时序」 |
| 「known simulator-vs-Verilog drift is documented」 | → 删掉「register-driven read address skew」场景（skew 消除），保留/改写竞态场景 |
| 「same-cycle write+aread race has an executable pre-write regression fence」 | → 钉住值从「写前值」改为「写后值」，测试用例 MUST 同步改写 |

⚠️ 这三条 Requirement 是**可执行层锁**（它们点名了具体测试与断言），
改它们 MUST 与代码改动**同一个 commit** 落地，MUST NOT 先改 spec 后改代码。

### D5: 文档同步

- `docs/simulation/ARCHITECTURE.md`「Memory read port timing」小节
- 根 `AGENTS.md` CONVENTIONS 的「Mem read port」条目
- `src/codegen_verilog.cpp::print_mem_read_port` 的大段注释（它详细描述了当前 drift）

### D6: 回归面（必须全绿）

改动触及 **Simulator 的时序语义**，影响面远超 `ch_mem` 本身：

| 回归 | 为什么必须跑 |
|---|---|
| `test_mem_comb_read` | 钉住竞态返回值（必改）+ aread codegen 发射形态（方案 A 下会变） |
| `test_mem` / `test_mem_timing` / `test_mem_connection` | 核心内存语义 |
| `test_cache_sync` / `test_cache_complete` | cache 用 `ch_mem` + 寄存器提供 1-cycle 依赖 → **skew 消除后 cache 的时序可能整体移位** |
| `test_branch_predictor_v2` / `spinalhdl_branch_predictor_v2` | 用 `ch_mem` 存预测表 |
| `test_verilator_memory_e2e` / `test_verilator_complex_e2e` | 双后端 parity（Verilog 侧不变 → 两侧差异会暴露） |
| `test_rv32i_pipeline` / `test_rv32i_pipeline_behavior` / `test_fifo_example` | **imem MUST 用 `sread`**（`pc_lag` 对齐依赖 1-cycle）→ 改 `aread` 不应影响，但必须实证 |
| `perf_tests --all` | 时序变化可能影响吞吐（perf 是 15-20 min 的独立套件） |

⚠️ **Verilator 后端不受本 change 影响**（它跑的是生成的 Verilog，语义本就是 0-cycle）
→ 因此本 change 的实质是「让 interpreter 追上 Verilog」，**双后端 parity 只会变好不会变坏**。

## Non-Goals（非目标）

1. **`sread` 的时序**（仍 1-cycle，imem 的 `pc_lag` 对齐依赖它）
2. **窄传输 / 非对齐访问** 的内存语义
3. **cache 子系统的重新设计**（本 change 只改**读端口求值时机**，不动 cache 的结构）
4. **`ch_mem` API 变更**（`aread` / `sread` 的签名与语义入口不变）

## Impact

| 项 | 影响 |
|---|---|
| `src/simulator_init.cpp` | async read port 的归类（D2） |
| `include/ast/mem_port_impl.h` + `src/ast/mem_port_impl.cpp` | 数据流边 + 输出落点（D1/D3） |
| `src/codegen_verilog.cpp` | `print_header`（wire 声明）/ `print_proxy`（跳过）/ `print_mem_read_port`（注释） |
| `openspec/specs/mem-read-timing/spec.md` | MODIFIED ×3（D4） |
| `docs/simulation/ARCHITECTURE.md` + 根 `AGENTS.md` | drift 描述（D5） |
| `tests/test_mem_comb_read.cpp` | 竞态用例的期望值反转 + codegen 断言改写（D4/D6） |
| 风险 | ⚠️ **中高**。① 改的是 **Simulator 的时序语义**（不是局部 bug 修复），全仓时序敏感的测试都要重跑；② `sread` 与 `aread` 混用（cache / FIFO / RV32I）的地方可能整体移位一拍；③ capability `mem-read-timing` 的三条 Requirement 是**可执行层锁**，改它们等于承认「旧行为是错的」——必须能说清为什么旧行为被钉住只是权宜之计 |

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
