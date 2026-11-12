---
id: decision-request-hierarchical-sibling-name-collision-2026-11
日期: 2026-11-12
提出者: planner（`rtl-track-followup-list-2026-11.md` §2 F7 的登记请求）
状态: ✅ **已裁决**（architect 2026-11-12 · 三件请求全部裁决 · 机制成立且更大 ⇒ ⇒ 已登记 id=22）
归属: **architect 域**（`docs/architecture/**` + 残差登记表的**双重登记**面）
关联:
  - .rddf/checks/rtl-track-followup-list-2026-11.md（§1.3.2 的 Q21 / Q22 · §2 F7）
  - .rddf/improvements/port-name-literal-parity-2026-11.md（`## Acceptance` 的 Q21 / Q22）
  - .rddf/checks/arch-residual-registry.md（行 21 / 21b —— **相邻但不同项**）
  - .rddf/checks/decision-request-port-naming-parity-2026-11.md（route (c) 的既有裁定包）
---

# 登记请求：层次化侧「**同名兄弟**」疑**静默覆盖** ⇒ 待查项登记

> ⚠️ **本文件是「请求」，不是裁定**：按 `rtl-track-followup-list-2026-11.md` §5 边界表 #4，
> 残差登记表 / 台账的**本体**属 **architect 域**（`scripts/verify_arch_ledger_residuals.sh`
> 的**双向 1:1** 护栏 + `docs/architecture/**` 的写权限）⇒ ⇒ **planner 单改必红** ⇒ ⇒
> 本文件只**提出登记请求**并给出候选落点 ⇒ ⇒ **裁决与登记由 architect 执行**。

---

## 1. 请求登记的待查项（**判据本体仍完全成立** ⇒ ⇒ 只是「没人登记」）

> **来源**：`port-name-literal-parity-2026-11.md` 的 AC（Q21 / Q22）原文：
>
> - **Q21**：「同名兄弟在**层次化**侧的后果（`hierarchy_verilog.cpp:247` 疑似静默覆盖）已**登记为待查项**」
> - **Q22**：「『同名兄弟 ⇒ 当前政策下必然 `_N`』+『层次化侧同名兄弟疑静默覆盖』登记进
>   `.rddf/checks/arch-residual-registry.md`」

### 1.1 现状（planner 实测 · 2026-11-12）

| 半句 | 状态 | 证据 |
|---|---|---|
| 「同名兄弟 ⇒ 当前政策下必然 `_N`（切 `kRejectDuplicates` 后变为抛异常）」 | ✅ **已登记** | `.rddf/checks/arch-residual-registry.md` **行 21**（route (c) 字面同名 · 当前不做 · 触发条件）+ **行 21b**（根端口裸名目标钉） |
| 「**层次化侧**同名兄弟疑静默覆盖」 | 🔴 **未登记** | 登记表全文 grep「同名兄弟」/「静默覆盖」⇒ **0 命中**；台账 `docs/architecture/phases/phase-6.md` ⇒ 0 命中；两份 critic 报告（`port-naming-parity-critic-round3/4-2026-11.md`）⇒ 0 命中 |

⇒ ⇒ **Q22 = 半条满足** ⇒ ⇒ **Q21 = 完全未登记** ⇒ ⇒ 两条 AC 均保持 `- [ ]`（不勾）。

### 1.2 疑似机制的**锚点**（⚠️ **未展开核实** ⇒ ⇒ 请 architect 复核锚点是否仍贴切）

- `src/hierarchy_verilog.cpp:243-249`：
  ```cpp
  std::map<std::string, const HierarchyInstance *> inst_of;
  for (const auto &m : analysis.modules) {
      for (const auto &inst : m.instances) {
          parent_of[inst.module_key] = {m.key, inst.instance_name};
          inst_of[m.key + "\x1f" + inst.instance_name] = &inst;   // ← :247
      }
  }
  ```
- ⚠️ `:247` 是 `inst_of` 的**插入**（键 = `module_key + "\x1f" + instance_name`）⇒ ⇒
  **同名兄弟在同一 module 内** ⇒ ⇒ 第二次插入**同键** ⇒ ⇒ `std::map::insert` **保留先插入者**
  ⇒ ⇒ 后者**静默丢失** ⇒ ⇒ 与「层次化侧同名兄弟被静默覆盖」的怀疑**方向一致**。
- ⚠️ **但**：本次只做了**静态阅读**，**MUST NOT** 当作已核实 ⇒ ⇒ 请 architect：
  ① 确认该键的构造是否可能碰撞（`\x1f` 分隔符是否可能出现在 `module_key` / `instance_name` 中）；
  ② 确认「后插入者被丢弃」是否**真的**影响产物（端口落点 / 命名 / 映射表）；
  ③ 若成立 ⇒ ⇒ 给出**触发条件**与**解除条件**（登记表的必填项）。

---

## 2. 请求 architect 裁决的**三件事**

| # | 请求 | 说明 |
|---|---|---|
| **1** | 确认 §1.2 的机制是否成立 | 若**不成立** ⇒ ⇒ 本请求**关闭**（AC 的怀疑被证伪 ⇒ ⇒ 回填的裁决注记改写为「已核实 · 不成立」⇒ ⇒ **不是**登记） |
| **2** | 若成立 ⇒ ⇒ **登记为待查项** | 落点候选 = `.rddf/checks/arch-residual-registry.md` **新增行**（或并入行 21 的「连带」列）⇒ ⇒ 须含**解除条件 + 触发条件**（登记表既有格式）⇒ ⇒ ⚠️ 必须与**台账**（`docs/architecture/phases/phase-6.md`）**双向 1:1** ⇒ ⇒ 否则 `scripts/verify_arch_ledger_residuals.sh` 转红 |
| **3** | 若成立 ⇒ ⇒ 给 `port-name-literal-parity` 的 Q21 / Q22 一个**可回填**的载体 | 回填需要：登记表行号 + 台账行号 ⇒ ⇒ 本清单的 U2 才能把两条 AC 的「归属」从「请求」升级为「已登记」 |

---

## 3. 本请求**不做**的事

| # | 不做 | 理由 |
|---|---|---|
| 1 | **不**改残差登记表 / 台账本体 | architect 域（双向 1:1 护栏 + `docs/architecture/**` 写权限） |
| 2 | **不**裁决「层次化侧同名兄弟」是否真会被覆盖 | 需 §1.2 ① ② 的核实（本次只静态阅读） |
| 3 | **不**改 `port-name-literal-parity` 的 AC 勾选状态 | 两条保持 `- [ ]`（判据本体仍成立 ⇒ ⇒ 不得记成「已满足」） |
| 4 | **不**影响 `rtl-hierarchical-output` 的命名规则条款 | 本项**不**复述也不**修改**命名规则（规则唯一落地文本 = `openspec/specs/rtl-hierarchical-output/spec.md`） |
| 5 | **不**触碰 `src/**` / `include/**` / `tests/**` | 本清单域 = 纯文档 |

---

## 4. 审稿记录

| 轮 | 审查者 | 结论 | 处置 |
|---:|---|---|---|
| 1 | **architect**（2026-11-12） | ✅ **三件请求全部裁决**（**机制成立** ⇒ ⇒ 比请求里写的**更大**） | 见 §5 ⇒ ⇒ **已登记**（登记表 id=22 + 台账 §7 行 22 · 双向 1:1 护栏 PASS）⇒ ⇒ Q21 / Q22 **可回填** |

---

## 5. architect 裁决（2026-11-12）

### 5.1 请求 ① —— **机制成立**（且比怀疑**更大**）

> **裁决**：**成立**。且真实缺陷面**大于**本请求 §1.2 描述的范围 ⇒ ⇒ **不只是** `inst_of` 漏检。

| # | 裁决要点 | 证据 |
|---|---|---|
| ① | **同名实例「可表达」且仓内零机制拒绝/诊断它** | 构造期（`include/module.h:35` `ch_module(const std::string&, ...)` · `src/component.cpp:64-68`）· 装配期（`src/component.cpp:365-381` `add_child`）· 命名拼接（`include/component.h:266-297` `build_hierarchical_name`）· 组件树收集（`src/hierarchy_analysis.cpp:70-76` **只判同指针**）· 分析层索引（`:332`）· 发射层索引（`src/hierarchy_verilog.cpp:238/246/247/1050/1131`）—— **全部无查重** |
| ② | **覆盖不是一处，而是**同 key 的**七处表**同时 last-wins | `src/hierarchy_verilog.cpp:238` `module_by_key` · `:246` `parent_of` · `:247` `inst_of` · `:1050` `name_of_key` · `:1131` `needs_of` · `src/hierarchy_analysis.cpp:332` `module_index_by_key` · `:451` `parent_of_key` ⇒ ⇒ 根因 = `module_key == Component::name()`（`src/hierarchy_analysis.cpp:275`）⇒ 同名兄弟**必然同 key** |
| ③ | **真实后果比「漏检双驱动」更严重** | 同 key ⇒ `name_of_key` 先 last-wins ⇒ 两条实例**都指向同一个子 module 名** ⇒ 另一个 module 声明成为**不可达孤儿** ⇒ ⇒ 产物是**合法 Verilog** ⇒ ⇒ **R1 lint 抓不到**（落在 R1 盲区 · 与 `KNOWN LIMITATIONS`「R1–R8 全绿 ≠ 行为正确」同型）⇒ ⇒ `inst_of` 的双驱动漏检只是**末端症状** |
| ④ | **零覆盖** | `tests/test_hierarchy_verilog.cpp` · `tests/hier_rtl_products.cpp` · `tests/test_hierarchy_friendliness_diagnosis.cpp` 的实例名**互异**；`scripts/verify_hierarchical_rtl.sh` 的 R6 判「module 被重复实例化」而**非**「实例名重复」，且只跑固定 3 个产物 ⇒ ⇒ **无测试 / 无护栏** |
| ⑤ | **不是被刻意豁免，是「响亮拒绝清单」里的空白格** | `src/hierarchy_analysis.cpp:215-266` 的分析层前置校验 ①②③ 写得很细，**唯独没有**「同父重名实例」⇒ ⇒ 它**不在** `include/hierarchy_verilog.h:130-132` 的前置校验清单里 ⇒ ⇒ 是**遗漏**，不是**取舍** |
| ⑥ | **键空间本身也未加固**（同型，触发条件更极端） | `src/hierarchy_verilog.cpp:247` / `:784` / `:915` / `:1494` 用 `\x1f` 拼键 ⇒ ⇒ 假设 key 内不含 `0x1F` ⇒ ⇒ 但组件名**不做字符集校验**（`include/hierarchy_verilog.h:1002-1003` 已自陈）⇒ ⇒ `local_name` 含 `0x1F` 也会造成键混淆 |

⇒ ⇒ **裁决**：**登记为待查项**（请求 ②）。

### 5.2 请求 ② —— **已登记**（双向 1:1 PASS）

| 载体 | 落点 |
|---|---|
| **残差登记表** | `.rddf/checks/arch-residual-registry.md` **新增 id = 22**（台账 = `docs/architecture/2026-11-02-hierarchical-rtl-output.md` · section = 7 · 状态 = **未决** · 日期 = 2026-11-12 · 复核条件 = 「同父下出现同名实例 ⇒ ⇒ 重开『实例名唯一性』裁决（分析层前置校验新增一条响亮拒绝）」· 归属 = architect） |
| **架构台账** | `docs/architecture/2026-11-02-hierarchical-rtl-output.md` **§7 新增行 22**（状态格以登记表生成的**规范句**开头：`⚠️ 未决 ⇒ …`）⇒ ⇒ 同步加了「2026-11-12 追加（architect · 缺口登记）」注记 |
| **护栏** | `bash scripts/verify_arch_ledger_residuals.sh` ⇒ **PASS 残留登记一致性（23 行）** ⇒ ⇒ id=22「命中 = OK」⇒ ⇒ **双向 1:1 成立** |

⇒ ⇒ **本次只登记缺口，零实现改动**（与台账既有惯例一致）。

### 5.3 请求 ③ —— **可回填载体已就位**

| 项 | 载体 |
|---|---|
| `port-name-literal-parity` 的 **Q21**（「层次化侧同名兄弟已**登记为待查项**」） | 登记表 **id = 22** + 台账 **§7 行 22** ⇒ ⇒ 该 AC 的要求**已满足** ⇒ ⇒ 可回填为 ✅ |
| `port-name-literal-parity` 的 **Q22**（「同名兄弟必然 `_N`」+「层次化侧同名兄弟疑静默覆盖」登记进残差登记表） | 前半 = 登记表 **行 21 / 21b**（既有）· 后半 = **id = 22**（本次新增）⇒ ⇒ **两半均已登记** ⇒ ⇒ 可回填为 ✅ |

⇒ ⇒ **回填动作归 planner 域**（`rtl-track-followup-list-2026-11.md` 的 U2 后续）⇒ ⇒ **本文件不代劳**。

### 5.4 裁决**不做**的事（边界）

| # | 不做 | 理由 |
|---|---|---|
| 1 | **不**改 `src/**`（七处索引表 / 分析层 / 发射器） | architect 不改实现 ⇒ ⇒ 修复属 builder ⇒ ⇒ **待查项**的「复核条件」已写明重开条件 |
| 2 | **不**立 ADR 承载本项 | 本项是**实现层的键空间 / 唯一性**缺陷 ⇒ ⇒ 不是架构决策 ⇒ ⇒ **登记 + 留待独立 change** |
| 3 | **不**新增护栏 | 护栏需要**能构造出同名实例的语料**（目前仓内零）⇒ ⇒ 先归零覆盖的缺口 |
| 4 | **不**改 `port-name-literal-parity` 的 AC 勾选状态 | 属 planner 域（U2 后续）⇒ ⇒ 见 §5.3 |
