# ADR-072: 「RTL 生成能力产品化」—— 顶层端口面收敛 + 产品边界与验收契约

- **Status**: **Accepted**（2026-11-16 · architect role（build agent）· oracle 第 1 轮 **REJECT**（3 BLOCKER + 5 SHOULD_FIX）⇒ ⇒ 全闭 ⇒ critic 第 1 轮 **REJECT**（1 BLOCKER + 3 SHOULD_FIX + 2 NIT）⇒ ⇒ 全闭 ⇒ critic 第 2 轮（**复审**）**APPROVE** · **0 BLOCKER** · `mergeable: yes`）⚠️ **§2.1 D1 的条件式（V1 失败 ⇒ 退到 ④）仍是「显式标注的未决分支」** ⇒ ⇒ 该分支一旦触发 ⇒ ⇒ 决策内容变更 ⇒ ⇒ MUST 重新审稿
- **Date**: 2026-11-16
- **Author**: architect role（build agent）
- **Parent**: **ADR-046**（RV32I 收敛 · **D10** SoC 顶层端口冻结）· **ADR-058**（层次化 RTL 输出 · 语义边界与等价性）· **ADR-060**（层次化端口策略与 module 复用）· **ADR-061**（粒度由用户显式控制）· **ADR-070**（RV32I 成层粒度 + 兄弟边修法）· **ADR-071**（`FENCE` / 未知 opcode 冻结语义）
- **关联 change**: `backlog: rtl-productization-surface-2026-11`（**尚未立项** ⇒ ⇒ 立项后回填本指针 + AC 映射）
- **AC 映射**: 本 ADR §5 的 **V1–V15** → 待立项 change `rtl-productization-surface-2026-11` 的 AC（编号待回填）
- **审稿记录**: `.rddf/reviews/ADR-072-rtl-productization-surface.md`（oracle ×1 · critic ×2（第 2 轮 = 复审 · `mergeable: yes`）· 全 finding 逐条应答已处置）
- **超长自检**: 正文 **287 行**（> 250）⇒ ⇒ **自问「这是不是设计规格？」= 否** ⇒ ⇒ 决策本身 = §2 的 7 条 + §3 的 9 行对比表 + §4 的后果 + §5 的判据；**逐条推导 / 证据 / 爆炸半径逐项**留在 §1.2 的事实表（19 条 · 带 `file:line`）与 §2.1 的必改表（12 行 · 逐句清单）⇒ ⇒ **未拆分**（仓内 ADR 既有体量：ADR-060 = 779 行 / ADR-066 = 638 行 ⇒ ⇒ 287 在既有区间内）

---

## 1. 背景与问题

### 1.1 诉求（用户 2026-11-16）

两问合一件：(1) **评估** CppHDL 能否生成**功能完整的 RISC-V CPU 核 RTL**；(2) **推进**为「**RTL 生成能力产品化**」，
覆盖面 = 通用生成器 + 独立 Verilog testbench · 固件转储器/ELF 装载 · examples 全覆盖 · 层次化输出示例 ·
端口名与 C++ 侧一致 · RV32I 重复实现收口 · RTL 卫生。

### 1.2 实测事实（2026-11-16 · 可复算）

| # | 事实 | 证据 |
|---|---|---|
| ① | ✅ **扁平 RTL 可生成**：RV32I SoC 产物 **130 端口**（6 输入 + 124 输出）· **诊断 0 条** · manifest **213 条** | `build/examples/riscv-mini/emit_rtl_rv32i_soc --out <dir>` ⇒ `OK：端口 213 个 · 诊断 0 条`；产物端口表逐行核对 = 130（与 `examples/riscv-mini/tb_rv32i_soc.sv:60` 自述「130（6 输入 + 124 输出 · 2026-11-16 实测）」**独立吻合**） |
| ② | ✅ **层次化 RTL 可生成**：**8 module**（7 stage + `top`）· 顶层 **68 端口** | 只读探针 `toVerilogHierarchical` ⇒ `IfStage 13 / IdStage 31 / ExStage 21 / MemStage 11 / WbStage 11 / HazardUnit 22 / ForwardingMux_32_ 10 / top 68`；`module_count == 8` 与 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp:171` 的断言一致 |
| ③ | ✅ **独立 Verilog TB 可跑**：`checks=24 fails=0`（小程序**手推**编码） | `examples/riscv-mini/tb_rv32i_soc.sv` · 断言值由 ISA 语义手推（文件头逐条列出 7 个字的编码） |
| ④ | ✅ **固件链路完整**：ELF32 + `PT_LOAD` · `.bss` 补零 · 越界 exit 3 · 三格式（`mem`/`vh`/`manifest`） | `tools/rv32i-fw-dump/main.cpp` 头注释 + 实现 · `scripts/verify_rv32i_fw_dump.sh` |
| ⑤ | ✅ **ISA（用户态）覆盖完整** + 非法指令**可观测冻结**（`illegal_reason` = **4 态**：0 合法 / 1 已知未实现 / 2 未知 opcode / **3 保留组合**） | `include/cpu/pipeline/rv32i_decoder.h:217-220`（4 态赋值）+ `include/cpu/pipeline/stages/id_stage.h:324` · ADR-071 Accepted |
| ⑥ | 🔴 **顶层端口面 = 130 / 68，其中 **96 / 34** 是后代 `ch_out` 冒泡**（无语义） | 扁平：根组件自有 **32** + 框架 **2** + 冒泡 **96** = 130 · 层次化：32 + 2 + **34** = 68 · 冒泡者形如 `top_pipeline_unnamed_output_17` / `top_itcm_unnamed_output` |
| ⑦ | 🔴 **codegen 没有「顶层面」概念** —— 端口选择规则 = 「图中**每个** `type_output` 都是顶层端口」 | `src/codegen_verilog.cpp:410-448`（`make_flat_module_view` 收集 `type_input`/`type_output`/`type_clock`/`type_reset` ⇒ 只对 `type_input` 有 driver 者做过滤） |
| ⑧ | 🔴 **manifest 不携带产品的端口表** —— `is_top_io` 的语义是「**根组件自有** IO」（`entry.is_top_io = (owner == &root)`）⇒ ⇒ 产物里 **98 个**顶层端口在 manifest 中标为 `is_top_io: false` | `src/rtl_emit/manifest.cpp:155-156`（注释自陈「**不是** inline 后代输出」）· 实测：manifest `is_top_io: true` = **32**，产物顶层端口 = **130** |
| ⑨ | 🔴 **「examples 全覆盖」真实兑现 9/28，且缺口**结构性不可见** | `CH_TRACKED_EXAMPLE_TARGETS` 实有 **28** 项（`examples/CMakeLists.txt:34-76` 逐行数）· tsv 数据行 **10**（9 target + 1 `kind=dut`）· 护栏 `scripts/verify_examples_rtl_sim.sh:89-107` ⓪ 只比「tsv 行数 vs tsv 自己的锚点」⇒ ⇒ **从不读 tracked 集合** |
| ⑩ | 🔴 **排除表漂移**：`.rddf/checks/examples-rtl-sim-exclusions.md:23` 写「共 **19** 项 ⇒ 落选 10 项」⇒ ⇒ 实为 **28** 项 ⇒ 落选 **19** 项（**9 项两表皆无**） | 同上文件 §2 标题行 vs `examples/CMakeLists.txt` 实测 |
| ⑪ | ⚠️ **端口名** = 根组件 32 个**已具名**（`top_prog_we` ↔ C++ 成员 `prog_we` ⇒ **前缀 `top_` 仍在**）· 冒泡的 96/34 个**无语义**（`unnamed_output_N`，编号随 eval-list 序漂移） | 产物端口表逐行核对 · 残差 id=21（route (c) 字面同名）**已由用户 2026-11-15 裁决「不要求」** |
| ⑫ | ⚠️ **卫生面默认关闭**：`emit_hygiene` **默认 off** | `include/rtl_emit/cli.h:46-49`（默认-off 的**成文理由只有一条** = 保产物逐字节不变，批次表 ★3） |
| ⑬ | 🔴 **capability 契约与「端口面收敛」逐字相反** | `openspec/specs/rtl-hierarchical-output/spec.md:48-50`：「祖先 = RTL 顶层 ⇒ 照扁平规则（任意 `ch_out` ⇒ 顶层 `output`，**包括已被父层消费的** ⇒ **MUST NOT** 降成纯内部 wire）」⇒ ⇒ **D1 MUST 携带该 capability 的 MODIFIED delta**，否则 change 过不了归档合并 |
| ⑭ | 🔴 **Verilator backend 的端口面由 ctx 全量推导** ⇒ ⇒ D1 后会引用**不存在的 Vtop 字段** | `src/core/verilator_backend.cpp:857-867`（`build_port_access_table` 遍历 eval-list 收**全部** `type_output`）+ `:238-343`（`emit_sim_main_postlude` 给每个 `type_output` 生成 `v-><vname>` 访问 case）⇒ ⇒ `libVtop.so` **编译必炸** ⇒ ⇒ 全部 auto-mode `verilator_e2e`（**77** 个）转红 |
| ⑮ | ⚠️ **ctx-only 公开 API 无法收敛**（ctx 内**无**组件注册表） | `include/core/context.h:83-247`（全类无 Component 注册表）· `include/codegen_verilog.h:65`（`toVerilog(filename, ctx)`）⇒ ⇒ 仓内 **49** 处活跃 ctx-only 调用点（`examples/` 21 · `tests/` 18 · `samples/` 5 · `src/` 5 · 口径：排除注释行与头文件声明）⇒ ⇒ **无根可推导** ⇒ ⇒ 这些路径**无法**执行 D1 的收敛规则。⚠️ **产品路径今天走的正是 ctx-only**：`src/rtl_emit/cli.cpp:181` = `toVerilog(options.verilog_path, ctx.get(), emit_opts)` |
| ⑯ | ✅ **产品路径已持有根** ⇒ ⇒ 「拿不到根」不是障碍 | `src/rtl_emit/cli.cpp:144-161`（`run_emit_cli` 自造 root ⇒ `build_rtl_manifest(*root, ctx.get())`）· `ch_device::instance()` 同理 |
| ⑰ | ⚠️ **D1 会让 axi4_lite TB 的 S8 场景整体消失** | `examples/axi4/tb_axi4_lite_example.sv:393-401`（「S8：inline 后代端口与顶层端口同源」= **8** 条 `expect_*`）⇒ ⇒ `checks` 45 → **37** · `scenarios` 8 → **7** ⇒ ⇒ tsv 第 40 行的 `min_checks=45` **必须重登记** |
| ⑱ | ⚠️ **仓内漂移**：根 `AGENTS.md` 的 `illegal_reason` 仍写 **3 态** | `AGENTS.md` KNOWN LIMITATIONS §RV32I ISA 覆盖边界 vs 事实 ⑤（4 态，commit `0e44c0a`）⇒ ⇒ **另立小项订正**，不塞进本 change |
| ⑲ | 🔴 **D1 的冲突面横跨 2 份 capability + 2 份 Accepted ADR**（**不止**「顶层端口表枚举句」） | ① `openspec/specs/rtl-emit-portability/spec.md:57-62`：「发射器 **MUST NOT** 因 `is_top_io` 而改变端口面或端口顺序」⇒ ⇒ **与 D1 逐字相反**，且它是残差 id=23「保持现状」的**成文依据**；② 同文件 `:8-34`：manifest MUST 携带 v1 的**全部 10 个字段**（新增字段 ⇒ ⇒ **schema 版本变更**）+ `tests/test_rtl_manifest.cpp:114` 钉 `schema == "rtl-emit-manifest-v1"`；③ `openspec/specs/rtl-hierarchical-output/spec.md:36-40`：「端口表收缩 MUST **只发生在成层子树**」；④ 同文件 `:190-214`：「悬空子输入 MUST 向上冒泡为顶层输入」+「扁平与层次化**都**暴露为顶层可驱动输入」；⑤ 同文件 `:318-330`：「**排他句（承重）**：层次化顶层 MUST 含**全部** inline 后代的 `ch_out`」；⑥ `docs/adr/ADR-058-hierarchical-rtl-output.md:63-64` 与 `docs/adr/ADR-061-hierarchical-granularity-user-control.md:132`：「任意 `ch_out` ⇒ 顶层 `output`」——**写死的枚举**，不是活指针 ⇒ ⇒ 属**签批的决策内容** |

### 1.3 评估结论（**能 / 不能的精确边界**）

> **结论：能力上「能」—— 产物形态上「还不能称为 CPU 核 RTL 产品」。**

- ✅ **「能生成」已由事实 ①②③④⑤ 实证**：一个真实 RV32I 5 级流水线的**可综合 Verilog**（扁平 + 8 module 层次化）·
  **可独立运行的 Verilog TB** · **ELF 固件装载链路** · **用户态 RV32I ISA 覆盖**。
- 🔴 **「产品」尚不成立**，三个**正交**的缺口：

| # | 缺口 | 为什么它是「产品」级问题（不是「偏好」） |
|---|---|---|
| **G1** | **顶层端口面未收敛**（事实 ⑥⑦⑧⑬⑭⑮） | 交付给下游的是「一个 130 端口的核」，其中 **96 个是内部信号的裸奔**（`unnamed_output_N` ⇒ 编号随 eval-list 序漂移 ⇒ **端口名不稳定**）⇒ ⇒ 「端口名与 C++ 侧一致」这条诉求在**冒泡端口上根本无法成立**（它们没有 C++ 成员名可对）⇒ ⇒ 且 manifest 与产物**不自洽**（事实 ⑧）⇒ ⇒ 且与既有 capability **逐字相反**（事实 ⑬） |
| **G2** | **特权级缺失 ⇒ 「冻结是终态」** | 事实 ⑤ 只覆盖**用户态**；遇未实现指令 ⇒ **冻结且永不恢复** ⇒ ⇒ 「**功能完整**」若不声明该限制 ⇒ 是**误导性宣传** ⇒ ⇒ 该限制**不能**被本次产品化顺手塞掉（`AGENTS.md` KNOWN LIMITATIONS 要求独立立项） |
| **G3** | **覆盖缺口不可见**（事实 ⑨⑩） | 护栏 ⓪ 是**自洽判据** ⇒ ⇒ 「先做 10 个 ⇒ 永远只有 10 个」这个 E4 明文要防的风险**仍然成立**，且已经漂了（19 → 28） |

### 1.4 三个缺口**正交**（⇒ ⇒ 可分批落地，不必原子）

G1 = **发射器面** · G2 = **ISA 集成面**（属特权级独立立项 ⇒ ⇒ 与 G1 零写点交集）· G3 = **登记/护栏面**。

⚠️ 但 **G1 的「同批必改」集合很大**（§2.1 表的 **12 项**）⇒ ⇒ **G1 MUST 原子落地**（一次改完端口面 + manifest + TB + backend + 两份 capability + 两份 Accepted ADR 的 addendum + 护栏/测试期望集 + 公开面登记 + 残差 id=23 两侧）⇒ ⇒ 拆批 ⇒ ⇒ 中间态出现「声称成立但不成立」。

---

## 2. 决策

### 2.1 **D1**（G1）= 顶层端口面 = **根组件自有 `__io`**（**直接收敛** · 推翻残差 id=23 的 2026-11-11 裁决）

**契约（新）**：

> 顶层端口表 = `default_clock` → `default_reset` → **根组件自有 `__io`**（无内部 driver 的 `ch_in` + 全部 `ch_out`）
> ⇒ ⇒ 后代组件的 `ch_out`（含被父层消费的）**一律降为内部 `logic`**（不是端口）。

⇒ ⇒ **RV32I SoC**：扁平 **130 → 34**（6 输入 + 28 输出）· 层次化 **68 → 34**。
⇒ ⇒ **与 ADR-046 D10 同向**：D10 冻结的正是 `Rv32iPipelineTop` 的**根组件自有 32 个** ⇒ ⇒ 本决策让**产物顶层**收敛到那一套 ⇒ ⇒ **不是新造契约，是对齐**。

⚠️ **D1 的形态是条件式（**决策段内可见**，不是脚注）**：

> **D1 = ①**（顶层端口 == 根组件自有 `__io`）**｜ 若 §5 V1 的落地确认失败**（ctx-only 路径的处置登记未落，**或**产品路径未能切到带根重载 ⇒ ⇒ D1 在产品路径上不生效）
> **⇒ ⇒ 退到 ④**（只修 manifest 不自洽，顶层端口面不动）
> ⇒ ⇒ 退到 ④ = **决策内容变更** ⇒ ⇒ MUST **重新审稿**（不得就地扩权）。
> ⚠️ **V1 不是「未知量探针」** —— 事实 ⑯ 已判「产品路径**已持有根**」⇒ ⇒ V1 只是**确认 (a) 的登记与 ⑩ 的 CLI 切换已落地** ⇒ ⇒
> ⚠️ **实施方 MUST NOT 自行改选 (b)**（= 49 处调用点迁移）⇒ ⇒ 改选 = 决策内容变更 ⇒ ⇒ 需复审。

⚠️ **机制**：
- **层次化路径**：`toVerilogHierarchical` 拿得到 root ⇒ ⇒ 把「顶层端口 == root 自有 IO」写进其端口选择（现状 = 父层消费的子 `ch_out` 也进顶层端口表）。
- **扁平路径**：`make_flat_module_view` 只拿 `ctx` ⇒ ⇒ MUST 携带根组件的 IO 集 ⇒ ⇒ 且 `print_decl` 的「角色③」路径（`src/codegen_verilog.cpp:830-844`）**已存在** ⇒ ⇒ 把冒泡 output 从 `view_.ports` 移到 `wires` **即可复用**（不必新写声明逻辑）。
- ⚠️ **ctx-only 公开 API 的处置（**已写死** · 事实 ⑮）**：**选 (a) 保留旧行为 + 登记为「非产品遗留路径」**。
理由：`toVerilog(filename, ctx)` / `verilogwriter(ctx)`（`include/codegen_verilog.h:65`）**无根可推导**
⇒ ⇒ 迁移 **49** 处活跃调用点的爆炸半径**远超 D1 本身**，且 (a) 与「**产品面**收敛」的目标**不冲突**
⇒ ⇒ **产品面 = `rtl_emit` 路径**（`run_emit_cli`），ctx-only 路径**不是产品面**。
⇒ ⇒ ⚠️ **但 (a) 的前提 = 产品路径 MUST 切到带根重载** ⇒ ⇒ 见同批必改第 ⑩ 项（`src/rtl_emit/cli.cpp:181`
今天调的是 ctx-only 的 `toVerilog(path, ctx, opts)` ⇒ ⇒ **不切 = D1 在产品路径上根本不生效**）。
⇒ ⇒ **MUST NOT** 既不写死处置、也不登记（那会让实施者就地扩权）。
⇒ ⇒ **非产品遗留路径 MUST 在 manifest / 工具文档里显式标注**（否则下游会把它当产品路径用）。

⚠️ **同批必改（爆炸半径 · MUST 同批 · 承重前置排在最前）**：

| # | 必改项 | 原因（事实） |
|---|---|---|
| ① | 🔴 **Verilator backend 的端口面**：`build_port_access_table` + `emit_sim_main_postlude` MUST 改为**由发射产物端口表派生**（复用 `emit_sim_main_postlude_for_ports` 的显式集合形态 · ADR-058 E1 同型）· `tests/test_verilator_backend.cpp` 的 `port_access_snapshot` 基线重算 | ⑭ —— 否则 `libVtop.so` 编译必炸 ⇒ **77** 个 `verilator_e2e` 全红 |
| ② | 🔴 **capability delta（**逐句清单** · 事实 ⑲）** —— MUST 走 change 流程（禁直改主 spec），**两份 capability 各一条 MODIFIED**：<br>① `rtl-emit-portability/spec.md:57-62`：「发射器 MUST NOT 因 `is_top_io` 而改变端口面或端口顺序」⇒ **删/改**为「发射器 MUST 按**产品端口表**（root 自有 IO）产端口面；`is_top_io` 仍只是**消费方**的筛选信号」<br>② `rtl-emit-portability/spec.md:8-34`：manifest 的 **10 字段**条款 ⇒ ⇒ 新增字段 ⇒ ⇒ **schema 版本修订**（`rtl-emit-manifest-v1` → **v2**）+ `tests/test_rtl_manifest.cpp:114` 的 schema 名断言同步<br>③ `rtl-hierarchical-output/spec.md:36-40`：「端口表收缩 MUST 只发生在成层子树」⇒ **改**为「收缩发生在**顶层端口面**与成层子树**两处**，前者由 D1 契约定，后者由粒度定」<br>④ `rtl-hierarchical-output/spec.md:190-214`：「悬空子输入 MUST 向上冒泡为顶层输入」+「两份产物**都**暴露为顶层可驱动输入」⇒ ⇒ **D1 下后代悬空 `ch_in` 不再上顶层** ⇒ ⇒ 该条**结构性不成立** ⇒ ⇒ MUST 改为「后代悬空 `ch_in` **不上顶层**；根自己的悬空 `ch_in` 仍 MUST 上顶层」<br>⑤ `rtl-hierarchical-output/spec.md:318-330`：「**排他句（承重）**：层次化顶层 MUST 含**全部** inline 后代的 `ch_out`」⇒ ⇒ **D1 直接推翻** ⇒ ⇒ MUST 删/改 | ⑬⑲ —— **不逐句清理 ⇒ ⇒ 归档后两份主 spec 仍冻着与新产品逐字相反的条款 ⇒ ⇒ 双权威** |
| ③ | **manifest 契约**：MUST 显式区分三类（**产品端口** / **内部信号** / **框架基础设施**）⇒ ⇒ `is_top_io` 一字段**承载不了** ⇒ ⇒ MUST **新增字段**（如 `in_port_table`）或**新增 `port_table[]` 数组** ⇒ ⇒ 落地后 manifest 与产物**自洽**（34 == 34） | ⑧ |
| ④ | **10 份手写 TB**（`examples/**/*.sv`）的端口映射 · **axi4_lite 的 S8 场景整体删除** ⇒ ⇒ `min_checks` 45 → 37 · `scenarios` 8 → 7 | ⑰ —— 冒泡端口从产物消失 ⇒ 现有连线**全部悬空**（Verilator 报未声明标识符 ⇒ 必红） |
| ⑤ | **`scripts/lib/` 的 F3 checker**（`--check-top-ports`）的契约文本 + `scripts/verify_flat_rtl_emit.sh:119` 的 F3 契约 echo 文本 + `scripts/verify_hierarchical_rtl.sh` R8 期望集 + `verify_hierarchical_port_naming.sh` 语料 + `tests/test_hierarchy_verilog.cpp` 的顶层端口文本断言 | 现状契约**成文**写死「clock → reset → 无 driver 的 input → **全部 ch_out**」⇒ ⇒ 不是实现细节 |
| ⑥ | **`.rddf/checks/examples-rtl-sim-targets.tsv`** + `examples-rtl-expectations.md` | 与 ④ 同源 |
| ⑦ | **公开面登记**：manifest 新字段 + 带根重载 = **公开 API 变更**（非移除）⇒ ⇒ 走「行为变更（非符号移除）」节 + `docs/CHANGELOG.md` | `.rddf/checks/public-api-surface.md` 机制 |
| ⑧ | **残差 id=23 的状态**（`.rddf/checks/arch-residual-registry.md` + `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7，**两侧同批**） | 「保持现状」的裁决被**推翻** ⇒ ⇒ 护栏 `verify_arch_ledger_residuals.sh` 判双向 1:1 ⇒ ⇒ 单改一侧必红 |
| ⑨ | 🔴 **两份 Accepted ADR 的 dated addendum**：`ADR-058:63-64` + `ADR-061:132`（「任意 `ch_out` ⇒ 顶层 `output`」是**写死的决策内容**，不是活指针）⇒ ⇒ MUST 补 **dated addendum**（记录 D1 对**产品路径**的取代 + 保留 ctx-only 遗留路径的旧语义）⇒ ⇒ **决策内容变更 ⇒ ⇒ 需复审** | ⑲ —— 否则 Accepted ADR 与新契约**双权威**（正是 §4.3 ③ 自己警告的「第二份必然漂移」） |
| ⑩ | 🔴 **CLI 切到带根重载**：`src/rtl_emit/cli.cpp:181` 调的 `toVerilog(path, ctx, opts)` ⇒ ⇒ MUST 改调**带根**重载（`root` 已在 `cli.cpp:144-161` 持有）⇒ ⇒ **否则 D1 在产品路径上根本不生效** | ⑮⑯ |
| ⑪ | 🔴 **承重测试载体**（不漏 = 「声称原子但不原子」）：`tests/test_rtl_manifest.cpp:114,228-240`（schema 名 + JSON 字段形状）· `tests/test_rtl_tb_stub.cpp:64-78`（TB 骨架「只连顶层端口」的索引来源 = 产品端口表）· `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp:218-225`（「层次化端口行 == 扁平端口行**前缀**」判据 —— 扁平变 34 后该**前缀关系**仍成立与否**未被任何判据覆盖**）· `tests/test_rtl_emit_contract.cpp` · `tests/test_hierarchy_equivalence.cpp`（「精确期望集」公式 `:198,258` 的语义随 D1 改变）⇒ ⇒ **MUST 同批复核**，禁「顺手」 | 事实 ⑲ 触发的连锁面 |
| ⑫ | 🔴 **用户面/架构文档同步**（PHASE GATE「文档同步」义务）：`docs/usage_guide/05-component-design.md:552,680`（「子模块输出**全部**暴露在顶层端口列表」+ 根 module 端口枚举）· `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md:49-55,88` · `PORT-NAMING-GUIDE.md:78` ⇒ ⇒ 「扁平规则」从此**一分为二**（ctx-only 旧规则 vs 带根新规则）⇒ ⇒ 这些文档 MUST 写明**哪条入口适用哪条规则** | 事实 ⑲ —— 否则即**双权威漂移** |

⚠️ **NOT IN SCOPE**：**不**改 ADR-046 D10 冻结的 32 端口本身（`Rv32iPipelineTop` 的 `__io` 一字不动）⇒ ⇒ C6 端口快照**零改动**。

### 2.2 **D2** = **产品单元**的定义（**四件套**）

> 一个「**RTL 生成产品单元**」= ① 一份 **manifest**（机器可读契约）· ② 一份 **Verilog 产物** ·
> ③ 一份**独立可跑的 Verilog TB**（自带激励 + 断言 + `RESULT: checks=<N> fails=0` + 失败非零退出码）·
> ④ 一条**覆盖登记**（进 `.rddf/checks/examples-rtl-sim-targets.tsv`）。

⇒ ⇒ 「**产品化完成**」的判据 = **四件套齐备且 TB 独立可跑**（⇒ ⇒ 诉求 ①③④ 的**共同收口**）。
⇒ ⇒ 「产品化**未**完成」的形态 = 只有 ①②（**工具**，不是**产品**）。

### 2.3 **D3**（G2）= ISA 边界 = **RV32I 用户态**（**不含**特权级）

- 「功能完整」在本产品语境下的**确切含义** = RV32I **用户态**（含 `FENCE`=NOP · 非法指令 **4 态** `illegal_reason` 的**可观测冻结**）·
  **不含** CSR 指令 / `ECALL` / `EBREAK` / `MRET` / trap 入口 / 中断。
- ⇒ ⇒ 特权级 = **独立立项**（`.rddf/improvements/rv32i-privileged-csr-2026-11.md` · **已 draft** · metis 17 条已采纳）⇒ ⇒
  **本产品化 MUST NOT 顺手把它塞进来**（`AGENTS.md` KNOWN LIMITATIONS 要求独立立项 · 且 D3 明确它与 G1/G3 **零写点交集** ⇒ ⇒ 塞进来无任何工程收益）。
- ⇒ ⇒ **但产品 MUST 显式声明该限制**（manifest / 产品文档至少一处）⇒ ⇒ 理由 =
  「**冻结是终态**」（ADR-071 §4.3：没有恢复路径 ⇒ 只能复位）⇒ ⇒ 不声明 = **误导性宣传**。
- ⇒ ⇒ 「功能完整」这个词在本仓语境下 **MUST NOT** 被用来暗示特权级已覆盖。
- ⚠️ **事实 ⑤ 的边界**：`rv32i-privilege` capability 已存在（组件级 `mepc`/`mcause` 写入配对）⇒ ⇒
  但「mtvec / mstatus / `MRET` / mtval」仍属特权级立项 ⇒ ⇒ **组件级存在 ≠ 指令级集成** ⇒ ⇒ D3 的边界**按指令级**划。

### 2.4 **D4** = **通用 TB 的产品形态** = **机械部分生成 + 判据人工写**

| 部分 | 谁产出 | 理由 |
|---|---|---|
| 端口连线 · clk/rst · VCD 开关 | **生成器**（`--emit-tb-stub` 已有） | 纯机械 ⇒ ⇒ 可自动化 |
| **判据结果汇总骨架**（`RESULT: checks=<N> fails=0` 打印 + 失败时非零退出码） | **生成器** | 纯机械 ⇒ ⇒ **且它是「可独立跑」的载体**（⇒ ⇒ 诉求 ① 的「独立」二字落在这里） |
| **激励 + 断言 + 期望值** | **人**（手推） | ⇒ ⇒ 期望值 **MUST 手推**（禁抄仿真输出）⇒ ⇒ **不可自动化** ⇒ ⇒ 两后端同源 ⇒ parity 对该族缺陷**结构性失明** |

⇒ ⇒ **期望管理（关键）**：「examples 全覆盖」= **人工工作**，**不是**生成器工作 ⇒ ⇒ 排期 MUST 按人工成本算，
⇒ ⇒ **MUST NOT** 把「通用生成器」承诺成「一键产出完整 TB」。

### 2.5 **D5**（G3）= 覆盖口径 = **维持首批 10** + **登记完备性 MUST 补**

- **覆盖面** = 首批 **10**（9 个 tracked target + 1 个 `kind=dut`）⇒ ⇒ **用户 2026-11-16 裁决维持** ⇒ ⇒
  tsv 锚点 `REGISTERED_TARGETS: 10` **不变**。
- ⇒ ⇒ 但 **登记完备性** MUST 补（这是**事实订正 + 防漂**，不是扩范围）：
  - **每个 tracked target MUST 落在两者之一**：tsv（已覆盖）**或** 排除表（未覆盖 + 理由 + 归位条件）⇒ ⇒
    护栏新增该判据 ⇒ ⇒ **19 → 28 的漂移被结构性堵死**。
  - 排除表的**计数 MUST 从 tracked 集合派生**（禁手写计数）⇒ ⇒ 修掉事实 ⑩ 的漂移。
  - ⇒ ⇒ 现状的 **19 个未覆盖** item MUST 逐条补理由（其中 10 条**已有**理由 ⇒ ⇒ 只需补新增的 9 条）。

### 2.6 **D6** = RV32I 收口的**判据**（**流程契约** · 不是一次性工作）

- **判据** = 「**同一语义只有一个权威实现**」⇒ ⇒ B4 C1–C4 已把 ALU / Decoder 的重复实现收掉（差分清单 Δ1–Δ10）⇒ ⇒
  **本条 MUST 变成常设前置门**：**任何新组件接入流水线的 change MUST 先过差分测试**
  （形态 = `tests/test_rv32i_alu_differential.cpp`：差分 + **手推**期望 + 变异自证）⇒ ⇒
  否则「收口」只是一次性清扫，下一个组件会重新引入重复实现。
- ⇒ ⇒ 特权级接入（未来）**同样适用** ⇒ ⇒ 这是 D3 的**未来**保障。

### 2.7 **D7**（诉求 ⑦）= **RTL 卫生进默认档**（**仅 CLI 层默认**）

- **A 类（进默认档 · 属产物正确性）**：零未引用信号 · 零恒等直通（`assign X = X`）· 零未声明引用 ·
  `verilator --lint-only` exit 0 · 逐字节确定性 · manifest 诊断 0 条。
- **B 类（进默认档 · 属形态偏好）**：`default_nettype none` · 确定性元数据头（时间只进 manifest）·
  不发 blanket `lint_off` · 复位写法规范化。
- ⚠️ **作用域（**必须限定** · 防误伤 backend**）**：**只翻 `CliOptions::emit_hygiene` 的默认值**；
  **库层默认不动** —— `EmitOptions{}` / `verilogwriter(ctx)` 单参构造是**每个 auto-mode Verilator 仿真**的路径
  （`src/core/verilator_backend.cpp:709`）⇒ ⇒ 翻它 = 改 **77** 个 `verilator_e2e` 的编译形态 + 缓存 key + 多处文本断言基线
  ⇒ ⇒ **超出本决策范围**。
- ⚠️ **为什么 B 类也能进默认档**：`emit_hygiene` 默认为 off 的**唯一**理由 = 「保层次化产物逐字节不变」（批次表 ★3）
  ⇒ ⇒ **D1 已改默认产物字节** ⇒ ⇒ 该理由**消失**。
- ⇒ ⇒ 护栏已覆盖两种形态（F1-a 原样 / F1-b 剥 lint_off / F8 卫生产物）⇒ ⇒ 但 **F2–F7 的靶子**在默认翻转后
  从「默认产物」变为「卫生产物」⇒ ⇒ **MUST 逐条复核**其判据在新靶子上仍成立（**禁**一句「结构不变」带过）。
- ⇒ ⇒ 仓内两处显式 `--emit-hygiene` 调用（`scripts/verify_examples_rtl_sim.sh:201`、`scripts/verify_flat_rtl_emit.sh:212,245`）
  在默认 on 后变冗余 ⇒ ⇒ **同批清理或明文保留**（Zero-Debt 的「No dead code」要求二选一，**不许**沉默）。

---

## 3. 选项对比表

| # | 选项 | 做法 | **结论** | 理由（**真的更差**，不是「不如推荐项」） |
|---|---|---|---|---|
| **①** | **端口面：直接收敛到根组件自有 `__io`**（D1） | 改端口选择规则 + manifest + TB + backend + capability + 护栏期望集，**同批** | ✅ **选中** | **唯一**能让 manifest 与产物**自洽**的形态（事实 ⑧：98 个顶层端口在 manifest 里标 `is_top_io: false`）⇒ ⇒ 让「端口名 == C++ 成员名」在**顶层**真正成立（冒泡端口没有 C++ 成员名可对）⇒ ⇒ 产物顶层**收敛到 ADR-046 D10 冻结的那 32 个** ⇒ ⇒ **是「与既有冻结同向」**；且它是**唯一**能让 backend 的端口面与产物**同源**的形态（事实 ⑭） |
| ② | 端口面：维持现状（残差 id=23 的 2026-11-11 裁决） | 顶层 130 / 68 端口照旧 | ❌ **拒**（**用户 2026-11-16 已推翻**） | **manifest 与产物不自洽**（98 端口错标）⇒ ⇒ 下游按 manifest 建 TB 会**漏连 98 个端口**⇒ ⇒ 冒泡端口名 `unnamed_output_N` **编号随 eval-list 序漂移**⇒ ⇒ 「端口名稳定」不成立⇒ ⇒ 且与 capability 的**成文契约**逐字相反（事实 ⑬）⇒ ⇒ 留着 = 长期持有一个**已知错误**的契约 |
| ③ | 端口面：双档（默认档零变化 + 产品档收敛） | 加一个开关 | ❌ **拒**（用户 2026-11-16 裁决「直接收敛」） | 长期维护**两个交付产物形态** ⇒ ⇒ 护栏要守两套期望集 ⇒ ⇒ 且「默认档」会变成没人用的遗留路径（Zero-Debt 的「No dead code」反噬）⇒ ⇒ **注**：D1 的 ctx-only 路径处置（事实 ⑮ 方案 (a)）**不是**这个选项 —— 那是**非产品**路径，不是第二档**产品** |
| ④ | 端口面：只加 manifest 字段（不收敛产物） | manifest 补 `in_port_table` | 🟡 **退路**（D1 的条件分支） | **能修 manifest 不自洽**（G1 的「契约面」一半）⇒ ⇒ 但**顶层仍有 96 个无语义端口**⇒ ⇒ 「CPU 核 RTL」的交付面**不干净** ⇒ ⇒ 是「半步」；**代价**：backend 端口面与产物**不同源**的问题**仍在**（事实 ⑭ 照旧）⇒ ⇒ 且 capability 的矛盾句**仍在** ⇒ ⇒ **必须同时改那句** |
| ⑤ | ISA 边界：把特权级纳入本轮 | 顺手做 CSR / trap | ❌ **拒** | `AGENTS.md` KNOWN LIMITATIONS **要求独立立项**（不是「禁止」，是「必须走立项」）⇒ ⇒ 且 improvement draft **已存在**（`rv32i-privileged-csr-2026-11` · metis 17 条已过）⇒ ⇒ 塞进来 = 两份权威 + 绕过立项门；且 D3 已证明它与 G1/G3 **零写点交集**⇒ ⇒ **工程收益为零** |
| ⑥ | ISA 边界：只说「不含特权级」，**不**在产品面声明 | 只在文档角落提一句 | ❌ **拒** | 「冻结是终态」是**运行时可观测的终态**⇒ ⇒ 不在 manifest / 产品文档里声明 ⇒ ⇒ 下游会把「功能完整」读成「跑错也能恢复」⇒ ⇒ 这是**误导性宣传**，不是「不够详细」 |
| ⑦ | 覆盖：把锚点钉到 28（全集） | 护栏要求 28 全有 TB | ❌ **拒**（用户 2026-11-16 裁决「维持首批 10」） | 「首批 + 落选登记」已是 E4 的既定形态（排除表逐条带**归位条件**）⇒ ⇒ 改成 28 ⇒ ⇒ 等于把**人工 TB 工作量**（19 个手写 TB）变成**护栏红**⇒ ⇒ 与裁决冲突 |
| ⑧ | 覆盖：维持现状（锚点 10，且不管漂移） | 只修排除表的数字 | 🟡 **不足** | 修数字是**一次性**的 ⇒ ⇒ 下次清单再长，漂移照旧 ⇒ ⇒ **必须**有「每个 tracked target 落在两者之一」的**判据**（D5）⇒ ⇒ 否则事实 ⑩ 的漂移会复发 |
| ⑨ | 卫生：保持 `emit_hygiene` 默认 off | 不动 | ❌ **拒** | 「默认 off」的唯一理由（保字节不变）**已被 D1 消灭** ⇒ ⇒ 继续 off = 产品面**带着 blanket `lint_off`** 出货 ⇒ ⇒ 诉求 ⑦ 兑现不了 |

---

## 4. 后果 / 代价

### 4.1 正面

- ✅ **「功能完整的 RISC-V CPU 核 RTL」从「能力描述」变成「可交付的产品契约」**（D1+D2+D3+D7 合成）。
- ✅ **manifest 与产物自洽**（事实 ⑧ 的缺陷关闭）⇒ ⇒ 下游可**机器消费**产品契约。
- ✅ **顶层端口名 == C++ 成员名 + 前缀**在**顶层**成立 ⇒ ⇒ 诉求 ⑤ 在**产品面**兑现（冒泡端口消失 ⇒ 无反例）。
- ✅ **backend 的端口面与产物同源**（事实 ⑭ 关闭）⇒ ⇒ 「同源 = 同错」的缺陷族少一个来源。
- ✅ **覆盖欠账从「静默」变「可审计」**（D5）⇒ ⇒ E4 明文要防的风险被结构性堵死。

### 4.2 负面（**代价**）

| # | 代价 | 缓解 |
|---|---|---|
| ① | 🔴 **D1 的爆炸半径大**：① backend ② capability delta（**两份 spec × 5 句 + schema 版本**）③ manifest 契约 ④ 10 份 TB ⑤ 6 处护栏/测试期望集 ⑥ tsv/expectations ⑦ 公开面登记 ⑧ 残差 id=23 两侧同批 ⑨ **两份 Accepted ADR 的 addendum** ⑩ **CLI 切带根重载** ⑪ **承重测试载体** ⑫ **用户面/架构文档同步** ⇒ ⇒ **12 项 MUST 同批** | ⇒ ⇒ **MUST 原子落地**（拆批 ⇒ 中间态出现「声称成立但不成立」）⇒ ⇒ 一次性排期，**不**拆 |
| ② | ⚠️ **ctx-only API 的处置 = 49 个调用点**（事实 ⑮）⇒ ⇒ 但**已写死 (a)**（保留旧行为 + 登记为**非产品**遗留路径）⇒ ⇒ 迁移成本**不由本决策承担** | ⇒ ⇒ (a) 的前提 = **产品路径 MUST 切到带根重载**（同批必改 ⑩）⇒ ⇒ 否则 D1 在产品路径上**不生效** |
| ②′ | ⚠️ **两份 Accepted ADR 的 dated addendum = 决策内容变更** ⇒ ⇒ 需**复审**（不是「顺手加一行」） | ⇒ ⇒ 仓内先例 = ADR-060 v1.20 / ADR-062 v1.7（判据收窄时**同步修订 ADR 本体**） |
| ③ | ⚠️ **D1 改的是默认产物** ⇒ ⇒ 任何依赖「顶层 130 / 68 端口」的快照全失效 | ⇒ ⇒ 一次性重算（**禁**写死数字；判据写成「基线 + 新增 − 删除 == 实测」） |
| ④ | ⚠️ **axi4_lite 的 S8 场景整体消失** ⇒ ⇒ `min_checks` 45 → 37 · `scenarios` 8 → 7 | ⇒ ⇒ tsv/expectations **同批**重登记（事实 ⑰）⇒ ⇒ 该场景验证的是「冒泡机制本身」，D1 后该机制**不存在** ⇒ ⇒ 不是判别力损失 |
| ⑤ | ⚠️ **观测盲区从层次化面扩展到默认产物面**（ADR-070 风险③ 的同型） | ⇒ ⇒ **已接受**（根组件自有的 32 个 `obs_*`/`dbg_*` 一个不少 ⇒ ⇒ §5 V4 是承重判据）⇒ ⇒ 但 MUST 在 capability 里**显式**登记该盲区 |
| ⑥ | ⚠️ **19 个 examples 的 TB 是人工工作**（D4 ⇒ 不可自动化）⇒ ⇒ 「examples 全覆盖」**不会**因本次产品化而达成 | ⇒ ⇒ **明写进产品限制**（D2 的四件套按首批 10 计）⇒ ⇒ 排期按人工成本另算 |
| ⑦ | ⚠️ **D3 让产品带一个「功能不完整」的声明** | ⇒ ⇒ 这是**诚实性收益**：「冻结是终态」必须可见 ⇒ ⇒ 特权级独立立项跟进 |
| ⑧ | ⚠️ **D7 把 `default_nettype none` 等带进默认 CLI 产物** ⇒ ⇒ 下游若依赖 blanket `lint_off` 掩盖的宽度告警 ⇒ ⇒ 会**新暴露**告警 | ⇒ ⇒ 这是**暴露真问题**，不是回归 ⇒ ⇒ MUST 在同批把既有 10 个 TB 的 lint 跑一遍确认 |

### 4.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| ① | ⚠️ **D1 的实现面**在扁平路径上需要「谁是根」⇒ ⇒ 事实 ⑯ 证明**产品路径已持有根** ⇒ ⇒ 但 ctx-only 路径（事实 ⑮）**无根可推导** | ⇒ ⇒ §2.1 的**条件式**（①｜V1 失败 ⇒ ④）+ 处置**已写死 (a)** + 同批必改 ⑩（CLI 切带根重载）⇒ ⇒ **不许**硬改公开 API 而不登记 |
| ② | ⚠️ **降顶层端口 ⇒ 可能让某条真实依赖消失** | ⇒ ⇒ **D1 前的顶层 32 个 `dbg_*` / `obs_*` 一个都不能少**（⇒ ⇒ 收敛只砍**冒泡**的）⇒ ⇒ §5 的 **V4** 是承重判据（**负向对照**：删一个 `dbg_*` ⇒ 必红） |
| ③ | ⚠️ **D5 的「登记完备性」判据**可能**恒绿**（若 tracked 集合的解析与 tsv 的解析**同源同错**）⇒ ⇒ 且「**同款但独立实现**」会制造**第二份必然漂移的解析**（本仓已有前例：tsv 列漂移致 10 个发射器失去归属） | ⇒ ⇒ 缓解 = **单一共享解析函数**（`scripts/lib/`，由 `run_all_ported_tests.sh` 与新判据**共同 source**）+ **空解析 fail-closed** + §5 的 **V12** 投毒自证 ⇒ ⇒ **禁**「独立实现第二份解析」 |
| ④ | ⚠️ **D2 的「四件套」判据**可能被读成「每个 example 都要 TB」⇒ ⇒ 与 D5 的「首批 10」冲突 | ⇒ ⇒ D2 是**产品单元**定义 · D5 是**覆盖面**定义 ⇒ ⇒ 两者**层级不同**，不冲突 |
| ⑤ | ⚠️ **D6 是「流程契约」** ⇒ ⇒ 若不挂成**可执行的前置门**，它会退化成一句口号 | ⇒ ⇒ §5 的 **V14** 要求它有**可执行载体**（CI step 或护栏）⇒ ⇒ 无载体 ⇒ ⇒ 本条**降级为「文档约定」**并**如实登记**（不得默认它已生效） |

---

## 5. Verification（**前瞻**验收判据 —— 可勾选 · 未跑不勾）

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出）⇒ ⇒ 「两后端 / 两份同源产物一致」**≠** 正确性证据。
> ⚠️ 每条 MUST 配**变异自证**（故意做错 ⇒ ⇒ 必红）。

### 5.1 D1（端口面 · **承重**）

- [ ] **V1** 🔴 **ctx-only 路径处置的落盘确认（**已写死 (a)** · 非「未知量探针」）**：(a) 的登记**已落**（manifest / 工具文档标注「非产品遗留路径」）+ **(b) 未被选中**已记录理由
      ⇒ ⇒ 且 **产品路径（`rtl_emit` CLI）已切到带根重载** ⇒ ⇒ **否则 D1 在产品路径上不生效**
      ⇒ ⇒ ⚠️ **禁**实施方自行改选 (b)（= 49 处调用点迁移）⇒ ⇒ 改选 = 决策内容变更 ⇒ ⇒ 需复审
- [ ] **V2** 🔴 **端口表判据**：RV32I 扁平产物顶层端口表 **== {default_clock, default_reset} ∪ 根组件自有 32 个 IO**
      ⇒ ⇒ **逐字逐序**比对（数量 = **34**）⇒ ⇒ 且**冒泡端口零出现**（`unnamed_output` / `top_pipeline_*` / `top_itcm_*` / `top_dtcm_*` 在端口表中 **零命中**）
- [ ] **V3** 🔴 **manifest 自洽**：manifest 显式携带**产品端口表** ⇒ ⇒ 「manifest 标为产品端口的集合」**==** 产物端口表
      ⇒ ⇒ 数量相等（**34 == 34**）⇒ ⇒ 且 **MUST 用新增字段**（不靠 `is_top_io` 猜）⇒ ⇒ 且 **schema 版本已升 v2**
- [ ] **V4** 🔴 **观测能力不退化**：根组件自有的 **28 个 `obs_*` / `dbg_*` 输出**在收敛后**全部仍在**顶层端口表内
      ⇒ ⇒ 且 **独立 TB 的判据数不降**（`riscv` TB 仍 `checks=24 fails=0`）⇒ ⇒ **负向对照**：随机删一个 `dbg_*` ⇒ ⇒ 必红
- [ ] **V5** 🔴 **backend 同源**：`verilator_e2e` 全套（**77**）**转绿** ⇒ ⇒ 且 `port_access_snapshot` 与产物端口表**逐项一致**
      ⇒ ⇒ **负向对照**：把 backend 的端口表改回「ctx 全量 `type_output`」⇒ ⇒ 必红（编译期失败）
- [ ] **V6** 🔴 **契约逐句清理（5 句 + 1 schema）**：归档后 ① `rtl-emit-portability` 的「MUST NOT 因 `is_top_io` 改端口面」句 **已改** ② manifest schema **已升 v2** 且 10 字段条款已同步 ③ `rtl-hierarchical-output:36-40` 的「收缩只发生在成层子树」句 **已改** ④ 同 spec `:190-214` 的「悬空子输入冒泡为顶层输入」句 **已改** ⑤ 同 spec `:318-330` 的「层次化顶层 MUST 含全部 inline 后代 `ch_out`」**排他句** **已改**
      ⇒ ⇒ 且 `ADR-058:63-64` + `ADR-061:132` 的 **dated addendum 已落**
      ⇒ ⇒ 🔴 **负向对照**：任一句残留旧措辞 ⇒ ⇒ 必红
- [ ] **V7** **确定性**：两次发射**逐字节**相同（收敛后仍成立）
- [ ] **V8** **lint**：收敛后的产物 `verilator --lint-only` **exit 0** · **零 `%Error`**（⇒ ⇒ 与 D7 联测）
- [ ] **V9** 🔴 **变异自证**：把端口选择规则改回「全部 `ch_out`」⇒ ⇒ **V2 与 V3 必红**（否则 D1 零判别力）

### 5.2 D2 / D3 / D4 / D7

- [ ] **V10** **四件套**：首批 10 个产品单元逐个具备 ①②③④ ⇒ ⇒ 且 ③ 的 TB **独立可跑**（退出码 + `RESULT: checks=<N> fails=0`）
- [ ] **V11** **ISA 边界可见**：产品面（manifest / 文档）**明文**声明「不含特权级 / 冻结是终态」⇒ ⇒ 可 grep
- [ ] **V12** **卫生默认档**：**CLI 默认产物**满足 A 类 6 条 + B 类 3 条 ⇒ ⇒ 且 **`verify_flat_rtl_emit.sh` 的 F1–F6 + F8 逐条**在新靶子上仍绿（**禁**一句「结构不变」带过）
      ⇒ ⇒ 🔴 **变异自证**：去掉 `default_nettype none` ⇒ ⇒ 必红
- [ ] **V13** **库层默认未被误伤**：`EmitOptions{}` / `verilogwriter(ctx)` 的默认**不变** ⇒ ⇒ 且 `verilator_e2e` 的编译形态 + 缓存 key 未因本决策改变

### 5.3 D5 / D6

- [ ] **V14** **登记完备性**：每个 tracked target ∈ tsv ∪ 排除表 ⇒ ⇒ 且排除表计数**从 tracked 集合派生**（禁手写）
      ⇒ ⇒ 🔴 **变异自证**：tracked 集合加一项 ⇒ ⇒ **必红** ⇒ ⇒ 且解析 MUST 为**单一共享函数**（`scripts/lib/`）+ 空解析 fail-closed
- [ ] **V15** **漂移订正**：排除表的「共 N 项」与实测 tracked 数**一致**（现状 19 vs 28 ⇒ ⇒ MUST 改 + 补 9 条理由）
      ⇒ ⇒ **D6 的可执行载体**：「新组件接入流水线 MUST 先过差分测试」有**可执行载体**（CI step 或护栏）⇒ ⇒
      ⇒ ⇒ 无载体 ⇒ ⇒ 本条**降级为文档约定**并**如实登记**（不得默认它已生效）

---

## 6. 落点与后续

| # | 事项 | 归属 | 载体 |
|---|---|---|---|
| ① | **D1 的落地前置**（ctx-only 处置落盘 + **CLI 切带根重载**）⇒ ⇒ 决定 D1 走 ① 还是 ④ | builder | 新 change `rtl-productization-surface-2026-11` 的 T1 |
| ② | **D1 的实施**（**12 项同批**，顺序：① backend → ② capability delta（**两份 spec**）→ ⑨ **ADR-058/061 addendum** → ⑩ **CLI 切带根** → ③ manifest（schema v2）→ ④ TB → ⑤ 护栏/测试期望集 → ⑥ tsv → ⑪ 承重测试载体复核 → ⑫ 文档同步 → ⑦ 公开面登记 → ⑧ 残差 id=23） | builder（⑨ 需 **architect** 复审） | 同上 T2（**原子**） |
| ③ | **D2 / D3 / D4 的产品面落地**（四件套判据 + ISA 边界声明 + TB 骨架的判据汇总段） | planner（产品文档 / 能力面）+ builder（骨架） | 同上 T3 + capability delta |
| ④ | **D5 的登记完备性 + 漂移订正**（护栏新判据 + 排除表补 9 条） | builder | 同上 T4（⇒ ⇒ 与 ② **写点零交集** ⇒ ⇒ 可并行） |
| ⑤ | **D6 的可执行载体** | builder | 同上 T5（⇒ ⇒ 零交集） |
| ⑥ | **D7 的 CLI 默认档切换**（`emit_hygiene` 默认 on；**库层不动**） | builder | 同上 T6（⇒ ⇒ **必须晚于 ②** ⇒ ⇒ 否则两次改产物字节） |
| ⑦ | **特权级**（G2 的兑现） | planner → builder | **独立** change（`rv32i-privileged-csr-2026-11` · **已 draft**）⇒ ⇒ **本产品化不碰** |
| ⑧ | ⚠️ **「examples 全覆盖」的其余 19 项** | 用户 / planner | 独立排期（D4 ⇒ ⇒ **人工工作** ⇒ ⇒ 不在本产品化的完成判据内） |
| ⑨ | ⚠️ **诉求 ⑤ 的字面同名**（route (c)） | **用户** | **残差 id=21 已由用户 2026-11-15 裁决「不要求」** ⇒ ⇒ 本 ADR **不重开** ⇒ ⇒ D1 落地后顶层已是「C++ 成员名 + `top_` 前缀」⇒ ⇒ **诉求 ⑤ 在顶层闭环**（**闭环 = 成员名可推导 + 前缀稳定**，**不是**字面同名） |
| ⑩ | ⚠️ **仓内漂移订正**（事实 ⑱：`AGENTS.md` 的 `illegal_reason` 3 态 vs 实际 4 态） | builder | **独立小项**（**不**塞进本 change）⇒ ⇒ 属**事实订正**，非本 ADR 的义务 |

> ⚠️ **本 ADR 的 Status 在 oracle + critic 全闭前 MUST 保持 `Draft`** ⇒ ⇒ 「把未决问题写成已决」属
> `arch` 档 BLOCKER 第四类 ⇒ ⇒ **未审先 Accepted = 违规**。
> ⚠️ **D1 的条件式写在决策段内**（§2.1）：①｜V1 失败 ⇒ ④ ⇒ ⇒ 若真退到 ④ = **决策内容变更**
> ⇒ ⇒ MUST **重新审稿**（不得就地扩权）。
