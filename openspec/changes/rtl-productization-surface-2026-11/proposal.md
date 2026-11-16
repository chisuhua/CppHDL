# Proposal: RTL 生成能力产品化 —— **顶层端口面收敛** + 产品单元四件套（`rtl-productization-surface-2026-11`）

> **来源**：`.rddf/improvements/rtl-productization-surface-2026-11.md`（planner · 2026-11-16 · metis 1 轮 · 7 P0 + 3 P1 + 2 P2 **全处置**）
> **架构依据**：`docs/adr/ADR-072-rtl-productization-surface-and-boundary.md`（**Accepted** · 2026-11-16 · oracle ×1 + critic ×2 复审 APPROVE）
> ⚠️ **🚪 开工前门（未闭）**：**ADR-072 v2 MUST 先落地** —— metis 实测推翻了 D1 的 **7 处 MUST 修订 + 6 条 MUST 裁决**
> （清单见本 proposal §「🚪 门」）⇒ ⇒ **v2 未落地前本 change MUST NOT 开工、spec delta MUST NOT 写**
> ⇒ ⇒ 理由：**半份 delta 归档 ⇒ 主 spec 冻着与新产品逐字相反的条款 ⇒ 双权威**（= critic 第 1 轮 BLOCKER 的根因）
> ⚠️ **顺序硬约束**：本 change 的 **T1（v2 门）MUST 先于** T2–T13 ⇒ ⇒ 且 **T13（D7 默认档）MUST 晚于 T3（CLI 切带根）**
> ⇒ ⇒ 否则产物字节**改两次** ⇒ ⇒ 且 T14（残差闭口）MUST 晚于 **archive**（闭口证据须指向 archive 目录）

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

---

## What Changes

### A. **ADR-072「12 项同批必改」→ task 映射**（用户 2026-11-16 明确要求）

> ⚠️ **metis 修正后的映射** —— 3 项移出 / 2 项改写 / 1 项拆细 ⇒ ⇒ 12 → **T2–T13（12 个 task）+ T14（archive 后）**。
> ⚠️ **不为 12 项逐行建 task**（metis AI-slop finding）⇒ ⇒ 验收落在 **4 个判据面**（J1–J4）上，簿记类产物作为判据的**附属**。

| ADR-072 项 | 原判据 | **处置** | **task** | 落点判据 |
|---|---|---|---|---|
| ① Verilator backend 端口面 | 「由发射产物端口表派生」 | 🔴 **移出** —— backend **拿不到 root**（`include/core/verilator_backend.h:94-96`）⇒ ⇒ auto-mode **无数据来源** ⇒ ⇒ ① **不可执行** ⇒ ⇒ 改为「backend = 遗留路径」 | **T1**（v2 撤回 ①） | 🚪 门 G-1 |
| ② capability delta（2 份 spec） | 「各一条 MODIFIED」 | ⚠️ **改写** —— 实为 **1 REMOVED+ADDED + 3 MODIFIED + 2 REMOVED+ADDED**（共 6 条 Requirement）⇒ ⇒ 且 **2 条标题会变假** ⇒ ⇒ MODIFIED 改不了标题 | **T7** | J3 / AC6 |
| ③ manifest 契约 | 「三类互斥」 | ⚠️ **改写** —— `ports[]` **仍列全量**（`test_rtl_emit_contract.cpp:278` 的 `top_ports < man.ports.size()`）⇒ ⇒ **新增维度**不收敛旧字段；且「34」= **2 + 32**（clock/reset 的 `is_top_io` 是 `false`） | **T4** | J2 / AC3 |
| ④ 10 份 TB 的端口映射 | 「同批必改」 | ✅ 保留（`axi4_lite` S8 **8** 条判据整体消失 ⇒ `min_checks` 45 → **37** · `scenarios` 8 → **7**） | **T5** | J1 / AC1 / AC5 |
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
| **T2** | **新增带根公开入口**（**只增不改**）：`toVerilog(path, Component&, EmitOptions)` + `make_flat_module_view(Component&, bool)` ⇒ ⇒ ctx-only 入口**保留** = 非产品遗留路径 | 新入口可编译 + ctx-only 49 处调用点**零改动**（`git diff` 只增不改） | T1 |
| **T3** | **CLI 切到带根入口**（`src/rtl_emit/cli.cpp:181`）⇒ ⇒ 产品面 = 带根路径 | 产物端口表 == 产品端口表（逐字逐序）· 计数 = `2 + 根自有 IO 数` | T2 |
| **T4** | **manifest schema v1 → v2**：新增产品端口表维度（`port_table[]` 或 `in_port_table`）⇒ ⇒ `ports[]` **不变**（仍列全量）⇒ ⇒ `is_top_io` **仍只是消费方筛选信号**（不再定义发射器行为） | 「标为产品端口的集合」== 产物端口表（数量相等）· `top_ports < man.ports.size()` 仍成立 | T3 |
| **T5** | **10 份独立 TB 的端口映射** + `axi4_lite` S8 场景删除 + tsv / expectations 重登记（`min_checks` 45 → 37 · `scenarios` 8 → 7） | 10 份 TB 全部可独立跑 + `riscv` 仍 `checks=24 fails=0` + `axi4_lite` `checks=37 fails=0` | T3 · T4 |
| **T6** | **护栏与测试期望集重算**：`flat_rtl_emit_check.py:79-92` 的 F3 **契约算法**改写 · `verify_flat_rtl_emit.sh:119` 的 F3 echo · `verify_hierarchical_rtl.sh` R8 期望集 · `verify_hierarchical_port_naming.sh` 语料 · `tests/test_hierarchy_verilog.cpp` **4 处** · `tests/test_rtl_manifest.cpp` · `tests/test_rtl_tb_stub.cpp` · `tests/test_rtl_emit_contract.cpp` · `tests/test_hierarchy_equivalence.cpp` | 各护栏 `--self-test` 全绿（**含新增的「回到旧算法 ⇒ 必红」投毒**） | T3 · T4 |
| **T7** | **两份 capability 的 spec delta**（2 份 spec · 6 条 Requirement）⇒ ⇒ 🚪 **MUST 等 T1 的 v2 裁决**（`不属于任何组件的节点` / `悬空子输入` / `既有路径逐字节不变` / `全 inline 逐字节一致` / `层次化等价` 五条的改写依赖裁决） | `verify_openspec_delta.sh` 绿 + `openspec validate <change>` 非零错误 + 每条 Requirement 含 `SHALL`/`MUST` + **新契约文本可 grep 到** | T1 · T6 |
| **T8** | **`ADR-058` / `ADR-061` 的 dated addendum**（+ `ADR-058:187-191` 的第二条）⇒ ⇒ **决策内容变更 ⇒ ⇒ 需 architect 复审** | 审稿记录落 `.rddf/reviews/` · addendum 逐条引用本 change | T1 |
| **T9** | **公开面登记**：新增符号（`toVerilog(path, Component&, …)` / `make_flat_module_view(Component&, …)`）⇒ ⇒ `.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md` 一行 ⇒ ⇒ **「新增符号」**（非「行为变更」） | `verify_public_api_visibility.sh` 绿 | T2 |
| **T10** | **顶层端口判定的唯一来源** = manifest 产品端口表 ⇒ ⇒ `tb_stub.cpp:59` / `test_rtl_emit_contract.cpp:142` / `flat_rtl_emit_check.py:79-92` 三处**只消费** | 三处算出的产品端口集 == manifest 产品端口表（**逐项相等**）· 负向对照：任一处回到旧算法 ⇒ ⇒ 必红 | T4 · T6 |
| **T11** | **用户面 / 架构文档同步**：`docs/usage_guide/05-component-design.md:552,680` · `docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md:49-55,88` · `PORT-NAMING-GUIDE.md:78` ⇒ ⇒ 写明**哪条入口适用哪条规则** | 文档中「扁平规则」**一分为二**的表述可 grep 到 | T3 |
| **T12** | **D7：卫生进默认档（仅 CLI 层）** —— `CliOptions::emit_hygiene` 默认 on + 逃生口 `--no-emit-hygiene` ⇒ ⇒ **库层 `EmitOptions{}` / `verilogwriter(ctx)` 不动** ⇒ ⇒ 两处护栏里的显式 `--emit-hygiene` **保留**（护栏**必须**显式打卫生产物） | CLI 默认产物满足 A 类 6 条 + B 类 3 条 · `F1–F6 + F8` 逐条在新靶子上仍绿 · `STATIC_REQUIRE(ch::EmitOptions{}.emit_hygiene == false)` | **T3 之后** |
| **T13** | **产品面声明**：① 四件套判据（首批 10 个产品单元逐个具备 manifest + Verilog + 独立可跑 TB + 覆盖登记）② ISA 边界成文声明（**不含**特权级 / **冻结是终态**）③ 独立 TB 的判据汇总骨架（`RESULT: checks=<N> fails=0` + 失败非零退出码） | 三个声明/骨架**精确串**可 grep 到 + 四件套判据可跑 | T5 · T7 |
| **T14** | **残差 id=23 闭口**（**archive 之后**）—— 台账 §7 + 登记表**两侧同批** ⇒ ⇒ 状态 = `已收口` · 闭口证据 = `openspec/changes/archive/<date>-rtl-productization-surface-2026-11/proposal.md` | `verify_arch_ledger_residuals.sh` PASS（**26 → 27 行**）· 台账状态格由 `--render` 生成 | **archive 之后** |

---

## 验收标准

> ⚠️ **期望值 MUST 手推**（禁抄仿真输出）⇒ ⇒ 「两后端 / 两份同源产物一致」**≠** 正确性证据。
> ⚠️ **禁裸数字**：所有计数 MUST 从产物 / 清单**派生**（产品端口数 = manifest 产品端口表计数 · 覆盖数 = tsv 的 `REGISTERED_TARGETS` · `verilator_e2e` 数 = `ctest -N -L verilator_e2e`）。

- [ ] **AC1** 🔴 产品的端口表 == manifest 产品端口表（**逐字逐序**）⇒ ⇒ 计数 = `2 + 根自有 IO 数`（RV32I = 34）⇒ ⇒ 产物端口表中**零个**后代冒泡端口
- [ ] **AC2** 🔴 **变异自证**：端口选择规则改回「全部 `ch_out`」⇒ ⇒ AC1 必红
- [ ] **AC3** 🔴 manifest 显式携带产品端口表（**新字段**）⇒ ⇒ 「标为产品端口的集合」== 产物端口表 ⇒ ⇒ 且 `ports[]` 仍列全量
- [ ] **AC4** 🔴 **变异自证**：删掉 manifest 的产品端口表维度 ⇒ ⇒ AC1 + AC3 必红
- [ ] **AC5** 🔴 根组件自有的 **28 个 `obs_*` / `dbg_*`** 在收敛后**全部仍在**顶层端口表内 ⇒ ⇒ 且 `riscv` 独立 TB 仍 `checks=24 fails=0` ⇒ ⇒ **负向对照**：删一个 `dbg_*` ⇒ ⇒ 必红
- [ ] **AC6** 🔴 两份主 spec 的受影响 Requirement **逐句** == 新契约 ⇒ ⇒ **新契约文本可 grep 到**（正面判据）⇒ ⇒ **旧措辞零残留**（负向判据）⇒ ⇒ 且 `ADR-058` / `ADR-061` 的 dated addendum 已落
- [ ] **AC7** 🔴 **变异自证**：任一旧措辞残留 ⇒ ⇒ AC6 必红
- [ ] **AC8** 🔴 四件套判据可跑（首批 10 个产品单元逐个齐备）+ ISA 边界声明**精确串**可 grep（「不含特权级」/「冻结是终态」）+ 独立 TB 的判据汇总骨架可跑
- [ ] **AC9** 两次发射**逐字节**相同
- [ ] **AC10** 收敛后的产物 `verilator --lint-only` **exit 0** · **零 `%Error`**
- [ ] **AC11** 🔴 **库层默认未被误伤**：`STATIC_REQUIRE(ch::EmitOptions{}.emit_hygiene == false)` + `git diff` 证明 `sim_main` prelude 与 `emit_sim_main_postlude` 文本未变
- [ ] **AC12** 🔴 CLI 默认产物满足卫生 A 类 6 条 + B 类 3 条 ⇒ ⇒ 且 `F1–F6 + F8` 在新靶子上**逐条**仍绿 ⇒ ⇒ **变异自证**：去掉 `default_nettype none` ⇒ ⇒ 必红
- [ ] **AC13** 🔴 `tb_stub` / `test_rtl_emit_contract` / `flat_rtl_emit_check` 的产品端口集 == manifest 产品端口表（**逐项相等**）⇒ ⇒ **负向对照**：任一处回到旧算法 ⇒ ⇒ 必红
- [ ] **AC14** 🔴 后代悬空 `ch_in` 的产物语义 == v2 裁决形态 ⇒ ⇒ **负向对照**（构造一个后代悬空 `ch_in`）
- [ ] **AC15** 🔴 新增带根入口 = **新增公开符号**（非行为变更）⇒ ⇒ 登记进 `public-api-surface.md` + `docs/CHANGELOG.md` ⇒ ⇒ `verify_public_api_visibility.sh` 绿

---

## Capabilities

| capability | 动作 | 触及的 Requirement（**标题逐字**） |
|---|---|---|
| `rtl-emit-portability` | **REMOVED + ADDED**（标题含 `rtl-emit-manifest-v1` ⇒ ⇒ 标题**必变** ⇒ ⇒ MODIFIED 改不了标题） | 旧：`端口清单（manifest）MUST 遵循 \`rtl-emit-manifest-v1\` schema` ⇒ 新：`端口清单（manifest）MUST 遵循 \`rtl-emit-manifest-v2\` schema` |
| `rtl-hierarchical-output` | **MODIFIED × 3 + REMOVED + ADDED × 2 + MODIFIED × 1** | MODIFIED：`层级粒度 MUST 由显式声明决定（默认 inline）` · `成层且非根的 module 端口 MUST 覆盖自身 \`ch_in\` / \`ch_out\`，且每个端口可追溯` · `顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定`；REMOVED + ADDED：`悬空子输入 MUST 向上冒泡为顶层输入`（标题会变假）· `不属于任何组件的节点 MUST 归顶层 module`（与 root-owned 冲突） |

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
| D5 登记完备性 + D6 可执行载体 | 与 D1 **写点零交集** ⇒ ⇒ 拆开可并行 | `rtl-productization-coverage-registration-2026-11`（**待立项**） |
| `VerilatorBackend` 的端口面改造 | backend **拿不到 root** ⇒ ⇒ auto-mode 无数据来源 ⇒ ⇒ **不可执行** | ADR-072 v2（🚪 门 G-1） |
| ctx-only 49 处调用点的迁移 | 爆炸半径远超 D1 ⇒ ⇒ ctx-only = 非产品遗留路径 | ADR-072 v2 已写死 (a) |

---

## 🚪 门（**开工前 MUST 闭**）

| # | 事项 | 归属 |
|---|---|---|
| **G-0** | **ADR-072 v2 落地**（7 处 MUST 修订 + 6 条 MUST 裁决 ⇒ 见 improvement 草案的 🚪 门表 G-1…G-7）⇒ ⇒ 含**架构裁决**：后代悬空 `ch_in` 的产物语义 · `:98` / `:478` / `:518` / `:529` 四条冲突 | architect |
| **G-1** | **spec delta 写齐**（2 份 spec · 6 条 Requirement）⇒ ⇒ `verify_openspec_delta.sh` 绿 + `openspec validate <change>` 非零错误 | builder（依赖 G-0） |
| **G-2** | **公开面前置登记**（新增符号 + CHANGELOG）⇒ ⇒ **开工前**，不是事后补记 | builder |

> ⚠️ **当前状态（如实登记 · 勿当疏漏）**：`openspec validate rtl-productization-surface-2026-11` **报 ERROR** ——
> 「Change must have at least one delta」⇒ ⇒ 这是**有意为之**：delta 依赖 G-0 的裁决 ⇒ ⇒
> **半份 delta 归档 = 主 spec 冻着与新产品逐字相反的条款 = 双权威**（critic 第 1 轮 BLOCKER 的根因）⇒ ⇒
> ⇒ ⇒ 故**宁可不写、也不写半份** ⇒ ⇒ 该 ERROR 是「delta 未齐」的**正确信号**。
> ⚠️ **CI 不受影响**：`.github/workflows/ci.yml:1223` / `:2139` 只跑 `scripts/verify_openspec_delta.sh`（**当前 4/4 绿**）⇒ ⇒
> 根 `AGENTS.md` 已登记「CI **不**跑 `openspec validate`」（CLI 1.14.x 漂移 ⇒ ⇒ 仓级为红是既有事实 ⇒ ⇒ 与本 change 无关）。
