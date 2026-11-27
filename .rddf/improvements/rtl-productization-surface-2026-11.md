---
name: rtl-productization-surface-2026-11
priority: P1
phase: phase-6
category: framework
type: feature
残余: >-
  **D5 + D6 仍未立项**（其独立 change `rtl-productization-coverage-registration-2026-11` **未归档**）⇒ ⇒ 主 spec delta 形状错 / 后端端口面不可执行两项已闭，但残余未销
状态: in-progress
进度: "**立项（2026-11-16）** · **已实施并归档**（change `rtl-productization-surface-2026-11` ⇒ ⇒ `openspec/changes/archive/2026-11-17-rtl-productization-surface-2026-11/`）· ✅ **开工前门 P-0 已闭**（2026-11-16 · ADR-072 v2 + v2.1 Accepted · `ef0d284` / `e00b321`）⇒ ⇒ **P-1 spec delta 已交付并合并**（G-1…G-8 全部由 ADR-072 v2 交付）· ⚠️ **状态 = `in-progress` 而非 `implemented`**（§0.0 词表）：本项承诺的 **D1 / J1–J4 已落地**（产品端口面 **130 → 34** · manifest schema **v1 → v2** · 两份 capability delta 已合并进 `openspec/specs/`）⇒ ⇒ 但 **D5 / D6**（覆盖登记完备性 + 可执行载体）仍**未立项**（`rtl-productization-coverage-registration-2026-11` **文件不存在**）⇒ ⇒ 「未纳入本轮的部分 MUST 已登记为独立立项」条件**未满足** ⇒ ⇒ **补齐该草案后可改 `implemented`**"
主题: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）—— 顶层端口面收敛 + 产品单元四件套
依赖: rtl-emit-product-tooling-2026-11（implemented）· examples-rtl-sim-coverage-2026-11（implemented）· rv32i-hierarchical-example-2026-11（implemented）· rtl-hierarchical-output capability · rtl-emit-portability capability
来源: 用户 2026-11-16「评估能否生成功能完整的 RISC-V CPU 核 RTL + 推进为 RTL 生成能力产品化」+ **ADR-072**（Accepted 2026-11-16 · oracle ×1 + critic ×2 复审 APPROVE）
生成时间: 2026-11-16
evidence: probe:scripts/verify_flat_rtl_emit.sh
---

# Improvement: RTL 生成能力产品化 —— **顶层端口面收敛** + 产品单元四件套

**主题**: 真层次化 RTL 输出（成层粒度由用户显式声明 · 默认 inline · 与扁平逐字节一致）—— 顶层端口面收敛 + 产品单元四件套

---

## 架构依据

**唯一权威** = `docs/adr/ADR-072-rtl-productization-surface-and-boundary.md`（**Accepted** · 2026-11-16 · oracle ×1 + critic ×2 复审 APPROVE · 审稿记录 `.rddf/reviews/ADR-072-rtl-productization-surface.md`）。

### A.1 现状事实（2026-11-16 **实测**，非推断）

| # | 事实 | 证据 |
|---|---|---|
| ① | **扁平 RTL 可生成**：RV32I SoC 产物 **130 端口**（6 输入 + 124 输出）· **诊断 0 条** · manifest **213 条** | `build/examples/riscv-mini/emit_rtl_rv32i_soc --out <dir>` ⇒ `OK：端口 213 个 · 诊断 0 条`；产物端口表逐行核对 = 130（与 `examples/riscv-mini/tb_rv32i_soc.sv:60` 自述**独立吻合**） |
| ② | **层次化 RTL 可生成**：**8 module**（7 stage + `top`）· 顶层 **68 端口** | 只读探针 `toVerilogHierarchical` ⇒ `IfStage 13 / IdStage 31 / ExStage 21 / MemStage 11 / WbStage 11 / HazardUnit 22 / ForwardingMux_32_ 10 / top 68` |
| ③ | **顶层端口面 = 130 / 68，其中 96 / 34 是后代 `ch_out` 冒泡**（无语义 · `unnamed_output_N` · 编号随 eval-list 序漂移） | 根组件自有 **32** + 框架 **2** + 冒泡 **96** = 130 · 32 + 2 + **34** = 68 |
| ④ | **codegen 没有「顶层面」概念** —— 端口选择 = 「图中**每个** `type_output` 都是顶层端口」 | `src/codegen_verilog.cpp:410-448`（只对 `type_input` 有 driver 者过滤） |
| ⑤ | **manifest 不携带产品的端口表** —— `is_top_io` 语义 = 「**根组件自有** IO」（`entry.is_top_io = (owner == &root)`）⇒ ⇒ 产物里 **98 个**顶层端口在 manifest 中标为 `is_top_io: false` | `src/rtl_emit/manifest.cpp:155-156` · 实测 `is_top_io: true` = **32** vs 产物顶层 **130** |
| ⑥ | **「顶层端口」在三处各自实现，且口径互不相同** | `scripts/lib/flat_rtl_emit_check.py:79-92`（**契约算法**：role∈{clock,reset} ∪ dir==output ∪ (dir==input ∧ !driven)）· `src/rtl_emit/tb_stub.cpp:59`（`p.is_top_io`）· `tests/test_rtl_emit_contract.cpp:142`（`e.is_top_io`） |
| ⑦ | **`VerilatorBackend` 拿不到 root** —— 构造只收 `verilog_path` · 端口表来自 `ctx->get_eval_list()` | `include/core/verilator_backend.h:94-96` · `src/core/verilator_backend.cpp:857-867` ⇒ ⇒ **「backend 端口表由产物端口表派生」在 auto-mode 上无数据来源** |
| ⑧ | **显式入口已存在**：`try_initialize_with_verilog(path, ports, dm)` + `backend_port_set(emitted, root_module)` | `include/core/verilator_backend.h:113,121` · `include/hierarchy_verilog.h:154` ⇒ ⇒ **层次化**产物可被 backend 消费；**扁平**产物无对应入口 |
| ⑨ | **ctx-only 公开 API 无根可推导** —— context 全类**无** Component 注册表 | `include/core/context.h:83-247` · `include/codegen_verilog.h:65,68` ⇒ ⇒ 仓内 **49** 处活跃 ctx-only 调用点（`examples/` 21 · `tests/` 18 · `samples/` 5 · `src/` 5）· ⚠️ **产品路径今天走的正是 ctx-only**：`src/rtl_emit/cli.cpp:181` = `toVerilog(path, ctx.get(), emit_opts)` |
| ⑩ | **无任何以 `Component&` 为根的公开入口** ⇒ ⇒ 「带根重载」是**新增公开 API**，不是「切换调用点」 | `include/codegen_verilog.h:65,68,151,347`（只有 `ctx` 形参） |
| ⑪ | **capability 契约与「端口面收敛」逐字相反** | `rtl-emit-portability/spec.md:61-62`：「发射器 **MUST NOT** 因 `is_top_io` 而改变端口面或端口顺序」· `rtl-hierarchical-output/spec.md:40`（收缩只发生在成层子树）· `:190`+`:201-207`（悬空子输入冒泡为顶层 input + 两份产物**都**暴露）· `:324-325`（排他句：层次化顶层 MUST 含**全部** inline 后代 `ch_out`）· `:518`（全 inline 产物与扁平逐字节一致）· `:98`（owner==nullptr 的节点 MUST 归顶层 module） |
| ⑫ | **Accepted ADR 的决策内容被实质修订** | `ADR-058:63-64` + `ADR-061:132`：「任意 `ch_out` ⇒ 顶层 `output`」= **写死的枚举**，不是活指针 ⇒ ⇒ 需 dated addendum |
| ⑬ | **「examples 全覆盖」真实兑现 9/28，缺口结构性不可见** | `CH_TRACKED_EXAMPLE_TARGETS` 实有 **28** 项 · tsv 数据行 **10**（9 target + 1 `kind=dut`）· 护栏 `verify_examples_rtl_sim.sh:89-107` ⓪ 只比「tsv 行数 vs tsv 自己的锚点」⇒ ⇒ **从不读 tracked 集合** |
| ⑭ | **排除表漂移** | `.rddf/checks/examples-rtl-sim-exclusions.md:23` 写「共 **19** 项 ⇒ 落选 10 项」⇒ ⇒ 实为 28 ⇒ 落选 **19** 项（**9 项两表皆无**） |
| ⑮ | **axi4_lite 的 S8 场景 = 8 条判据**（比较 inline 后代端口 vs 顶层端口） | `examples/axi4/tb_axi4_lite_example.sv:393-401` ⇒ ⇒ 收敛后整体消失 ⇒ ⇒ `min_checks` 45 → **37** · `scenarios` 8 → **7** |

### A.2 评估结论（用户第一问）

> **能力上「能」—— 产物形态上「还不能称为 CPU 核 RTL 产品」。**

- ✅ **「能生成」已由事实 ①②③⑭ 实证**：可综合 Verilog（扁平 + 8 module 层次化）· 可独立运行的 Verilog TB（`checks=24 fails=0` · 手推固件）· ELF 固件装载链路（`mem`/`vh`/`manifest`）· RV32I **用户态** ISA 覆盖 + 非法指令 **4 态** 可观测冻结。
- 🔴 **「产品」尚不成立**：**G1** 顶层端口面未收敛 + 契约不自洽 · **G2** 特权级缺失（「冻结是终态」）· **G3** 覆盖缺口不可见。

---

## 范围

### IN（本 change）

- **D1** 顶层端口面 = **根组件自有 `__io`**（框架 clock/reset + 根自有 IO）⇒ ⇒ 后代 `ch_out` 一律降为内部 `logic` ⇒ ⇒ RV32I 扁平 **130 → 34** / 层次化 **68 → 34**。
- **D1 的配套契约面**：manifest 显式携带产品端口表（schema **v1 → v2**）· 两份 capability 的 delta · 两份 Accepted ADR 的 dated addendum · 10 份手写 TB 的端口映射 · 护栏与测试的期望集重算 · 公开面登记 · 用户面/架构文档同步。
- **D7** `emit_hygiene` **仅 CLI 层**默认 on（库层 `EmitOptions{}` / `verilogwriter(ctx)` 不动）。
- **D2/D3/D4** 的**产品面落地**：产品单元四件套判据 · ISA 边界的成文声明（**不含**特权级 / 冻结是终态）· 独立 TB 的判据汇总骨架。

### OUT（本 change 明确不做）

| 项 | 理由 | 归属 |
|---|---|---|
| **19 个 example 的 TB** | D4 ⇒ 期望值 MUST 手推 ⇒ **不可自动化** ⇒ 是**人工工作** | 独立排期（用户 / planner） |
| **特权级**（CSR / `ECALL` / `EBREAK` / `MRET` / trap / 中断） | 根 `AGENTS.md` KNOWN LIMITATIONS 要求**独立立项** | `rv32i-privileged-csr-2026-11`（**已 draft**） |
| **D5 登记完备性 + D6 可执行载体** | 与 D1 **写点零交集** ⇒ ⇒ 拆成独立 change 可并行 | `rtl-productization-coverage-registration-2026-11`（**待立项**） |
| **残差 id=23 的闭口** | 闭口证据 MUST 指向 `openspec/changes/archive/<date>-<name>/` ⇒ ⇒ **结构上不能与归档同批** | **archive 之后**的收尾提交 |
| **`VerilatorBackend` 的端口面改造** | 事实 ⑦ —— backend **拿不到 root** ⇒ ⇒ auto-mode **无数据来源** ⇒ ⇒ ① 在 ADR-072 里**不可执行** ⇒ ⇒ 移出本 change（**需 v2 裁决**） | ADR-072 v2 |
| **ctx-only 49 处调用点的迁移** | 爆炸半径远超 D1 本身 ⇒ ⇒ ctx-only = **非产品遗留路径**（登记，不迁移） | ADR-072 v2 已写死 (a) |

---

## Why

1. **manifest 与产物不自洽**（事实 ⑤）⇒ ⇒ 下游按 manifest 建 TB 会**漏连 98 个端口** ⇒ ⇒ 「产品契约」当前**不可机器消费**。
2. **顶层端口名不稳定**（事实 ③）⇒ ⇒ 冒泡端口名 `unnamed_output_N` 的编号随 eval-list 序**漂移** ⇒ ⇒ 「端口名 == C++ 成员名」这条诉求在冒泡端口上**根本无法成立**（它们没有 C++ 成员名可对）⇒ ⇒ 诉求 ⑤ **只能在顶层闭环**。
3. **成文契约与目标相反**（事实 ⑪⑫）⇒ ⇒ 归档时若不逐句清理，两份主 spec + 两份 Accepted ADR 会**冻着与新产品逐字相反的条款** ⇒ ⇒ **双权威**。
4. **产品交付物未被定义**（metis 隐藏意图）⇒ ⇒ ADR-072 之前，「产品化」= 一堆功能；本项把它落成**四件套**（manifest + Verilog + 独立可跑 TB + 覆盖登记）⇒ ⇒ 评审才有共同验收对象。
5. **为什么现在做**：诉求 ①③④⑤⑦ 的功能面全部卡在端口面上 ⇒ ⇒ 固件/ELF（②）已归档、层次化示例（④）已归档 ⇒ ⇒ **端口面是最后一道**。

---

## What Changes

### 变更面（4 个判据面 + 若干附属产物）

> ⚠️ **不为 12 项逐行建 task**（metis AI-slop finding）⇒ ⇒ 验收落在**判据面**上，簿记类产物作为判据的**附属**。

| 判据面 | 内容 | 附属产物 |
|---|---|---|
| **J1 产物端口表** | 顶层端口表 == 框架 clock/reset ∪ **根组件自有 `__io`**（逐字逐序）；后代 `ch_out` 降为内部 `logic` | 10 份 TB 的端口映射 · `axi4_lite` S8 删除 · `min_checks` 45→37 |
| **J2 manifest 自洽** | manifest 显式携带**产品端口表**（schema v2）⇒ 「manifest 标为产品端口的集合」== 产物端口表 | `tests/test_rtl_manifest.cpp` · `tests/test_rtl_emit_contract.cpp` |
| **J3 契约文本一致** | 两份 capability 的逐句 delta + 两份 Accepted ADR 的 dated addendum | `verify_flat_rtl_emit.sh` F3 算法 · `verify_hierarchical_rtl.sh` R8 · `verify_hierarchical_port_naming.sh` · `tests/test_hierarchy_verilog.cpp` |
| **J4 产品面声明** | 四件套判据 + ISA 边界声明 + 判据汇总骨架 | `rtl-emit-portability` 的 `is_top_io` 场景改写 · 工具文档 |

### 关键机制决定（**本 change 的技术核心**）

| # | 决定 | 理由（实测） |
|---|---|---|
| **M1** | **新增带根公开入口**（`toVerilog(path, Component&, EmitOptions)` + `make_flat_module_view(Component&, bool)`）⇒ ⇒ **只增不改** ctx-only 入口 | 事实 ⑨⑩ —— ctx-only **无根可推导** ⇒ ⇒ 旧入口**保留**（= 非产品遗留路径）⇒ ⇒ ctx-only 49 处**不迁移** |
| **M2** | **CLI 切到带根入口**（`src/rtl_emit/cli.cpp:181`）⇒ ⇒ **产品面 = 带根路径** | 事实 ⑨ —— 不切 ⇒ D1 在产品路径上**不生效** |
| **M3** | **manifest schema v1 → v2**：新增「产品端口表」维度（`port_table[]` 或 `in_port_table` 字段）⇒ ⇒ `ports[]` **仍列全量**（节点清单） | `tests/test_rtl_emit_contract.cpp:278` 的 `top_ports < man.ports.size()` —— 若 `ports[]` 收敛 ⇒ 该断言恒红 ⇒ ⇒ `ports[]` 语义不变，**新增维度** |
| **M4** | **「顶层端口」判定收敛到唯一来源** = manifest 的产品端口表 ⇒ ⇒ `tb_stub.cpp:59` / `test_rtl_emit_contract.cpp:142` / `flat_rtl_emit_check.py:79-92` 三处**只消费**，不再各自实现 | 事实 ⑥ —— 三处口径互不相同 ⇒ ⇒ D1 后若不改 ⇒ `tb_stub` 会把 124 个 output 全连到 34 端口的 DUT 上 |
| **M5** | **「34」的构成写死** = 框架基础设施 **2**（clock/reset）+ 根自有 IO **32** ⇒ ⇒ manifest 的 `is_top_io` 对 clock/reset 是 `false` ⇒ ⇒ 「产品端口表」**≠** `is_top_io` | 事实 ⑤ —— `is_top_io == true` 只有 32 ⇒ ⇒ V3 的「34 == 34」必须写成「32 + 2」 |
| **M6** | **后代悬空 `ch_in` 的语义 = fail-closed**（发射返回 `ok=false` 点名路径）⇒ ⇒ **不上顶层** | ⚠️ **需 v2 裁决**（ADR-072 未定）—— D1 只说「不上顶层」⇒ ⇒ 产物里它变成什么**未定义** ⇒ ⇒ 三个选项（未驱动 wire / tie-off / 发射失败）语义完全不同 |
| **M7** | **backend 保持 ctx-only 旧语义**（= 遗留路径）⇒ ⇒ **① 从本 change 移出** ⇒ ⇒ 产品产物的验证载体 = **独立 Verilog TB** | 事实 ⑦⑧ —— backend 无 root ⇒ ⇒ auto-mode 无数据来源；层次化产物可用既有 `backend_port_set` 入口，**扁平**产物无对应入口 |
| **M8** | **残差 id=23 的闭口 = archive 之后的收尾**（闭口证据指向 archive 目录） | `.rddf/checks/arch-residual-registry.md:19-20` —— archive 类 MUST 含 `proposal.md` ⇒ ⇒ **结构上不能与归档同批** |

---

## Acceptance

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出）⇒ ⇒ 「两后端 / 两份同源产物一致」**≠** 正确性证据。
> ⚠️ **禁裸数字**：所有计数 MUST 从产物 / 清单**派生**（34 = 产品端口表计数 · 10 = tsv 的 `REGISTERED_TARGETS` · `verilator_e2e` 数 = `ctest -N -L verilator_e2e`）。

- [ ] **AC1** 🔴 **J1**：`manifest` 的产品端口表 == 产物端口表（**逐字逐序**）⇒ ⇒ 计数 = `2 + 根自有 IO 数`（RV32I = 34）⇒ ⇒ 且产物端口表中**零个**后代冒泡端口（`unnamed_output` / `top_pipeline_*` / `top_itcm_*` / `top_dtcm_*` 零命中）
- [ ] **AC2** 🔴 **J1 变异自证**：把端口选择规则改回「全部 `ch_out`」⇒ ⇒ **AC1 必红**
- [ ] **AC3** 🔴 **J2**：manifest 显式携带产品端口表（**新字段**，不靠 `is_top_io` 猜）⇒ ⇒ 「标为产品端口的集合」== 产物端口表（数量相等）⇒ ⇒ 且 `ports[]` **仍列全量**（`top_ports < man.ports.size()` 仍成立）
- [ ] **AC4** 🔴 **J2 变异自证**：删掉 manifest 的产品端口表维度 ⇒ ⇒ **AC1 与 AC3 必红**
- [ ] **AC5** 🔴 **J1 观测不退化**：根组件自有的 **28 个 `obs_*` / `dbg_*`** 在收敛后**全部仍在**顶层端口表内 ⇒ ⇒ 且 `riscv` 独立 TB 仍 `checks=24 fails=0` ⇒ ⇒ **负向对照**：随机删一个 `dbg_*` ⇒ ⇒ 必红
- [ ] **AC6** 🔴 **J3**：归档后两份主 spec 的受影响 Requirement **逐句** == 新契约 ⇒ ⇒ 且**旧措辞零残留**（可 grep）⇒ ⇒ 且 `ADR-058` / `ADR-061` 的 dated addendum 已落 ⇒ ⇒ 🔴 **正面判据**：**新契约文本可 grep 到**（禁只判「旧串不存在」—— 整段删掉也绿）
- [ ] **AC7** 🔴 **J3 变异自证**：任一旧措辞残留 ⇒ ⇒ **AC6 必红**
- [ ] **AC8** 🔴 **J4**：产品面**明文**声明「不含特权级」+「冻结是终态」（**精确串**，可 grep）⇒ ⇒ 且四件套判据可判（首批 10 个产品单元逐个具备 manifest + Verilog + 独立可跑 TB + 覆盖登记）
- [ ] **AC9** **确定性**：两次发射**逐字节**相同（收敛后仍成立）
- [ ] **AC10** **lint**：收敛后的产物 `verilator --lint-only` **exit 0** · **零 `%Error`**（⇒ ⇒ 与 D7 联测）
- [ ] **AC11** 🔴 **D7 库层未被误伤**：`EmitOptions{}` / `verilogwriter(ctx)` 的默认**不变** ⇒ ⇒ **载体** = `STATIC_REQUIRE(ch::EmitOptions{}.emit_hygiene == false)` + `git diff` 证明 `sim_main` prelude 与 `emit_sim_main_postlude` 文本未变
- [ ] **AC12** **D7 默认档**：**CLI 默认产物**满足卫生 A 类 6 条 + B 类 3 条 ⇒ ⇒ 且 `verify_flat_rtl_emit.sh` 的 **F1–F6 + F8** 在新靶子上**逐条**仍绿（**禁**一句「结构不变」带过）⇒ ⇒ **变异自证**：去掉 `default_nettype none` ⇒ ⇒ 必红
- [ ] **AC13** 🔴 **M4 三处口径一致**：`tb_stub` / `test_rtl_emit_contract` / `flat_rtl_emit_check` 的产品端口集 == manifest 产品端口表（**逐项相等**）⇒ ⇒ **负向对照**：让其中一处回到旧算法 ⇒ ⇒ 必红
- [ ] **AC14** 🔴 **M6 后代悬空 `ch_in`**：语义 == v2 裁决的形态 ⇒ ⇒ **负向对照**（构造一个后代悬空 `ch_in` ⇒ ⇒ 按裁决的形态断言）
- [ ] **AC15** 🔴 **公开面登记**：新增带根入口 = **新增公开符号**（**不是**「行为变更」）⇒ ⇒ 登记进 `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` 一行 ⇒ ⇒ `verify_public_api_visibility.sh` 绿
- [ ] **AC16** 🔴 **无归属节点（ADR-072 V16 · D1-g）**：用户直接创建的 `owner == nullptr` 节点 ⇒ **不上**顶层端口表 + **`CHWARN` 已触发** + 发射**不失败** ⇒ ⇒ **负向对照**：删 `CHWARN` ⇒ 必红
- [ ] **AC17** 🔴 **inline 折叠进根的悬空 `ch_in` MUST 被拒（ADR-072 V18）**：`ok == false` + error 点名（端口 + module + 层级）+ `files` 为空 ⇒ ⇒ 既有用例 `tests/test_hierarchy_verilog.cpp:2181-2192`（`REQUIRE(r.ok)`）MUST 重设计为验证拒绝
- [ ] **AC18** 🔴 **冒泡机制零残留（ADR-072 V19 · D1-h）**：`UndrivenPolicy::Bubble` / `*Origin::Bubbled` / `ensure_bubbled_port` / `BubbledInput` 跨 `src/` `include/` `tests/` `scripts/` grep == 0 + 公开面移除登记 + `test_component_granularity.cpp:248-254` 的 `static_assert` 已同步
- [ ] **AC19** 🔴 **A 类零扰动（ADR-072 V5′）**：ctx-only 遗留路径产物**逐字节不变**（= 130 端口）⇒ ⇒ 且 `ctest -L verilator_e2e` 全套仍绿 ⇒ ⇒ **负向对照**：把 backend 端口表改回「带根」⇒ 编译期必红

---

## Capabilities

| capability | 动作 | 触及的 Requirement（逐字标题） |
|---|---|---|
| `rtl-emit-portability` | **REMOVED + ADDED**（标题含 `rtl-emit-manifest-v1` ⇒ ⇒ 标题**必变** ⇒ ⇒ MODIFIED 改不了标题） | 旧：`端口清单（manifest）MUST 遵循 \`rtl-emit-manifest-v1\` schema` ⇒ 新：`端口清单（manifest）MUST 遵循 \`rtl-emit-manifest-v2\` schema，且 MUST 携带产品端口表`（新标题逐字取 ADR-072 §2.8 R-1） |
| `rtl-hierarchical-output` | **MODIFIED × 4 + REMOVED + ADDED × 1**（= **5 条**） | **MODIFIED（4）**：`层级粒度 MUST 由显式声明决定（默认 inline）`（`:32`）· `不属于任何组件的节点 MUST 归顶层 module`（`:98` · 标题中性）· `成层且非根的 module 端口 MUST 覆盖自身 \`ch_in\` / \`ch_out\`，且每个端口可追溯`（`:240`）· `跨层端口策略 MUST 是单一权威的线性枚举，且各取值的失败语义 MUST 明确`（`:402`）<br>**REMOVED + ADDED（1）**：`悬空子输入 MUST 向上冒泡为顶层输入`（`:190` · 标题会变假） |

> ⚠️ **delta 尚未写**（🚪 门 = ADR-072 v2 ⇒ ⇒ 见下方 G-1…G-8）⇒ ⇒ 原因：上表 **6 条** Requirement 的改写**依赖 v2 的裁决** ⇒ ⇒ **本 change 在 delta 写齐前 MUST NOT 归档**（否则主 spec 会冻着与新产品逐字相反的条款 ⇒ ⇒ 正是 critic BLOCKER 的根因）。

> ⚠️ **delta 的唯一权威 = `docs/adr/ADR-072-rtl-productization-surface-and-boundary.md` §2.8（`:355` 定 6 条）** ⇒ ⇒ 本草案**不再自持口径**（原「唯一权威口径 · proposal T7 MUST 与此一致」的表述**已作废** —— 它列了 7 条，含 3 条已被裁决**不需 delta** 的 `:478`/`:518`/`:529`，且**漏 `:402`**）。
> ⇒ ⇒ **6 条** = **4 MODIFIED**（`:32` `:98` `:240` `:402`）+ **2 REMOVED+ADDED**（`rtl-emit-portability` 的 manifest schema · `:190`）
> ⇒ ⇒ **4 条 MUST NOT 进 delta**：`:478` · `:518` · `:529` · `:637`（ADR-072 v2 已逐一裁决）

> ✅ **2026-11-22 · 状态订正（builder 回填）**：上段的「**delta 尚未写**」**已过时** ⇒ ⇒
> delta **已写并合并**（`rtl-emit-portability/spec.md` 携带产品端口表维度 ·
> `rtl-hierarchical-output/spec.md:38-55` 已写「顶层端口面 = 框架 clock/reset + 根组件自有 `__io`；
> 后代 `ch_out` MUST NOT 上顶层」·「悬空子输入冒泡」那条已退役为**恒拒绝**）⇒ ⇒ change 已归档
> （`openspec/changes/archive/2026-11-17-rtl-productization-surface-2026-11/`）⇒ ⇒
> **AC 判定以归档 change 的 proposal + verifier 报告为准**（本文件的 checkbox 是立项期快照 · 未逐条回填）。
> ⚠️ **frontmatter `状态:` 已由 `draft` 改为 `in-progress`**（理由见 `进度:` 字段 · §0.0 词表）。

---

## Impact

| 面 | 影响 |
|---|---|
| **下游集成者** | 顶层端口 **130 → 34** ⇒ ⇒ 按名连线的 TB **必须删 96 根**；manifest schema **v1 → v2** ⇒ ⇒ 解析器**必须按 `schema` 分支**；`emit_hygiene` 默认 on ⇒ ⇒ 产物字节变化 |
| **仓内 10 份独立 TB** | 端口映射全改（`examples/**/*.sv`）· `axi4_lite` 少 8 条判据 |
| **护栏** | `verify_flat_rtl_emit.sh` 的 F3 **契约算法**改写 · `verify_hierarchical_rtl.sh` 的 R8 期望集重算 · `verify_hierarchical_port_naming.sh` 语料重算 |
| **仿真器** | `VerilatorBackend` 保持 ctx-only 旧语义（**M7**）⇒ ⇒ **产品产物（34 端口）与仿真器产物（130 端口）行为等价、端口面不同** ⇒ ⇒ 该差异 MUST 在文档里显式声明 |
| **公开面** | 新增 `toVerilog(path, Component&, …)` + `make_flat_module_view(Component&, …)` ⇒ ⇒ **新增符号**（非行为变更）⇒ ⇒ 走 `public-api-surface.md` + CHANGELOG |
| **零风险** | `Rv32iPipelineTop` 的 `__io` **一字不动**（ADR-046 D10 的 32 端口快照零改动）⇒ ⇒ C6 端口快照零改动 |

---

## metis 第 1 轮 gap 分析与处置（2026-11-16 · planner）

> metis 输出 **7 条 P0 + 3 条 P1 + 2 条 P2**。**P0-1 ② 与 P0-3 推翻了 ADR-072 的两处** ⇒ ⇒ 本草案的 What Changes **已按 metis 修正**，且把 ADR-072 的必要修订登记为 **🚪 门**（见下）。

### P0 处置

| # | finding | 处置 |
|---|---|---|
| **P0-1 ①** | **「带根重载」不存在** ⇒ ⇒ ⑩ 是**新增公开 API**（ADDED），不是「切换调用点」；⑦ 的「行为变更」分类错 | ✅ **已采纳** —— What Changes **M1**（新增入口 · 只增不改）+ AC15（分类改为「新增公开符号」） |
| **P0-1 ②** | **backend 拿不到 root** ⇒ ⇒ ①「backend 端口表由产物端口表派生」在 auto-mode **无数据来源** ⇒ ⇒ ① 与 V5 不能同时成立 | ✅ **已采纳** —— **M7**：backend 保持 ctx-only 旧语义 = 遗留路径 ⇒ ⇒ **① 从本 change 移出** ⇒ ⇒ 后果（产品产物 34 端口 vs 仿真器产物 130 端口 · 行为等价）写进 Impact ⇒ ⇒ ⚠️ **ADR-072 的 ① + V5 MUST 在 v2 撤回**（🚪 门 G-1） |
| **P0-1 ③** | 「34」的构成未写明（clock/reset 的 `is_top_io` 是 `false`）⇒ ⇒ 「三类互斥」与「34==34」矛盾 | ✅ **已采纳** —— **M5**（34 = 2 + 32）+ AC1 |
| **P0-1 ④** | manifest `ports[]` 收敛不收敛 ⇒ ⇒ `test_rtl_emit_contract.cpp:278` 的 `top_ports < man.ports.size()` | ✅ **已采纳** —— **M3**（`ports[]` 不变 · 新增维度）+ AC3 |
| **P0-1 ⑤** | ②「各一条 MODIFIED」与子项矛盾（实为 3 条 Requirement）⇒ ⇒ 且 MODIFIED 改不了标题 | ✅ **已采纳** —— Capabilities 段改为**逐条 Requirement 列举**（1 REMOVED+ADDED + 3 MODIFIED + 2 REMOVED+ADDED） |
| **P0-2 ①** | **后代悬空 `ch_in` 的语义未定义**（D1 的最大洞） | ⚠️ **登记为 🚪 门 G-4**（architect 裁决）⇒ ⇒ 本草案给**推荐形态**（**M6** = fail-closed）+ AC14 |
| **P0-2 ⑤** | 「顶层端口」三处各自实现 ⇒ ⇒ MUST 指定唯一来源 | ✅ **已采纳** —— **M4**（唯一来源 = manifest 产品端口表）+ AC13 |
| **P0-2 ⑥** | **拆 change**（D1 / D5+D6 / D2-D4 / ⑧ 闭口）+ ⑧ **结构上不能与归档同批** | ✅ **已采纳** —— 范围段划清 OUT + **M8**（⑧ 移出）+ D5/D6 登记为独立立项 |
| **P0-3 ①** | **死 AC**：V10（四件套）今天已全绿 ⇒ 零判别力；V11「可 grep」无精确串；V12「逐条复核」不是判据；V13 无载体 | ✅ **已采纳** —— AC8 改为「每个产品单元 ∈ tsv + 四件套齐备」+ AC8 给**精确串**；AC11 给**载体**；AC12 明确「F1–F6 + F8 逐条」 |
| **P0-3 ②** | **缺正面契约 AC**（V6 只判「旧串不存在」⇒ 整段删掉也绿） | ✅ **已采纳** —— AC6 增加**正面**判据（新契约文本可 grep）+ 负向对照 AC7 |
| **P0-3 ③** | **缺后代悬空 `ch_in` 验收** | ✅ **已采纳** —— AC14 |
| **P0-3 ④** | 缺 `:518`（全 inline 逐字节一致）与 `:98`（owner==nullptr 归顶层）的处置 | ⚠️ **登记为 🚪 门 G-5**（这两条**本身**是冲突条款 ⇒ ⇒ 需 architect 裁决） |
| **P0-3 ⑤** | **⑪ 的承重测试清单漏了最大的文件** `tests/test_hierarchy_verilog.cpp` | ✅ **已采纳** —— J3 的附属产物点名该文件（`:2079` 顺序断言 · `:2876-2884` 顶层端口表 · `:2972-2994` 全 inline 逐字节 · `:3154` 顶层端口全覆盖） |
| **P0-3 ⑥** | `VERILATOR_E2E_TOTAL: 77` 与其下一行注记「−1 ⇒ 76」自相矛盾 ⇒ ⇒ 「77」不可作 AC 常量 | ✅ **已采纳** —— Acceptance 段的「禁裸数字」条款（计数一律派生） |
| **P0-3 ⑦** | ⑧ 的目标状态（`已收口` / `已裁决` · 后缀 · 日期 · 闭口证据）从未写 | ✅ **已采纳** —— M8 明确闭口证据指向 archive 目录（**MUST 含 `proposal.md`**）⇒ ⇒ 状态格在 archive 后回填 |
| **P0-3 ⑧** | `.rddf/state/.planner-handoff.json` 停在 2026-10-27 · `proposals_ready` 全部已归档 | ⚠️ **非门 · 实施走**内置流程**（本 change 的实施**不依赖** `rdd-builder` 的 handoff 前置 ⇒ ⇒ 故**不**作为 🚪 门）⇒ ⇒ 刷新 handoff 归**后续独立项**（增强层入口前置 · 与本 change 零写点交集） |

### P1 / P2 处置

| # | finding | 处置 |
|---|---|---|
| P1-8 | V10/V11/V13/V15 四条弱 AC | ✅ 全采纳（见 P0-3 ① 的 AC8/AC11/AC12；V15 拆成 AC14 + 🚪 门 G-4） |
| P1-9 | D7 留了未决二选一（两处显式 `--emit-hygiene` 清理**或**保留） | ✅ **已定死** —— 两处**保留**（护栏**必须**显式打卫生产物 ⇒ ⇒ 删了会让 F8 失去靶子）⇒ ⇒ 逃生口 = `--no-emit-hygiene` |
| P1-10 | 所有计数改为派生 | ✅ 已采纳（Acceptance 段「禁裸数字」条款） |
| P2-11 | D5 的 9 条缺失排除项可点名 | ⚠️ 归 **D5 的独立 change**（本 change 不做）⇒ ⇒ 9 条点名已记入该 change 的立项准备 |
| P2-12 | proposal 的「用户可见变更」必须逐项写出下游拿到什么 | ✅ 已采纳 —— proposal 的 `## 用户可见变更` 段（护栏 R1 的载体） |

### 🚪 开工前门（✅ **已闭 2026-11-16** · ADR-072 **v2 + v2.1** Accepted · `ef0d284` / `e00b321`）

> ⚠️ **ADR-072 的 D1 有 7 处 MUST 修订 + 6 处 MUST 裁决**（metis 实测推翻）⇒ ⇒ **change 的实施 MUST 以 v2 为准** ⇒ ⇒ **v2 未落地前，本 change MUST NOT 开工、delta MUST NOT 写**。

| # | v2 MUST 修订/裁决 | 来源 |
|---|---|---|
| **G-1** | **撤回 ① 与 V5**（backend 端口面改造）⇒ ⇒ 改为「backend = 遗留路径」+ 显式声明产品/仿真器端口面差异 | metis P0-1 ② |
| **G-2** | **⑩ 改写为「新增带根公开入口」** + ⑦ 的公开面分类改为「新增符号」 | metis P0-1 ① |
| **G-3** | **② 的 delta 形状**改为「两份 capability · 逐条 Requirement 列举」⚠️ **v2 已把此条改写**（H-5 同步）：`:518` **不需 delta**（v1 曾要求「补」）· `:98` 走 **MODIFIED**（**不是** R+A）· 另 `:402` **MUST** 进 delta ⇒ ⇒ 权威口径 = ADR-072 `:355` 的 **4 MODIFIED + 2 R+A** | metis P0-1 ⑤ + P0-3 ④ · **v2 改写** |
| **G-4** | **裁决「后代悬空 `ch_in`」的产物语义**（推荐 = fail-closed） | metis P0-2 ① |
| **G-5** | ✅ **已闭** —— **裁决 `:98` 与 `:478` / `:518` / `:529` 四条的冲突**（root-owned vs owner==nullptr 归顶层；「既有路径逐字节不变」vs D1；「全 inline 逐字节一致」vs D1）⇒ ⇒ **v2 裁决**：`:98` = MODIFIED（归**体内** + 保留 `CHWARN` + 不拒绝）· `:478` / `:518` / `:529` / `:637` **不需 delta** | metis P0-2 ① + P0-3 ④ |
| **G-6** | **「34」的构成**写死（2 + 32）· **manifest `ports[]` 不收敛**（新增维度）· **三处顶层判定收敛到唯一来源** | metis P0-1 ③④ + P0-2 ⑤ |
| **G-7** | **① 的 blast radius 收缩后**，重列「同批必改」清单（原 12 项 → 本草案的 4 判据面 + 附属产物） | metis AI-slop finding |
| **G-8** | 🆕 **裁决 `:98`「不属于任何组件的节点 MUST 归顶层 module」在产品端口面下的处置** —— **推荐形态**（对齐 M6 的 fail-closed 取向，但**不同**：`:98` 的节点不是「悬空」而是「无归属」⇒ ⇒ **不**是发射失败）⇒ ⇒ 推荐 = **「归顶层 module 的**体内**（不再是顶层**端口**）+ MUST 保留 `CHWARN` 点名 + MUST NOT 被拒绝」** ⇒ ⇒ 理由：① `:98` 的既有义务是「归属空洞**必须可观测**」⇒ ⇒ `CHWARN` MUST 保留 ② 「归顶层 module」= **归属**，不等于「占顶层端口」⇒ ⇒ 改「端口归属」不违反 `:98` 的意图 ③ 与 M6 的差别 MUST 写进 v2（悬空 = **无驱动源** ⇒ ⇒ fail-closed；无归属 = **有逻辑无归属** ⇒ ⇒ 归体内 + 告警） | critic 第 1 轮 focus 5（对称补推荐形态 ⇒ 省一轮 v2 往返）· 冲突面 = `rtl-hierarchical-output/spec.md:98-111` |

> ⚠️ **编号命名空间**：本表用 **G-1…G-8**（= ADR-072 v2 的修订/裁决清单）⇒ ⇒ **change proposal 的门表用 P-0 / P-1 / P-2**（= 另一套）⇒ ⇒ 两者**不冲突** ⇒ ⇒ 引用时 MUST 带出处。
