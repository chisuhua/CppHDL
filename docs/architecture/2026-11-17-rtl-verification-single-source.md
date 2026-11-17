# RTL 验证的单一真相源与生成式 TB —— 完整方案（架构主题文档）

- **状态**: **Draft · v4**（architect · 2026-11-17 · oracle 第 1 轮 REJECT（6B + 9S）⇒ v2 全闭 · critic 第 1 轮 REJECT（4B + 6S）⇒ v3 全闭 · **复审 APPROVE_WITH_CONDITIONS · 0 BLOCKER · `mergeable: yes`** ⇒ v4 处置 SHOULD_FIX）
- **关联决策**: **ADR-074**（验证单一真相源 + 生成式 TB）· **ADR-075**（端口具名化 = 产品面前提）
- **上位决策**: ADR-072（顶层端口面 + 四件套 · D2/D4）· ADR-073（SoC 产物形态 + SoC 层 TB · D3/D6）· ADR-058/061（层次化输出）· ADR-035（Verilator backend）
- **关联 change**: **待立项**（`backlog: rtl-verification-single-source-2026-11`）
- **文档定位**: **当前态唯一权威**（`docs/architecture/README.md` 登记 · 既有日期化文件 = 历史快照）

---

## 1. 为什么要做（问题陈述）

三条执行器路径（解释器 / 仓内 Verilator backend / 独立 Verilog TB）· **3 组同义载体**：

| 组 | 载体 1 | 载体 2 | 载体 3 | 症状 |
|---|---|---|---|---|
| **G-期望** | `.rddf/checks/*.md` 手推表 | `tb_*.sv` 的 `expect_*` | `tests/*` 的 `REQUIRE` | 同一批期望值写 3 遍 |
| **G-端口** | C++ 成员名 | 产物端口名（`top_unnamed_input_N`） | **手写映射表** | 🔴 **当前已漂**（见 §2.2 ⑭） |
| **G-发射** | `emit_rtl.cpp`（43 行薄壳） | `emit_rtl_soc.cpp`（483 行自解析） | `emit_rtl_hier.cpp`（52 行薄壳） | 两个层次化入口形态不一致 |

**用户诉求（2026-11-17）**：「从原始 `*example.cpp` 构建端口连线 + 提供激励/断言/期望值的 cpp + 直接产出 `.sv` TB」，且「不要过于碎片化」。
**本方案的核心判断**：诉求成立，但**不能按字面实现** —— 字面实现会撞两条既有纪律（§4.1.2 D5 · §4.1.2 D2），且会引入「两个语义实现」的隐性分裂（§4.1.1）。

---

## 2. 现状快照（2026-11-17 · 实测）

### 2.1 三条执行器路径（**ADR-073 D3 已定 · 实质不变**）

| 路径 | 定位 | 判什么 | 产物端口面 |
|---|---|---|---|
| **(A)** 解释器 / JIT | 逻辑迭代 | C++ 设计 | 无 |
| **(B)** 仓内 Verilator backend | 逐拍 parity | C++ 设计 vs 扁平遗留产物 | `make_flat_view(ctx, …, nullptr)` ⇒ **含后代 `ch_out`** |
| **(C)** 独立 Verilog TB | **交付验证** | **交付的 Verilog 产物** | 产品面（根自有 `__io`） |

⚠️ **(B) 跑的不是 (C) 交付的那个字节** ⇒ ⇒ **不把 (C) 迁到 (B)** 的硬理由（ADR-074 D6）。

### 2.2 关键事实（可复算）

| # | 事实 | 证据 |
|---|---|---|
| ① | ✅ 库层支持层次化产物 | `src/rtl_emit/cli.cpp:77-78, 186-208`（`2db2eda`） |
| ② | 🔴 `emit_rtl_soc.cpp` 仍自解析 + 自发射（483 行）⇒ 其「`run_emit_cli` 只发扁平」前提**已被 ① 推翻** | `examples/axi4/emit_rtl_soc.cpp:5-8, 64-89, 415-483` · `examples/axi4/CMakeLists.txt:41-42` |
| ③ | ✅ 库层已有 TB 骨架生成器（连线 + clk/rst + VCD；激励/断言留 `// TODO:`） | `include/rtl_emit/tb_stub.h:59` · `src/rtl_emit/tb_stub.cpp:95-98, 154-158` |
| ④ | 🔴 无反射 ⇒ 未具名端口 `cpp_member` = `unnamed_input` 占位 | `include/rtl_emit/manifest.h:110-125` |
| ⑤ | ✅ N2 棘轮已把「可解析到成员名」立为 MUST（仅新增端口）；**豁免表 1447 行**，护栏锚点 `EXPECTED_EXEMPT_PORTS=1447` | `scripts/verify_port_naming.sh:30-32, 50` |
| ⑥ | ✅ 具名先例：RV32I 顶层 32 根具名 | `examples/riscv-mini/src/rv32i_pipeline_top.h:62-73` |
| ⑦ | 🔴 **产品顶层未具名 = 32 个**（`axi4_lite_example.cpp` **20** + `axi_soc_example.cpp` 的 `AxiSocTop` **12**（`:161-181`））⇒ ⚠️ **另有 23 个内层端口**（`AxiLiteTestMaster`，`:41-72`）**不属产品顶层** | 逐 `__io` 块计数 · `.rddf/checks/soc-rtl-expectations.md:28-50`（顶层 14 = 框架 2 + 根 12） |
| ⑧ | 🔴 护栏 ②' 硬禁 `EXPECT-SRC` 指向 `.cpp`（⚠️ **只查指针形态**，不保证「值由人推」） | `scripts/verify_examples_rtl_sim.sh:214-217` |
| ⑨ | ⚠️ 护栏 ⑥ 只判 `checks >= min_checks`（不判 TODO 残留） | `scripts/verify_examples_rtl_sim.sh:486-490` |
| ⑩ | ⚠️ tick 语义 = comb → 沿 → comb → comb ⇒ **tick 后读 = 沿后值**（与 `@(posedge clk); #1;` 对齐）；C++ 侧可在 tick 之间 `eval_combinational()` 观测组合值 | `src/simulator.cpp:299-320` · `include/simulator.h:82` |
| ⑪ | ⚠️ **`done` 粘滞 ⇒ 禁边沿判定** ⇒ 现有 TB 用**固定拍数**（`repeat (16)`），**不是**等沿 | `examples/axi4/tb_axi_soc_example.sv:9-18, 105-120` |
| ⑫ | ⚠️ 现有 TB 真正需要的机器能力 = **有界 `do-while` 等非粘滞握手** + **`for` 循环写 4 个寄存器（含计算地址 `0xA5000000 + i*4`）** | `examples/axi4/tb_axi4_lite_example.sv:134-209, 265-280` |
| ⑬ | 🔴 **`port_table[]` 元素只有 `{name, dir, width, role}` —— 无 `cpp_member`**；`cpp_member` 只在全量 `ports[]` | `include/rtl_emit/manifest.h:74-83` · `:51` |
| ⑭ | 🔴 **手写映射表当前就是漂的** —— 头注释写 `awprot=input_1 · arprot=input_8`，连线是 `.top_unnamed_input_1(arprot)` / `.top_unnamed_input_8(awprot)` | `examples/axi4/tb_axi4_lite_example.sv:18-25`（注释）vs `:84-93`（连线） |
| ⑮ | 🔴 `tests/test_axi_lite_verilator_e2e.cpp:74-142` 复制一份 `AxiLiteTop`（双权威）；C3/C7 禁 tests 反向 include examples | `scripts/verify_no_duplicate_rv32i_headers.sh:998`（C3）· `:170-196`（C7） |
| ⑯ | ⚠️ `docs/architecture/` 缺当前态索引（已建 `README.md`）；`docs/usage_guide/05-component-design.md:1024` 也有一份 `top_unnamed_input_1` 映射表 | 目录 · 该文件 |

### 2.3 缺口清单

| ID | 缺口 | 对应 |
|---|---|---|
| **Q1** | 期望值三处重复 | §4.1 |
| **Q2** | 端口身份靠手写映射表（**已漂**） | §4.2 |
| **Q3** | 发射面两形态 | §4.3 |
| **Q4** | 生成物无新鲜度判据 | §4.1.2 D2 |
| **Q5** | 「期望值出处」与「机器可执行值」两难 | §4.1.2 D5 |
| **Q6** | 护栏⑥ 不判 TODO 残留 | §4.4 |
| **Q7** | `AxiLiteTop` 双权威 | §4.5 |
| **Q8** | `docs/architecture/` 无索引 | §4.6（已建） |
| **Q9** | **「DSL 标识符 ↔ 产物端口名」无映射载体**（`port_table[]` 无 `cpp_member`） | §4.2.3 |

---

## 3. 目标架构

```
   权威验证源（DSL · 扩展名 .verif · 不参与 C++ 编译）
   examples/axi4/verif_<example>.verif
        │
        ▼  扫描器（fail-closed：集合外构造 ⇒ 非零退出）
      IR（**唯一语义载体**）
        ├── ① IR 解释器（驱动 Simulator · 解释器/JIT 后端）
        │      ⇒ C++ 侧判据（快速回归）
        └── ② Verilog 渲染器
               ├ 端口连线 + clk/rst + VCD ← manifest.port_table[]（库层 write_tb_stub）
               ├ 激励/断言 ← IR（**表达式在生成期求值 ⇒ 产物只出现字面量**）
               └ EXPECT-SRC ← .rddf/checks/*.md#anchor（推导层）
               ▼
        tb_<example>.sv（**生成物 · checked-in** · 由新鲜度判据守）
```

**四条链**：

| 链 | 形式 | 前置 |
|---|---|---|
| **语义链** | `.verif` → **IR** → ① 解释器 / ② 渲染器 ⇒ ⇒ **语义只有一份实现** | — |
| **身份链** | DSL 标识符 = `ports[].cpp_member` ⇒ **join（键 = `name`）** ⇒ `port_table[].name` ⇒ 产物端口名 | **端口具名化**（§4.2） |
| **期望链** | `.md`（推导 + 锚点）↔ `.verif`（机器值）⇒ **双向 1:1** | — |
| **产物链** | 库层 `run_emit_cli` → `top.v` + `manifest.json` | §4.3 |

⚠️ **禁写「成员名 ≡ 产物端口名」** —— 事实 ⑨（`2026-11-02-hierarchical-rtl-output.md:302`：字面相同不可达）+ 根 module 名可配置（`include/hierarchy_verilog.h:33`）⇒ ⇒ 身份链是**显式映射**，不是等号。

---

## 4. 完整方案

### 4.1 方案 A —— 验证单一真相源（ADR-074 落地）

#### 4.1.1 为什么是 `.verif` 而不是「合法 C++ + 宏」

「合法 C++（宏展开）+ 行级扫描」看似一个文件两个读者，实则：**宏展开走预处理器、IR 只在 `.sv` 侧** ⇒ ⇒ **两个语义实现** ⇒ ⇒ 「语义只有一份」是假象。
⇒ ⇒ **源文件不参与 C++ 编译**（扩展名 `.verif`，防被 glob / 编译单元拾取）⇒ ⇒ **一份 IR 真正承载语义**。
⇒ 代价：失去 C++ 编译期语法检查 ⇒ 缓解 = 扫描器 fail-closed + IR 解释器实际执行。

#### 4.1.2 原语与表达式文法（**封闭**）

**语句原语**：`RESET` · `SET` · `TICK` · `SETTLE` · `WAIT` · `EXPECT` · `EXPECT_STABLE` · `REPEAT(n){…}`（隐式 `_i`）· `OBSERVE`。

**表达式文法**（生成期求值）：字面量（十进制 / `0x` / `0b`）· `+ - * & | ^ ~ << >>` · 位选 `[hi:lo]` · 括号 · `_i`。
**禁止**：变量声明 · 条件分支 · 预处理 · 多行调用 · 引用实现头/函数/全局量。

**⚠️ 时序锚点（**唯一语义**）**：**驱动发生在「沿后采样窗」内**（不消耗拍）· **观测发生在「最近一次结算之后」**。

| 原语 | 语义 | IR 解释器 | Verilog 渲染 | 锚点 |
|---|---|---|---|---|
| `RESET(name, polarity)` | 复位线 + 释放沿（**释放 MUST 在 negedge**） | 复位驱动 | `logic` + 释放沿 | 沿前 |
| `SET(port, expr)` | 驱动输入（**不推拍**） | `sim.set_input_value` / node_id 级等价入口 | `<port> = <字面量>;`（紧接上一个 `#1` 之后） | **沿后采样窗** |
| `TICK(n)` | 推进 n 拍（每拍 = 驱动 → 沿 → 结算） | `sim.tick()` ×n | `repeat (n) @(posedge clk); #1;` | 沿 |
| `EXPECT(port, expr)` | 断言（**累加计数，不中断**） | 比较 `get_port_value` | 与上一个 `#1` **同点**比较 | **沿后已结算** |
| `SETTLE()` | 不推拍 · 只重算组合 | `sim.eval_combinational()` | `#1;`（⚠️ **不是 `#0`**） | 沿后 |
| `WAIT(sig, max)` | **非粘滞**信号 · 有界 · **至少推进 1 拍** | 有界轮询 | `do … while` 等价 + 超时判红 | 沿 |
| `EXPECT_STABLE(port, n)` | 跨 n 拍保持 | 逐拍比较 | 逐拍比较 | 沿后 |
| `REPEAT(n){…}` | 展开 + `_i` 绑定 | 循环 | 生成期展开 | — |
| `OBSERVE(port)` | 观测子集 | 登记 | — | — |

⚠️ **`#0` 被明确否决** —— 它落在 inactive region，**NBA 未结算** ⇒ ⇒ `.sv` 读到旧值而 C++ 读到沿后值 ⇒ **两个执行后端必然分叉**（判别性构例 = `TICK(1)` 后立即 `EXPECT` 寄存器输出，见 AC15）。

**⚠️ 端口绑定（闭合「DSL 标识符 → C++ 端口对象」）**：`Simulator` 的访问器是**静态类型化**的（`include/simulator.h:111, 415`）且框架无反射 ⇒ ⇒ **新增 `node_id` 级非类型化公开访问器**（`data_map_` 已按 `node_id` 索引：`:127-137, 193-204`）⇒ ⇒ IR 解释器按 **`context` 节点名建 `name → node_id` 表**（**非手写** · 不再是回调表）。
⇒ ⇒ **端口具名因此是「双必需」**：`.sv` 连线可生成（ADR-075）+ `name → node_id` 可唯一解析（`unnamed_input` 重名不可区分）。

⚠️ **`WAIT` 只能用于非粘滞信号** —— 粘滞信号（如 `done`，事实 ⑪）MUST 用 `TICK(n)`（否则第二笔事务起立刻返回 ⇒ 静默错语义）。
⚠️ **`REPEAT` + 表达式求值是必需项**（事实 ⑫：4 个寄存器的循环写含计算地址）。
⚠️ **`EXPECT` 值 MUST 是文法内表达式**（禁符号/函数引用）⇒ 堵死「期望值转录实现」的通道。
⚠️ **`EXPECT` 不中断**（累加 checks/fails）⇒ 对齐 `.sv` 的 `RESULT: checks/fails` 模型；用 Catch2 `REQUIRE` 会**首条失败即中止** ⇒ 拿不到全部结论（ADR-074 AC8 需要）。

#### 4.1.3 端口连线 = 库层能力

`write_tb_stub(out, manifest, opts)` **已经**生成 DUT 连线 + clk/rst + VCD（事实 ③）⇒ ⇒ 本方案**只扩展它**：把 `// TODO:` 段替换为 IR 渲染结果。
⚠️ **端口面只消费 `port_table[]`**（`tb_stub.h:52-54`）⇒ 渲染器 **MUST NOT** 从 `ports[]` 自行推导顶层端口。

#### 4.1.4 期望值分层

| 层 | 载体 | 内容 | 谁消费 |
|---|---|---|---|
| **推导层** | `.rddf/checks/*.md` | 手推叙事 + 锚点（`{#anchor}`） | 人 · 审稿 · `EXPECT-SRC` |
| **机器层** | `.verif` 的 `EXPECT(port, expr)` | 机器值 | IR 解释器 · 渲染器 |

⚠️ **如实声明**：护栏 ②'（事实 ⑧）**只查指针形态** ⇒ ⇒ **「值由人推导」的实质防线是 process + review**；可机械的补强只有：`.md` 锚点区格式 fail-closed · **双向 1:1** · `.md` **MUST NOT** 由生成器产出。

### 4.2 方案 B —— 端口身份链（ADR-075 落地）

#### 4.2.1 承诺边界

| 承诺 | 不承诺 |
|---|---|
| ✅ `manifest.cpp_member` 可机械解析 ⇒ `port_table[].name ↔ ports[].cpp_member` 建 **1:1 唯一映射** | ❌ 产物端口名 == 成员名**字面相同**（事实 ⑨ 不可达）⇒ ❌ **禁** `top_` 字符串拼接（根 module 名可配置） |

#### 4.2.2 迁移范围（**32 个产品顶层端口**）

| 文件 | 顶层 `__io` | 端口数 | 处置 |
|---|---|---|---|
| `examples/axi4/axi4_lite_example.cpp` | `AxiLiteTop` | **20** | MUST 具名 |
| `examples/axi4/axi_soc_example.cpp` | `AxiSocTop`（`:161-181`） | **12** | MUST 具名 |
| `axi_soc_example.cpp:41-72` | `AxiLiteTestMaster` | **23** | **不做**（子 module 端口面 ⇒ 另一类产品可见变更） |
| 其余全仓 | — | **1415** | **不做**（既有债 · 棘轮） |

⇒ 豁免表 `1447 → 1415`（移除 32 行 + 基线更新 **同批**）。

#### 4.2.3 映射载体（Q9）= **两表 join + fail-closed**（**不新增 schema**）

- join 键 = `name`（两表都是 Verilog 名）；判据：每个 `port_table[]` 元素命中**恰好一个** `ports[]` 元素且 `cpp_member != "unnamed_input"`；缺失 / 不唯一 / 占位 ⇒ **生成器非零退出**。
- ⚠️ **不选**「给 `port_table[]` 加 `cpp_member`」⇒ 那是 schema v2 变更 ⇒ 要动 capability `rtl-emit-portability` + ADR-072 R-1。

#### 4.2.4 连带影响

| 影响面 | 说明 |
|---|---|
| 产物端口名 | `top_unnamed_input_N` ⇒ `top_<成员名>`（**仅 32 个**）⇒ **用户可见** ⇒ CHANGELOG |
| 手写 TB 连线 | 两个 `.sv` 的连线名全部失效 ⇒ MUST 与具名化**同批**改 |
| 期望源端口表 | `.rddf/checks/{examples,soc}-rtl-expectations.md` 的映射表 ⇒ 由生成器产出 |
| manifest 快照 | `port_table[].name` 变 ⇒ 护栏 ⑧（逐字逐序）同批重生成 |
| **`docs/usage_guide/05-component-design.md:1024`** | 也有一份 `top_unnamed_input_1` 映射表 ⇒ 归 Step 6（**不在 architect 写集**） |
| ADR-046 D10 | RV32I 的 32 根（**已具名**）⇒ 不动 |

### 4.3 方案 C —— 发射面收敛（G-发射）

| 动作 | 净变化 |
|---|---|
| `emit_rtl_soc.cpp` ⇒ 库层 `run_emit_cli`（强制 `--hierarchical`，同 `emit_rtl_hier.cpp:38-40`） | 483 → ~60 行 |
| `--check-products` 的**机制**下沉 `src/rtl_emit/`（RV32I 侧可复用）；**SoC 专属期望**留参数 | 库层 +1 入口 |
| `emit_rtl.cpp`（43 行）⇒ 并入 `axi4_lite_example.cpp` 的 `--emit-rtl <dir>` 模式 | −1 文件 / −1 target |
| 陈旧注释（`emit_rtl_soc.cpp:5-8` + `CMakeLists.txt:41-42`） | 订正 |

⚠️ **治理义务**：`--check-products` 下沉 = **新增公开 CLI 选项 + 新公开头** ⇒ MUST 登记 `.rddf/checks/public-api-surface.md` + CHANGELOG；且 `scripts/verify_examples_rtl_sim.sh:414` 直接调 `emit_rtl_axi_soc --check-products` ⇒ **行为与退出码 MUST 逐字保持**（ADR-073 V1–V3 的唯一执行者）。
⚠️ **不动**：`.rddf/improvements/` 与 `openspec/changes/archive/**` 的同款表述（历史记录）。

### 4.4 护栏改造（`scripts/verify_examples_rtl_sim.sh`）

| 判据 | 现状 | 改造后 | 性质 |
|---|---|---|---|
| ② TB 存在 | 判文件存在 | **不变**（checked-in） | — |
| ②' `EXPECT-SRC` | 指向 `.md` · 禁 `.cpp` | **不变**（字面不减）+ **锚点区格式 fail-closed** | 补强 |
| ⑥ 断言数 | `checks >= min_checks` | **改为等式**：`min_checks` == **展开后运行期 checks 数**（= `.sv` 的 `RESULT: checks=N`；⚠️ 不是静态 `EXPECT` 语句数 —— `REPEAT` 内的会计多次）⇒ ⇒ **tsv 表头 / `#COLUMNS` / 护栏读取三处同批** | 修改 |
| **⑥' 新鲜度** | — | 重新生成 ⇒ **逐字节相同** | 新增 |
| **⑥'' 双向 1:1** | — | `.md` 锚点 ↔ `.verif` `EXPECT`（缺一边即红） | 新增 |
| **⑥‴ TODO 残留** | — | TB 内 `// TODO:` == 0 | 新增 |
| **⑧' 生成性** | — | TB 内**无** `top_unnamed_*` 字面量 | 新增 |
| ⑧ 端口表 | manifest ↔ 产物 | **不变** | — |

⚠️ **⑥' 防不了生成器自身 bug**（同源同错 ⇒ 逐字节相同 ⇒ 恒绿）⇒ ⇒ **真正的抓手是 ⑥ 的等式**（人登记 vs 机器运行期条数）。
⚠️ **变异自证形态**（ADR-074 D8）：**≥3 条时序变异**（注入点 + 期望签名写明）—— ① 采样时机（共享源 ⇒ **必须由 ⑥'' 独立抓红**）② 复位释放沿（negedge → posedge）③ 端口方向互换（事实 ⑭ 的事故重演）⇒ ⇒ **两侧同时变绿 = 零判别力 ⇒ 判红**。
⚠️ **新增 4 条 + 修改 1 条护栏判据，各自 MUST 配负向对照**（⑥ 是**修改**，其余新增）：⑥ 等式（删一条 `EXPECT`）· ⑥' 新鲜度（手改生成 `.sv`）· ⑥'' 双向（删一个锚点）· ⑥‴ TODO（塞一行 `// TODO:`）· ⑧' 生成性（塞一个 `top_unnamed_`）⇒ ⇒ 逐条确认转红。

### 4.5 顶层组件抽取（Q7 · **必须在 DSL 之前**）

| 现状 | 目标 |
|---|---|
| `AxiLiteTop` 在 `axi4_lite_example.cpp:17` **且** `tests/test_axi_lite_verilator_e2e.cpp:74` 有副本 | 抽到 `include/axi4/axi4_lite_example_top.h` ⇒ 两处 include（**C3/C7 不放宽**） |
| `AxiSocTop` + `AxiLiteTestMaster` 在 `axi_soc_example.cpp` 内 | 抽到 `include/axi4/axi_soc_example_top.h`（含两个类）⇒ DSL 的 IR 解释器需要它 |

⚠️ **为什么必须在 DSL 之前**：`verif_*.verif` 的 IR 解释器必须能实例化 DUT ⇒ 顶层类必须可从库路径拿到。

### 4.6 文档治理（Q8）

| 处置 | 对象 | 归属 |
|---|---|---|
| **当前态唯一权威** | 本文件 | ✅ 已完成 |
| **索引** | `docs/architecture/README.md`（新） | ✅ 已完成 |
| dated 订正 | `2026-11-16-soc-rtl-product-and-tb.md`（§3 清单第 2 行与实现不符） | ✅ 已完成 |
| **不在 architect 写集** | `docs/developer_guide/**` · `docs/usage_guide/**`（含 `:1024`）· `docs/CHANGELOG.md` · `PORT-NAMING-GUIDE.md` · `include/core/AGENTS.md` | ⬅ **Step 6（task）** |

---

## 5. 可枚举改动清单

### 5.1 新增

| # | 文件 | 形态 | 估行 |
|---|---|---|---|
| N1 | `include/axi4/axi4_lite_example_top.h` | 抽出的顶层类（**含 `main` 之外的全部内容**） | ~90 |
| N2 | `include/axi4/axi_soc_example_top.h` | `AxiLiteTestMaster` + `AxiSocTop` | ~280 |
| N3 | `include/rtl_emit/verif_ir.h` | IR 类型 + 表达式节点 + 原语枚举 | ~160 |
| N4 | `src/rtl_emit/verif_scan.cpp` | 扫描器（**fail-closed**）+ 表达式求值 | ~260 |
| N5 | `src/rtl_emit/verif_run.cpp` | **IR 解释器**（驱动 `Simulator`） | ~200 |
| N6 | `src/rtl_emit/verif_render_verilog.cpp` | Verilog 渲染器（复用 `write_tb_stub` 端口段） | ~220 |
| N7 | `include/rtl_emit/verif_check.h` / `src/rtl_emit/verif_check.cpp` | 双向 1:1 · 新鲜度 · 条数等式 | ~200 |
| N8 | `examples/axi4/verif_axi4_lite_example.verif` | 验证源（7 场景 / 37 断言） | ~230 |
| N9 | `examples/axi4/verif_axi_soc_example.verif` | 验证源（4 场景 / 14 断言） | ~150 |
| N10 | `examples/axi4/verif_run.cpp`（**通用**薄壳） | 读任意 `.verif` ⇒ IR 解释器跑（`name → node_id` 由 ctx 派生） | ~60 |
| N11 | `tests/test_verif_dsl.cpp` | 扫描器 **fail-closed** 负向用例 + 表达式求值 + 时序锚点（AC15 构例） | ~180 |
| N12 | `tests/test_verif_render.cpp` | 渲染 + 双向 1:1 + 新鲜度 + 条数等式 | ~160 |
| N13 | `tests/test_verif_mutation.cpp`（或 `scripts/verify_verif_mutations.sh`） | **≥3 条变异**（注入点 + 期望签名）+ 新增 4 条护栏判据的负向对照 | ~150 |

### 5.2 修改

| # | 文件 | 改法 |
|---|---|---|
| M1 | `examples/axi4/axi4_lite_example.cpp` | 改为 include N1 + **20 端口具名** + `--emit-rtl` 模式 |
| M2 | `examples/axi4/axi_soc_example.cpp` | 改为 include N2 + **12 端口具名** |
| M3 | `examples/axi4/emit_rtl.cpp` | **删除**（并入 M1） |
| M4 | `examples/axi4/emit_rtl_soc.cpp` | 483 → ~60（库层 `run_emit_cli` + `--check-products` 调库层） |
| M5 | `examples/axi4/CMakeLists.txt` | target 调整（+`verif_run` / −`emit_rtl_axi4_lite`）+ 删旧注释 + ⚠️ **同步 tsv 的 `emitter` 列**（`.rddf/checks/examples-rtl-sim-targets.tsv:49` 现登记 `emit_rtl_axi4_lite` ⇒ ⇒ 不删会指向不存在的二进制，**会把 AC13 绊倒**） |
| M6 | `examples/axi4/tb_axi4_lite_example.sv` | **改为生成物** |
| M7 | `examples/axi4/tb_axi_soc_example.sv` | **改为生成物** |
| M8 | `scripts/verify_examples_rtl_sim.sh` | ⑥ 改等式 + 新增 ⑥'/⑥''/⑥‴/⑧' + 生成步骤入流水线 |
| M9 | `scripts/verify_port_naming.sh` | `EXPECTED_EXEMPT_PORTS` **1447 → 1415** |
| M10 | `.rddf/checks/port-naming-exempt.tsv` | 移除 **32** 行 |
| M11 | `tests/test_axi_lite_verilator_e2e.cpp` | 删 `AxiLiteTop` 副本 ⇒ include N1 |
| M12–M14 | `docs/adr/ADR-072` / `ADR-073` / `docs/architecture/2026-11-16-…` | dated addendum | ✅ **已完成** |
| M15 | `docs/architecture/README.md` | 索引 | ✅ **已完成** |
| M16 | `docs/usage_guide/05-component-design.md` | `:1024` 映射表 + §5.9 同步（Step 6） |
| M17 | `docs/CHANGELOG.md` | 用户可见变更（端口名）+ 新公开面（`Simulator` 访问器 · `--check-products` 下沉） |
| M18 | `AGENTS.md` 锚点 | `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `EXAMPLES_TEST_SOURCES` / 「examples 的 main() 程序」计数（**按实际新增的 CTest / Catch2 / main 属件计**，Step 5 落实） |
| **M19** | `include/simulator.h` + `src/simulator.cpp` | **新增 `node_id` 级非类型化访问器**（公开 API ⇒ 登记 `.rddf/checks/public-api-surface.md`） |
| **M20** | `.rddf/checks/examples-rtl-sim-targets.tsv` | `min_checks` 语义「下界 → **等式**」（表头 / `#COLUMNS` 同批）+ 增列「TB 是否生成」 |
| **M21** | 根 `CMakeLists.txt`（或 `src/` 侧注册点） | 新增 `src/rtl_emit/verif_*.cpp` 的编译注册（**No orphan source**） |
| **M22** | `tests/CMakeLists.txt` | 新增 `test_verif_dsl` / `test_verif_render` / `test_verif_mutation` 的 `add_catch_test` 注册（⇒ `CTEST_TOTAL` 锚点同步，见 M18） |

### 5.3 净变化（**碎片指标 = 同义载体数，不是文件数**）

| 指标 | 前 | 后 |
|---|---|---|
| **G-期望** 载体（**产品验证面**） | **3**（`.md` / `tb_*.sv` / — ） | **2**（推导层 `.md` + 机器层 `.verif`）⚠️ **`tests/*` 的组件级判据不在本 change 范围**（它是 (A) 路径的判据，与 (C) 的交付验证不同层）⇒ ⇒ 若把 `tests/*` 计入，则「3 处」的说法**不成立**（见 §7 R10） |
| **G-端口** 载体 | **3**（含**已漂**的手写表） | **1**（成员名 → manifest join → 产物名） |
| **G-发射** 实现处 | **2** | **1**（库层 `run_emit_cli`） |
| 判据**语义实现**数 | — | **1**（IR） |
| 手写端口映射表 | 2（TB + usage_guide） | **0** |

---

## 6. 分阶段落地 + 验收

| Step | 内容 | 依赖 | 验收 |
|---|---|---|---|
| **1** | **顶层组件抽取**（N1/N2 + M11） | — | C3/C7 **不放宽** + `ctest -L verilator_e2e` 绿 |
| **2** | **发射面收敛**（M3/M4/M5 注释） | Step 1 | `verify_examples_rtl_sim.sh` 绿 + `ctest -LE perf` 绿 |
| **3** | **端口具名（32）**（M1/M2 + M9/M10 + TB/期望源同步） | Step 1,2 | `verify_port_naming.sh` 绿 + ⑧ 绿 + 产物名 = `top_<成员名>` |
| **4** | **DSL + IR + 解释器**（N3–N5/N7 + N8/N9 + N10） | Step 1,3 | 扫描器 fail-closed 负向用例绿 + IR 解释器跑出 37/14 条 |
| **5** | **生成 TB + 护栏**（N6 + M6/M7 + M8 + M18） | Step 4 | ⑥ 等式 / ⑥' 新鲜度 / ⑥'' 双向 / ⑥‴ TODO / ⑧' 生成性 全绿 + ≥3 变异各自转红 |
| **6** | **文档同步**（M16/M17 + guide） | Step 1–5 | `grep -rn "只发扁平\|top_unnamed_input" docs/ examples/ include/ src/` 在**活动面**归零 |

⚠️ **Step 2 单独做完不构成诉求闭环**（只收 G-发射）⇒ 汇报 MUST 说清。
⚠️ **Step 4 依赖 Step 1**（IR 解释器要实例化 DUT）；**Step 3 依赖 Step 1**（具名要改抽出的头）。

---

## 7. 已知残留与边界

| # | 残留 | 后果 | 缓解 |
|---|---|---|---|
| R1 | DSL 是新机制 ⇒ 有自身缺陷面 | 渲染 bug ⇒ 少断言 ⇒ 假绿 | fail-closed + ⑥ 条数等式 + ⑧' |
| R2 | 双后端同源失明 | 一起绿 | 逐原语一致性（check_id 集合相等）+ ≥3 变异 + `.md` 核对 |
| R3 | `.md` 保留锚点副本 | 数值两处 | ⑥'' 双向 1:1 |
| R4 | DSL 无 C++ 编译期检查 | 语法错只能运行时发现 | fail-closed + 实际执行 |
| R5 | 手写 TB 逃生口 | 两种形态并存 | 覆盖度登记 tsv（D7） |
| R6 | **子 module 端口名仍不可自解释**（`AxiLiteTestMaster` 23 条） | 层次化产物可读性 | 已知边界（ADR-075 §2.1） |
| R7 | 全仓 **1415** 条端口债 | 多数 example 产物名不可自解释 | 棘轮只减不增 |
| R8 | `docs/**` 不在 architect 写集 | 用户面文档滞后 | Step 6 + 验收命令 |
| R9 | 护栏 `--self-test` 的投毒对象改变（TB 成生成物） | 自检失效 | Step 5 内改为投毒 **IR / 生成器** |
| R10 | **`tests/*` 的组件级判据仍是期望值的第三个载体**（`tests/test_axi_lite_slave_conformance.cpp` 的 15 个 `REQUIRE` 字面值） | 「G-期望 3 处」若把它计入则不成立 | **如实登记**：它是 **(A) 路径（组件级规范符合性）** 的判据，与 (C) 的交付验证**不同层** ⇒ ⇒ 本 change **不动**；若将来要收敛 ⇒ 单独立项 |
| R11 | `Simulator` 新增 node_id 级访问器 = **公开面扩张** | 下游可见 | 按仓规登记 `public-api-surface.md` + CHANGELOG（M19） |

---

## 8. 判据清单（**建议 AC · change 级** · 立项后由 planner 统一编号）

> ⚠️ **两套编号**：**ADR-074 §5 / ADR-075 §5** 是**决策级**判据（AC1–AC16 / AC1–AC10）；本节是**change 级**建议 AC（AC1–AC20）⇒ ⇒ **planner 立项时 MUST 统一编号并给出双向对照**（避免两套编号漂移）。

| # | 判据 | 载体 |
|---|---|---|
| AC1 | **发射入口实现**内无直接调用 `toVerilogHierarchical`：`git grep -n "toVerilogHierarchical" -- 'examples/**/emit_rtl*.cpp'` == 空 且 `examples/axi4/emit_rtl.cpp` 已删；⚠️ **白名单（不改）**：`examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp:161,181`（库层 API 测试）· `examples/riscv-mini/{AGENTS.md,CMakeLists.txt}` 注释 | shell |
| AC2 | 32 个产品端口 `cpp_member != "unnamed_input"` 且 `port_table[] ↔ ports[]` **1:1 唯一** | probe |
| AC3 | `.md` 锚点 ↔ `.verif` `EXPECT` **双向 1:1**（缺一边即红） | shell |
| AC4 | `.md` 锚点区格式坏 ⇒ 红；`.md` **不由生成器产出** | shell |
| AC5 | 生成的 `.sv` 内**无** `top_unnamed_*` | grep |
| AC6 | 生成的 `.sv` 内 `// TODO:` == **0** | grep |
| AC7 | 重新生成 ⇒ **逐字节相同** | shell |
| AC8 | `tsv.min_checks` == **展开后运行期 checks 数**（= `.sv` 的 `RESULT: checks=N`；⚠️ 不是静态 `EXPECT` 语句数） | shell |
| AC9 | 同一 `check_id` **集合相等** + 各自结论相同（**IR 解释器** vs `.sv` 两**执行后端**） | 双跑 |
| AC10 | ≥3 条时序变异各自转红（注入点 + 期望签名写明；共享源变异 MUST 由 AC3 独立抓红）+ **新增 4 条护栏判据的负向对照**逐条转红 | 变异脚本 |
| AC11 | 扫描器对集合外构造 **fail-closed**（负向用例） | shell |
| AC12 | 生成覆盖度登记在 tsv（**增列** + `#COLUMNS` 同步） | shell |
| AC13 | `verify_examples_rtl_sim.sh` 绿 + `--self-test` 绿 | shell |
| AC14 | `ctest -LE perf` 绿 + `ctest -L verilator_e2e` 绿 | ctest |
| AC15 | `verify_examples_rtl_sim.sh:214-217`（禁 `EXPECT-SRC` → `.cpp`）**逐字未改** | diff |
| AC16 | 豁免表 `1447 → 1415` 且 `verify_port_naming.sh` 绿 | shell |
| AC17 | 活动面 `grep "只发扁平\|top_unnamed_input"` 归零 | grep |
| AC18 | 子 module 端口名（`AxiLiteTestMaster` 23 条）**未变** | diff |
| AC19 | **时序锚点判别性构例**：`TICK(1)` 后立即 `EXPECT` 寄存器输出 ⇒ IR 解释器与 `.sv` 同结论 | 双跑 |
| AC20 | **端口绑定**：`verif_run.cpp` **不含逐例绑定表**（同一份可跑两个 example） | probe |

---

## 9. 关联文档

- **ADR-074**（决策面）· **ADR-075**（前置面）
- ADR-072（D2 四件套 · D4 **被部分 supersede**）· ADR-073（D3 分工 · D4 端口数不变）
- `docs/architecture/README.md`（索引）· `PORT-NAMING-GUIDE.md`（**Step 6**）
