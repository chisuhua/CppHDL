---
name: rtl-emit-product-tooling-2026-11
priority: P1
phase: phase-6
category: framework
type: feature
状态: draft
主题: Verilog codegen 完整化
依赖: core-node-driver-visibility（已实施）· rtl-hierarchical-output · hierarchical-port-naming-guard
来源: 用户拍板 2026-11-10「生成器可以作为项目提供的能力」+ 2026-11-10 实测（`toVerilog` 产物形态）
生成时间: 2026-11-10
evidence: probe:examples/riscv-mini/src/rv32i_pipeline_top.h + /tmp/opencode/rv32i_rtl/rv32i_soc.v
---

# Improvement: RTL 生成能力**产品化**（CLI + 端口清单 + 产物级护栏 + codegen 卫生）

- **Date drafted**: 2026-11-10
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）
- **来源**: 用户 2026-11-10 拍板「① 生成器可以做为项目提供的能力，对任意的合法设计都可以用这个工具生成 RTL 代码」+ 本轮 planner 实测产物形态
- **归属**: 本项**只**做「生成能力」；**端口命名**归 `port-naming-ratchet-2026-11`（独立立项）· **示例侧仿真覆盖**归 `examples-rtl-sim-coverage-2026-11`（独立立项）

---

**主题**: Verilog codegen 完整化

## 架构依据

### A.1 现状事实（2026-11-10 **实测**，非推断）

| # | 事实 | 证据 |
|---|---|---|
| ① | `toVerilog()` 产出的 RTL **能跑**，且行为正确（20/20 golden 通过，退休 178 条指令） | 独立 Verilog testbench 跑**生成产物**（`/tmp/opencode/rv32i_rtl/`）· `verilator --lint-only` 0 warning |
| ② | 但 `toVerilog()` **只是 verilator 后端的内部输入**（`src/core/verilator_backend.cpp:709` 用同一个 `verilogwriter`），**不是**产品能力 ⇒ **零 CLI · 零安装 · 零 manifest · 零端口选择** | 仓内唯一调用点是 verilator 后端；`examples/` 里的 `toVerilog` 调用都是「顺手生成一个 .v」的 demo |
| ③ | ⚠️ **产物形态零测试** —— 没有任何测试断言过 `toVerilog()` 的**端口集 / 名字 / 确定性 / lint** | `scripts/` 下只有 `verify_hierarchical_rtl.sh`（**层次化**产物）· 扁平产物**无护栏** |
| ④ | ⚠️ **顶层端口选择 = 「ctx 里所有 `type_output` 节点」** ⇒ **子组件内部输出被一并提升到顶层** | `src/codegen_verilog.cpp:74-86`（`make_flat_module_view`）· 实测：`Rv32iPipelineTop` 产物 107 个输出里 **80 个是内部子模块输出**（`top_pipeline_if_stage_unnamed_output*` 等） |
| ④′ | ⚠️ **metis 订正**：顶层 **input 按定义恒无 driver**（有 driver 的已在 `make_flat_module_view:94-103` 被滤成内部 wire）⇒ ⇒ 「无 driver」**不能**区分「根端口」与「泄漏端口」⇒ ⇒ 正确判据是「**owner ≠ 根组件** 且无 driver」 | `src/codegen_verilog.cpp:94-103` |
| ④″ | ⚠️ **metis 订正**：框架**无反射** ⇒ 「根组件的 `io_type` 成员」**不可枚举**（`__io` 宏只生成匿名 struct）⇒ ⇒ 可实现的唯一口径是「**owner 为根组件**的端口节点」（`lnodeimpl::get_parent()`；`src/hierarchy_analysis.cpp:272` 的 `mod_of` 即此法） | `include/core/io.h:319-330` · `include/core/lnodeimpl.h:201` |
| ⑤ | ⚠️ **无 driver 的 `ch_in` 一律被当成顶层端口** ⇒ 内部缺陷泄漏到产品接口面 | 同上（`push_of_type` 的 filter 只有「有 driver ⇒ 内部线」/「无 driver ⇒ 顶层端口」两桶）· 实测：`hazard_unit.mem_is_load` 泄漏成 `top_pipeline_hazard_unit_unnamed_input_4` |
| ⑥ | ⚠️ 该泄漏端口是**活的**（变异 tie 1 ⇒ 9 项 golden 转红、只退休 21 条指令）⇒ 4 态仿真器里不接 ⇒ X ⇒ **整个设计变 X** | 变异实测（`mutation_memload_tied1.log`） |
| ⑦ | ⚠️ 产物缺 `` `default_nettype none `` · 缺生成元数据 · 含**声明未用的信号**（**107** 个）· 含**proxy 恒等直通**（**43** 条 `assign X_ = X;`，其中 `X_` 无其它引用 ⇒ 整条链是死路）<br>⚠️ **planner 复核订正（2026-11-10）**：原稿写「含**自赋值死逻辑**（`assign a = a;`）」⇒ ⇒ **实测为 0**（产物 558 条 `assign` 中**字面自赋值 0 条**）⇒ ⇒ **原表述作废**；真正的死逻辑形态是上面两条（零引用信号 / proxy 恒等直通）⇒ ⇒ F5 判据 MUST 改成可判别形态（见 F5 行） | `rv32i_soc.v` 产物全文（2026-11-10 逐条扫描：558 assign · 0 字面自赋值 · 107 零引用信号 · 43 恒等直通） |
| ⑧ | ⚠️ 产物头 blanket 关掉 `WIDTHEXPAND` / `WIDTHTRUNC` ⇒ **宽度 bug 结构性不可见** | `rv32i_soc.v:1-2`（层次化侧靠 R3「剥掉 lint_off 重 lint」补 · **扁平侧无对应护栏**） |
| ⑨ | 复位写法反直觉：`always_ff @(posedge default_clock or posedge default_reset) begin if (!default_reset) q <= next; else q <= INIT; end` ⇒ 读起来像「复位时走 next」 | `rv32i_soc.v:1439-1444` · 语义正确（`default_reset` 高有效）但**可读性差、易误改** |
| ⑩ | 框架**已有**驱动状态诊断原语：`ch::core::drive_state` + `diagnose_drive()`（`include/core/lnodeimpl.h:236,258`）⇒ **MUST 复用**，MUST NOT 另造 | 已实施于 `framework-undriven-field-diagnostic-2026-10`（capability `core-node-driver-visibility`） |
| ⑪ | 层次化与扁平**共用**同一份命名实现（`include/codegen_naming.h`）⇒ 改命名逻辑 MUST 同时改两处（**两处共用是优点**，不是重复） | `src/codegen_verilog.cpp:69` + `include/hierarchy_analysis.cpp` 均调 `build_verilog_name_table` |

### A.2 为什么现在做

① 是**所有 RTL 侧改进的公共地基**：端口清单（manifest）是独立 testbench 生成 / 端口文档 / 下游集成的**唯一数据源**（用户 ②③ 两项都依赖它）⇒ ⇒ 先有 manifest，后有 TB 生成与示例覆盖。
④⑤ 是**产品接口面的正确性问题**（不是"不好看"）：80/107 的端口是噪音，1 个悬空端口是**功能性缺陷**。
③ 是**质量下限问题**：仓内一切"两后端一致"都靠同一份 C++ 生成 ⇒ 产物形态若无人看守，codegen 的退化不会有人发现。

---

## 范围

### ✅ 在内

| 编号 | 内容 |
|---|---|
| **C1** | **CLI 工具** `tools/cpphdl-emit-rtl`：参数 `--top <Component>`（必填 · 显式选择根组件）· `-o/--out <dir>` · `--module-name <name>` · `--manifest <file>`（JSON）· `--format flat\|hier`（`hier` 复用既有 `toVerilogHierarchical`）· `--strict`（诊断即失败）<br>⚠️ **metis 订正**：「对**任意**合法设计可用」与「编译期注册表」**互斥** ⇒ ⇒ MUST 明确形态：**工具 = 库 + 薄 CLI**，用户在**自己的可执行文件**里用 `CH_EMITTABLE_COMPONENT(...)` 登记 ⇒ ⇒ 本仓的 `cpphdl-emit-rtl` **MUST NOT** 链接 `examples/**`（否则装出来的是「链了示例代码的二进制」）⇒ ⇒ 护栏须静态扫描该禁令 |
| ~~**C2**~~ | 🔴 **已撤回（2026-11-10 用户裁决 · 保留契约）** —— 原「顶层端口 = owner 为根组件的节点」**与已签批契约正面冲突**：`openspec/specs/rtl-hierarchical-output/spec.md:309-316` 明文「顶层端口表 MUST 是 clock → reset → 无 driver 的 `input` → **全部** `ch_out`（⇒ **含 inline 后代的 `ch_out`**）」，且生产代码**零** `CH_HIERARCHY_LAYER`（仅宏定义 + infra）⇒ RV32I SoC **全 inline** ⇒ **那 80 个输出按契约本就必须在顶层，不是泄漏**。<br>⇒ 替代做法（**不动端口面**）：manifest **全列**端口，并用新增字段 `owner`（所属组件路径）+ `is_top_io`（bool · 是否根组件自有 IO）**让下游自己区分**「根组件语义端口」与「inline 后代输出」。详见 C4。 |
| **C3** | **悬空输入的诊断（只诊断 · 不动端口面）**：`owner ≠ 根组件` 且无 driver 的 `ch_in` MUST 记入 manifest 的 `diagnostics[]`（复用 `diagnose_drive()`）· `--strict` 下失败、非 strict 下 CHWARN。<br>⚠️ **端口面 MUST NOT 改变**（C2 已撤回）⇒ ⇒ 该 `ch_in` **仍会**是顶层端口（契约要求）⇒ ⇒ 本项只负责**让它可被发现**。<br>⚠️ 修正的具体形态 MUST 落在**设计侧**，**落点已指名** ⇒ ⇒ `rv32i-dead-component-consolidation-2026-11` 的 **C8**（`hazard.io().mem_is_load <<= ch_bool(false);` 装配点 tie-off ⇒ ⇒ 该口有 driver ⇒ ⇒ 不再是顶层端口 ⇒ ⇒ 顶层 input 8 → 7）⇒ ⇒ 其它设计的悬空输入仍由本项的**诊断兜底** ⇒ ⇒ 两者**互补不重复**。<br>⚠️ **顶层 input 无内部 driver 是常态，不是缺陷** ⇒ ⇒ 判据 MUST NOT 写成「每个 input 必须有 driver」 |
| **C4** | **端口身份 = 名字 + 方向 + 宽度 + 归属**：manifest 记录 `name` / `dir` / `width` / `role`（`clock`/`reset`/`data`）/ `driven`（bool · **节点级**：是否被消费）/ **`effective`（bool · 功能级**：驱动它是否产生可观测行为）** / **`owner`（所属组件路径）** / **`is_top_io`（bool · 根组件自有 IO，而非 inline 后代输出）** / **`cpp_member`（C++ 成员路径 · 由 `port-naming-ratchet` 第 1 步消费，见其 N5）** / `semantic`（**可选 · v1 无生产者**）⇒ ⇒ **端口名语义**归 `port-naming-ratchet-2026-11`（本项只消费，不定义）<br>⚠️ `owner` / `is_top_io` 是 **C2 撤回后的替代手段** ⇒ ⇒ 工具**不改端口面**，只让下游**能自己筛**（例：TB 生成器可选「只连 `is_top_io == true`」）；⚠️ 该筛选是**消费方的选择**，MUST NOT 成为发射器的默认行为<br>⚠️ **metis 订正**：`role: data_unused` 无判定原语 ⇒ **v1 删掉该取值**，改用 `driven: bool`<br>⚠️ **planner 复核订正（2026-11-10）**：metis 把 `ext_clk` 归为「**不是** unused 的例子」（依据 `rv32i_pipeline_top.h:137` 有连线）⇒ ⇒ **只对了一半**：`ext_clk` **节点级确有消费者**（→ `pipeline.io().clk`），但 **功能级是死路** —— `include/cpu/pipeline/**` 对 `io().clk` **零消费者**（grep 无命中）且**变异 tie 1 ⇒ 零行为影响** ⇒ ⇒ 它属于**第三类：「有连线但无功能」**（既非「活端口」也非「悬空端口」）⇒ ⇒ 单个 `driven: bool` **表达不了**这一类 ⇒ ⇒ 故拆成 `driven`（节点级）+ `effective`（功能级）两字段 ⇒ ⇒ `ext_clk` 成为 `driven=true ∧ effective=false` 的**标准样例** |
| **C5** | **产物级护栏** `scripts/verify_flat_rtl_emit.sh`（**对标**既有 `scripts/verify_hierarchical_rtl.sh` 的 R1–R8 面）：F1 产物 lint 干净（**剥掉 blanket `lint_off` 后重 lint**）· F2 **确定性**（两次发射逐字节相同）· F3 顶层端口集 **== manifest** · F4 **无悬空顶层输入** · F5 无自赋值死逻辑 · F6 **backend 产物 == 产品产物**（`verilator_backend` 的 work_dir/top.v 与 `toVerilog()` 逐字节相同）· F7 `--self-test`（逐条原地投毒 ⇒ 必红 ⇒ 还原复绿）· 全部 fail-closed（缺工具 ⇒ exit 2） |
| **C6** | **codegen 卫生批次**：产物头加 `` `default_nettype none ``（+ 末尾 `` `default_nettype wire ``，**每文件一次**）· 加生成元数据头（工具版本 / 根组件名 / 源文件；⚠️ **生成时间只进 manifest · 不进 `.v`** ⇒ 否则与 F2 的逐字节确定性冲突）· 剪掉自赋值死逻辑 · 复位写法规范化为 `if (reset) q <= INIT; else q <= next;`（**语义不变** · 只改写法）· `lint_off` 由 blanket 改为**按节点白名单**<br>⚠️ **metis 订正**：C6 改的是**共享发射器**（`src/hierarchy_verilog.cpp:1562` 同一 `ModuleEmitter`）⇒ ⇒ 层次化产物**字节必变** ⇒ ⇒ MUST 加 `ModuleView::emit_hygiene` 开关（**默认 off** · 仅 flat 产品路径开）⇒ ⇒ 否则「hier 产物逐字节不变」这条验收**不可满足** |

### ❌ 不在本项

- **端口命名规则与命名 API** ⇒ 归 `port-naming-ratchet-2026-11`（公开 API 变更 · 独立立项）
- **层次化发射器的成层策略** ⇒ 归既有 `hierarchical-corpus-regression-2026-11` 修订 + 本仓既有 ADR-060/066（**MUST NOT** 在本项动成层语义）
- **示例侧的独立 Verilog 仿真与单测** ⇒ 归 `examples-rtl-sim-coverage-2026-11`
- **任何 `src/**` 的行为语义改动**（本项只改**产物形态**，不改仿真行为）

---

## Why

### W.1 「能生成」与「是产品」之间有一整段距离

实测产物**完全可用**：3149 行 · 单 module `top` · lint 干净 · 跑独立 testbench 20/20 golden 通过。⇒ ⇒ **生成能力本身是成立的**。
但从「一个 C++ API 能吐出 .v」到「项目对外提供 RTL 生成能力」，缺的是：入口（CLI / 安装 / 参数）· 契约（端口是谁的？名字是什么？）· 守门（产物形态谁看？）。

⇒ ⇒ 现状下，用户要生成 RTL，只能**写一段临时代码**（本次就是如此：临时 generator + 临时 Makefile 式编译 + 临时 TB）⇒ ⇒ **这不是能力，是巧合**。

### W.2 端口面的两个缺陷是**正确性**问题，不是观感问题

- ~~**80/107 的内部输出被提升到顶层** ⇒ …接口面不可用~~ 🔴 **此条前提已证伪（2026-11-10）**：那 80 个输出是 `openspec/specs/rtl-hierarchical-output/spec.md:309-316` **明文要求**的（全 inline 设计 ⇒ 后代 `ch_out` MUST 上顶层）⇒ ⇒ **不是泄漏，是契约**。<br>⇒ ⇒ 真实的可用性缺口改述为：**端口面缺少「哪个是我的、哪个是内部的」这一元信息** ⇒ ⇒ 由 C4 的 `owner` / `is_top_io` 在 manifest 里补，**不**改端口面。
- **`mem_is_load` 悬空顶层端口是活的** ⇒ 变异 tie 1 ⇒ 设计只退休 21 条指令（正常 178）⇒ ⇒ **4 态仿真器里不接它 = X 传播 = 整个核变 X** ⇒ ⇒ 用户按「端口表」接线就会踩坑，且**没有任何诊断告诉他**。

⇒ ⇒ 这两条 MUST 在「产品化」这一层解决（而不是在某个 example 里绕过）。

### W.3 「两后端一致」对 codegen 退化**结构性失明**

仓内既有纪律明确：「两后端一致」不能当正确性证据（同源生成 ⇒ 同构缺陷一致地错）。⇒ ⇒ 同一逻辑适用于 codegen：**没有任何独立信号能发现产物形态退化**（端口多出 80 个、名字漂移、lint 关掉）⇒ ⇒ 必须有**产物级护栏**（F1–F7）作为独立判据。

### W.4 为什么 manifest 是承重件

用户 ②（固件/ELF）与 ③（examples 独立仿真）都要求「知道每个端口叫什么、是什么方向、能不能驱动」⇒ ⇒ 若每个下游各自解析 .v 文本 ⇒ ⇒ 三处解析器必然漂移 ⇒ ⇒ **manifest 是唯一数据源**。

---

## What Changes

### C1 — CLI 工具

```
tools/cpphdl-emit-rtl --top Rv32iPipelineTop \
    -o build/rtl --module-name rv32i_soc \
    --manifest build/rtl/rv32i_soc.manifest.json --strict
```
- 新增 `tools/CMakeLists.txt` + `add_executable(cpphdl-emit-rtl)` + `install(TARGETS ...)`。
- `--top` 的组件解析方式：编译期**注册表**（`CH_EMITTABLE_COMPONENT(Rv32iPipelineTop)` 宏展开成 `ch::emit::registry` 条目）⇒ ⇒ 避免运行时反射（框架无反射能力）。
- 退出码：`0` 成功 · `1` 诊断/护栏失败 · `2` **用法错误或缺前置**（fail-closed）。

### C3 — 悬空输入的诊断（**不动端口面** · C2 已撤回）

- `make_flat_module_view()`（`src/codegen_verilog.cpp:62`）**不改** ⇒ ⇒ 端口选择语义**零变更**（契约归 `rtl-hierarchical-output`）。
- 无 driver 的 `ch_in`：MUST 走 `diagnose_drive()` 判定 ⇒ ⇒ `no_node`/`literal`/`driven` 三态已有 ⇒ ⇒ 本项只**读取**该判定，把「`owner ≠ 根组件` 且无 driver」记入 manifest `diagnostics[]`。
- ⚠️ **兼容边界**：既有 `toVerilog()` 的产物**逐字节不变**（本项不碰端口选择、不碰端口顺序）⇒ ⇒ 唯一变更来自 C6 的 `emit_hygiene`（**默认 off**）。

### C4 — manifest 格式（JSON）

```json
{
  "schema": "rtl-emit-manifest-v1",
  "tool": "cpphdl-emit-rtl",
  "generated_at": "2026-11-10T…Z",
  "root_component": "Rv32iPipelineTop",
  "module_name": "rv32i_soc",
  "ports": [
    {"name": "default_clock", "dir": "in",  "width": 1,  "role": "clock"},
    {"name": "ext_clk",      "dir": "in",  "width": 1,  "role": "data", "driven": true,  "effective": false, "semantic": "SoC 时钟输入（节点级接到 pipeline.io().clk，但该口功能级零消费者 ⇒ 驱动它不产生任何行为）"},
    {"name": "prog_addr",    "dir": "in",  "width": 32, "role": "data", "driven": true,  "effective": true,  "semantic": "固件写地址（字索引）"},
    {"name": "obs_data_addr","dir": "out", "width": 32, "role": "data", "driven": false, "effective": true,  "semantic": "数据地址（字节）"}
  ],
  "diagnostics": [ {"kind": "undriven-input", "port": "hazard.mem_is_load", "severity": "error"} ]
}
```
- `role` 枚举：`clock` / `reset` / `data`（🔴 **critic 第 2 轮：`data_unused` 已删** ⇒ F4 不得再依赖它）
- `driven`（**节点级** · 是否被消费）与 `effective`（**功能级** · 驱动它是否产生可观测行为）**是两个正交字段**（⚠️ **planner 复核订正 2026-11-10**：metis 原判「`ext_clk` 不是 unused 例子」只对一半 ⇒ 它是 `driven=true ∧ effective=false` 的标准样例 ⇒ 单个 bool 表达不了 ⇒ 故拆两字段）⇒ ⇒ `driven=false` 的 input ⇒ **`diagnostics[]` MUST 有 `undriven-input` 条目**（F4 判据）⇒ ⇒ `driven=true ∧ effective=false` ⇒ **warning 级**（`kind: "wired-but-inert"`，非 error ⇒ 不阻断 `--strict`）
- `semantic` 字段**可选**：C++ 侧显式标注时写入 ⇒ ⇒ 为 `port-naming-ratchet-2026-11` 的显式命名预留承载位。

### C5 — 产物级护栏 `scripts/verify_flat_rtl_emit.sh`

| 面 | 判据 | 失败语义 |
|---|---|---|
| F1 | **两段式**：① **基线** —— 在**未改动**的 `main` 上跑「剥 blanket `lint_off` 后重 lint」，把结果写成**白名单文件**（每条 `file:line:warning-id:理由`），护栏判「出现的 warning 集 **⊆ 白名单**」；② **增量** —— 出现白名单外的 warning 即红 | exit 1 + 打印 lint 原文 + 差集 |
| F2 | 两次发射逐字节相同（确定性） | exit 1 + unified diff |
| F3 | **三方比对**（⚠️ **critic 第 2 轮订正判据**：C2 撤回后 manifest **全列**端口 ≈ 115 条 ⇒ **不得**再用「34」作期望集）
| F4 | **悬空顶层输入 MUST 可被发现**（**不是**「必须消失」）：每个 `driven == false` 的 input MUST 在 `diagnostics[]` 有对应条目，`--strict` 下失败 | exit 1 + 点名端口 |
| F5 | **死逻辑 MUST 清零**（**两段可判别**，禁写成「无 `assign x = x;`」——实测字面自赋值本就为 0 ⇒ 该写法恒绿 ⇒ 零判别力）：<br>① **零引用信号数 == 0**（声明后除自身外无任何引用的 `logic`/`wire`）⇒ 当前 **107**<br>② **proxy 恒等直通数 == 0**（`assign A_ = A;` 且 `A_` 除该 assign 外无其它引用 ⇒ 整条链是死路）⇒ 当前 **43** | exit 1 + 行号 + 数量 |
| F6 | **backend 行为未退化**：`ctest -L verilator_e2e`（**77** 条）全绿 ＋ `build_port_access_table()` 对每个 `port_access_` 条目都能取到非空 Vtop 字段<br>⚠️ **metis 订正**：原判据「backend 产物 == 产品产物（逐字节）」**恒绿**（`VerilatorBackend::generate_verilog` 与 `toVerilog()` 是**同一个 `verilogwriter`**）⇒ ⇒ 换成**可失败**的行为判据 | exit 1 + ctest 输出 |
| F7 | `--self-test`：逐条原地投毒 ⇒ 必红 ⇒ 还原复绿 | exit 1 |
| — | **fail-closed**：缺 `verilator` / 缺产物 ⇒ exit **2**（禁恒 exit 0） | exit 2 |

⚠️ F1 会**暴露既有宽度问题** ⇒ ⇒ 首轮 MUST 记录基线并**分类**（可修 / 需白名单 / 需上游修）⇒ ⇒ **禁**用 blanket `lint_off` 掩盖 ⇒ ⇒ 白名单 MUST **按节点**且**逐条**登记（不许整文件关）。

### C6 — codegen 卫生批次

- 产物头：`` `default_nettype none `` + 生成元数据注释块 + **保留**既有 `lint_off` 但**收窄**为按节点白名单。
- 剪自赋值：发射器在生成 `assign` 前判 `src == dst` ⇒ 相同则跳过。
- 剪未用信号：`print_decl` 阶段做一次**可达性**剪枝（仅剪**确定无引用**的；跨模块引用 MUST 保守保留）。
- 复位写法：`if (!reset) q <= next; else q <= INIT;` ⇒ 规范化为 `if (reset) q <= INIT; else q <= next;`（**逐拍语义等价** ⇒ 既有 golden MUST 零变化 ⇒ **判别力**：护栏 F1–F7 全绿 + 既有 257 个 ctest 全绿）。

---

## Acceptance

### 产物形态

- [ ] `tools/cpphdl-emit-rtl --top Rv32iPipelineTop -o <dir> --manifest <file>` 产出 `.v` + manifest；`.v` 可被 `verilator --lint-only` 通过
- [ ] manifest 的 `ports[]` 与 `.v` 的端口**双向一致**（F3）
- [x] ~~顶层端口数从 115 降到 34~~ 🔴 **已撤回（保留契约）** ⇒ 改为：manifest MUST 为每个端口给出 `owner` + `is_top_io`，且 `is_top_io == true` 的条目集合 MUST **等于** RV32I SoC 的语义端口集（**7 in + 27 out**，其中 `mem_is_load` 由下游 tie-off 后仍以 `owner=…hazard_unit` / `is_top_io=false` 出现）
- [ ] 产物含 `` `default_nettype none `` + 生成元数据头（C6）
- [ ] 产物**无死逻辑**（F5 两段）：零引用信号数 == 0（当前 107）· proxy 恒等直通数 == 0（当前 43）⇒ ⇒ ⚠️ **planner 复核订正（2026-11-10）**：原判据「产物**无** `assign x = x;` 自赋值」**实测恒绿**（产物字面自赋值 = 0）⇒ ⇒ **作废**，改按 F5 两段判 |
- [ ] 两次发射产物**逐字节相同**（F2）

### 悬空输入

- [x] ~~`hazard_unit.mem_is_load` MUST NOT 出现在顶层端口面~~ 🔴 **判据已作废（critic 第 2 轮：与撤回后的 C2/C3 互斥）** ⇒ 改为：该端口 **MUST 仍是顶层端口**（契约要求 · 无 driver 的 input 一律上顶层），但 manifest MUST 标 `owner = …pipeline.hazard_unit` / `is_top_io = false` / `driven = false` ⇒ ⇒ 让消费方**看得见**；修正落在**设计侧**（`rv32i-dead-component-consolidation` 的装配点 tie-off）
- [ ] 每个顶层 input 的 `driven`（**节点级**）与 `effective`（**功能级**）字段 MUST 如实反映实况⇒ ⇒ 用户一眼能区分「**能驱动但没人消费**」（`driven=false`）/「**接了线但不产生任何行为**」（`driven=true ∧ effective=false`）/「**真活口**」<br>⚠️ **planner 复核订正（2026-11-10）**：metis 原判「`ext_clk` **有**消费者 ⇒ 原稿把它当 unused 例子作废」**只对一半** —— `ext_clk` **节点级有**消费者（`rv32i_pipeline_top.h:137`）但**功能级是死路**（`include/cpu/pipeline/**` 对 `io().clk` 零消费者 + 变异 tie 1 ⇒ 零行为影响）⇒ ⇒ **`ext_clk` 恰是第三类的标准样例**（既非活口也非悬空）⇒ ⇒ 故判据 MUST 同时覆盖两字段，且 `driven=true ∧ effective=false` MUST 记 warning 级 `wired-but-inert`
- [ ] 护栏 F4 能抓「新增一个 **owner ≠ 根** 且无 driver 的 `ch_in`」（投毒自证）
- [ ] ⚠️ **F1 的基线不绿也能自检**（投毒 = 手工往白名单外造一条 ⇒ 必红）⇒ ⇒ 禁「首轮只报告不失败」这种无判据状态进入验收
- [x] ~~`set(flat.inputs) == set(hier.inputs)`~~ 🔴 **已删除** —— 对**任何带悬空子输入的设计**恒假（`hier` 按 `ADR-060 §2.2` 冒泡悬空子输入，而 flat 依契约同样保留）⇒ ⇒ 该 AC 不可满足，且 C2 已撤回后**不再需要**
- [ ] manifest 的 `ports[]` **顺序** == `.v` 端口声明顺序（本项不改排序键 ⇒ 顺带验证「零改动」）
- [ ] ⚠️ **新增**：`toVerilog()` 当前 `catch (...)` 静默吞异常（`src/codegen_verilog.cpp:42-50,131-133`）⇒ ⇒ CLI 的 exit-code 契约**不可达** ⇒ ⇒ MUST 新增**可报告失败**的入口（`EmitResult`），且旧入口保留吞异常语义（兼容）

### 护栏

- [ ] `scripts/verify_flat_rtl_emit.sh` 存在且可独立运行；`--self-test` 逐条投毒 ⇒ 必红 ⇒ 还原复绿（F7）
- [ ] 缺 `verilator` 时退出码 **2**（fail-closed）
- [ ] 护栏进 CI（与既有 `verify_hierarchical_rtl.sh` 同 job）

### 不回归

- [ ] 既有 **257** 个 ctest 全绿（`ctest --output-on-failure`）
- [ ] 既有 `scripts/verify_hierarchical_rtl.sh`（R1–R8）全绿且**产物逐字节不变**（⇒ ⇒ 依赖 C6 的 `emit_hygiene` 默认 off；**若**决定让 hier 也走卫生批次 ⇒ ⇒ 本条 MUST 改为「产物**结构**不变（R6/R8 绿）」并**显式登记字节变更**）
- [ ] verilator 后端的 `work_dir/top.v` 与产品产物**逐字节相同**（F6）⇒ ⇒ 后端行为零变化
- [ ] **端口命名规则 MUST NOT 在本项被定义**（归 `port-naming-ratchet-2026-11`）⇒ ⇒ 本项的 manifest `name` 字段在命名改进落地前**允许**仍无语义

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| **`rtl-emit-portability`**（**新**） | ADDED | 本项的规范所有者：**端口清单（manifest）** 的 schema（`rtl-emit-manifest-v1`，含 `owner` / `is_top_io` / `driven`）· 悬空输入的**诊断义务**（不改端口面）· 产物卫生（`default_nettype none` / 元数据头 / 无自赋值 / 复位写法）· CLI 契约<br>⚠️ **端口选择语义不在本 capability**（C2 已撤回 · 端口面由 `rtl-hierarchical-output` 所有） |
| `core-node-driver-visibility` | MODIFIED | 追加「未驱动且非根端口」这一处置义务（复用 `diagnose_drive()`，**MUST NOT** 另造驱动状态判定） |
| `rtl-hierarchical-output` | **不动** | 本项 MUST NOT 改成层语义 ⇒ ⇒ 层次化侧只**消费**同一份命名/端口基础设施 |

⚠️ **命名规则的规范所有者仍是 `rtl-hierarchical-output`**（`hierarchical-port-naming-guard` 已裁定「命名规则本身归 `rtl-hierarchical-output`，guard capability MUST NOT 复述」）⇒ ⇒ 本项 MUST NOT 在 `rtl-emit-portability` 里另写一份命名规则。

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `src/codegen_verilog.cpp` | ✅ **C2 撤回后 = 零改动**（`make_flat_module_view()` 不改 ⇒ 端口面与顺序**逐字节不变**）⇒ ⇒ 原「端口选择逻辑改 / 产物形态变化 / 既有 golden MUST 更新」**全部作废**；唯一变更来自 C6 的 `emit_hygiene`（默认 off） |
| `src/core/verilator_backend.cpp` | ✅ **C2 撤回后 = 零改动**（端口面无变化 ⇒ `port_access_` / `emit_sim_main_postlude` 与 flat 天然一致）⇒ ⇒ 原「MUST 应用同一过滤函数」的处置**作废**；F6 仍保留为**行为判据**（`ctest -L verilator_e2e` 全绿 + 端口表不失配） |
| `include/codegen_naming.h` | 改名逻辑**共用** ⇒ ⇒ 扁平/层次化**必须同时改**（**优点**：不会漂） |
| 既有 257 个 ctest | **MUST 全绿** ⇒ ⇒ 若某测试逐字节比对 .v ⇒ 需更新 golden（**登记**） |
| `scripts/verify_hierarchical_rtl.sh` | **MUST 仍全绿** ⇒ ⇒ 层次化端口名与扁平**逐字相同**这条契约 ⇒ ⇒ 本项**不改端口面** ⇒ 天然保持；原「C2 改端口选择**不许**破坏它（⇒ ⇒ 根组件口径须**同时**用于两层） |
| CI | 新增一个护栏 job/step ⇒ ⇒ **CI 墙钟** +（verilator lint ≈ 1s/设计 ⇒ 可接受） |
| 计数锚点 | `CTEST_TOTAL` **不变**（本项不新增 ctest 条目）⇒ ⇒ 但若新增「护栏自测」进 ctest 则 +1 ⇒ ⇒ **同步** `AGENTS.md` 的 `CTEST_TOTAL` 行 |

### 已知风险

| 风险 | 缓解 |
|---|---|
| **F1 剥 `lint_off` 后暴露大量既有宽度问题** ⇒ 首轮红 ⇒ ⇒ 阻塞 | 首轮**先跑一次盘点**（只报告不失败）⇒ ⇒ 分类（可修 / 白名单 / 上游）⇒ ⇒ **禁** blanket 掩盖 |
| ~~端口选择改动破坏层次化逐字一致~~ | ✅ **风险已消除（C2 撤回）** ⇒ 本项不碰端口选择 ⇒ 该契约天然保持 |
| ~~80 个内部输出去掉后，既有依赖它们的用户代码破~~ ✅ **已不适用（C2 撤回）** ⇒ 端口面不变 ⇒ 该风险消失；新风险 = 下游若照 `is_top_io` 筛选需自行承担（本项只提供元信息） ⇒ 原缓解措施：
| **manifest schema 未来漂移** | `schema: rtl-emit-manifest-v1` + 版本字段 ⇒ ⇒ 下游 MUST 按 schema 判版本 |

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `port-naming-ratchet-2026-11` | **依赖本项的 manifest**（端口名的规范载体是 manifest 的 `name`+`semantic`）⇒ ⇒ **本项先做** |
| `examples-rtl-sim-coverage-2026-11` | **依赖本项的 CLI + manifest**（TB 骨架由 manifest 生成）⇒ ⇒ **本项先做** |
| `memory-timing-contract-2026-11` | 无耦合（改的是流水线时序，不是 codegen）⇒ ⇒ 可并行 |
| `firmware-load-chain-2026-11` | 无耦合（改的是 SoC 端口契约 ⇒ ⇒ **会改 manifest 的 `semantic`** ⇒ ⇒ 需同步 manifest 快照） |
| `hierarchical-corpus-regression-2026-11` | 本项 MUST NOT 动成层语义 ⇒ ⇒ 两者可并行；**共用** `codegen_naming.h` ⇒ ⇒ 改命名时 MUST 同批 |

## 事实订正（planner 复核 · 2026-11-10）

> **触发**：planner 复核 commit `1dc1d98` 时逐条复现本文档的每个数值锚点 ⇒ 发现 **2 处不可复现 / 1 处内部矛盾** ⇒ ⇒ 纯文本订正，**未动任何决策**（C2 撤回 / B1 保留契约等**全部不动**）。

| # | 原值 | 实测值 | 复现方式 | 影响 |
|---|---|---|---|---|
| **F1** | 产物「含**自赋值死逻辑**（`assign a = a;`）」⇒ ⇒ F5 判据写成「无 `assign x = x;`」 | 产物 **558** 条 `assign` 中**字面自赋值 0 条** | 扫 `rv32i_soc.v` 的 `assign` 目标/源配对 | ⚠️ **F5 原判据恒绿 ⇒ 零判别力**（正是仓内「判别力」纪律要治的形态）⇒ ⇒ F5 改成**两段可判别**：① **零引用信号数 == 0**（当前 **107**）② **proxy 恒等直通数 == 0**（当前 **43** 条 `assign X_ = X;` 且 `X_` 无其它引用）⇒ ⇒ A.1⑦ 的表述同步改写 |
| **F2** | metis：「**`ext_clk` 不是 unused 的例子**（`rv32i_pipeline_top.h:137` 有消费者）⇒ ⇒ 原稿举例作废」 | `ext_clk` **节点级有**消费者（→ `pipeline.io().clk`）但**功能级是死路** | `grep -rn "io().clk" include/cpu/pipeline/` ⇒ **零命中**；变异把该口 tie 1 ⇒ **零行为影响** | ⚠️ 文档内**自相矛盾**：`:133` 的 manifest 示例仍写「当前实现未使用」⇒ ⇒ 正确表述 = 「**有连线但无功能**」的**第三类** ⇒ ⇒ manifest 的 `driven: bool` **表达不了** ⇒ ⇒ 拆成 `driven`（节点级）+ **`effective`（功能级）** 两字段 ⇒ ⇒ `ext_clk` 成为 `driven=true ∧ effective=false` 的**标准样例**（warning 级 `wired-but-inert`） |
| **F3** | —（新增） | — | — | ⚠️ **`mem_is_load` 的修正原本 7 份草案**无任何一条承接** ⇒ ⇒ 落点补入 `rv32i-dead-component-consolidation-2026-11` 的 **C8**（装配点 tie-off）⇒ ⇒ 本文档 C3 的「落在设计侧」由「泛指」改为**指名落点** |

**处置**：F1 ⇒ A.1⑦ / F5 表行 / AC 三处同步改写；F2 ⇒ C4 / manifest 示例 / AC / metis finding 9 四处同步改写 + schema 加 `effective` 字段；F3 ⇒ 本文档 C3 + 姊妹文档 C8 双向指名。
**未变**：C2 撤回、B1 保留契约、F1–F7 的**判据面**、capability 划分**全部不动**（F1 只是把一条恒绿判据换成可判别形态，F2 只是把 manifest 的**元信息**变细，不改端口面）。

## metis 第 1 轮 gap 分析与处置（2026-11-10）

> **轮次**：1（该 improvement 总计 1 轮）· **审查者**：metis（预规划 gap 分析）
> ⚠️ 本节是**过程日志**，不是决策 —— 决策在正文各节的「⚠️ metis 订正」标注处。

### ✅ 已订正（事实错误 / 恒绿判据）

| # | metis finding | 处置 |
|---|---|---|
| 1 | F3「manifest == 产物」= **同源自证恒绿**（两者同出一份 `ModuleView`） | F3 改为**三方比对**：manifest ∥ `verilator --json-only` 独立提取 ∥ **手推期望集** |
| 2 | F6「backend 产物 == 产品产物」**恒绿**（同一个 `verilogwriter`） | F6 改为 `ctest -L verilator_e2e` 全绿 + 端口表不失配；Impact 表的「backend 零行为改动」改为「**端口表同步收窄**」（否则 `emit_sim_main_postlude` 引用不存在的 Vtop 字段 ⇒ `.so` 编译失败） |
| 3 | 「根组件的 `io_type` 成员」**不可枚举**（框架无反射） | ~~C2 改为「owner 为根组件的端口节点」~~ ⇒ ⇒ 🔴 **critic 第 1 轮判定该修正本身与已签批 spec 冲突 ⇒ C2 已整体撤回**（见下节 critic 段） |
| 4 | 「无 driver 的 `ch_in`」**措辞自反**（顶层 input 按定义无 driver） | C3 改为「owner ≠ 根 **且** 无 driver」；F4 判据同步改写 |
| 5 | F1「首轮先盘点再分类」= **无判据**（`--self-test` 的「还原复绿」不可观测） | F1 改为**两段式**：基线白名单（钉在护栏脚本里）+ 增量判红 |
| 6 | F2 逐字节确定性 vs C6「生成时间」**冲突** | 生成时间**只进 manifest**，不进 `.v` |
| 7 | C6 改**共享发射器** ⇒ hier 产物**字节必变** | C6 加 `ModuleView::emit_hygiene` 开关（默认 off · 仅 flat 产品路径） |
| 8 | 端口数**算术错**（原稿「8 in + 27 out = 35」） | 改为 **7 in + 27 out = 34**（泄漏的 `mem_is_load` MUST 消失） |
| 9 | `role: data_unused` 无判定原语 ⇒ ⇒ 删掉该取值 ⇒ 改 `driven: bool` | 删掉该取值 ⇒ 改 `driven: bool`（节点级）⇒ ⇒ ⚠️ **planner 复核订正**：单一 bool 表达不了「有连线但无功能」第三类 ⇒ ⇒ 拆成 `driven`（节点级）+ `effective`（功能级）两字段 ⇒ ⇒ 原稿举例（`ext_clk` 作废）**不是作废，而是改判为第三类的标准样例**（见 A.1 / C4） |
| 10 | `semantic` 字段**无生产者** | 降级为**可选 · v1 无生产者** ⇒ MUST NOT 出现在判据里 |
| 11 | `--strict` 退出码**不可达**（`toVerilog` `catch(...)` 吞异常） | 新增验收：`toVerilog` MUST 有**可报告失败**的入口（`EmitResult`），旧入口保留兼容语义 |
| 12 | 缺 `set(flat.inputs) == set(hier.inputs)`（`hier_eq` 前提） | 新增为验收项 |
| 13 | 「对任意设计可用」与「编译期注册表」**互斥** | C1 明确形态：**工具 = 库 + 薄 CLI**；本仓二进制 MUST NOT 链 `examples/**` |

### ⚠️ 待用户裁决（metis 提出，本项不自行决定）

1. **C2 与既有契约的冲突面**：`ADR-060 §2.2` 明确「**冒泡上来的悬空输入 ⇒ 顶层可驱动**」（层次化 R8 的 `bubble` 期望表钉住了它）⇒ ⇒ C3「悬空输入 MUST NOT 成为顶层端口」在**层次化侧**与之**正面冲突**⇒ ⇒ 两条路线：(a) 不动冒泡契约 + 加 `flat.inputs ⊆ hier.inputs` 判据；(b) 改冒泡契约 ⇒ ⇒ `rtl-hierarchical-output` MUST 标 MODIFIED + R8 期望表同步 + `hier_eq` 前提重证 ⇒ ⇒ **需 architect 裁决**。
2. **`C1` 的形态**：本仓 `cpphdl-emit-rtl` 是「工具」还是「库 + 薄 CLI」？（影响「对任意合法设计可用」的兑现方式）。

## critic 第 1 轮（plan, build）—— **REJECT** · mergeable: no · scope_exceeded: false

> **轮次**：1/2（预算 2 轮）· 审查者：critic · 依 §评审预算：**只有 BLOCKER 触发返工**

### 🔴 BLOCKER-1（**需 architect**）：C2 与**已签批 spec** 正面冲突

| 事实 | 证据 |
|---|---|
| 已签批契约：顶层端口表 MUST 是 `clock → reset → 无 driver 的 input → **全部 `ch_out``（⇒ **含 inline 后代的 `ch_out`**） | `openspec/specs/rtl-hierarchical-output/spec.md:309-316`（Scenario「RTL 顶层 module MUST 走扁平规则」） |
| 生产代码**零** `CH_HIERARCHY_LAYER`（仅宏定义 + infra：`include/component.h` / `include/module.h` / `include/hierarchy_verilog.h` / `src/component.cpp`）⇒ RV32I SoC **全 inline** | `grep -rln CH_HIERARCHY_LAYER include/ src/ examples/` |

⇒ ⇒ **那 80 个「内部子组件输出」按契约本就必须在顶层**，**不是泄漏** ⇒ ⇒ C2 把它们移出顶层 = **改契约**。
⇒ ⇒ 而本文档 Capabilities 表把 `rtl-hierarchical-output` 标为「**不动**」⇒ **同一文档自相矛盾**。

**处置**：⏸ **不在 planner 域裁决** ⇒ 需 architect 裁定「ADR-061 决策② 作用域表 / ADR-062 §5 是否修订」。
候选路线：(a) 承认契约正确 ⇒ C2 **撤回**，只保留 C3（诊断）+ manifest（端口全列，但标 `owner` 与 `is_top_io`）；(b) 修订 spec ⇒ 走 ADR 取代流程。

### 🔴 BLOCKER-2（同一根因）：新增 AC `set(flat.inputs) == set(hier.inputs)` 在一般情形**不可满足**

`hier` 按 `ADR-060 §2.2` 冒泡悬空子输入（`spec:190-196`），而 flat 按 owner 过滤掉它们 ⇒ ⇒ 对**任何带悬空子输入的设计**该 AC 恒假。
oracle ⑦ 的 tie-off 只修了 RV32I **这一个实例**，未解决**一般规则**。

**处置**：⏸ 随 BLOCKER-1 一并交 architect；若采路线 (a) ⇒ 该 AC **删除**。

### ✅ 已采纳的 SHOULD_FIX

- ① F3 清单**由谁生成**未说明 ⇒ 补：清单来自 CLI 参数（**非 emitter 自产**），否则回退成自证。
- ② C3 的 tie-off 是**行为修正**（flat 产物该端口从「顶层 X 输入」变「常量 0 内部线」）⇒ 在 Impact 明写（C++ golden 不受影响，解释器未驱动读 0）。
- ③ 「如实登记」缺载体 ⇒ 统一改写为「登记进 `.rddf/checks/arch-residual-registry.md`（状态/日期/闭口证据/复核条件）」。
