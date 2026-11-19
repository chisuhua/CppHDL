# RTL 验证的单一真相源与生成式 TB —— 完整方案（架构主题文档）

- **状态**: **Draft · v4**（architect · 2026-11-17 · oracle 第 1 轮 REJECT（6B + 9S）⇒ v2 全闭 · critic 第 1 轮 REJECT（4B + 6S）⇒ v3 全闭 · **复审 APPROVE_WITH_CONDITIONS · 0 BLOCKER · `mergeable: yes`** ⇒ v4 处置 SHOULD_FIX）· **2026-11-18 订正 §4.3（发射面收敛口径 · 事实订正 + 形态裁决 + 逐符号切分表）⇒ oracle 第 2 轮 REJECT（3B）⇒ 复审 REJECT（1 新 B + B3 收尾）⇒ **均已处置** ⇒ critic 第 1 轮 **APPROVE_WITH_CONDITIONS**（0 BLOCKER）⇒ SHOULD_FIX **10 条已处置**（含 1 条**事实错误**：⑧ 实为「集合 + 顺序」双重判据 ⇒ 原「只用 set 比较」的告诫**作废**）· **2026-11-18 追加 §4.3.3 职责分离裁决（设计定义/激励分离 + `emit_rtl_soc.cpp` 整文件删除 + `axi4_full_example` 退役 + SoC 手写激励过渡态）⇒ oracle 第 2 轮 REJECT（2B）⇒ 复审 BR1 闭 / B2 引出 NB1·NB2 ⇒ 全闭 ⇒ critic 第 1 轮 REJECT（1B = AC11/D1 冲突）⇒ 复审 **APPROVE · 0 BLOCKER · `mergeable: yes`** ⇒ 审稿记录见 `.rddf/reviews/blueprint-rtl-verification-single-source.md`**
- **关联决策**: **ADR-074**（验证单一真相源 + 生成式 TB）· **ADR-075**（端口具名化 = 产品面前提）
- **上位决策**: ADR-072（顶层端口面 + 四件套 · D2/D4）· ADR-073（SoC 产物形态 + SoC 层 TB · D3/D6）· ADR-058/061（层次化输出）· ADR-035（Verilator backend）
- **审稿记录**: `.rddf/reviews/blueprint-rtl-verification-single-source.md`
- **关联 change**: `rtl-verification-single-source-2026-11`（**已立项 · 未归档**）· `example-file-boundary-2026-11`（**已立项** · §4.3 的切分表与形态裁决由其 **3.2 批次**落实）· `emit-cli-into-example-2026-11`（**已立项** · `emit_rtl.cpp` 删除）· `axi4-verif-driven-cpp-2026-11`（**已立项**）
- **文档定位**: **当前态唯一权威**（`docs/architecture/README.md` 登记 · 既有日期化文件 = 历史快照）
- **行号约定**：本文件的行号引用**仅作辅助定位** ⇒ ⇒ 权威锚点是**符号名 / 章节标题 / 表格行文本**（行号随编辑漂移）。

---

## 1. 为什么要做（问题陈述）

三条执行器路径（解释器 / 仓内 Verilator backend / 独立 Verilog TB）· **3 组同义载体**：

| 组 | 载体 1 | 载体 2 | 载体 3 | 症状 |
|---|---|---|---|---|
| **G-期望** | `.rddf/checks/*.md` 手推表 | `tb_*.sv` 的 `expect_*` | `tests/*` 的 `REQUIRE` | 同一批期望值写 3 遍 |
| **G-端口** | C++ 成员名 | 产物端口名（`top_unnamed_input_N`） | **手写映射表** | 🔴 **当前已漂**（见 §2.2 ⑭） |
| **G-发射** | `examples/axi4/emit_rtl.cpp`（**32 行**薄壳 ⇒ 归 change `emit-cli-into-example` 删除） | `examples/axi4/emit_rtl_soc.cpp`（**388 行** · 壳层 39 行**已**走库层 CLI ⇒ 归 change `example-file-boundary-2026-11` 的 3.2 **整文件删除**） | `examples/riscv-mini/emit_rtl_hier.cpp`（**48 行**薄壳） | 两个层次化入口形态不一致（收敛口径见 §4.3） |

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
| ② | 🔴 `emit_rtl_soc.cpp`（⚠️ **已于 2026-11-18 整文件删除** · `example-file-boundary-2026-11` 的 §4.3.3） 的 **checker 机制**仍自实现（**388 行**；壳层 39 行**已**走库层 CLI ⇒ 「自解析 + 自发射」已不成立）⇒ 其「`run_emit_cli` 只发扁平」前提**已被 ① 推翻** | `examples/axi4/emit_rtl_soc.cpp`（⚠️ **已于 2026-11-18 整文件删除**）的 `:53, 90, 115, 138, 169, 218`（自实现符号当时的位置）· `:350-388`（壳层当时的位置）· `examples/axi4/CMakeLists.txt:41-42` |
| ②' | ⚠️ **历史快照**：同题的另一本日期化蓝图 `docs/architecture/2026-11-16-soc-rtl-product-and-tb.md` 仍写「自己解析 + 自己发射 · 旧行数」⇒ ⇒ 它是**历史快照 ⇒ 不追溯改写**（本文件是**当前态唯一权威**）⇒ ⇒ 但它**不得**被当作现状依据 | `docs/architecture/2026-11-16-soc-rtl-product-and-tb.md`（历史快照）· 本文件头部「文档定位」 |
| ③ | ✅ 库层已有 TB 骨架生成器（连线 + clk/rst + VCD；激励/断言留 `// TODO:`） | `include/rtl_emit/tb_stub.h:59` · `src/rtl_emit/tb_stub.cpp:95-98, 154-158` |
| ④ | 🔴 无反射 ⇒ 未具名端口 `cpp_member` = `unnamed_input` 占位 | `include/rtl_emit/manifest.h:110-125` |
| ⑤ | ✅ N2 棘轮已把「可解析到成员名」立为 MUST（仅新增端口）；**豁免表 1415 行**，护栏锚点 `EXPECTED_EXEMPT_PORTS=1415`（⚠️ 原写 1447 = **陈旧**；本轮订正） | `scripts/verify_port_naming.sh:30-32, 50` |
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

> ⚠️ **本节于 2026-11-18 订正**（architect 裁决 · 落实于 change `example-file-boundary-2026-11` 的
> **3.2 批次**）：原表的**现状描述**与**行数目标**双双陈旧 ⇒ ⇒ 本节改为**结构判据**。
> 订正依据 = 实测（`emit_rtl_soc.cpp` = **388 行**；壳层 `main` = `:350-388`）。

| 动作 | 净变化 |
|---|---|
| `examples/axi4/emit_rtl_soc.cpp` **整个文件删除** ⇒ 发射并入 `tb_axi_soc_example.cpp` 的 `--emit-rtl` 模式（**恒** `--hierarchical` · 与 change 2 对 Lite 的做法对称） | **−1 文件 / −1 target**（⚠️ 原「壳层 39 行 ⇒ 库层 `run_emit_cli`」**已被 2026-11-18 的 §4.3.3 裁决取代** —— 保留壳层仍会留下一个「无激励的独立发射程序」，与「设计定义/激励分离」相悖） |
| `--check-products` 的**机制**下沉 **`include/rtl_emit/`（公开头）+ 实现 `src/rtl_emit/`**（RV32I 侧可复用）；**SoC 专属期望**留 `tb_axi_soc_example.cpp` 作参数 | 库层 +1 入口 |
| `examples/axi4/emit_rtl.cpp`（**32 行**）⇒ 并入 `tb_axi4_lite_example.cpp` 的 `--emit-rtl <dir>` 模式（**归 change `emit-cli-into-example`**） | −1 文件 / −1 target |
| 陈旧注释 —— **整文件删除后不再适用**（原「订正头部旧行数 / 删已失真句 / 更新用法行」三条随文件一起消失）⇒ ⇒ 但 `examples/axi4/CMakeLists.txt` 的 target 注（「发射时带 `--hierarchical`」）**随 target 一起删除** | 随删除消失 |

#### 4.3.1 逐符号切分表（`emit_rtl_soc.cpp` · **9 个**符号）

> 归属维度 = **规则 vs 值**（**不是**「编排 vs 值」）：**规则** = 产品无关的判定逻辑
> （⇒ 可下沉）· **值** = 某个产品的期望（⇒ 留 `tb_axi_soc_example.cpp`，ADR-074「不覆盖」）。
> ⚠️ **2026-11-18 载体更新**：`emit_rtl_soc.cpp` **整个文件删除**（§4.3.3）⇒ ⇒ 表内「值」的落点
> 由「example」**明确为** `tb_axi_soc_example.cpp`（`emit_rtl_soc.cpp` 删除时，其 5+3 项值**一并落到 tb**）。
> ⇒ ⇒ **「下沉」的门槛 = 规则侧产品无关**（判定逻辑不依赖任何产品）⇒ ⇒ **具名第二消费者是加分项、
> 不是门槛** ⇒ ⇒ 但**门槛未满足者 MUST 如实标注**（如 `tamper_hier`：第二消费者**语义不同**）⇒ ⇒
> ⇒ 两者冲突时**以本表为准**（change 3 的 AC17 / F2 已同步）。

| 符号 | 行 | 性质 | 归属 | 依据 |
|---|---|---|---|---|
| `kExpectedModules` | `:43` | **值**（module 集冻结） | **`tb_axi_soc_example.cpp`** | ADR-074「不覆盖」 |
| `kExpectedTopPorts` | `:50` | **值**（顶层端口数 = 14） | **`tb_axi_soc_example.cpp`** | 同上 |
| `product_modules` | `:53` | **规则**（从产物派生 module 集） | **下沉** | 与产品无关；✔ 第二消费者 = `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp`（`count_occurrences("\nmodule ")`）⇒ ⇒ **同一语义两份实现** |
| `find_verilator` | `:67` | **规则（环境依赖 ⇒ 不下沉）** | **不下沉** | 仓内先例：lint 门属**产物级护栏**，不进代码（`examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` 的「lint 门不在这里」注记）⇒ ⇒ **路径作参数传入** ⇒ ⇒ ⚠️ **口径调和**：**工具发现**（环境依赖）留 example；**lint 判据**（`--lint-only` 退出码 == 0）是**产品无关规则 ⇒ 下沉**，其 **linter 路径**与 **`--top-module` 名**一并作参数 ⇒ ⇒ 两者不矛盾 |
| `make_inputs` | `:90` | **值**（SoC 顶层自有端口面 · 收 `AxiSocTop&`） | **`tb_axi_soc_example.cpp`** | 产品专属 |
| `make_observations` | `:115` | **值**（SoC 观测面 · 收 `AxiSocTop&`） | **`tb_axi_soc_example.cpp`** | 同上 |
| `make_stimulus` | `:138` | **值**（`0x5A` / `0xA5` 的 GPIO·AXI 事务表） | **`tb_axi_soc_example.cpp`** | ⚠️ **类型通用但内容是 SoC 的期望值** ⇒ ⇒ 属值 ⇒ ⇒ **不下沉** |
| `tamper_hier` | `:169` | **规则**（变异自证编排：由观测信号反查子端口 → 取反 RHS） | **下沉** | ⚠️ **第二消费者语义不同**：`test_rv32i_hier_rtl.cpp` 是**硬编码 3 条 `assign` 名** + RHS 置 `'0'` ⇒ ⇒ **归一化另立项**（⇒ 下沉**不得**宣称「同一语义两份实现」） |
| `check_products` | `:218` | **规则**（4 项判据的判定逻辑）+ **值**（判据引用） | **拆分**：规则下沉 · 值留 `tb_axi_soc_example.cpp` | 规则侧 = 「module 集相等」·「端口数 == 期望 且 后代未驱动 == 0」·「lint 退出码 == 0」·「双形态逐拍等价」⇒ ⇒ 全部**产品无关**；值侧 = 期望 module 集 / 期望端口数 / 激励面 / 观测面 / 变异目标 **+ 根组件名（owner 基准）· lint 的 `--top-module` 名 · 最小变化观测量阈值** ⇒ ⇒ 全部作**参数**传入 ⇒ ⇒ ⚠️ 后三项**当前被焊死**在 example 侧（`e.owner != "top"` · `--top-module top` · `varying >= 2`）⇒ ⇒ **根名可配**（`rtl-soc-output` Req-1）⇒ ⇒ **MUST NOT** 让库层内置字面根名或阈值 **+ 下沉 API 签名 MUST 收「**设计 factory**」（与 `run_emit_cli` 的 `DesignFactory` 同型）—— ⚠️ 现 `check_products` 的 CHECK 2/4 要**重建设计**（`ch_device` + `build_rtl_manifest`，`:221-226`）⇒ ⇒ 值侧参数 = 上述 8 项 **+ factory** |

⇒ ⇒ **下沉集 = 完整 2 个（`product_modules` · `tamper_hier`）+ 拆分 1 个（`check_products` 的规则侧）**
⇒ ⇒ **库层 MUST NOT 内置产品值**：MUST NOT 出现字面根名（`"top"`）/ MUST NOT 内置 `>= 2` 之类阈值 ⇒ ⇒ 根名与阈值一律**参数传入**
⇒ ⇒ ⚠️ **编排归属**（钉死）：4 项判据的**顺序** · `failures` 累加 · `eq/` 目录产出 · `PRODUCTS-CHECK` 结论行 · 退出码 0/1 · CHECK 3 的 `SKIP` 语义 ⇒ ⇒ **全部随规则侧整体下沉、行为逐字保持**（AC18 兜底）⇒ ⇒ **MUST NOT** 只下沉「判定函数」而把编排留在 example（那会留下第二份编排）
⇒ ⇒ ⚠️ **适用范围**：本条**只约束库层 API** ⇒ ⇒ **不约束护栏**（护栏侧的 `top` 是**既有事实** ⇒ 不动）
⇒ ⇒ **结构判据**（**不是**行数靶）：被下沉的符号 MUST 在 example 侧**无定义**；
example 侧 MUST NOT 出现「产物派生的 module 集 / 顶层端口面 / 变异编排」的第二份实现。

⚠️ **门槛未满足者**：`tamper_hier` 的第二消费者**语义不同**（见上）⇒ ⇒ 「下沉」的理由是
「它是产品无关的编排规则」，**不是**「已有第二消费者」⇒ ⇒ RV32I 侧的语义归一化**另立项**。

#### 4.3.2 形态裁决（2026-11-18）

**SoC 发射入口 MUST 恒发层次化产物**（默认模式亦然）⇒ ⇒ **这是恢复已登记的交付契约，不是新决策** ⇒ ⇒
**不新立 ADR**、**不改** `rtl-soc-output` 的既有 requirement。

⇒ ⇒ **依据**（默认模式 = 5 module 早已在**四处**登记 ⇒ ⇒ **扁平才是偏离方**）：
`examples/axi4/tb_axi_soc_example.sv` 的头注（`emit_rtl_axi_soc --out <dir>` ⇒ 5 个 module）·
`examples/axi4/CMakeLists.txt` 的 target 注（发射时带 `--hierarchical`）·
`.rddf/checks/examples-rtl-sim-targets.tsv` 的 SoC 行 note（5 个 module · 顶层端口 14）·
已归档 change `soc-rtl-product-and-tb-2026-11` 的交付契约。

⇒ ⇒ **代价 / 必须同步的后置条件**（缺一即红）：
· **护栏 ⑧' MUST 扩到 `kind=soc`**（现仅对 `kind=hier` 生效）⇒ ⇒ 否则「入口退回扁平」这条回归路径
  **无任何常驻判据** ⇒ ⇒ 且 MUST 配 **soc 行的负向对照**（投毒 ⇒ 必红）
  ⇒ ⇒ ⚠️ **载体**：护栏 self-test 的清单**当前无 `kind=soc` fixture**（`REGISTERED_SOC` 锚点 = 0）
  ⇒ ⇒ 配 soc 负向对照 MUST **新增一条 soc fixture 行** **且** 把该锚点从 **0 改成 1**
  （否则「⓪ SoC 桶不守恒」判据会红）⇒ ⇒ 这两项副作用是 **⑧' 扩展的必然后果**，不是可选项
· **护栏 ③ 对 `kind=soc` 行**：分支 A 后**默认即层次化** ⇒ ⇒ ③ **无需改代码**，变的只是**产物形态**
  （③ 在默认模式发 · ④″ 在 `:652-654` 覆写同一目录）⇒ ⇒ 但**产物形态 MUST 登记**
· **产物等价判据 MUST 开「分支 A 例外」**：默认模式的差异 MUST **恰为形态变化**且**可机器判定** ——
  落盘物集合 **== {`top.v`, `manifest.json`}** ∧ `manifest.json` **逐字节相同** ∧ module 集 **1 → 5** ∧
  `top.v` 的**顶层端口表**与基线扁平产物**逐字逐序相同**（⇒ 差异只在**形态**、不在**端口面**）；
  `eq/` 与 `--check-products` 模式 MUST **零差异** ⇒ ⇒ 否则「恒发层次化」与「产物逐字节等价」两条判据**互斥**
· **CHANGELOG MUST 有一行**（默认模式产物形态变化 = **用户可见变更**）
· **⑧ 在层次化产物上复核**：实测成立 ⇒ ⇒ ⑧ = **集合 + 顺序双重判据**（工具
  `scripts/lib/flat_rtl_emit_check.py`：先集合差集，再 `actual != expected` 的**有序**比较）
  ⇒ ⇒ 「逐字逐序」措辞**准确**，**MUST NOT** 弱化为只比集合 ⇒ ⇒ **⑥ 今天已在层次化产物上成立**
  （④″ 已用 `--check-products` 覆写 `$work/top.v` ⇒ ⑤ 编译跑的就是层次化产物）
· ④″ 的 `--check-products` 注入从「必要」变「冗余」（**保留** —— 它还做 checker）

⇒ ⇒ **登记**：裁决**前**该偏离**持续存在**（现状 = 存在一条能产出非层次化 SoC 产物的路径）⇒ ⇒
归 §7 **R12**（⇒ 不得等裁决才登记）。

⇒ ⇒ **载体更新（2026-11-18 · 见 §4.3.3）**：本裁决原写「载体 = `emit_rtl_soc.cpp`」⇒ ⇒
该文件**整文件删除**后，载体改为 **`tb_axi_soc_example.cpp` 的 `--emit-rtl` 模式**（**决策不变** =
恒发层次化）⇒ ⇒ 护栏 ③ / ④″ 的调用点同步改指该 tb（不得继续调 `emit_rtl_axi_soc`）。

#### 4.3.3 职责分离裁决（2026-11-18）

> **要解决的问题**：`examples/axi4/` 下「设计定义」与「激励」混在同一 `.cpp`（`axi_soc_example.cpp`
> 的 `main` 里既有实例化又有手写事务序列），且**发射**由**无激励的独立程序**（`emit_rtl*.cpp`）承担
> ⇒ ⇒ 同一设计有三个入口（`*_example.cpp` 的 main · `emit_rtl*.cpp` 的 main · `tb_*.sv`）。
> **范围边界**：只定 `examples/axi4/` 的**文件职责与发射载体**；**不改**库组件端口语义、**不改**
> ADR-074 D2 的 `.verif` 封闭原语集、**不动** RV32I 侧（另一批 example）。

| 角色 | 物理载体 | 内容 | **禁止** |
|---|---|---|---|
| **设计定义** | `examples/axi4/*_top.h` | `Component` 子类 + `describe()` 的**实例化与连接** | **无 `main`** · 无激励 · 无发射逻辑 · 无文件落盘 |
| **激励驱动** | `examples/axi4/tb_*.cpp` | `main`：读 `.verif`（过渡期可手写）→ 驱动 → 断言；`--emit-rtl <dir>` 模式：调库层 `run_emit_cli` | 设计连接（那在设计定义里） |
| **激励源** | `examples/axi4/tb_*.{verif,sv}` | 只有激励与断言 | 设计连接 · 手写端口映射表 |

**三条可判定判据**：
1. `*_top.h` 内 `grep -c 'int main'` == **0**
2. `tb_*.cpp` 内 MUST **NOT** 定义 `Component` 子类（连接逻辑）—— 只 include 设计定义头
3. 生成式 `tb_*.sv` 内 MUST NOT 出现端口赋值以外的设计逻辑（护栏 ⑧'' 已判「无 unnamed 字面量 + 连线 == port_table」）

**「设计定义自己生成层次化 RTL」的机制**：**没有 `main` 就不能自己生成** —— 发射能力只能挂在**驱动方**。
⇒ ⇒ 落地 = `tb_axi_soc_example --emit-rtl --out <dir>`（§4.3.2 分支 A 下 `--hierarchical` 恒加），
与 change `emit-cli-into-example` 对 Lite 的 C3 **同一机制**（不是两套）。
⇒ ⇒ ⚠️ **根名纪律（继承 change 2 的 C2）**：`--emit-rtl` 模式的 factory MUST 为
`AxiSocTop(nullptr, "top")` ⇒ ⇒ ⚠️ **两种模式的根名同为 `"top"`**（`ch_device` 的默认名也是 `"top"`，
见 `include/device.h:28`）⇒ ⇒ 本纪律钉的是「emit 模式的**字面根名**与 `check_products` 的 owner 基准
（`e.owner != "top"`）**一致**」⇒ ⇒ 根名漂移 ⇒ **假红或假绿**

**`axi4_full_example` 退役（2026-11-18 裁决）**：
· `Axi4Top`（`examples/axi4/axi4_full_example.cpp`（⚠️ **已于 2026-11-18 退役** · `example-file-boundary-2026-11` A7））是 `include/axi4/axi4_full.h` 的 `Axi4Slave`
  的**唯一 example 用法**，但其 `main` **只打印功能列表**（`toVerilog` 在 `:137` 被注释）⇒ **不构成验证**
· **覆盖证据** = `tests/test_axi4_full_burst.cpp`（`add_dual_backend_test` · **dual-backend**）
  ⇒ ⇒ 该设计的行为面已有常驻回归
· ⇒ ⇒ **退役**（删除该文件 + target + CTest 条目 + tracked 名单项）⇒ ⇒ **显式缩面**，
  MUST 在 change 里登记（tracked **28 → 27** · CTest 条目 `spinalhdl_axi4_full` 消失 · 不是静默）
· ⇒ ⇒ 退役**不**影响 `include/axi4/axi4_full.h`（库组件保留；它仍有 `tests/test_axi4_full_burst.cpp`）

**`tb_axi_soc_example` 的激励 = 手写过渡态（如实登记）**：
SoC 的手写 TB 根因 = **UART 8N1 帧解码**（逐拍采样成数组 + 首个低电平定位 + 变长位偏移）
**超出 `.verif` 封闭原语集（ADR-074 D2）** ⇒ ⇒
· **本 change**：`tb_axi_soc_example.cpp` 的激励是 **重写，不是「迁移」** —— ⚠️ **实测**：现
  `axi_soc_example.cpp:52-131` 的 main 只有 **2 笔 GPIO 事务**（写 0xAB / 读 DATA_IN），而 tsv SoC 行
  登记的 **4 场景**是「写 GPIO ⇒ 0xA5 · 读 DATA_IN ⇒ 0x5A · **写 UART ⇒ 8N1 帧** · rst_n 拉低」
  ⇒ ⇒ **两边场景集合与期望值本来就不一致**（C++ 侧无 UART、GPIO 写值都不同），且现 main
  **无 `RESULT:` 协议行**（纯打印 + `return 0`）⇒ ⇒ 「迁移」一词掩盖了**工作量级差**与**语义差**
· **C++ 侧激励的裁定**：**最小可用集** = GPIO 写 / 读 + 复位，MUST 输出 `RESULT: checks=N fails=0`
  （**N 由 C++ 侧自定，不要求与 `.sv` 侧的 14 对齐**）⇒ ⇒ **激励语义的完备性在 `.sv` 侧**
  ⇒ ⇒ 因此 tsv SoC 行 **MUST NOT** 加 `interp-entry`（否则护栏 ⑨ 会把「C++ 侧 N ≠ 14」判红 ——
  那是**判据用错**，不是缺陷）⇒ ⇒ ⚠️ **被否决的替代**：在 C++ 侧重写 UART 8N1 解码以对齐 4 场景 ——
  那等于在 C++ 侧重做「超出封闭原语集」的工作，**做完即消灭 R13 的立项理由**，且双重手写（C++/SV 各一份）比现状更糟
· **扩展原语集** = **ADR-074 D2 的签批决策变更 ⇒ 独立立项**（不在本蓝图的实施面）
⇒ ⇒ **MUST NOT** 把手写激励记成「已收敛」；归 §7 **R13**

**被取代 / 删除的文件处置表**（连带面必须枚举完整）：

| 文件 | 处置 | 连带面（缺一即漏） |
|---|---|---|
| `examples/axi4/emit_rtl_soc.cpp` | **删除** | target `emit_rtl_axi_soc` · tsv SoC 行 **`emitter` 列 → `tb_axi_soc_example` + `emitter_args=--emit-rtl`** · **① 基线前提（F0）** · `examples/axi4/CMakeLists.txt` 的 target 注 ⇒ ⇒ ⚠️ **其 target 不在 `install(TARGETS …)` 面**（实测 `:30` 只装 4 个 example） |
| `examples/axi4/emit_rtl.cpp` | **删除**（归 change 2） | target `emit_rtl_axi4_lite` · tsv `emitter` 列 → `tb_axi4_lite_example` + `emitter_args=--emit-rtl` |
| `examples/axi4/axi4_full_example.cpp` | **删除（退役）** | target · `add_spinalhdl_ported_test(axi4_full …)`（CTest `spinalhdl_axi4_full`）· `CH_TRACKED_EXAMPLE_TARGETS`（**28 → 27**）· `install(TARGETS …)` 删该项 · **`.rddf/checks/examples-rtl-sim-exclusions.tsv` 删该行**（`verify_examples_rtl_sim_coverage.sh` 的 ghost 判据：落选表 ⊆ tracked ⇒ 残留即必红）· **`.rddf/checks/port-naming-exempt.tsv` 删 31 行 + `EXPECTED_EXEMPT_PORTS` 1415 → 1384** · `examples/axi4/AGENTS.md` FILES 行 · **`include/axi4/AGENTS.md` 的 `## EXAMPLES` 两条（`axi4_full_example.cpp` / `axi_soc_example.cpp`）** · 锚点重算 |
| `examples/axi4/axi_soc_example.cpp` | **删除**（类已住 `axi_soc_example_top.h`；激励迁出后**只剩空壳** ⇒ Zero-Debt 下不能留） | target **改名** `tb_axi_soc_example`（CTest 条目名 `spinalhdl_axi4_soc` **不变** ⇒ `add_spinalhdl_ported_test(axi4_soc tb_axi_soc_example)`）· `CH_TRACKED_EXAMPLE_TARGETS` 同名项 · `install(TARGETS …)` 该项改名 · tsv SoC 行 **`source` / `target` / `emitter`（+`emitter_args`）三列** · **`.rddf/checks/input-port-required-registry.md:45` 的 `AxiSocTop` 用例路径 → `tb_axi_soc_example.cpp`** · `examples/axi4/AGENTS.md` FILES 行 · 锚点重算 |
| `examples/axi4/tb_axi_soc_example.cpp` | **新建** | 与上面的改名**同批**落（否则中间态基线不可采） |

⚠️ **锚点算术**：`axi4_full_example` 退役（CTest **−1**）· `axi_soc_example` → `tb_axi_soc_example`
（**改名 ⇒ 净 0**）⇒ ⇒ **净效果 = CTest 总数 −1** ⇒ ⇒ change 3 的 C3/AC10 原锚点算术（268 → 269）
**MUST 重算**（不得沿用 `+N` 的旧值）。
⇒ ⇒ **另一处独立锚点**：`EXPECTED_EXEMPT_PORTS` **1415 → 1384**（`scripts/verify_port_naming.sh:50`；
`port-naming-exempt.tsv` 里 `axi4_full_example` 有 **31** 行）⇒ ⇒ 蓝图 §2.2 事实⑤ 现写 **1447** 亦陈旧（本轮同批订正）。

**planner 同步清单**（本裁决落地的前置 · `openspec/changes/**` **不在 architect 写集**）：

| # | 位置 | 现写 | MUST 改为 |
|---|---|---|---|
| 1 | change 3 `proposal.md` 的 **Out of Scope**（「`emit_rtl_soc.cpp` **整个文件**的合并｜本 change 只做 3.2」） | 排除整文件合并 | **纳入**（§4.3.3 已裁决整文件删除） |
| 2 | change 3 的 **F3**（「壳层退化为组装参数 + 调用」） | 保留文件 | **整文件删除** + 发射并入 `tb_axi_soc_example.cpp` |
| 3 | change 3 `tasks.md` 的 **A2**（把 `emit_rtl_soc.cpp` 列为 include 消费者） | 保留为消费者 | **删除该消费者**（文件已删） |
| 4 | change 3 的 **F0 ①**（基线前提 = 「唯一变量 = `emit_rtl_soc.cpp`」） | 单变量 | **多变量**（`emit_rtl_soc.cpp` + `axi_soc_example.cpp` + 新增 tb）⇒ ⇒ 基线 MUST 在**批次 F 代码改动前**采集 |
| 5 | change 3 的 **C3 / AC10**（锚点算术） | 旧算术（`+1`） | **按上表重算**（axi4_full 退役 −1 · axi_soc 改名 0） |
| 6 | change 3 的**批次 A/B**（无「去 main / 退役」条目） | 无 | 新增 **A7**（axi4_full 退役）/ **B6**（axi_soc 改名 + `RESULT:` 协议行 + 删 `toVerilog`）⇒ ⇒ ⚠️ 编号空间 = **`tasks.md` 的批次编号**（`proposal.md` 的 A7–A12 已被 `axi4_catch_test` helper 占用） |
| 7 | `proposal.md` 的 **`:298`**（Out of Scope「把 `emit_rtl_soc.cpp` 在『零 diff』判据下**保持**」）· **`:307`**（G-2 理由）· **`:318`**（所有权表「`emit_rtl_soc.cpp` \| — \| 零 diff \| 3.2 owner」）· **`:321-322`** | 「保留 / 零 diff」 | **改述为「整文件删除」**（零 diff 判据对**已删文件**无对象 ⇒ 该判据 MUST 随文件一起失效，不得当作「通过」） |
| 8 | `proposal.md` 的 **`:26`**（消费者表把 `emit_rtl_soc.cpp` 列为 include 消费者 + 「**7 处**」计数） | 含该消费者 | **删该消费者 + 计数减 1**（与 `tasks.md` A2 **同批** ⇒ 否则计数与清单不一致） |
| 9 | **AC22 的可执行复算命令**（`proposal.md:220` + `tasks.md:108,130`）`grep … examples/axi4/emit_rtl_soc.cpp` | 指向**将被删**的文件 | **改指新载体** `tb_axi_soc_example.cpp`（⚠️ grep **缺失文件** ⇒ 空/报错 ⇒ 判据**静默失效**）⇒ ⇒ AC17「壳层」/ AC18 的 `:263-273` 锚**同批重述** |
| 10 | **④″ 的调用契约**（`scripts/verify_examples_rtl_sim.sh:653`；change 2 的 A6 刻意让 ④″ **不读** `emitter_args`） | ④″ 将调 `tb_axi_soc_example --out … --emit-hygiene --check-products`（**无 `--emit-rtl`**）⇒ 与 change 2 C3「默认模式未知参数 ⇒ exit 2」**冲突** | **钉死**：tb 的 `--check-products` = **「发射 + 自检」模式标记**（内部等价 `--emit-rtl --hierarchical --check-products`）⇒ ④″ 的**判据逻辑无需改**（这符合 `emit_rtl_soc.cpp:371-377` 的现状：`--check-products` 时补 `--hierarchical`）⇒ ⇒ ⚠️ **调用点的「二进制名」改指**归 **row 9 / §4.3.2**（`emit_rtl_axi_soc` → `tb_axi_soc_example`）⇒ ⇒ 两件事分开：**逻辑不改 · 路径必改**⇒ ⇒ ⚠️ **该模式下其余 argv MUST 照常透传**（④″ 的真实调用是 `--out $work --emit-hygiene --check-products` ⇒ ⇒ 若把 `--emit-hygiene` 当未知参数 ⇒ exit 2 ⇒ ④″ 红）⇒ ⇒ ⚠️ **Lite 侧无需** `--check-products` 模式（无任何护栏消费点 ⇒ 对称加它 = 死代码） |
| 11 | **旧角色的最终词** | — | `tb_axi_soc_example --check-products` 与 `tb_axi_soc_example --emit-rtl` **两条模式都必须存在**：前者给 ④″（产品级自检），后者给 ③（产物发射）⇒ ⇒ **MUST NOT** 只留一条 |
| **12** | **change 3 的 `AC11` / `tasks.md` 的 `D1`**（「棘轮豁免表 23 行**只改第一列**（**不删行**）⇒ `EXPECTED_EXEMPT_PORTS=1415` **不变**」） | **1415 不变 · 不删行** | ⚠️ **BLOCKER 级冲突**：本裁决的 `axi4_full_example` 退役要**删 31 行** ⇒ 锚点 **1415 → 1384** ⇒ ⇒ AC11/D1 MUST 拆成两段：**3.1（头归位）** = 23 行只改路径（锚点不变）· **A7（退役）** = 删 31 行 + 锚点 1415 → **1384** ⇒ ⇒ 不拆 ⇒ **验收判据互相矛盾、落地即红**（`verify_port_naming.sh:92-97` 的 R2 逐行数条目） |
| 13 | `openspec/specs/axi-interconnect-routing/spec.md:242`（引用 `examples/axi4/tb_axi_soc_example.cpp`（2026-11-18 改名）`:264`） | 文件删除后**行引用悬空** | **登记为已知漂移**（spec 非护栏面 ⇒ 不红；按仓惯例不改历史引用，只在实施时记一笔） |

⚠️ **归属**：`openspec/changes/**` = **planner 写集** ⇒ ⇒ architect **不代改**；本清单是**交接凭据**。

⚠️ **治理义务**（订正）：`--check-products` 的**机制**下沉 = **新增公开头**（库层机制 API · **函数** +
参数）⇒ ⇒ ⚠️ **example 侧的 `--check-products` 开关保留**（它是**产品专属开关** ⇒ 库层 CLI 不新增该选项）
⇒ MUST 登记
`.rddf/checks/public-api-surface.md` + CHANGELOG；且 `scripts/verify_examples_rtl_sim.sh` 的 **④″ 调用点**
（`kind=soc` 分支）直接调 `emit_rtl_axi_soc --check-products` ⇒ **行为与退出码 MUST 逐字保持**
（⚠️ 分支 A 后 **module 数 == 5** 的唯一执行者仍为 ④″；护栏 ⑧'（形态退化）是**互补**判据 —— ④″ 的 `--check-products` 会**覆写 `$work`**（`scripts/verify_examples_rtl_sim.sh` 的 ④″ 段）⇒ ⇒ ⑤ 编译的是 ④″ **重发**的层次化产物 ⇒ ⇒ **③ 当次产物的形态只有 ⑧' 在查** ⇒ ⇒ 二者仍**不是**同一判据（⚠️ **2026-11-18 订正**：原写「冗余」与实现相反 —— 该措辞会诱导后人把 ⑧' 「优化」回 `kind=hier`，从而重新打开 R12 的回归路径））。
⚠️ **登记载体订正**：`public-api-surface.md` 的扫描范围 = **顶层 `include/*.h`** + 两个已裁决 core 头
⇒ ⇒ `include/rtl_emit/*.h` 属**子目录头 ⇒ 不在扫描范围内** ⇒ ⇒ 登记到那里**零判别力**
⇒ ⇒ **真载体** = `.rddf/checks/example-only-headers.tsv` 的**豁免通道**（消费者恰为 examples ⇒
反向判据会报「可移出登记表」）+ CHANGELOG 一行；`public-api-surface.md` 只作**记录**
⇒ ⇒ ⚠️ 该 tsv 与其护栏**当前尚未建**（由 change `example-file-boundary-2026-11` 的 D6/F6 新建）
⇒ ⇒ **在它建成之前，本次公开面登记无判别力载体**（须如实登记为「待载体」，不得记成已登记）。

⚠️ **「不动」的例外**（订正）—— 三类归属不同，MUST 分开处置：
· **归档记录**（`docs/archive/**` · `openspec/changes/archive/**`）= 历史 ⇒ **不动**
· **`docs/architecture/**` 的既有日期化蓝图** = **历史快照 ⇒ 不追溯改写**（见 §2.2 事实 ②'）⇒ ⇒ 但**不得**被当作现状依据
· **`draft` 活文档**（`.rddf/improvements/rtl-verification-single-source-2026-11.md`）= **活文档 ≠ 历史**
  ⇒ ⇒ MUST **同批订正** ⇒ ⇒ ⚠️ 该路径属 **planner 写集** ⇒ ⇒ 归属 = **planner 侧**（同批）
· **未归档的 active change**（`openspec/changes/rtl-verification-single-source-2026-11/`）= **活文档 ≠ 历史**
  ⇒ ⇒ MUST **同批订正** ⇒ ⇒ 归属 = **builder 侧**（该 change 的 F0 ②）

⚠️ **「同批」的可判定边界** = **同一 change 归档之前**（⇒ ⇒ 归档后它即成历史 ⇒ ⇒ 不再追溯）。

⚠️ **ADR-074 的「不覆盖」条无需改动**（2026-11-18 判定）：该条把 `--check-products` 的库层下沉
**指向本节**并给出登记义务 ⇒ ⇒ 本节订正后指向**仍然有效** ⇒ ⇒ 不做无谓的 ADR 文本改动。
⇒ ⇒ ⚠️ **dated 说明**：ADR-074 写「C 是实施收敛（蓝图 §4.3，**无取舍** ⇒ 不单立 ADR）」—— 该
「无取舍」前提**已被本节的 A/B 取舍取代**；**结论不变**（= 恢复 ADR-073 D1 / `rtl-soc-output` Req-1
的**已登记契约** ⇒ 仍不单立 ADR）⇒ ⇒ **ADR 文本不动**，以此 dated 说明取代该前提。

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
| `AxiLiteTop` 在 `axi4_lite_example.cpp:17` **且** `tests/test_axi_lite_verilator_e2e.cpp:74` 有副本 | 抽到 `include/axi4/axi4_lite_example_top.h` ⇒ 两处 include（**C3/C7 不放宽**）⇒ ⇒ ⚠️ **2026-11-18 覆盖**：本行是**第一阶段**（抽到 include/）⇒ ⇒ **终态由 §4.3.3 + change 3 的 A1 覆盖**（再 `git mv` 到 `examples/axi4/`） |
| `AxiSocTop` + `AxiLiteTestMaster` 在 `axi_soc_example.cpp` 内 | 抽到 `include/axi4/axi_soc_example_top.h`（含两个类）⇒ DSL 的 IR 解释器需要它 ⇒ ⇒ ⚠️ **2026-11-18 覆盖**：同上（第一阶段 include/ ⇒ 终态 `examples/axi4/`） |

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
| M2 | `examples/axi4/axi_soc_example.cpp` | **删除**（类已住 `axi_soc_example_top.h`；激励迁出后只剩空壳 ⇒ §4.3.3 处置表）⇒ 由 **`tb_axi_soc_example.cpp`**（新建）承接激励 + `--emit-rtl` |
| M3 | `examples/axi4/emit_rtl.cpp` | **删除**（并入 M1） |
| M4 | `examples/axi4/emit_rtl_soc.cpp` | **整文件删除** ⇒ 发射并入 `tb_axi_soc_example.cpp` 的 `--emit-rtl`（§4.3.3）；checker 的**规则侧**下沉库层（逐符号归属见 §4.3.1 · 值侧随迁移到 tb） |
| M5 | `examples/axi4/CMakeLists.txt` | target 调整（+`verif_run` / −`emit_rtl_axi4_lite`）+ 删旧注释 + ⚠️ **同步 tsv 的 `emitter` 列**（`.rddf/checks/examples-rtl-sim-targets.tsv:49` 现登记 `emit_rtl_axi4_lite` ⇒ ⇒ 不删会指向不存在的二进制，**会把 AC13 绊倒**） |
| M6 | `examples/axi4/tb_axi4_lite_example.sv` | **改为生成物** |
| M7 | `examples/axi4/tb_axi_soc_example.sv` | **改为生成物** |
| M8 | `scripts/verify_examples_rtl_sim.sh` | ⑥ 改等式 + 新增 ⑥'/⑥''/⑥‴/⑧' + 生成步骤入流水线 |
| M9 | `scripts/verify_port_naming.sh` | `EXPECTED_EXEMPT_PORTS` **1447 → 1415** ⇒ ⇒ ⚠️ **2026-11-18 顺序约束**：本行（rtl-verification 的 Step 3）MUST **先于** change 3 的 A7 落地 ⇒ ⇒ 否则 change 3 会先把它降到 **1384**，本行的 `→ 1415` 即**失锚**（同一锚点被两个 change 先后改 ⇒ 顺序 MUST 钉死） |
| M10 | `.rddf/checks/port-naming-exempt.tsv` | 移除 **32** 行 |
| M11 | `tests/test_axi_lite_verilator_e2e.cpp` | 删 `AxiLiteTop` 副本 ⇒ include N1 |
| M12–M14 | `docs/adr/ADR-072` / `ADR-073` / `docs/architecture/2026-11-16-…` | dated addendum | ✅ **已完成** |
| M15 | `docs/architecture/README.md` | 索引 | ✅ **已完成** |
| M16 | `docs/usage_guide/05-component-design.md` | `:1024` 映射表 + §5.9 同步（Step 6）⇒ ⇒ ⚠️ **2026-11-18 扩**：`emit_rtl_axi_soc` 的命令示例（`:1004-1005,1055`）**同批改指** `tb_axi_soc_example`（Step 6 的 grep 只查「只发扁平 / `top_unnamed_input`」⇒ **抓不到命令名变更** ⇒ MUST 单独加词） |
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

> **「缓解」列的状态词**：**裁决**（口径已定）· **落实**（实施未闭）· **闭口判据**（何时算闭）
> ⇒ ⇒ R1–R11 用的是散文式缓解 ⇒ ⇒ R12 起采用本词表（**两段式**：裁决 ✅ / 落实 ⚠️ 未闭）。
>
> ⚠️ **落实状态变更**（**唯一在读的一处**，避免读者把「未闭」当现状）：**R12 于 2026-11-18 落实闭合**（commit `0395cfc`；闭口证据 = 护栏 ⑧' 覆盖 `kind ∈ {hier, soc}` + 投毒18 + `REGISTERED_SOC: 1`）。
> 其余残留行的「未闭」仍属实 ⇒ ⇒ 本节的未闭条目仍是**当前态**，不是历史。

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
| **R12** | **SoC 发射入口的默认模式发扁平产物**（2026-11-18 实测：`emit_rtl_soc.cpp` 只在 `--check-products` 模式补 `--hierarchical` ⇒ 护栏 ③ 对 `kind=soc` 发的**正是**默认模式）⇒ ⇒ 现状 = **实现偏离已登记的交付契约**（默认模式 = 5 module 早已在 TB 头注 / `CMakeLists.txt` target 注 / tsv SoC 行 note / 已归档 change 的交付契约 **四处**登记）⇒ ⇒ 与 `rtl-soc-output` 的既有 requirement 冲突 | SoC 的「交付产物」形态不确定 ⇒ 消费方可能拿到 1-module 的 SoC | **裁决** ✅ 已裁决（2026-11-18 · SoC 发射入口恒发层次化 ⇒ **恢复契约**）· **落实** ✅ **已闭**（2026-11-18 · commit `0395cfc`）· **闭口判据** = 护栏 ⑧' 覆盖 `kind=soc` ⇒ ✅ **已满足**：⑧' 的生效条件已改为 `kind ∈ {hier, soc}`（`scripts/verify_examples_rtl_sim.sh`）⇒ ⇒ 判别力 = **投毒18**（soc 产物压成单 module ⇒ 必红，**在 `--self-test` 内**）+ **soc 清单未投毒 ⇒ 仍绿**（`REGISTERED_SOC: 1`）；生产轮 soc 行实测 `✅ ⑧' 层次化形态成立（module 数 5 · 根 = top）` ⇒ ⇒ **本条关闭**（回顾：关闭前「入口退回扁平」这条回归路径**无任何常驻判据** —— 这正是本条的成因）· ⚠️ **本蓝图 §7 不被** `verify_arch_ledger_residuals.sh` 覆盖（该护栏的台账路径硬编码为另一本蓝图，见 `scripts/lib/ledger_residual.py`）⇒ ⇒ 本行的判据只能来自**实现侧护栏** · **状态复算**（本行**无自动守护** ⇒ 每次 phase-exit / 被质疑时手动执行，≈10 秒）：四式全真才维持「已闭」（**MUST 用 `grep -F`** —— 串里含 `[ ]` 走 BRE 会被当字符类；且**分片匹配**，避免把竖线写进表格单元格）：① `grep -qF '[ "$kind" = "hier" ]' scripts/verify_examples_rtl_sim.sh` · ② `grep -qF '[ "$kind" = "soc" ]' scripts/verify_examples_rtl_sim.sh` · ③ `grep -qF 'flatten_soc' scripts/verify_examples_rtl_sim.sh` · ④ `grep -qF 'REGISTERED_SOC: 1' scripts/verify_examples_rtl_sim.sh` ⇒ ⇒ 任一落空 ⇒ **本行即刻翻回「⚠️ 未闭」**（闭口判据消失即 reopen，无需新裁决） ⚠️ **正/负两面口径**：正向（⑧' 对 soc）**常驻**（生产轮每跑必查 · CI 在跑）；**负向**（投毒18）只在 `--self-test`，而**该 self-test 当前未挂 CI** ⇒ ⇒ 若要「⑧' 被改回 hier-only ⇒ 自动红」成为系统性保证，需另立项把 `scripts/verify_examples_rtl_sim.sh --self-test` 挂进 CI（一行 yml，属 builder 写集） |
| **R13** | **`tb_axi_soc_example` 的激励是手写过渡态**（根因 = UART 8N1 帧解码超出 `.verif` 封闭原语集 · ADR-074 D2）⇒ ⇒ 激励语义的**完备性只在 `.sv` 侧**（C++ 侧 = 最小可用集：GPIO 写/读 + 复位） | 「设计定义/激励分离」在 SoC 侧**只到文件级、未到语义级** | **裁决** ✅ 已裁决（§4.3.3）· **落实** ⚠️ 未闭（**扩展 `.verif` 原语集** = ADR-074 D2 的签批决策变更 ⇒ **独立立项**）· **检查时机** = 产生 `tb_axi_soc_example.cpp` 的 change（`example-file-boundary-2026-11`）**归档时**由 verifier 核对此行 + 产生该 ADR 修订的 change 归档时再核对一次 · **漂移判据** = tsv SoC 行 **MUST NOT** 加 `interp-entry`（护栏 ⑨ 的条数等式**不适用于** SoC 过渡期 —— C++ 侧 N ≠ `.sv` 侧 14 是**预期**，不是缺陷）⇒ ⇒ 若将来加 `interp-entry` ⇒ 先决条件 = R13 已闭 · **MUST NOT** 把手写记成已收敛 ⚠️ **本蓝图 §7 不被** `verify_arch_ledger_residuals.sh` 覆盖（该护栏的台账路径硬编码为另一本蓝图，见 `scripts/lib/ledger_residual.py`）⇒ ⇒ 同 R12：**不得**往它加行 |

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
