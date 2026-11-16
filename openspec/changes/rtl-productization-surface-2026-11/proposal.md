# Proposal: RTL 生成能力产品化 —— **顶层端口面收敛** + 产品单元四件套（`rtl-productization-surface-2026-11`）

> **来源**：`.rddf/improvements/rtl-productization-surface-2026-11.md`（planner · 2026-11-16 · metis 1 轮 · 7 P0 + 3 P1 + 2 P2 **全处置**）
> **架构依据**：`docs/adr/ADR-072-rtl-productization-surface-and-boundary.md`（**Accepted** · 2026-11-16 · oracle ×1 + critic ×2 复审 APPROVE）
> ✅ **🚪 开工前门 P-0 已闭**（2026-11-16 · **ADR-072 v2 + v2.1 Accepted** · `ef0d284` / `e00b321`）⇒ ⇒ **P-1（spec delta）解封**
> ~~历史留痕：P-0 未闭前曾写「v2 MUST 先落地 ⇒ 本 change MUST NOT 开工、delta MUST NOT 写」—— 理由 = 半份 delta 归档会让主 spec 冻着与新产品逐字相反的条款（双权威）；该风险已由 v2 落地消除。~~
> ⚠️ **顺序硬约束**：① **T1（v2 门）已完成** ⇒ T2–T13 / T15 / T16 可直接开工
> ⇒ ⇒ ② **T7（spec delta）MUST 先于 T2–T6**（**契约先行**：delta 是实现的输入，不是产物；`§2.8` 已把改法规格化到「可照抄」粒度）
> ⇒ ⇒ ③ **T12（D7 卫生默认档）MUST 晚于 T3（CLI 切带根）**（否则产物字节改两次）
> ⇒ ⇒ ④ 🔴 **T15（冒泡机制退役）MUST 与 T6（护栏/测试期望集）同批**（同一 commit，或 T15 紧接 T6 且中间态不过 CI —— 否则中间 commit **编译即红**：删枚举后 `test_component_granularity.cpp:248-254` 的 `static_assert` 与 4 个 `[bubble]` 用例同时失效）
> ⇒ ⇒ ⑤ **T14 / T17 MUST 晚于 archive**（闭口证据须指向 archive 目录；Purpose 由 `openspec archive` 之后手工写）

---

## Why

### 缺陷（实测 · 可复算 · 2026-11-16）

1. **manifest 与产物不自洽** —— manifest 的 `is_top_io` 语义是「**根组件自有** IO」（`src/rtl_emit/manifest.cpp:155-156`），
   实测 `is_top_io: true` = **32**；而产物顶层端口 = **130** ⇒ ⇒ **98 个顶层端口在 manifest 里标为 `false`**
   ⇒ ⇒ 下游按 manifest 建 TB **会漏连 98 个端口** ⇒ ⇒ 「产品契约」当前**不可机器消费**。
2. **顶层端口名不稳定** —— 冒泡端口形如 `top_pipeline_unnamed_output_17`，编号随 eval-list 序**漂移**
   ⇒ ⇒ 它们**没有 C++ 成员名可对** ⇒ ⇒ 诉求「端口名与 C++ 侧一致」**只能在顶层闭环**。
3. **codegen 没有「顶层面」概念** —— 端口选择 = 「图中**每个** `type_output` 都是顶层端口」
   （`src/codegen_verilog.cpp:410-448`）⇒ ⇒ 「顶层端口」在三处各自实现且**口径互不相同**：
   `scripts/lib/flat_rtl_emit_check.py:79-92`（契约算法）/ `src/rtl_emit/tb_stub.cpp:59` / `tests/test_rtl_emit_contract.cpp:142`。
4. **成文契约与目标逐字相反** —— `rtl-emit-portability/spec.md:61-62`：「发射器 **MUST NOT** 因 `is_top_io` 而改变端口面」；
   `rtl-hierarchical-output/spec.md:40` / `:190` / `:324-325` / `:518` / `:98` 五处同样相反
   ⇒ ⇒ 不逐句清理 ⇒ ⇒ 归档后**双权威**。
5. **产品交付物未被定义** —— 「产品化」此前是一堆功能，没有共同验收对象。

### 为什么现在做

诉求 ①③④⑤⑦ 的功能面**全部卡在端口面**上：固件/ELF（②）已归档、层次化示例（④）已归档 ⇒ ⇒ **端口面是最后一道**。

### 跨 change 协调（与 `soc-rtl-product-and-tb-2026-11` 的共用面）

> ⚠️ **本节是 B-3 的镜像落点**（对方 proposal 有同名小节）⇒ ⇒ 两侧口径 MUST 一致。
> ADR-073 D10 声称两份 change「写点零交集」⇒ ⇒ **实测不成立**：该表把本 change 的写集写成「`src/**` · `include/**` · 10 份既有 TB · 两份 capability」，
> 但本 change 的同批必改 ④⑤⑥⑪ 与 T5/T6/T11/T13 **还要写**下列 6 个工件。

| # | 共用工件 | 本 change 侧 | `soc-rtl-product-and-tb` 侧 | 执行顺序 |
|---|---|---|---|---|
| ① | `.rddf/checks/examples-rtl-sim-targets.tsv` | T5（`axi4_lite` `min_checks` 45→37 · `scenarios` 8→7） | T6（+`kind=soc` 行 · `REGISTERED_SOC: 1`） | **MUST 串行** —— 后落者 MUST 同一 commit 内重算**两桶**锚点 |
| ② | `scripts/verify_examples_rtl_sim.sh` | T13 / AC8（新增四件套判据） | T6（判据 ⓪ 改**三桶**） | **MUST 串行** ⇒ ⇒ ⚠️ **编号顺延**：现存到 **⑦**（`:14` 的 ④ 已是「TB 能编译」）⇒ ⇒ 新判据是 **⑧**（本文 T13 / AC8 已订正；**ADR-072 D2 / V10 原文的「新增判据④」是编号笔误**，待 architect 侧订正） |
| ③ | `tests/hier_rtl_products.cpp` | 同批必改 ④（`:114-141` · `:476`） | ✅ **不再共用** —— 对方 2026-11-16 裁决「选 (a)」⇒ ⇒ 载体迁到 examples 侧 | — |
| ④ | `docs/usage_guide/05-component-design.md` | T11（`:552` · `:680`） | T11（新增「SoC 装配」小节） | **可并行**（不同小节），MUST 同批提交评审 |
| ⑤ | 🔴 `.rddf/checks/arch-residual-registry.md` + `docs/architecture/2026-11-02-hierarchical-rtl-output.md:§7` | T14（残差 id=23 闭口 · **archive 之后**） | T9（`uart.irq` / `timer_irq` 新增残留） | 🔴 **MUST 串行** —— `verify_arch_ledger_residuals.sh` 对台账 §7 ↔ 登记表做**严格 1:1 双向**校验（状态格须为生成句 + fail-closed）⇒ ⇒ 并行改 ⇒ **必然红** |
| ⑥ | `docs/CHANGELOG.md` | T9（新增公开符号 1 行） | v1.1 已加 1 条 | append-only ⇒ ⇒ 后落者 MUST rebase 后重排 |

🔴 **最硬的跨 change 耦合（= B-4）：`PortProvenanceOrigin::Bubbled` 的退役**

本 change 的 **D1-h** 要**物理删除** `UndrivenPolicy::Bubble` · `HierarchyPortOrigin::Bubbled` · `PortProvenanceOrigin::Bubbled` ·
`ensure_bubbled_port` · `BubbledInput`；而 `soc-rtl-product-and-tb-2026-11` 的 **AC1 / AC3 直接断言 `origin == Bubbled` 计数 == 0**
⇒ ⇒ 后落的一方**编译期就红**：

| 谁先落 | 后落者 MUST 做什么 |
|---|---|
| 对方先 | 本 change 的 **T6 期望集** MUST 同批改 **`examples/axi4/emit_rtl_soc.cpp` 的 `--check-products` 实现**（删 `Bubbled == 0` 子句 —— D1-h 已删除该枚举值；`origin ∈ {Self, ClockReset}` 覆盖同一集合） |
| 本 change 先 | 对方的 AC1 / AC3 MUST 去掉 `Bubbled == 0` 子句（**对方 proposal 已把该子句标为「条件性子句」** ⇒ ⇒ 预先处置已完成） |

⚠️ **ADR-073 D10 的「T0 MUST 先于本 change 合并」经复核比字面弱**（不推翻该决策）：首批 10 里唯一的 SoC 走**扁平发射**
（`examples/riscv-mini/emit_rtl.cpp:31`），AXI SoC 今天**无任何**层次化发射路径（`tests/hier_rtl_products.cpp` / `scripts/**` grep 不到 `axi_soc`）
⇒ ⇒ 本 change 的 D1-f 落地**不立刻打破任何既有护栏**；真实顺序要求是「对方 T0 在其 T2 发射之前」= 对方**内部**自成立。

---

## What Changes

### A. **ADR-072 **v2** 的 12 项同批必改**（**含 ④′ 冒泡机制退役**）**→ task 映射**（用户 2026-11-16 明确要求 · **H-4 同步**）

> ⚠️ **metis 修正后的映射** —— 3 项移出 / 2 项改写 / 1 项拆细 ⇒ ⇒ 12 → **T2–T13（12 个 task）+ T14（archive 后）**。
> 🆕 **v2 增补（H-4）**：④′（冒泡机制退役）⇒ ⇒ **T15**（ADR-072 §2.1.10 的 ④′ 行）· 另加 2 条**簿记** task ⇒ ⇒ **T16**（账本锚点）· **T17**（两处 Purpose 归档后手工写）⇒ ⇒ **T2–T17（16 个 task）**，其中 **T14 / T17 在 archive 之后**。
> 🆕 **P0/P1 审计新增 3 个 task**：**T15**（冒泡机制退役 · ADR-072 D1-h 同批必改 ④′）· **T16**（账本锚点同步）· **T17**（两处 Purpose 归档后手工写）⇒ ⇒ 均**不被** ADR-072 的「12 项」覆盖（D1-h 是 v2 新增的第 12 项细分、锚点与 Purpose 属簿记）。
> ⚠️ **不为 12 项逐行建 task**（metis AI-slop finding）⇒ ⇒ 验收落在 **4 个判据面**（J1–J4）上，簿记类产物作为判据的**附属**。

| ADR-072 项 | 原判据 | **处置** | **task** | 落点判据 |
|---|---|---|---|---|
| ① Verilator backend 端口面 | 「由发射产物端口表派生」 | 🔴 **移出** —— backend **拿不到 root**（`include/core/verilator_backend.h:94-96`）⇒ ⇒ auto-mode **无数据来源** ⇒ ⇒ ① **不可执行** ⇒ ⇒ 改为「backend = 遗留路径」 | **T1**（v2 撤回 ①） | 🚪 门 **improvement G-1** |
| ② capability delta（2 份 spec） | 「各一条 MODIFIED」 | ⚠️ **改写** —— 实为 **2 份 capability · 6 条 Requirement**（**ADR-072 v2 `:355` 的权威口径**）：**4 MODIFIED**（`:32` 层级粒度 · `:98` 无归属节点 · `:240` 成层且非根 · **`:402` 跨层端口策略**）+ **2 REMOVED+ADDED**（`rtl-emit-portability` 的 manifest schema · `:190` 悬空子输入）⇒ ⇒ **另 4 条不需 delta**（`:478` · `:518` · `:529` · `:637`）⇒ ⇒ ⚠️ **改法要点与标题逐字 MUST 照抄 ADR-072 §2.8**（禁自由发挥） | **T7** | J3 / AC6 |
| ③ manifest 契约 | 「三类互斥」 | ⚠️ **改写** —— `ports[]` **仍列全量**（`test_rtl_emit_contract.cpp:278` 的 `top_ports < man.ports.size()`）⇒ ⇒ **新增维度**不收敛旧字段；且「34」= **2 + 32**（clock/reset 的 `is_top_io` 是 `false`） | **T4** | J2 / AC3 |
| ④ 10 份 TB 的端口映射 | 「同批必改」 | ✅ 保留（`axi4_lite` S8 **8** 条判据整体消失 ⇒ `min_checks` 45 → **37** · `scenarios` 8 → **7**；⚠️ **删除面不止「场景」**：`examples/axi4/tb_axi4_lite_example.sv` 另有 **8 条 `logic s_*` 声明**（`:69-76`）与 **8 条实例引脚连接**（`:112-119` 的 `top_slave_unnamed_output*`）MUST 同批删 —— 只删场景 ⇒ `%Error-PINNOTFOUND`；且 `examples/riscv-mini/tb_rv32i_soc.sv` 的 **96 根 sink** MUST 同批删） | **T5** | J1 / AC1 / AC5 |
| ④′ 冒泡机制退役（**v2 新增** · ADR-072 §2.1.10 ④′） | 「（v1 无此项）」 | 🆕 **必改** —— D1-f 让后代悬空 `ch_in` 恒被拒绝 ⇒ ⇒ 冒泡端口**零生产** ⇒ ⇒ `UndrivenPolicy::Bubble` · `*Origin::Bubbled` · `ensure_bubbled_port` · **`BubbledInput`** · `dangling` 消费全部**不可达** ⇒ ⇒ ZERO-DEBT「No dead code」⇒ ⇒ **物理删除** + 公开面「移除 / 改名」登记 + `tests/test_component_granularity.cpp:248-254` 的 `static_assert` 同步（**不改 ⇒ 编译失败**） | **T15**（与 T6 **同批** ⇒ 否则中间态编译红） | AC18 / V19 |
| ⑤ 护栏与测试期望集 | 「4 处」 | ⚠️ **拆细 + 补漏** —— `verify_flat_rtl_emit.sh:119` 的 F3 **契约算法**（`flat_rtl_emit_check.py:79-92`）也要改；且**漏了最大的文件** `tests/test_hierarchy_verilog.cpp`（`:2079` 顺序断言 / `:2876-2884` 顶层端口表 / `:2972-2994` 全 inline 逐字节 / `:3154` 顶层端口全覆盖） | **T6** | J3 / AC6 |
| ⑥ tsv + expectations | 「同批必改」 | ✅ 保留（与 ④ 同源 ⇒ 合并进 **T5** 的第二半） | **T5** | J1 |
| ⑦ 公开面登记 | 「行为变更（非符号移除）」 | ⚠️ **改写分类** —— 新增带根入口 = **新增公开符号**（`toVerilog(path, Component&, …)` / `make_flat_module_view(Component&, …)`）⇒ ⇒ **不是**行为变更 | **T9** | AC15 |
| ⑧ 残差 id=23 闭口 | 「同批必改」 | 🔴 **移出同批** —— 闭口证据 MUST 指向 `openspec/changes/archive/<date>-<name>/`（`.rddf/checks/arch-residual-registry.md:19-20`）⇒ ⇒ **结构上不能与归档同批** | **T14**（archive 后） | M8 |
| ⑨ ADR-058 / ADR-061 dated addendum | 「同批必改」 | ✅ 保留（+ **扩充**：`ADR-058:187-191` 的「既有路径 MUST 逐字节不变」也涉及 ⇒ ⇒ 需第二个 addendum） | **T8** | J3 / AC6 |
| ⑩ CLI 切到带根重载 | 「切换调用点」 | ⚠️ **改写** —— 「带根重载」**不存在**（`include/codegen_verilog.h:65,68,151,347` 全是 `ctx` 形参）⇒ ⇒ ⑩ = **新增入口**（T2）+ **CLI 切换**（T3） | **T2 + T3** | J1 / AC1 |
| ⑪ 承重测试载体 | 「5 个文件」 | ✅ 保留 + **补 `tests/test_hierarchy_verilog.cpp`**（并入 **T6**） | **T6** | J3 |
| ⑫ 用户面 / 架构文档同步 | 「3 个文件」 | ✅ 保留（+ 补：「扁平规则」从此**一分为二** ⇒ ⇒ 文档 MUST 写明**哪条入口适用哪条规则**） | **T11** | J4 |
| **—**（新增 · metis P0-2 ⑤） | — | 🆕 **顶层端口判定的唯一来源**（manifest 产品端口表）⇒ ⇒ `tb_stub` / `test_rtl_emit_contract` / `flat_rtl_emit_check` 三处**只消费** | **T10** | AC13 |
| **—**（新增 · metis P0-2 ①） | — | 🆕 **后代悬空 `ch_in` 的产物语义**（**需 v2 裁决**，推荐 = fail-closed） | **T1**（裁决）+ **T6**（实施） | AC14 |
| **—**（ADR D7） | — | 🆕 **卫生进默认档**（**仅 CLI 层**；库层不动）⇒ ⇒ 逃生口 `--no-emit-hygiene` | **T12** | AC11 / AC12 |
| **—**（ADR D2/D3/D4） | — | 🆕 **产品面声明**：四件套判据 + ISA 边界成文声明 + 独立 TB 的判据汇总骨架 | **T13** | AC8 |

### B. **task 清单**（每项一个判据）

| task | 内容 | 判据（可 grep / 可跑） | 依赖 |
|---|---|---|---|
| **T1** | 🔴 **ADR-072 v2**（🚪 门）—— 7 处修订 + 6 条裁决（清单见 §「🚪 门」） | ADR-072 的 Status 行含 v2 · 审稿记录落 `.rddf/reviews/` | — |
| **T2** | **新增带根公开入口**（**只增不改**）：`toVerilog(path, Component&, EmitOptions)` + `make_flat_module_view(Component&, bool)` ⇒ ⇒ ctx-only 入口**保留** = 非产品遗留路径<br>⇒ ⇒ ⚠️ **「只增不改」的准确边界**：**公开签名 + 49 处调用点**零改动；而 `make_flat_module_view`（`src/codegen_verilog.cpp:410-448`）/ `ModuleView` / `ModuleEmitter` 的**内部改动是允许且必需的**（带根语义必须改端口选择）⇒ ⇒ **MUST NOT** 按「`git diff` 只增不改」理解（那**不可满足**） | ① 新增入口可编译且行为 == 契约 ② `grep -rn "toVerilog(" src/ tests/ samples/ examples/` 的 **49 处调用点签名零改动**（`git diff` 中这些行不出现） | T1 |
| **T3** | **CLI 切到带根入口**（`src/rtl_emit/cli.cpp:181`）⇒ ⇒ 产品面 = 带根路径 | 产物端口表 == 产品端口表（逐字逐序）· 计数 = `2 + 根自有 IO 数` | T2 |
| **T4** | **manifest schema v1 → v2**：新增产品端口表维度（`port_table[]` 或 `in_port_table`）⇒ ⇒ `ports[]` **不变**（仍列全量）⇒ ⇒ `is_top_io` **仍只是消费方筛选信号**（不再定义发射器行为） | 「标为产品端口的集合」== 产物端口表（数量相等）· `top_ports < man.ports.size()` 仍成立 | T3 |
| **T5** | **10 份独立 TB 的端口映射** + `axi4_lite` S8 场景删除 + tsv / expectations 重登记（`min_checks` 45 → 37 · `scenarios` 8 → 7） | 10 份 TB 全部可独立跑 + `riscv` 仍 `checks=24 fails=0` + `axi4_lite` `checks=37 fails=0` | T3 · T4 |
| **T6** | **护栏与测试期望集重算**：`flat_rtl_emit_check.py:79-92` 的 F3 **契约算法**改写 · `verify_flat_rtl_emit.sh:119` 的 F3 echo · `verify_hierarchical_rtl.sh` R8 期望集 · `verify_hierarchical_port_naming.sh` 语料 · `tests/test_hierarchy_verilog.cpp` **4 处 + 5 个冒泡相关用例（共 9 处）**：原 4 处（`:2079` 顺序 · `:2876-2884` 端口表 · `:2972-2994` 全 inline · `:3154` 全覆盖）+ **`:2044` `:2096` `:2162` `:2239` 的 4 个 `[bubble]` 用例** + **`:2181-2192` 防误拒用例**（后 5 个 MUST 重设计为「验证拒绝」—— **与 T15 同批**）<br>⇒ ⇒ ⚠️ **「删除」不可行**：`scripts/verify_hierarchical_rtl.sh:94-97` 要求生成器**恰好产出 3 个 `.v`** ⇒ 删掉冒泡设计 ⇒ 只剩 2 个 ⇒ 护栏全红 ⇒ **MUST 换一个正向设计顶上**，并同批更新 R6 的期望三元组与 R8 的端口表 · `tests/test_rtl_manifest.cpp` · `tests/test_rtl_tb_stub.cpp` · `tests/test_rtl_emit_contract.cpp` · `tests/test_hierarchy_equivalence.cpp`<br>⇒ ⇒ ⚠️ **跨 change（B-4）**：若 `soc-rtl-product-and-tb-2026-11` 先落 ⇒ 本 task MUST 同批改 **`examples/axi4/emit_rtl_soc.cpp` 的 `--check-products` 实现**（删 `origin == Bubbled` 子句 —— D1-h 已删除该枚举值；`origin ∈ {Self, ClockReset}` 覆盖同一集合） | 各护栏 `--self-test` 全绿（**含新增的「回到旧算法 ⇒ 必红」投毒**） | T3 · T4 |
| **T7** | **两份 capability 的 spec delta**（2 份 spec · **6 条 Requirement**，口径 = **ADR-072 v2 `:355`**）⇒ ⇒ **改法要点与新标题逐字 MUST 照抄 ADR-072 §2.8**（**MUST NOT** 自由发挥 —— 否则 = 在实施期决定契约）<br>⇒ ⇒ **4 MODIFIED**：`:32` 层级粒度 · `:98` 无归属节点（标题中性）· `:240` 成层且非根的 module 端口 · **`:402` 跨层端口策略**（删 `UndrivenPolicy` 的 `Bubble` 指称）<br>⇒ ⇒ **2 REMOVED+ADDED**：`rtl-emit-portability` 的 manifest schema（v1→v2，新标题逐字见 §2.8 R-1）· `:190` 悬空子输入（标题变假，新标题逐字见 §2.8 R-4）<br>⇒ ⇒ ⚠️ **4 条 MUST NOT 进 delta**（ADR-072 v2 已裁决「不需 delta」）：`:478` · `:518` · `:529` · `:637`<br>⇒ ⇒ ⚠️ **MODIFIED 是整条替换（含全部 Scenario）** ⇒ `:240` 有 **8 个** Scenario ⇒ MUST 逐条核对（`verify_openspec_delta.sh` 的 R3 **只查标题**，抓不到正文漏改；`:336-344` 末句「未驱动子 input MUST 冒泡」必须改） | `verify_openspec_delta.sh` 绿 + `openspec validate <change>` 非零错误 + 每条 Requirement 含 `SHALL`/`MUST` + **新契约文本可 grep 到** + **4 条不需 delta 的标题零出现在 delta 里**（机械形式：`grep -F <该标题逐字>` 各零命中，**限定在 `### Requirement:` 标题行与 `## MODIFIED/REMOVED/ADDED Requirements` 段内** —— 防 delta 正文交叉引用造成假阳性） | **T1**（**契约先行**：delta 是 T2–T6 的输入，不是产物） |
| **T8** | **`ADR-058` / `ADR-061` 的 dated addendum**（+ `ADR-058:187-191` 的第二条）⇒ ⇒ **决策内容变更 ⇒ ⇒ 需 architect 复审** | 审稿记录落 `.rddf/reviews/` · addendum 逐条引用本 change | T1 |
| **T9** | **公开面登记**：新增符号（`toVerilog(path, Component&, …)` / `make_flat_module_view(Component&, …)`）⇒ ⇒ `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` 一行 ⇒ ⇒ **「新增符号」**（非「行为变更」） | `verify_public_api_visibility.sh` 绿 | T2 |
| **T10** | **顶层端口判定的唯一来源** = manifest 产品端口表 ⇒ ⇒ `tb_stub.cpp:59` / `test_rtl_emit_contract.cpp:142` / `flat_rtl_emit_check.py:79-92` 三处**只消费** | 三处算出的产品端口集 == manifest 产品端口表（**逐项相等**）· 负向对照：任一处回到旧算法 ⇒ ⇒ 必红 | T4 · T6 |
| **T11** | **用户面 / 架构文档同步**：`docs/usage_guide/05-component-design.md:552,680` · `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md:49-55,88` · `PORT-NAMING-GUIDE.md:78` ⇒ ⇒ 写明**哪条入口适用哪条规则** | **精确串**（逐字可 grep）：文档 MUST 出现 `ctx-only 入口：` 与 `带根入口：` 两个小标题，且各自段内写明「顶层端口 = 哪个集合」 | T3 |
| **T12** | **D7：卫生进默认档（仅 CLI 层）** —— `CliOptions::emit_hygiene` 默认 on + 逃生口 `--no-emit-hygiene` ⇒ ⇒ **库层 `EmitOptions{}` / `verilogwriter(ctx)` 不动** ⇒ ⇒ 两处护栏里的显式 `--emit-hygiene` **保留**（护栏**必须**显式打卫生产物） | CLI 默认产物满足 A 类 6 条 + B 类 3 条 · `F1–F6 + F8` 逐条在新靶子上仍绿 · `STATIC_REQUIRE(ch::EmitOptions{}.emit_hygiene == false)` | **T3 之后** |
| **T13** | **产品面声明**：① 四件套判据（首批 10 个产品单元逐个具备 manifest + Verilog + 独立可跑 TB + 覆盖登记）② ISA 边界成文声明（**不含**特权级 / **冻结是终态**）③ 独立 TB 的判据汇总骨架（`RESULT: checks=<N> fails=0` + 失败非零退出码）<br>⇒ ⇒ **四件套判据的可执行载体 = `scripts/verify_examples_rtl_sim.sh` 的新增判据⑧**（逐个产品单元断言：manifest 存在且其**产品端口表** == 该单元 Verilog 产物的端口表（**逐字逐序**）· 产物存在 · 独立 TB 可跑且 `RESULT: checks=<N> fails=0` · tsv 有行）<br>⇒ ⇒ ⚠️ **该判据 MUST 标为「新增」且 MUST 在实施期证明判别力**（**今天判红的原因是「`port_table` 字段缺失」，不是「值不对」** ⇒ ⇒ 三个数字不要混用：`is_top_io:true` = **32** · 今天产物顶层 = **130** · 实施后产物顶层 = **34** ⇒ ⇒ **判别力自证 MUST 是正/负成对**：① 篡改 `port_table` 里**一个端口名** ⇒ 必红 ② 在产物里**插入一条冒泡端口** ⇒ 必红（「删掉某个产品单元的 manifest ⇒ 必红」只证明判据会打开文件，**不证明它区分对错**） | 三个声明/骨架**精确串**可 grep 到 + 护栏新判据⑧ 对存量 10 个单元**今天判红**（差 98）且实施后判绿 | T5 · T7 |
| **T15** | 🔴 **冒泡机制退役（ADR-072 D1-h · ZERO-DEBT 硬项 · 执行于 archive 之前）** —— 物理删除 `UndrivenPolicy::Bubble`（**默认改 `Reject`**）· `HierarchyPortOrigin::Bubbled` · `PortProvenanceOrigin::Bubbled` · `ensure_bubbled_port` · **`BubbledInput`**（`include/hierarchy_analysis.h:121,152`）· `src/hierarchy_verilog.cpp:144,1261` 的 `dangling` 消费 ⇒ ⇒ **公开面「移除 / 改名」登记**<br>⇒ ⇒ 🔴 **`InlineFolded` 一并退役**（ADR-072 §2.8 R-5 改法 ③ 明写「同理」）：D1 下它**不产生端口** ⇒ 站点 = `src/hierarchy_analysis.cpp:431,835,1049` · `src/hierarchy_verilog.cpp:1433-1435` · `include/hierarchy_analysis.h:39` · `include/hierarchy_verilog.h:48`<br>⇒ ⇒ 🔴 **三处断言红线（不同步 ⇒ 编译或测试红）**：`tests/test_hierarchy_verilog.cpp:1562` 的 `static_assert(HierarchyEmitOptions{}.undriven_policy == UndrivenPolicy::Bubble)` · `:2233` 的 `REQUIRE(… InlineFolded == 1)` · `:2249` 的 `REQUIRE(… Bubbled == 1)`（其所在用例 tag 是 **`[provenance]`**，**不是** `[bubble]`）· `tests/test_hierarchy_analysis.cpp:552` 的 `InlineFolded` 分支（`.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` + 探针）<br>⇒ ⇒ ⚠️ **不同步就编译失败**：`tests/test_component_granularity.cpp:248-254` 的 `static_assert(undriven_policy == ch::UndrivenPolicy::Bubble)`（canonical 默认值）⇒ MUST 改为新默认或删除该断言 | `grep -rn -e UndrivenPolicy::Bubble -e Origin::Bubbled -e ensure_bubbled_port -e BubbledInput src/ include/ tests/ scripts/` **零残留** · `verify_public_api_visibility.sh` 绿 · **删后 MUST 全量重建**（禁只重建被点名 target ⇒ 旧二进制假绿） | **T1 · T6（同批）** |
| **T16** | **账本锚点同步**：`AGENTS.md` 的 `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL`（T6 / T15 会增删 TEST_CASE ⇒ `verify_test_counts.sh` 红）· 若 `DUAL_BACKEND_PARITY_*` / `EXAMPLES_TEST_SOURCES` 受影响则一并改 | `verify_test_counts.sh` 绿 · `verify_dual_backend_parity.sh` 绿 · 锚点值**从实测派生**（禁写死）· 可用 `scripts/update_test_count_anchors.sh` | T6 · T15 |
| **T14** | **残差 id=23 闭口**（**archive 之后**）—— 台账 §7 + 登记表**两侧同批** ⇒ ⇒ 状态 = `已收口` · 闭口证据 = `openspec/changes/archive/<date>-rtl-productization-surface-2026-11/proposal.md`<br>⇒ ⇒ ⚠️ **行数由台账派生**（禁写死）：`soc-rtl-product-and-tb-2026-11` 的 T9 也会增行 ⇒ 基线随执行顺序变 | `verify_arch_ledger_residuals.sh` PASS · 台账状态格由 `--render` 生成 | **archive 之后** |
| **T17** | **两处 capability Purpose 归档后手工写**（`openspec archive` **不合并** delta 的 Purpose）：① `rtl-emit-portability/spec.md:4` 现为占位符 `TBD - created by archiving …` ⇒ 写实质 Purpose ② `rtl-hierarchical-output/spec.md:12-13` 写「悬空子输入 MUST 向上**冒泡**为顶层输入」⇒ **与新 R-4 方向相反** ⇒ MUST 改写为「未驱动的**后代**子输入 MUST 被**响亮拒绝**」 | 两处 Purpose 非占位符 · ② 的 Purpose 已改写 · `grep -n "冒泡" openspec/specs/rtl-hierarchical-output/spec.md` 的剩余命中**全部属于历史否定式**（`MUST NOT …冒泡…` 形态） ⇒ ⇒ ⚠️ **判据口径**：**零命中不可满足**（ADR-072 §2.8 R-5 改法 4 的替换句本身含「MUST NOT 冒泡成顶层 input」）⇒ 本条 = **「历史否定式除外」** ⇒ 该口径订正待 architect 落 ADR-072 §6 ⑨ 的 dated 订正 | **archive 之后** |

> ⚠️ **编号 ≠ 执行序**：T15 / T16 是 P0/P1 审计新增（编号顺延），**执行在最前段**（T15 依赖 T2，T16 依赖 T6 + T15）；T14 / T17 是**唯一两个在 archive 之后**执行的 task。

---

## 验收标准

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出）⇒ ⇒ 「两后端 / 两份同源产物一致」**≠** 正确性证据。
> ⚠️ **禁裸数字**：所有计数 MUST 从产物 / 清单**派生**（产品端口数 = manifest 产品端口表计数 · 覆盖数 = tsv 的 `REGISTERED_TARGETS` · `verilator_e2e` 数 = `ctest -N -L verilator_e2e`）。

- [ ] **AC1** 🔴 产品的端口表 == manifest 产品端口表（**逐字逐序**）⇒ ⇒ 集合 == `{default_clock, default_reset} ∪ 根自有 `__io` 名单`**（**手推锚**：`scripts/verify_no_duplicate_rv32i_headers.sh:167` 的 **4 `ch_in` + 28 `ch_out`** 名单 ⇒ 计数 = **34**）⇒ ⇒ 产物端口表中**零个**后代冒泡端口<br>⇒ ⇒ ⚠️ **`2 + 根自有 IO 数` 这个等式是定义式（用了就恒真）** ⇒ 判据 MUST 用**手推名单逐字比对**，不用计数相等
- [ ] **AC2** 🔴 **变异自证**：端口选择规则改回「全部 `ch_out`」⇒ ⇒ AC1 必红
- [ ] **AC3** 🔴 manifest 显式携带产品端口表（**新字段**）⇒ ⇒ 「标为产品端口的集合」== 产物端口表 ⇒ ⇒ 且 `ports[]` 仍列全量
- [ ] **AC4** 🔴 **变异自证**：删掉 manifest 的产品端口表维度 ⇒ ⇒ AC1 + AC3 必红
- [ ] **AC5** 🔴 根组件自有的 **28 个 `obs_*` / `dbg_*`** 在收敛后**全部仍在**顶层端口表内 ⇒ ⇒ 且 `riscv` 独立 TB 仍 `checks=24 fails=0` ⇒ ⇒ **负向对照**：删一个 `dbg_*` ⇒ ⇒ 必红
- [ ] **AC6** 🔴 两份主 spec 的受影响 Requirement **逐句** == 新契约 ⇒ ⇒ **新契约文本可 grep 到**（正面判据）⇒ ⇒ **旧措辞零残留**（负向判据）⇒ ⇒ 且 `ADR-058` / `ADR-061` 的 dated addendum 已落
- [ ] **AC7** 🔴 **变异自证**：任一旧措辞残留 ⇒ ⇒ AC6 必红
- [ ] **AC8** 🔴 四件套判据可跑 —— 载体 = `scripts/verify_examples_rtl_sim.sh` 的**新增判据⑧**（逐个产品单元：manifest 的**产品端口表** == 该单元 Verilog 产物的端口表（**逐字逐序**）· 产物存在 · 独立 TB 可跑且 `RESULT: checks=<N> fails=0` · tsv 有行）⇒ ⇒ ⚠️ **该判据今天恒红**（manifest 标 32 个顶层端口 vs 产物 130 ⇒ 差 98）⇒ ⇒ **判别力自证**：删掉某个产品单元的 manifest ⇒ ⇒ 必红；且 ISA 边界声明**精确串**可 grep（「不含特权级」/「冻结是终态」）+ 独立 TB 的判据汇总骨架可跑
- [ ] **AC9** 两次发射**逐字节**相同 ⇒ ⇒ ⚠️ **这是「确定性」，已由 T6 的护栏 F2 覆盖** ⇒ **不新增判据**（与 AC19「跨构建不扰动」**不是同一件事** —— 两者共用一个 task 组会让失败无法归因）⇒ ⇒ 本 AC 只要求：F2 在**新靶子**（带根入口产物）上仍绿
- [ ] **AC10** 收敛后的产物 `verilator --lint-only` **exit 0** · **零 `%Error`**
- [ ] **AC11** 🔴 **库层默认未被误伤**：`STATIC_REQUIRE(ch::EmitOptions{}.emit_hygiene == false)` + `git diff` 证明 `sim_main` prelude 与 `emit_sim_main_postlude` 文本未变
- [ ] **AC12** 🔴 CLI 默认产物满足卫生 A 类 6 条 + B 类 3 条 ⇒ ⇒ 且 `F1–F6 + F8` 在新靶子上**逐条**仍绿 ⇒ ⇒ **变异自证**：去掉 `default_nettype none` ⇒ ⇒ 必红
- [ ] **AC13** 🔴 `tb_stub` / `test_rtl_emit_contract` / `flat_rtl_emit_check` 的产品端口集 == manifest 产品端口表（**逐项相等**）⇒ ⇒ **负向对照**：任一处回到旧算法 ⇒ ⇒ 必红
- [ ] **AC14** 🔴 后代悬空 `ch_in` 的产物语义 == v2 裁决形态 ⇒ ⇒ **负向对照**（构造一个后代悬空 `ch_in`）
- [ ] **AC15** 🔴 新增带根入口 = **新增公开符号**（非行为变更）⇒ ⇒ 登记进 `public-api-surface.md` + `docs/CHANGELOG.md` ⇒ ⇒ `verify_public_api_visibility.sh` 绿
- [ ] **AC16** 🔴 **无归属节点处置（ADR-072 V16 · D1-g）**：用户**直接创建**的 `owner == nullptr` 节点 ⇒ 三面各一条断言 ① **不上**顶层端口表 ② **`HierarchyEmitResult.warnings` 非空且点名该节点**（⚠️ **MUST NOT 抓 stderr** —— 不可机械化；仓内既有做法 = 断言结构化通道，见 `tests/test_hierarchy_verilog.cpp:1688-1700`，生产者 `src/hierarchy_analysis.cpp:777`）③ 发射**不失败** ⇒ ⇒ **负向对照**：删掉该 warning ⇒ 必红
- [ ] **AC17** 🔴 **「inline 折叠进根」的悬空 `ch_in` MUST 被拒（ADR-072 V18 · D1-f 的边界格）**：构造 `RootUndrivenTop`（inline 子组件有未驱动 `din`）⇒ ⇒ `ok == false` + `error` 点名（端口 + module + 层级）+ `files` 为空 ⇒ ⇒ ⚠️ 该用例**今天断言 `REQUIRE(r.ok)`**（`tests/test_hierarchy_verilog.cpp:2181-2192` + 注释「拒绝它们 = 拒绝合法设计」）⇒ MUST 重设计为验证拒绝
- [ ] **AC18** 🔴 **退役符号零残留（ADR-072 V19 · D1-h）**：`UndrivenPolicy::Bubble` · `HierarchyPortOrigin::Bubbled` · `PortProvenanceOrigin::Bubbled` · **`InlineFolded`（两个 origin 枚举）** · `ensure_bubbled_port` · `BubbledInput` · `dangling` 向量 ⇒ **跨 `src/` `include/` `tests/` `scripts/` `grep -rn -e UndrivenPolicy::Bubble -e Origin::Bubbled -e Origin::InlineFolded -e ensure_bubbled_port -e BubbledInput -e dangling == 0`**（⚠️ **必须含 `dangling` 与 `InlineFolded`**：只查 `Bubbled` 会把 `src/hierarchy_verilog.cpp:144,146-178` 的 `dangling` 与「bubbles up」告警文案原样留下而 grep 全绿）<br>⇒ ⇒ 公开面「移除 / 改名」登记已落 · 四处断言红线已同步（T15 点名）· **删后 MUST 全量重建**
- [ ] **AC20** 🔴 **「可机器消费」的端到端证据**（本 change 初衷：下游只凭 manifest 就能造出东西）：**由 manifest 生成的 `tb_stub` 骨架 + 产品产物一起通过 `verilator --lint-only`**（零 `PINNOTFOUND` / `PINMISSING`）⇒ ⇒ **负向对照**：在 manifest 里删一个端口条目 ⇒ 必红（⚠️ 现状 `tb_stub` 只**打印**端口表 —— `src/rtl_emit/tb_stub.cpp:90-108`，其输出**从未被编译过** ⇒ 「可机器消费」一直只是被断言、从未被演练）
- [ ] **AC19** 🔴 **A 类零扰动（ADR-072 V5′）**：**ctx-only 遗留路径产物逐字节不变**（= 130 端口）⇒ ⇒ **基线 MUST 显式产出**：仓内**没有任何 golden `.v`**（`git ls-files '*.v'` 只有 1 个 fixture：`tests/fixtures/hierarchical_tree_extract/three_layer.v`）⇒ ⇒ 基线 = **改动前从 `HEAD` 构建的二进制**在同一 design 下产出的 `top.v`（`git worktree add` 或先跑一次留存）⇒ ⇒ 判据 = `diff <基线>/top.v <改动后>/top.v` **零差异** ⇒ ⇒ 且 `ctest -L verilator_e2e` 全套仍绿 ⇒ ⇒ **负向对照**：把 backend 端口表改回「带根」⇒ 编译期必红

### AC ↔ task 承载矩阵（**每个 AC MUST 有 task 承载** · 无承载 = 判据落空）

| AC | 承载 task | | AC | 承载 task |
|---|---|---|---|---|
| AC1 端口表 == manifest 产品端口表 | **T3 · T4** | | AC11 库层默认未误伤 | **T12** |
| AC2 变异：规则改回「全部 `ch_out`」⇒ AC1 红 | **T6**（投毒） | | AC12 CLI 默认产物卫生 A/B 类 | **T12** |
| AC3 manifest 携产品端口表 | **T4** | | AC13 三处口径 == manifest | **T10** |
| AC4 变异：删产品端口表维度 ⇒ AC1+AC3 红 | **T4**（投毒） | | AC14 后代悬空 `ch_in` 语义 == v2 裁决 | **T6** |
| AC5 28 个 `obs_*`/`dbg_*` 仍在 + `riscv` TB 24 | **T3 · T5** | | AC15 新增符号登记 | **T9** |
| AC6 两份 spec 逐句 == 新契约 + addendum | **T7 · T8** | | AC16 无归属节点（V16 · D1-g） | **T6** |
| AC7 变异：旧措辞残留 ⇒ AC6 红 | **T7**（负向判据） | | AC17 inline 折叠进根 ⇒ 拒绝（V18） | **T6** |
| AC8 四件套判据⑧ 可跑 + ISA 声明 | **T13** | | AC18 冒泡机制零残留（V19 · D1-h） | **T15** |
| AC9 两次发射逐字节相同 | **T3 · T6** | | AC19 ctx-only 遗留产物零扰动（V5′） | **T15 · T6** |
| AC10 收敛后 lint exit 0 | **T3 · T6** | | | |

---

## Capabilities

| capability | 动作 | 触及的 Requirement（**标题逐字**） |
|---|---|---|
| `rtl-emit-portability` | **REMOVED + ADDED**（标题含 `rtl-emit-manifest-v1` ⇒ ⇒ 标题**必变** ⇒ ⇒ MODIFIED 改不了标题） | 旧：`端口清单（manifest）MUST 遵循 \`rtl-emit-manifest-v1\` schema` ⇒ 新：`端口清单（manifest）MUST 遵循 \`rtl-emit-manifest-v2\` schema，且 MUST 携带产品端口表`（**新标题逐字取 ADR-072 §2.8 R-1**） |
| `rtl-hierarchical-output` | **MODIFIED × 4 + REMOVED + ADDED × 1**（= **5 条**；计数仅作速览，**以逐条列举为准**） | **MODIFIED（4）**：`层级粒度 MUST 由显式声明决定（默认 inline）`（`:32`）· `不属于任何组件的节点 MUST 归顶层 module`（`:98` · 标题**中性** ⇒ 可 MODIFIED）· `成层且非根的 module 端口 MUST 覆盖自身 \`ch_in\` / \`ch_out\`，且每个端口可追溯`（`:240`）· `跨层端口策略 MUST 是单一权威的线性枚举，且各取值的失败语义 MUST 明确`（`:402` · 删 `Bubble` 指称）<br>**REMOVED + ADDED（1）**：`悬空子输入 MUST 向上冒泡为顶层输入`（`:190` · 标题会变假 ⇒ 新标题见 ADR-072 §2.8 R-4） |

> ⚠️ **delta 共 5 条**（上表 4 MODIFIED + 1 R+A）+ `rtl-emit-portability` 的 1 R+A = **6 条** ⇒ ⇒ 与 ADR-072 v2 `:355` 一致。
> ⚠️ **不需 delta 的 4 条**（ADR-072 v2 裁决 · **MUST NOT** 出现在 delta 里）：`层次化 MUST 有独立的公开入口，既有路径 MUST 逐字节不变`（`:478`）· `默认（全 inline）产物 MUST 与扁平产物逐字节一致`（`:518`）· `层次化产物 MUST 与扁平产物等价（可机械判定）`（`:529`）· `顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定`（`:637`）
> ⇒ ⇒ 这 4 条的「既有路径 / 扁平产物 / 等价 / 三类」在**两个入口下都成立**（ADR-072 v2 已逐一裁决）⇒ ⇒ 改它们 = **改不该改的条款**。

> ⚠️ **delta 尚未写**（🚪 门 = T1）⇒ ⇒ 原因：6 条 Requirement 的改写**依赖 v2 的裁决** ⇒ ⇒ **本 change 在 delta 写齐前 MUST NOT 归档**
> ⇒ ⇒ 理由：半份 delta 归档 ⇒ ⇒ 主 spec 冻着与新产品逐字相反的条款 ⇒ ⇒ **双权威**（= critic 第 1 轮 BLOCKER 的根因）。

---

## 用户可见变更

> ⚠️ **护栏 `verify_public_api_visibility.sh` 的 R1 载体**（归档时 MUST 非空）。

1. **顶层端口面收缩**：产品的顶层端口 **130 → 34**（RV32I）⇒ ⇒ **按名连线的下游 TB 必须删 96 根连线** ⇒ ⇒ 层级化的顶层端口 **68 → 34**。
2. **manifest schema v1 → v2** ⇒ ⇒ 解析器**必须按 `schema` 字段分支**（v1 解析器读到 v2 清单 ⇒ ⇒ **不得**假定字段集）。
3. **`is_top_io` 的语义不变**（仍是「根组件自有 IO」）⇒ ⇒ 但**新增**「产品端口表」维度 ⇒ ⇒ 消费方**必须**改用它判定顶层端口（旧的 `is_top_io` 判据**不再**等于顶层端口集）。
4. **新增公开入口**：`toVerilog(path, Component&, EmitOptions)` + `make_flat_module_view(Component&, bool)` ⇒ ⇒ **新增符号**，非行为变更。
5. **`emit_hygiene` 默认 on（仅 CLI）** ⇒ ⇒ CLI 默认产物字节变化（`default_nettype none` + 无 blanket `lint_off` + 剪枝）⇒ ⇒ 逃生口 = `--no-emit-hygiene`；**库层默认不变**。
6. **产品产物与仿真器产物的端口面不同**（34 vs 130）⇒ ⇒ **行为等价**（差别只在顶层端口面）⇒ ⇒ 用 `VerilatorBackend` 仿真的是**仿真器产物**，不是**产品产物** ⇒ ⇒ 产品产物的验证载体 = **独立 Verilog TB**。

---

## Out of Scope

| 项 | 理由 | 归属 |
|---|---|---|
| 19 个 example 的独立 TB | 期望值 MUST 手推 ⇒ **不可自动化** ⇒ 人工工作 | 独立排期 |
| 特权级（CSR / `ECALL` / `EBREAK` / `MRET` / trap / 中断） | 根 `AGENTS.md` KNOWN LIMITATIONS 要求独立立项 | `rv32i-privileged-csr-2026-11`（**已 draft**） |
| D5 登记完备性 + D6 可执行载体 | ⚠️ **「写点零交集」不成立**（实测）：它要改 `.rddf/checks/examples-rtl-sim-targets.tsv` · `.rddf/checks/examples-rtl-sim-exclusions.md` · `scripts/verify_examples_rtl_sim.sh` / `verify_example_test_wiring.sh` ⇒ ⇒ 与本 change 的 T5/T13 及 `soc-rtl-product-and-tb` 的 T6/T8 **三线共用** ⇒ ⇒ **拆开仍成立**（范围确实不同），但 **MUST 串行**（见 §「跨 change 协调」⑤⑥ 同型的 1:1 护栏风险） | `rtl-productization-coverage-registration-2026-11`（**待立项**） |
| `VerilatorBackend` 的端口面改造 | backend **拿不到 root** ⇒ ⇒ auto-mode 无数据来源 ⇒ ⇒ **不可执行** | ADR-072 v2（🚪 门 G-1） |
| ctx-only 49 处调用点的迁移 | 爆炸半径远超 D1 ⇒ ⇒ ctx-only = 非产品遗留路径 | ADR-072 v2 已写死 (a) |

---

## 🚪 门（**开工前 MUST 闭**）

> ⚠️ **编号命名空间**：本表用 **P-0 / P-1 / P-2**（= proposal 的门）⇒ ⇒ **improvement 草案的 G-1…G-7** 是**另一套**（= ADR-072 v2 的修订/裁决清单）⇒ ⇒ 两者**不冲突** ⇒ ⇒ 引用时 MUST 带出处（「improvement G-n」/「proposal P-n」）。

| # | 事项 | 归属 |
|---|---|---|
| **P-0** | ✅ **已闭（2026-11-16）** —— **ADR-072 v2 + v2.1 Accepted**（`ef0d284` / `e00b321`）⇒ ⇒ 认领内容 = 7 处 MUST 修订 + 6 条 MUST 裁决（见 improvement 草案的 🚪 门表 **G-1…G-8**）+ 架构裁决（后代悬空 `ch_in` 的产物语义 · `:98`/`:478`/`:518`/`:529` 四条冲突） | ✅ architect 已交付 |
| **P-1** | ✅ **已解封（2026-11-16 · P-0 已闭）** —— **spec delta 写齐**（2 份 spec · **6 条 Requirement**）⇒ ⇒ `verify_openspec_delta.sh` 绿 + `openspec validate <change>` 非零错误 | builder |
| **P-2** | **公开面前置登记**（新增符号 + CHANGELOG）⇒ ⇒ **开工前**，不是事后补记 | builder |
| **P-3** | **ADR-058 / ADR-061 的 dated addendum 已落**（architect 域 · 对应 **T8 / AC6**）⇒ ⇒ 归档门 MUST 可追踪该外部依赖 | architect |

> ⚠️ **当前状态（如实登记 · 勿当疏漏）**：`openspec validate rtl-productization-surface-2026-11` **报 ERROR** ——
> 「Change must have at least one delta」⇒ ⇒ 这是**有意为之**：delta 依赖 G-0 的裁决 ⇒ ⇒
> **半份 delta 归档 = 主 spec 冻着与新产品逐字相反的条款 = 双权威**（critic 第 1 轮 BLOCKER 的根因）⇒ ⇒
> ⇒ ⇒ 故**宁可不写、也不写半份** ⇒ ⇒ 该 ERROR 是「delta 未齐」的**正确信号**。
> ⚠️ **CI 不受影响**：`.github/workflows/ci.yml:1223` / `:2139` 只跑 `scripts/verify_openspec_delta.sh`（**当前 4/4 绿**）⇒ ⇒
> 根 `AGENTS.md` 已登记「CI **不**跑 `openspec validate`」（CLI 1.14.x 漂移 ⇒ ⇒ 仓级为红是既有事实 ⇒ ⇒ 与本 change 无关）。
