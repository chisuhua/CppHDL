---
id: decision-request-hierarchical-sibling-name-collision-2026-11
日期: 2026-11-12
提出者: planner（`rtl-track-followup-list-2026-11.md` §2 F7 的登记请求）
状态: **待 architect 裁决**
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
| — | **architect** | ⬜ **未跑** | — |
| — | **总判定** | ⬜ **待裁决** | ⛔ 未裁决 ⇒ ⇒ `port-name-literal-parity` 的 **Q21 / Q22 保持未勾**（归属 = 本请求） |
