---
evidence: probe:scripts/probe_arch_ledger_residuals.sh
状态: ✅ **已闭**（建议 **B** 已落地 · 2026-11-14 复核：登记表 `arch-residual-registry.md` + 护栏 `verify_arch_ledger_residuals.sh` + CI 接线 ⇒ ⇒ 本单 §1.2 的 3 行陈旧已变「已收口 / 已裁决」⇒ ⇒ 本单 §1.3 的口径差亦已随行 1b 落地）
---

# 决策请求单：架构台账 §7 的**陈旧状态格**与**口径差**（2026-11-07）

> ⚠️ **本单的定位**：builder 侧把**实测证据做齐了**（可复算探针 + 逐行闭口证据 + 措辞
> 选项）⇒ ⇒ ⇒ **裁决权在 architect** ⇒ ⇒ ⇒ **本单只列「事实 / 选项 / 代价 / 建议」，
> 不做决定**（与 `.rddf/checks/architect-decisions-pending-2026-11.md`、
> `.rddf/checks/decision-request-input-port-guards-2026-11.md` 同体例）。
>
> ⚠️ **为什么走本单而不是直接改**：台账 `docs/architecture/2026-11-02-hierarchical-rtl-output.md`
> 在 `.rddf/config/arch-docs.json` 的 `arch_paths` 内 ⇒ ⇒ ⇒ **architect 域** ⇒ ⇒ ⇒
> ⇒ builder **MUST NOT** 单方面改 ⇒ ⇒ ⇒ ⇒ 且两个 change（`p8-…` · `hierarchical-corpus-…`）
> 的归档工件里**已经写明**「台账措辞归 architect ⇒ 本 change MUST NOT 改台账」
> ⇒ ⇒ ⇒ ⇒ ⇒ **不是**遗漏 ⇒ ⇒ ⇒ ⇒ 是**刻意**的边界。

## 0. 一句话背景

台账 §7 用**散文**记录残留的状态 ⇒ ⇒ ⇒ ⇒ 闭口了**不会**自动更新 ⇒ ⇒ ⇒ ⇒ 实测发现
**3 行**在闭口后**仍**写着「未决 / 无」⇒ ⇒ ⇒ ⇒ ⇒ 且**没有任何护栏**会发现（★
`scripts/verify_ledger_evidence.sh` 只核验 front-matter 的 `evidence:` 字段 ⇒ ⇒ ⇒ ⇒
**不**核验「状态格 vs 实际闭口」）⇒ ⇒ ⇒ ⇒ ⇒ **这类漂移是静默的**。

## 1. 实测证据（2026-11-07 · **仓内可复算**）

### 1.1 探针

```bash
bash scripts/probe_arch_ledger_residuals.sh          # 主路径
bash scripts/probe_arch_ledger_residuals.sh <台账副本>  # 负向对照（可选路径）
```

⚠️ 探针**只列事实**（★ 不做决定）⇒ ⇒ ⇒ 它的输出就是本单的 `evidence:` 指向物。

### 1.2 实测结果（**3 行陈旧**）

| 台账 §7 行 | 台账状态格（**实际内容**） | 闭口证据（**仓内路径**） | 判定 |
|---|---|---|---|
| **1** | 「⚠️ 未决 ⇒ 归 **P8**（公共 helper 立项）」 | `openspec/changes/archive/2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11` | ❌ **陈旧** |
| **1b** | 「⚠️ 未决 ⇒ **独立立项** ⇒ ⚠️ **MUST NOT** 并入 P8 …」 | `openspec/changes/archive/2026-11-07-hierarchical-corpus-regression-2026-11` | ❌ **陈旧** |
| **5** | 「⚠️ 无 ⇒ **独立 change** 加 `scripts/verify_*.sh`」 | `scripts/verify_hierarchical_port_naming.sh`（★ 2026-11-06 · **已进 CI**） | ❌ **陈旧** |

⇒ ⇒ **3 / 3** 登记的残留 id **全部**陈旧 ⇒ ⇒ ⇒ ⇒ ⇒ 这不是「个别行忘了改」
⇒ ⇒ ⇒ ⇒ ⇒ 而是「**散文状态格没有任何维护机制**」的**系统性**现象。

### 1.3 口径差（★ **用户 2026-11-07 已拍板 X1**，但台账原文未反映）

| 项 | 内容 |
|---|---|
| 台账 §7 行 1b 的**原文** | 「层次化产物在全仓测试语料上的**行为覆盖** = 仅 T3-005 的 5 个专用 DUT ⇒ ⇒ 层次化路径在既有语料上零覆盖」 |
| 已交付的**口径**（X1 · 用户拍板） | **行为覆盖** = **T1 行为级**（精选 **3** 个结构类：兄弟边+同类型多实例 / 双子组件 / root 持 mem + AXI 端口）+ **T0 产物级**（**9** 个复合 DUT 的发射 / 分层 / 端口集自洽 / 确定性 / ≠ 扁平 / lint）⇒ ⇒ + **豁免表**（未来新增语料时的第二条出路） |
| **差在哪** | 台账原文只承认「**行为**覆盖」⇒ ⇒ **T0（产物级）不落在该词内** ⇒ ⇒ ⇒ ⇒ ⇒ 若台账按字面理解 ⇒ ⇒ **T0 的交付不算数** ⇒ ⇒ ⇒ ⇒ ⇒ 而 T0 恰恰是「栅栏随语料增长而增长」的**承重件** |
| **已做的登记** | 差已**显式登记**在：change `hierarchical-corpus-regression-2026-11` 的 `proposal.md` §3.6 + `tasks.md` §B + capability `hierarchical-corpus-regression` 的 `## Purpose`（⇒ ⇒ **契约面已含双层口径**）⇒ ⇒ ⇒ ⇒ ⇒ **只差台账那一句** |

## 2. 选项（**互斥** · 需选一）

| # | 选项 | 内容 | 代价 | 风险 |
|---|---|---|---|---|
| **A** | **只改措辞**（最小） | 把 §7 的 **3 行**状态格改成「✅ 已收口（日期 · 归档名 / 护栏名）」；行 1b 的**描述**补上 X1 的双层口径 | 极低（3 行散文） | ⚠️ **同类漂移会再次发生** ⇒ ⇒ ⇒ 因为**仍无机制** |
| **B** | **改措辞 + 引入「残留登记表」** | A + 新建一份**机器可核**的残留登记（★ 每条 = `id | 状态 | 闭口证据路径 | 归属`）⇒ ⇒ ⇒ 台账 §7 的状态格**从登记表派生** ⇒ ⇒ ⇒ ⇒ 探针升级为**护栏**（CI 检出陈旧即红） | 中（1 份登记表 + 1 个护栏 + CI 接线）⇒ ⇒ ⇒ ⚠️ **需要 architect 定「登记表归属哪」**（台账内？独立文件？） | ⚠️ 若登记表与台账**双源** ⇒ ⇒ ⇒ ⇒ 会**引入新的漂移面**（★ 必须**单向派生**：台账 = 登记表的**渲染结果**） |
| **C** | **只登记为 backlog** | 不改台账，改为在 roadmap / backlog 留一条「台账 §7 状态格待同步」 | 极低 | ⚠️ 「已知待办」**不会**被自动检查 ⇒ ⇒ ⇒ ⇒ ⇒ 与现状的差别**只有一条记录** |

⇒ ⇒ ★ **本单的建议 = B**，但**B 的关键约束**是：**登记表是唯一真相，台账是它的渲染**
⇒ ⇒ ⇒ ⇒ 否则就是「用一份机器可核的东西换另一份要手工同步的东西」⇒ ⇒ ⇒ ⇒ ⇒ **不解决根因**。

## 3. 建议（★ 只给建议，不做决定）

1. **必须做**：行 **1b** 的**描述**补上 X1 的双层口径（★ 否则台账按字面读 ⇒ ⇒ **T0 不算数**）
   ⇒ ⇒ ⇒ 与行 1 / 行 5 的状态格同步改（★ 三处**同一轮**改 ⇒ ⇒ ⇒ ⇒ 否则又是一次部分同步）
2. **强烈建议**：走 **B**（登记表 + 派生）⇒ ⇒ ⇒ ⇒ 因为 **A 与 C 都治不了「下一行又忘了」**
   ⇒ ⇒ ⇒ ⇒ 且本仓的既有证据支持这条路线（★ `scripts/verify_input_port_required_registry.sh`
   的「登记表双向校验」= 同一形态的既有先例）
3. **无论选哪个**：`scripts/probe_arch_ledger_residuals.sh` 应当**留在仓里** ⇒ ⇒ ⇒ ⇒
   （★ 它是本单的 `evidence:` 指向物 ⇒ ⇒ ⇒ ⇒ ⇒ 删掉 ⇒ ⇒ ⇒ ⇒ ⇒ **证据链断** ⇒ ⇒ ⇒ ⇒ ⇒
   ⇒ ⇒ ⇒ ⇒ 且它**只读** ⇒ ⇒ ⇒ ⇒ ⇒ 零风险）

## 4. 复现命令

```bash
cd /workspace/project/CppHDL
bash scripts/probe_arch_ledger_residuals.sh
# 期望：3 行 STALE（1 · 1b · 5）
```

## 5. 相关

- 台账：`docs/architecture/2026-11-02-hierarchical-rtl-output.md`（§7「已知残留」）
- 探针：`scripts/probe_arch_ledger_residuals.sh`
- 已归档的 change：`2026-11-07-p8-hierarchical-equivalence-public-helper-2026-11` ·
  `2026-11-07-hierarchical-corpus-regression-2026-11`
- roadmap：`.rddf/roadmap/features/feat-hierarchical-rtl-output.md`（§③ 后续立项清单 · 全部已收口）
