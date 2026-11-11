# Proposal: 组件路径与节点名的前缀拼接口径（`node-name-prefix-path-contract-2026-11`）

> ⚠️ **本 change 是 `node-name-prefix-consistency` 的 P3「契约化」** ⇒ ⇒ **该 Requirement 的唯一承载**
> （`.rddf/roadmap/phase-6-rtl-productization.md:88`）⇒ ⇒ ⇒
> `port-name-literal-parity` 的 R1-b **只是契约化提案** ⇒ ⇒ ⇒
> **MUST NOT 另立一个 change 承载同一条 Requirement**（否则归档后主 spec 会出现两条同名 Requirement）。

## Why

### 缺陷（实测 · 可复算）

`Component::name()` 的**子路径拼接**（`include/component.h:266-279` 的 `build_hierarchical_name`）与
**节点名的前缀拼接**（`src/core/lnodeimpl.cpp:30-35` 的 `lnodeimpl` ctor）**口径不一致**：

| 处 | 逻辑 |
|---|---|
| `include/component.h:274` | `if (parent_path.empty() \|\| parent_path == "unnamed") return local_name;` ⇒ ⇒ **按父路径的**字面值**剥离路径段** |
| `src/core/lnodeimpl.cpp:33-35` | `name_ = parent_->name() + "." + name;` ⇒ ⇒ **不剥离** ⇒ ⇒ 父名为 `"unnamed"` 时得到 `unnamed.prog_addr` |

⇒ ⇒ 同一棵树，**组件路径**与**节点路径**给出**两个不同的名字** ⇒ ⇒ 任何「按节点名反查组件」的工具（层次化分析 / trace / 报告）都可能**找不到**。

⚠️ **根因不是「多写了两个字符」，而是**「`"unnamed"` 这个字符串哨兵被当成了『无路径段』标记**」⇒ ⇒ 而「无名」在语义上**只是一个普通名字**。

### 为什么现在做

① **方向 (i) 撞既有签批契约** —— 若让 `lnodeimpl` 也剥哨兵 ⇒ ⇒ 根组件的成员节点变成裸名 ⇒ ⇒ **违反** `openspec/specs/component-ownership-and-naming/spec.md:62`「其 Verilog 名 MUST 含该组件的路径段」⇒ ⇒ ⇒ 需 ADR 取代流程 ⇒ ⇒ ⇒ 故本 change 取**方向 (ii)**（改 `build_hierarchical_name` ⇒ 与 `lnodeimpl` **同口径**）；
② **方向 (i) 已登记为未决残差** —— 台账 §7 **行 21**（与 route (c) 字面同名一并处置）⇒ ⇒ ⇒ 本 change **不碰**它；
③ **实施体量极小**（`include/component.h:274` 删一行）⇒ ⇒ **仓内零影响 + 下游可见行为变更** ⇒ ⇒ 公开面登记是**唯一**的额外义务。

## What Changes

1. **`component-ownership-and-naming` ADDED ×1** —— Requirement「组件路径与节点名的前缀拼接 MUST 同口径」：
   - **Scenario「有父的组件实例路径由父路径与局部名构成」** —— `name()` MUST 等于 `<P> + "." + <L>`；MUST NOT 因 `<P>` 的**字面值**而剥离该路径段
   - **Scenario「无父的组件实例路径等于其局部名」** —— `parent == nullptr` 时 `name()` MUST 等于 `local_name()`；其**子**节点 MUST 以 `<local_name> + "."` 为前缀（与上一条同口径）
   - **「无名是普通名字，不是「无路径段」标记」** —— 保留为**陈述句**（**不是** MUST ⇒ ⇒ 不制造不可违反条款）

   ⇒ ⇒ **delta 正文逐字取自** `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-b 的围栏内容。

2. **实施体**（F-A）—— `include/component.h:274` 删掉 `|| parent_path == "unnamed"` ⇒ ⇒ 与 `src/core/lnodeimpl.cpp:30-35` **同口径** ⇒ ⇒ **一处一行**。

3. **公开面登记**（行为变更，非符号移除）—— `docs/CHANGELOG.md` 一行 + `.rddf/checks/public-api-surface.md` 的「**行为变更（非符号移除）**」节（若该节不存在 ⇒ 降级为 CHANGELOG 一行 + **如实登记该节缺失**）。
   ⚠️ **代价 MUST 同时明写**：「空名与用户真名 `unnamed` 在 `name()` 上**永久不可区分**」⇒ ⇒ 这一条**隐藏了一类配置错误** ⇒ ⇒ 彻底解决须**另立 item**（引入独立的「无名」标记而非复用字面名）⇒ ⇒ **不在本项**。

## 不做（Non-Goals）

- **不做**方向 (i)（让 `lnodeimpl` 也剥哨兵）⇒ ⇒ 撞 `:62` 的既有 Requirement ⇒ ⇒ 需 ADR 取代流程 ⇒ ⇒ 已登记为未决残差（台账 §7 行 21）。
- **不做**引入独立的「无名」标记 ⇒ ⇒ 那才能根治「空名与真名 `unnamed` 不可区分」⇒ ⇒ **另立 item**。
- **不做**引入「空名哨兵语义 MUST 明写」这类条款 ⇒ ⇒ metis finding 15 判定该 MUST **不可违反**（写不出违反样例）⇒ ⇒ 属 AI slop ⇒ ⇒ ⇒ 哨兵语义**只作事实登记**。
- **不做** 组件路径 / 节点名的**其它**口径变更 ⇒ ⇒ 本 change 只做「两者**同口径**」这一件事。

## 依赖与顺序

| # | 依赖 | 理由 |
|---|---|---|
| 1 | 无（**仓内零影响**） | 改 `include/component.h:274` 一行 ⇒ ⇒ 仓内所有调用点的行为**不变**（根组件名为 `""` 或 `"unnamed"` 两种情形下 `build_hierarchical_name` 的返回值**相同**）⇒ ⇒ ⇒ 仅**下游**（按 `name()` 精确匹配）可见变化 |

⇒ ⇒ **可与 `port-naming-member-parity-contract-2026-11` 并行**。

## 验收标准（AC）

- [x] `openspec/changes/node-name-prefix-path-contract-2026-11/specs/component-ownership-and-naming/spec.md` 的 delta 与 `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-b 围栏内容**逐字一致**
- [x] delta 用 `## ADDED Requirements` ⇒ ⇒ 否则 `scripts/verify_openspec_delta.sh` R3 会**静默不合并**
- [x] `bash scripts/verify_openspec_delta.sh node-name-prefix-path-contract-2026-11` 绿
- [x] `openspec validate --specs`（**非 strict**）**不新增**红项
- [x] `include/component.h:274` 的 `|| parent_path == "unnamed"` 已删 ⇒ ⇒ 与 `src/core/lnodeimpl.cpp:30-35` **同口径**
- [x] **既有** `component-ownership-and-naming/spec.md:62` 的 Requirement（「其 Verilog 名 MUST 含该组件的路径段」）**未被触碰**（本 delta 是**追加**）
- [x] 公开面登记已落（`docs/CHANGELOG.md` + `public-api-surface.md`），且**代价**（空名与真名 `unnamed` 永久不可区分）已明写
- [x] `rtl-hierarchical-output` 的那条 Requirement 由**另一个** change（`port-naming-member-parity-contract-2026-11`）承载 ⇒ ⇒ 本 change **MUST NOT** 出现它

### ✅ 验收记录（verifier · 2026-11-11）

> **验收报告**：`.rddf/reviews/b0-batch-verify-2026-11.md`（B0 批六项 · 本 change 的实施体在 B0 内完成）
> **逐条证据**见该报告 §4（`node-name-prefix-path-contract` 8/8 · `port-naming-member-parity-contract` 7/7）
> ⚠️ **归档注**：归档 MUST **禁** `--skip-specs` ⇒ ⇒ 本 delta 会被合并进主 spec
> ⇒ ⇒ ⇒ B1 的 **N4「规范落点」** = **归档本 change**（不是另写一份规范）

---

## 用户可见变更

> 本段按 `.rddf/checks/public-api-surface.md` 的载体机制填写（机制生效日 = 2026-11-09）。

- ⚠️ **行为变更（非符号移除）**：`ch::Component::name()` 的**子组件全路径**不再按父路径的**字面值**剥离路径段
  （删掉 `include/component.h` 的 `|| parent_path == "unnamed"`）⇒ ⇒ **与节点名前缀拼接（`src/core/lnodeimpl.cpp`）同口径**。
  - **唯一受影响的情形**：**根组件**的 name 为空（`local_name_` 归一成 `"unnamed"`，`src/component.cpp`）
    ⇒ ⇒ 其**子组件**的 `name()` 从**裸局部名**（`"kid"`）变为**带父前缀**（`"unnamed.kid"`）。
  - ⇒ ⇒ **下游若按 `name()` 精确匹配裸局部名 ⇒ MUST 更新**。
  - **常规路径**（父名非空，如 `"top"`）**逐字节不变** ⇒ ⇒ 仓内零影响。
- ⚠️ **已登记代价**：空名与用户**真名** `"unnamed"` 在 `name()` 上**永久不可区分**
  ⇒ ⇒ 这隐藏了一类配置错误 ⇒ ⇒ 彻底解决须**另立 item**（引入独立的「无名」标记）。
- **公开 API 移除 / 改名：无**（无符号被删或改名）⇒ ⇒ **不走** P14 的移除登记（只走「行为变更」节 + CHANGELOG）。
