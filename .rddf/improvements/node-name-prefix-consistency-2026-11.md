---
name: node-name-prefix-consistency-2026-11
priority: P1
phase: phase-6
category: framework
type: fix
状态: draft
主题: Verilog codegen 完整化
依赖: component-ownership-and-naming（节点名构造的规范所有者）· port-name-literal-parity-2026-11（字面同名路线的可行边界 · 本项是其**上游前置**）
来源: 用户 2026-11-10 拍板「按这个顺序推进」+ oracle 建议 4（把 F-A 从 route (c) 里拆出）+ planner 实测（2026-11-10）+ metis 第 1 轮（6 BLOCKER）
生成时间: 2026-11-10
evidence: probe:三探针（probe_bare_port_name / probe_fb / probe_member）—— 源码 MUST 逐字搬进 tests/test_port_name_parity.cpp（见 sister 草案的 P1）
---

# Improvement: 节点名拼接与组件名拼接的**前缀口径一致**（`"unnamed"` 哨兵语义收口）

- **Date drafted**: 2026-11-10
- **Drafted by**: planner 角色（build-agent）
- **Status**: 📝 **draft**（待用户拍板 ⇒ 拍板后转 builder 流程）
- **来源**：oracle 建议 4（2026-11-10）——「把 F-A 从 route (c) 里拆出来，做成 **B0 级零 churn 独立项**」⇒ ⇒ 本文件即该立项
- ⚠️ **⚠️ metis 订正（2026-11-10 · 6 BLOCKER）**：**本项不是「B0 零风险」** ⇒ ⇒ oracle 的「零 churn」前提**只对仓内当前成立**，**对下游不成立** ⇒ ⇒ 本项含**下游可见的行为变更** ⇒ ⇒ **定位已重写**（见 §A.2 / §A.4）
- ⚠️ **与 `port-name-literal-parity-2026-11` 的关系**：本项是「字面同名」路线的**上游前置**（把前缀口径钉死）⇒ ⇒ **本项做完不产生字面同名** ⇒ ⇒ ⇒ 进一步支撑 route (c)「当前不做」的结论

---

**主题**: Verilog codegen 完整化

## 架构依据

### A.1 实测事实（2026-11-10）

| # | 事实 | 证据 |
|---|---|---|
| ① | 🔴 **两处前缀拼接口径不一致**：`include/component.h:274` 的 `build_hierarchical_name` 对**空父路径**与**字面 `"unnamed"`** 特判 ⇒ ⇒ **返回裸 `local_name`**（去前缀）；而 `src/core/lnodeimpl.cpp:30-35` 的**节点名**拼接**无**同款特判 ⇒ ⇒ 恒 `parent_->name() + "." + name` ⇒ ⇒ ⇒ **同一个无名根下，根自己的节点与根的孩子的节点走了两套口径** | **实测**（`/tmp/opencode/probe_bare_port_name.cpp` · 根名 = `""`）：<br>`IN raw_name=unnamed.prog_addr  parent=unnamed` ← **根自己的节点**（带 `unnamed.`）<br>`IN raw_name=leaf.unnamed_input  parent=leaf` ← **根的孩子的节点**（**不带** `unnamed.`）<br>⇒ ⇒ 同一层级内两套形态 |
| ② | ⚠️ **哨兵从哪来**：`src/component.cpp:66` `local_name_(name.empty() ? "unnamed" : name)` ⇒ ⇒ **空名被替换成字符串哨兵 `"unnamed"`**（永不为空）⇒ ⇒ ⇒ `component.h:274` 的 `parent_path.empty()` 那一支**在实践中不可证为死支**（move-constructed 组件的 `name_` 可为空 · `src/component.cpp:99-114`）⇒ ⇒ ⇒ **准确表述**：该特判**实际只被 `"unnamed"` 这一支触发** | `src/component.cpp:66` · `src/component.cpp:99-114` |
| ③ | ⚠️ **「从未被命中」只对仓内当前成立**：<br>① `ch_module` 的 `instance_name` **无默认值**（`include/module.h:35`）⇒ ⇒ 仓内**零**无名 module<br>② 所有 `ch_device<T>` 的根名 = `"top"`（`include/device.h:28` 硬编码）<br>③ 现有产物**零** `unnamed.` 路径段<br>④ 🔴 **但 `Component` 的公开默认实参是 `name = ""`**（`include/component.h:61`）⇒ ⇒ **下游写 `MyDut dut;`（不传名）= 无名根** ⇒ ⇒ ⇒ 下游**随时可达** ⇒ ⇒ ⇒ ⚠️ **「仓内零影响」≠「下游零影响」** | `include/module.h:35` · `include/device.h:28` · `include/component.h:61`（**公开默认实参**）· 产物全文扫描 |
| ④ | ⚠️ **方向必须二选一**，且两条的后果**不同** | 见 §A.2 |
| ⑤ | ⚠️ **方向 (i) 会撞既有已签批 Requirement**：`openspec/specs/component-ownership-and-naming/spec.md:62`「其 Verilog 名 MUST 含该组件的路径段」⇒ ⇒ 若让 `lnodeimpl` 也剥哨兵 ⇒ ⇒ 无名根下根组件**自有成员节点**的 Verilog 名变裸（实测 `unnamed.counter` → 裸 `counter`）⇒ ⇒ ⇒ **违反 :62** ⇒ ⇒ ⇒ (i) 不是「零风险修法」，而是**架构取舍** ⇒ ⇒ ⇒ MUST 走 architect | `openspec/specs/component-ownership-and-naming/spec.md:58-62` · 实测 `probe_member.cpp` |
| ⑥ | ⚠️ **真正被破坏的不变量**是「**子组件** `name() == parent->name() + "." + local_name()`」（今天**不成立**：孩子是 `leaf`，父是 `unnamed`）；而「**节点名** == owner->name() + "." + local」在**修前修后都成立**（`lnodeimpl` 恒拼）⇒ ⇒ ⇒ 判据 MUST 打**不变量**，不是打两个不同层级的字面串 | `include/component.h:274` vs `src/core/lnodeimpl.cpp:30-35` |

### A.2 两个方向的对比

| 方向 | 做法 | 无名根下的形态 | 与 :62 | 影响面 | 判定 |
|---|---|---|---|---|---|
| **(i)** | 给 `lnodeimpl` 加同款特判 ⇒ ⇒ 根自己的节点**也**去前缀 | 根节点 `prog_addr`（裸）· 孩子节点 `leaf.unnamed_input`（裸）⇒ ⇒ **一致** | 🔴 **违反**（根的自有成员节点变裸 ⇒ 不含路径段）⇒ ⇒ 需 architect 改 :62 | 交付路径零影响（根名 = `"top"`）⇒ ⇒ 但**语义**变了 | ❌ **架构取舍** ⇒ ⇒ ⇒ **不在本项** ⇒ ⇒ ⇒ 登记为**未决残差**（沿用 sister 草案的同一条） |
| **(ii)** | 把 `"unnamed"` 从「哨兵」改回「**普通名字**」⇒ ⇒ 删掉 `component.h:274` 的 `\|\| parent_path == "unnamed"` ⇒ ⇒ 拼接恒为 `parent_path + "." + local_name` | 根节点 `unnamed.prog_addr` · 孩子节点 **`unnamed.leaf.<member>`** ⇒ ⇒ **一致** | ✅ **满足**（都含路径段）⇒ ⇒ **无需改任何已签批 spec** | 🟡 **仓内零影响**（该特判仓内从未命中）· 🔴 **下游可见**：无名根的**子组件 `name()` 返回值变了**（`leaf` → `unnamed.leaf`）⇒ ⇒ ⇒ **这是行为变更，不是符号移除** ⇒ ⇒ ⇒ 需 CHANGELOG + `public-api-surface.md`「行为变更」节登记 | ✅ **本项取此方向** ⇒ ⇒ **一处一行**（`include/component.h:274`）⇒ ⇒ **签名不变**，但**返回值语义**对无名根的子组件**变了** |

### A.3 ⚠️ 本项的真实定位（metis 订正 · 与 oracle 的原判不同）

- oracle 建议 4 把本项称为「**B0 级零 churn 独立项**」⇒ ⇒ **该定级只对仓内当前成立** ⇒ ⇒ **下游有行为变更** ⇒ ⇒ ⇒ **本项不是纯零风险** ⇒ ⇒ ⇒ 但它**仍比 route (c) 便宜得多**（一处一行 vs 公开 API 变更 + 全仓产物 churn）⇒ ⇒ ⇒ ⇒ 定位 = **「B0 级的仓内零影响 + 下游可见行为变更」** ⇒ ⇒ ⇒ 必须走 CHANGELOG 登记（§范围 P3）。
- ⚠️ **本项的真实驱动力有两个**（metis 指出，本项自己没分清）：
  ① **修口径不一致**（正当理由）：`Component` 的公开默认实参 ⇒ ⇒ **下游 `MyDut dut;`（不传名）= 无名根** ⇒ ⇒ 其子组件路径**丢父段**（今天的行为）⇒ ⇒ ⇒ 可碰撞、可误推断 ⇒ ⇒ ⇒ **这才是「该修」的正当理由**
  ② **给 route (c) 收口**：把哨兵固化为「普通名字」⇒ ⇒ 将来想做「根端口裸名」就多一条刚签的 Requirement 要 MODIFIED ⇒ ⇒ ⇒ **代价已由本项自己算出并写进 A.2**
  ⇒ ⇒ ⇒ ① 是**独立的正当性**（不依赖 route (c)）；② 是**连带效果** ⇒ ⇒ ⇒ **两者都要登记**，且 ① 才是本项的**主理由**。

### A.4 「零影响」的分层表述（metis 订正 · 禁过度声称）

| 层 | 结论 | 依据 |
|---|---|---|
| **仓内** | ✅ **产物零 diff**（**当前**成立） | `ch_module` 强制传名 · `ch_device` 根名 `"top"` · 现有产物零 `unnamed.` 路径段 ⇒ ⇒ 该特判**仓内从未命中** ⇒ ⇒ ⇒ 仓内 3 份既有 `.v` 在改动前后 `git diff --stat` **必须为 0**（**这是 AC，不是论证**） |
| **下游** | 🔴 **行为变更** | `Component` 的 `name = ""` 是**公开默认实参**（`include/component.h:61`）⇒ ⇒ 下游 `MyDut dut;` = 无名根 ⇒ ⇒ 修后其子组件 `name()` 从 `leaf` 变 `unnamed.leaf` ⇒ ⇒ ⇒ **必须 CHANGELOG + `public-api-surface.md` 登记**（**不是**符号移除 ⇒ ⇒ **不是** `public-api-remove-visibility` 的流程） |
| **无名根产物** | 🔴 **必然变**（**不是**「逐字节不变」） | 组件 `local_name()` 没变，但**扁平端口名**会变（`leaf_*` → `unnamed_leaf_*`）⇒ ⇒ 且 `hierarchy_analysis` 与 `codegen_verilog` **共用同一张全局表**（`src/hierarchy_analysis.cpp:195-201`）⇒ ⇒ ⇒ **两条产物路径同时变** ⇒ ⇒ ⇒ 「层次化产物逐字节不变」这条 AC **只在 root = `top` 时成立** ⇒ ⇒ ⇒ **必须**限定口径 |
| **去重后缀** | 🟡 **未被枚举的一维** | 同名兄弟的去重按 **base name** 计数（`include/codegen_naming.h:72-88`）⇒ ⇒ 新增 `unnamed_` 前缀会**改变 base 键** ⇒ ⇒ 对无名根的 `_N` 后缀可能变化 ⇒ ⇒ 仓内无无名根 ⇒ ⇒ **不爆发**，但「零影响」论证里**漏了这一维** |

---

## 范围

### ✅ 在内

| 编号 | 内容 |
|---|---|
| **P1** | **口径对齐（方向 (ii)）**：`include/component.h:274` 的 `build_hierarchical_name` ⇒ ⇒ **删掉** `\|\| parent_path == "unnamed"` 这一支 ⇒ ⇒ ⚠️ **准确表述**（metis 订正）：函数**仍有** `!parent` 支（对每个根都活着）**和** `parent_path.empty()` 支（不可证为死支 · move-constructed 组件的 `name_` 可为空）⇒ ⇒ ⇒ **准确效果** = 「对一切**有父**的组件恒拼接」⇒ ⇒ ⚠️ **禁**写「退化为恒定拼接」 |
| **P2** | **判据（函数级 + 不变量 + 产物层三层）**：<br>① **函数级**（最便宜 · 不依赖 ctx / 组件树 / ConstructionFrame）：`build_hierarchical_name(&parent_named_unnamed, "x") == "unnamed.x"` ⇒ ⇒ 抓 `component.h:274` 的**核心语义**<br>② **不变量级**（任意深度）：对 fixture（无名根 + 2～3 层）⇒ ⇒ `child->name() == parent->name() + "." + child->local_name()` ⇒ ⇒ 抓**任意深度**回归（不是只钉 `leaf` 这一层）⇒ ⇒ ⚠️ 断言**只钉组件路径那一段** ⇒ ⇒ **禁**把端口局部名混进断言（`unnamed.leaf.unnamed_input` 里的第二个 `unnamed` 是**端口局部名** ⇒ ⇒ 混用会让读者以为是笔误）<br>③ **产物层**（两条，分开）：<br>　• `NamedRootArtifactsUnchanged`：仓内 3 份既有 `.v` 在改动前后 `git diff --stat` **== 0** ⇒ ⇒ ⚠️ **`scripts/verify_hierarchical_rtl.sh` 对本改动结构性失明**（R8 的手推期望集只覆盖仓内 3 个根名 = `top` 的产物）⇒ ⇒ 它全绿**不构成任何证据** ⇒ ⇒ ⇒ 必须用 `git diff` 直证<br>　• `UnnamedRootArtifactPinned`：显式生成无名根产物，以 golden **钉住新形态**（`unnamed_leaf_*`）⇒ ⇒ 并在用例注释写明「**本用例的期望值因本项而变**」<br>④ **负向对照**：把 `component.h:274` 的特判加回去 ⇒ ⇒ ①② **必红** ⇒ ⇒ 还原复绿<br>⑤ ⚠️ **变异自证 MUST 含「重建」步骤**（根 `AGENTS.md` COMMANDS 段「还原源码 ≠ 复原」） |
| **P3** | **契约化（`## ADDED Requirements` · 不是 MODIFIED）**：`openspec/specs/component-ownership-and-naming/spec.md` **追加** 一条 Requirement：「组件实例的**子**路径 MUST 由「父路径 + `.` + 局部名」构成 ⇒ ⇒ MUST NOT 因父路径的**字面值**而单方面剥离路径段 ⇒ ⇒ 节点名的前缀拼接 MUST 与组件路径拼接**同口径**」⇒ ⇒ ⚠️ **禁**放成 `MODIFIED`（`scripts/verify_openspec_delta.sh` R3 要求 MODIFIED 标题与主 spec **逐字一致** ⇒ ⇒ 否则**静默不合并** —— 最坏的失败模式）⇒ ⇒ ⇒ **禁**顺带改 `:62` 的语义（那是方向 (i) 才需要的）⇒ ⇒ ⇒ **禁**加不可违反的 MUST（见 §范围「不在本项」）<br>⚠️ **落地面**：spec 变更 MUST 走 **openspec change 流程**（根 `AGENTS.md §OPENSPEC WORKFLOW`）⇒ ⇒ ⇒ **delta 形态 = `## ADDED Requirements`** ⇒ ⇒ ⇒ **architect 的动作 = 复核 delta 内容 + 批准其走 change 流程** ⇒ ⇒ ⇒ 完整 delta 文本见 `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-b |
| **P4** | **下游行为变更登记**：`docs/CHANGELOG.md` 一行 + `.rddf/checks/public-api-surface.md` 的「**行为变更（非符号移除）**」节（若该节不存在 ⇒ 降级为 CHANGELOG 一行 + **如实登记该节缺失**）⇒ ⇒ ⚠️ **不是** `public-api-remove-visibility` 的流程（符号未移除）⇒ ⇒ ⇒ 但**必须**登记（下游按 `name()` 精确匹配的会断）<br>⚠️ **MUST 同时明写「空名与用户真名 `unnamed` 在 `name()` 上永久不可区分」这一代价**（metis 边界情况 · 它隐藏了一类配置错误）⇒ ⇒ ⇒ 彻底解决须另立 item（引入独立的「无名」标记而非复用字面名）⇒ ⇒ **不在本项** |

### ❌ 不在本项

- **方向 (i)**（让 `lnodeimpl` 也剥哨兵 ⇒ ⇒ 根节点裸名 ⇒ ⇒ 撞 `:62`）⇒ ⇒ **需 architect 裁决** ⇒ ⇒ ⇒ 登记为**未决残差**（⚠️ **沿用** `port-name-literal-parity-2026-11` §裁决记录 ④ 的**同一条** ⇒ ⇒ ⇒ **本项不新增行** ⇒ ⇒ ⇒ 免得双向 1:1 护栏被**双重登记**打红）
- **route (c) 本身**（去根前缀 ⇒ ⇒ 字面同名）⇒ ⇒ 归 `port-name-literal-parity-2026-11` 的 P3 裁定包
- **`Component::name()` 的签名** ⇒ ⇒ 本项**签名零变更** ⇒ ⇒ 不走 `public-api-remove-visibility-2026-11`
- **`ch_module<T>("")` 缺与 `create_child` 对齐的 `CHREQUIRE`**（`include/module.h:35` vs `include/component.h:169`）⇒ ⇒ **独立 item** ⇒ ⇒ **不在本项**（scope creep · 且它会改变公开行为）
- **哨兵本身的存在**（`src/component.cpp:66` 的空名替换 · `src/core/context.cpp:59` 的同名逻辑）⇒ ⇒ 本项**不动**它 ⇒ ⇒ 只**统一它的语义解读**

---

## Why

### W.1 口径不一致会让读者按错误规则推断命名（正当理由）

A.1① + A.1⑥ ⇒ ⇒ 同一层级内两套形态 ⇒ ⇒ 读者按根自己的节点名去推断孩子节点名 ⇒ ⇒ **必然推断错** ⇒ ⇒ 而这正是仓内「虚假信心」纪律（`rv32i-dead-component-consolidation` W.1）要治的形态：**看起来一致、实际不一致**。
⇒ ⇒ ⚠️ 且这不是纯理论：`Component` 的公开默认实参（`include/component.h:61`）⇒ ⇒ **下游 `MyDut dut;`（不传名）= 无名根** ⇒ ⇒ ⇒ **下游随手可达**（A.3①）。

### W.2 它比 route (c) 便宜一个量级

方向 (ii) = **一处一行** + 零签名变更；route (c) = 公开 API 语义变更 + 全仓产物字节 churn + `ADR-061` 修订 + 唯一性论证 ⇒ ⇒ ⇒ **先做前者** ⇒ ⇒ ⇒ 后者的成本估算才有可信底座。

### W.3 不拆就会阻塞 B0

A.2 方向 (i) 撞 `:62` ⇒ ⇒ 需 architect ⇒ ⇒ ⇒ 若与 route (c) 捆绑 ⇒ ⇒ **B0 级的仓内零影响修复被绑到一个 P2 级裁决上** ⇒ ⇒ ⇒ 违反批次表的 B0 定义 ⇒ ⇒ ⇒ 必须拆。

### W.4 它把 route (c) 的代价**钉在文档里**

A.2 ⇒ ⇒ 方向 (ii) 把哨兵固化为「普通名字」⇒ ⇒ 字面同名就必须整体去掉根路径段 ⇒ ⇒ 撞 `:62` ⇒ ⇒ ⇒ 代价从「模糊」变成「**具体且已量化**」⇒ ⇒ ⇒ 这正是 P3 裁定包需要的输入。

---

## What Changes

### P1 — 口径对齐（一处一行）

```cpp
// include/component.h:266-279（现状 → 改后）
inline std::string build_hierarchical_name(const Component* parent, const std::string& local_name) {
    if (!parent) {
        return local_name;                       // ← 不变（对每个根都活着）
    }
    const std::string& parent_path = parent->name();
    if (parent_path.empty()) {                  // ← 删掉 `|| parent_path == "unnamed"`
        return local_name;                       // ← 保留（不可证为死支：move-constructed 组件的 name_ 可为空）
    }
    return parent_path + "." + local_name;      // ← 不变
}
```
- ⇒ ⇒ 改后与 `src/core/lnodeimpl.cpp:30-35` **同口径**（有父 ⇒ 恒拼 `parent_path + "." + local`）⇒ ⇒ ⇒ 无名根下：根自己的节点 `unnamed.prog_addr` · 孩子的节点 `unnamed.leaf.<member>` ⇒ ⇒ **一致** ✓
- ⚠️ **签名零变更**，但**返回值语义对无名根的子组件变了**（`leaf` → `unnamed.leaf`）⇒ ⇒ ⇒ **必须**走 P4 的下游登记

### P2 — 判据（三层）

- **落点：`tests/test_component_naming.cpp`**（⚠️ **metis 建议 3 · 解循环依赖**）—— 该文件**已存在且已注册**（`tests/CMakeLists.txt:1079` `add_catch_test(test_component_naming test_component_naming.cpp)`）⇒ ⇒ ⇒ **无循环依赖**（不再依赖尚未存在的 `tests/test_port_name_parity.cpp`）⇒ ⇒ ⇒ **`CTEST_TOTAL` 真零变化** ✓ ⇒ ⇒ ⇒ 且它本就是「命名判据」的归属地
- 三层判据（函数级 / 不变量级 / 产物层）见 §范围 P2 ①②③
- ⚠️ **构造作用域**：测试里裸构造无名根**不走** `ConstructionFrame` ⇒ ⇒ 直接 `make_unique<Root>(nullptr, "")` 的构造作用域帧会**留到析构**（`include/component.h:139-156` 的已知限制）⇒ ⇒ ⇒ **同一 TEST_CASE 里后建的节点会被误归属** ⇒ ⇒ ⇒ 用例**必须**显式加 `ConstructionFrame`，或把这条写进用例注释
- ⚠️ **函数级判据不需要 ctx** ⇒ ⇒ 它是**最便宜**的一条 ⇒ ⇒ ⇒ 优先写它

### P3 — 契约化

- `openspec/specs/component-ownership-and-naming/spec.md` **追加** Requirement（**`## ADDED Requirements`**，**不是** `MODIFIED`）：
  「组件的**子**路径 MUST 由「父路径 + `.` + 局部名」构成 ⇒ ⇒ **MUST NOT** 因父路径的**字面值**而单方面剥离路径段 ⇒ ⇒ 节点名的前缀拼接 MUST 与组件路径拼接**同口径**」
  - ⇒ ⇒ **可违反**：把 `component.h:274` 的特判加回去 ⇒ ⇒ 子组件 `name()` 变成裸局部名 ⇒ ⇒ **违反样例可构造** ✓ ⇒ ⇒ ⇒ **不违反「每条 MUST MUST 有可构造违反样例」**的纪律
  - ⚠️ **禁**再加「空名哨兵语义 MUST 明写」这类**不可违反**的 MUST（metis：它没有可观测行为 ⇒ ⇒ 写不出违反样例）⇒ ⇒ ⇒ 哨兵语义**在本项只作事实登记**（§A.1② + §已知风险），**不进 MUST**

### P4 — 下游行为变更登记

- `docs/CHANGELOG.md` 一行（下游按 `name()` 精确匹配的会断）
- `.rddf/checks/public-api-surface.md` 的「**行为变更（非符号移除）**」节（若该节不存在 ⇒ 降级为 CHANGELOG 一行 + **如实登记该节缺失**）
- ⚠️ **不是** `public-api-remove-visibility` 的流程（符号**未**移除）⇒ ⇒ ⇒ 但**必须**登记

---

## Acceptance

### 修复（P1 / P2）

- [ ] `include/component.h:274` 的 `|| parent_path == "unnamed"` 已删除 ⇒ ⇒ **一处一行**
- [ ] 🔴 **函数级判据**：`build_hierarchical_name(&parent_named_unnamed, "x") == "unnamed.x"` ⇒ ⇒ **不依赖 ctx / 组件树 / ConstructionFrame** ⇒ ⇒ 抓 `component.h:274` 的核心语义
- [ ] 🔴 **不变量判据**：对 fixture（无名根 + 2～3 层）⇒ ⇒ `child->name() == parent->name() + "." + child->local_name()` ⇒ ⇒ 抓**任意深度**回归 ⇒ ⇒ 断言**只钉组件路径那一段**（**禁**把端口局部名混进断言）
- [ ] 🔴 **产物层 · 仓内**：`NamedRootArtifactsUnchanged` ⇒ ⇒ 仓内 3 份既有 `.v` 的 `git diff --stat` **== 0** ⇒ ⇒ ⚠️ **`scripts/verify_hierarchical_rtl.sh` 对本改动结构性失明**（R8 的手推期望集只覆盖仓内 3 个根名 = `top` 的产物）⇒ ⇒ ⇒ 它全绿**不构成证据** ⇒ ⇒ ⇒ 必须用 `git diff` 直证
- [ ] 🔴 **产物层 · 无名根**：`UnnamedRootArtifactPinned` ⇒ ⇒ 以 golden **钉住新形态**（`unnamed_leaf_*`）⇒ ⇒ 用例注释 MUST 写明「**本用例的期望值因本项而变**」
- [ ] **负向对照**：把特判加回去 ⇒ ⇒ 函数级 + 不变量级 **必红** ⇒ ⇒ 还原复绿
- [ ] ⚠️ **变异自证 MUST 含「重建」步骤**（根 `AGENTS.md` COMMANDS 段「还原源码 ≠ 复原」）⇒ ⇒ 禁「删断言」式假变异
- [ ] ⚠️ **用例落 `tests/test_component_naming.cpp`**（**已存在且已注册**）⇒ ⇒ `CTEST_TOTAL` **零变化** ⇒ ⇒ **禁**写死「257」⇒ ⇒ 引用锚点
- [ ] **签名零变更**：`Component::name()` / `local_name()` 的**签名**不变 ⇒ ⇒ ⚠️ 但**返回值语义**对无名根的子组件**变了** ⇒ ⇒ ⇒ **必须**有 P4 的登记 ⇒ ⇒ ⇒ `scripts/verify_public_api_visibility.sh` 全绿
- [ ] 既有 ctest 全绿；`bash scripts/verify_test_counts.sh` 绿

### 契约化（P3）

- [ ] `component-ownership-and-naming` **追加** Requirement（delta 里 MUST 落 `## ADDED Requirements` ⇒ ⇒ **禁** `MODIFIED`）⇒ ⇒ `bash scripts/verify_openspec_delta.sh` 绿；`openspec validate --specs`（**非 strict**）**不新增**红项
- [ ] 新 Requirement **有可构造的违反样例**（把 `component.h:274` 的特判加回去 ⇒ ⇒ 违反）⇒ ⇒ ⇒ **禁**加不可违反的 MUST

### 下游登记（P4）

- [ ] `docs/CHANGELOG.md` 一行
- [ ] `.rddf/checks/public-api-surface.md` 的「行为变更（非符号移除）」节有一行（**若该节不存在 ⇒ 如实登记该节缺失**）

### 残差登记

- [ ] 方向 (i)（`lnodeimpl` 也剥哨兵 ⇒ ⇒ 撞 `:62` ⇒ ⇒ 需 architect）**沿用** `port-name-literal-parity-2026-11` §裁决记录 ④ 的**同一条** ⇒ ⇒ ⚠️ **本项不新增行**（免得双向 1:1 护栏被双重登记打红）⇒ ⇒ ⇒ `bash scripts/verify_arch_ledger_residuals.sh` 绿

---

## Capabilities

| capability | 动作 | 说明 |
|---|---|---|
| `component-ownership-and-naming` | **ADDED** | 追加**一条** Requirement ⇒ ⇒ ⚠️ **条款正文见唯一落地文本**（**本行不引条款原文**）⇒ ⇒ **唯一落地文本 = `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 R1-b** ⇒ ⇒ ⇒ ⚠️ **本项即该 Requirement 的承载方**（批次表 `.rddf/roadmap/phase-6-rtl-productization.md:88`）<br>⚠️ **metis 订正**：原稿写 `MODIFIED` ⇒ ⇒ **错** —— delta 里新增的 Requirement MUST 落 `## ADDED Requirements`；若放 `MODIFIED`，`verify_openspec_delta.sh` R3 要求标题与主 spec **逐字一致** ⇒ ⇒ 否则**静默不合并**（最坏的失败模式）⇒ ⇒ ⇒ **改** |
| `rtl-hierarchical-output` | **不动** | 产物端口命名归它，但本项改的是**节点名构造** ⇒ ⇒ 不越界 |
| `hierarchical-port-naming-guard` | **不动** | 沿用既有裁定：guard capability MUST NOT 复述命名规则 |

---

## Impact

### 爆炸半径

| 面 | 影响 |
|---|---|
| `include/component.h:274` | **一处一行** ⇒ ⇒ 效果 = 「对一切**有父**的组件恒拼接」（`!parent` 支与 `empty()` 支**都保留**） |
| 仓内 `.v` 产物 | ✅ **零 diff**（**当前**成立）⇒ ⇒ ⇒ `NamedRootArtifactsUnchanged` 用 `git diff` 直证（**不用** `verify_hierarchical_rtl.sh` —— 它对本改动**结构性失明**） |
| **无名根产物**（仓外 / 下游） | 🔴 **变**（`leaf_*` → `unnamed_leaf_*`）⇒ ⇒ 且**扁平与层次化两条路径同时变**（共用同一张全局表 · `src/hierarchy_analysis.cpp:195-201`）⇒ ⇒ ⇒ 「层次化产物逐字节不变」这条说法**只在 root = `top` 时成立** ⇒ ⇒ ⇒ **必须**限定口径 |
| 公开 API | **签名零变更**；**返回值语义**对无名根的子组件**变** ⇒ ⇒ ⇒ 走 P4（CHANGELOG + `public-api-surface.md`），**不走** `public-api-remove-visibility` |
| `CTEST_TOTAL` | **零变化**（用例落**已存在**的 `test_component_naming` target） |
| `component-ownership-and-naming` spec | 追加 **1** 条 Requirement（`## ADDED Requirements`）⇒ ⇒ 需 architect 复核（该 capability 是规范所有者） |

### 已知风险

| 风险 | 缓解 |
|---|---|
| **「仓内零影响」被后续改动推翻**（例如将来 `ch_device` 允许自定义根名 / 下游改用无名根） | ⇒ ⇒ `NamedRootArtifactsUnchanged` 把「仓内零影响」变成**可复跑的判据** ⇒ ⇒ 而非一次性论证 ⇒ ⇒ 且 P3 的 Requirement 会把口径**写进契约** ⇒ ⇒ ⇒ 后续改动 MUST 重新评估 |
| **「空名」与「用户真名 `unnamed`」在 `name()` 上永久不可区分**（metis 的边界情况 · **代价未在原稿登记**） | ⇒ ⇒ 修后二者在 `name()` 上合流 ⇒ ⇒ ⇒ **本项 MUST 在 CHANGELOG 明写这一条**（P4）⇒ ⇒ ⇒ ⚠️ 它**隐藏了一类配置错误**（用户拼错名字 ⇒ 静默变 `unnamed`）⇒ ⇒ ⇒ 若要彻底解决须另立 item（引入独立的「无名」标记而非复用字面名）⇒ ⇒ ⇒ **不在本项** |
| **方向 (i) 被误当成「同一个修复的两面」而顺带实施** | ⇒ ⇒ §不在本项**显式**排除 (i) ⇒ ⇒ 且 (i) 撞 `:62` ⇒ ⇒ ⇒ **必须**经 architect ⇒ ⇒ ⇒ 已登记为**未决残差**（沿用 sister 草案的同一条） |
| **AC 被写成「看起来没问题」** | ⇒ ⇒ 函数级 + 不变量级 = **当前红 → 修后绿**（可证）⇒ ⇒ 产物层两条**分别**覆盖「仓内不变」与「无名根新形态」⇒ ⇒ 负向对照 ⇒ ⇒ 四条各有各的判别力 |
| **变异自证做成「删断言」式假变异** | ⇒ ⇒ MUST **正向变异**（把特判加回去）⇒ ⇒ 且 MUST 含**重建**步骤 |
| **「数学上不可能」「可证」这类过度声称**（metis 定性） | ⇒ ⇒ **全部删除** ⇒ ⇒ 改为分层表述（§A.4）⇒ ⇒ 「仓内当前零影响」是**会过期**的经验事实 ⇒ ⇒ ⇒ AC 负责把它变成可复跑的判据 |

### 与其他 improvement 的关系

| 项 | 关系 |
|---|---|
| `port-name-literal-parity-2026-11` | **本项是它的上游前置**（把前缀口径钉死）⇒ ⇒ 它**不阻塞**本项 ⇒ ⇒ 且本项做完**不产生**字面同名 ⇒ ⇒ ⇒ 进一步支撑 route (c)「当前不做」的结论<br>⚠️ **残差登记去重**：方向 (i) 的残差**沿用**它的**同一条** ⇒ ⇒ 本项**不新增行** |
| `port-naming-ratchet-2026-11` | 无耦合（命名规则与棘轮）⇒ ⇒ 但两者共用 `component.h:274` 这处拼接 ⇒ ⇒ ⇒ **同批实施**可避免写点冲突 |
| `rtl-emit-product-tooling-2026-11` | 无耦合（manifest / CLI / 产物护栏）⇒ ⇒ 但其 C6 改共享发射器 ⇒ ⇒ 与本项**写点不重叠** ⇒ ⇒ ⇒ 可并行 |
| `rv32i-dead-component-consolidation-2026-11` | 无耦合 |

---

## metis 第 1 轮 gap 分析与处置（2026-11-10）

> **轮次**：1/1（该 improvement 总计 1 轮）· **审查者**：metis（预规划 gap 分析）
> ⚠️ **本节是过程日志，不是决策** —— 决策在正文各节的「⚠️ metis 订正」标注处。
> ⚠️ **原稿曾预留一个空的「metis 结论」小节**（dangling 引用）⇒ ⇒ **已删除** ⇒ ⇒ 本节为 metis 的**真实输出**与处置。

### ✅ 已订正（按 metis 的 6 个检查面）

| # | 检查面 | metis finding | 处置 |
|---|---|---|---|
| 1 | 隐藏意图 | 🔴 **本项的真实驱动力是「给 route (c) 收口」而非「修一致性缺陷」** ⇒ ⇒ 且 (ii) 的效果是**把 `"unnamed"` 固化为普通名字** ⇒ ⇒ 将来做「根端口裸名」就多一条刚签的 Requirement 要 MODIFIED | ✅ §A.3 把两个意图**分开登记**：① **修口径不一致**（**独立的正当性** · 不依赖 route (c)）· ② **给 route (c) 收口**（**连带效果** · 代价已算出）⇒ ⇒ ① 是主理由 |
| 2 | 隐藏意图 | 🟡 被降级的真实价值：`Component` 的**公开默认实参** ⇒ 下游 `MyDut dut;` = unnamed 根 ⇒ 其子组件路径**丢父段**（今天的行为）⇒ ⇒ 这才是「该修」的正当理由（可碰撞、可误推断） | ✅ A.1③ 已补该证据轴（`include/component.h:61`）⇒ ⇒ §A.3① 已把它提为**主理由** ⇒ ⇒ W.1 已重写 |
| 3 | 隐藏意图 | 🟡 未说出口的第三条意图：**保住「B0 零 churn」这个批次标签** ⇒ ⇒ 由此推出「用例必须并进别人的文件以保持 `CTEST_TOTAL` 不变」⇒ ⇒ **让指标反过来约束设计** | ✅ 已解除该倒置：用例落 `tests/test_component_naming.cpp` 是因为它**本就是命名判据的归属地**（且已注册）⇒ ⇒ ⇒ 不是为了迁就指标（见 P2） |
| 4 | 歧义 | 🔴 **「零影响」的作用域未定义**（仓内 vs 下游/API）⇒ ⇒ 证据绕开了唯一真正的入口：`Component` 的 `name = ""` 默认实参 | ✅ §A.4 **分层表述**（仓内 / 下游 / 无名根产物 / 去重后缀）⇒ ⇒ A.1③ 已补该入口 ⇒ ⇒ 「数学上不可能」「可证」措辞**全删** |
| 5 | 歧义 | 🔴 **「零公开 API 变更」与自述矛盾**（变的正是 `Component::name()` 对子组件的**返回值**）⇒ ⇒ AC「签名与**返回语义**不变」**不可满足** | ✅ 已改口为「**签名**零变更 · **返回值语义**对无名根的子组件**变**」⇒ ⇒ ⇒ 新增 **P4**（CHANGELOG + `public-api-surface.md`「行为变更（非符号移除）」节）⇒ ⇒ ⇒ **不走** `public-api-remove-visibility` |
| 6 | 歧义 | 🟡 「函数退化为恒定拼接」**不准确**（`!parent` 支对每个根都活着；`empty()` 支不可证为死支 —— move-constructed 组件 `name_` 可为空） | ✅ §范围 P1 / §What Changes P1 已改口径为「对一切**有父**的组件恒拼接」⇒ ⇒ 两支**都保留** |
| 7 | 歧义 | 🟡 缺陷陈述混了两件事 ⇒ ⇒ 真正被破坏的不变量是「**子组件** `name() == parent->name() + "." + local_name()`」；而「节点名 == owner->name() + "." + local」**修前修后都成立** | ✅ A.1⑥ 已写明 ⇒ ⇒ P2 判据改成**不变量**（任意深度）⇒ ⇒ ⇒ **禁**拿两个不同层级的字面串当判据 |
| 8 | 歧义 | 🟡 判据串把两个不同的 `unnamed` 混成一个（第二个是**端口局部名** `unnamed_input`） | ✅ P2 ② 已写明：断言**只钉组件路径那一段** ⇒ ⇒ **禁**把端口局部名混进断言 |
| 9 | 歧义 | 🔴 **依赖顺序循环**（自称上游前置，却要求并入一个**不存在**的 target） | ✅ P2 落点改为 **`tests/test_component_naming.cpp`**（**已存在且已注册** · `tests/CMakeLists.txt:1079`）⇒ ⇒ ⇒ **无循环依赖** + `CTEST_TOTAL` **真零变化** |
| 10 | 歧义 | 🟡 Capabilities 写 `MODIFIED`，但范围是「追加 1 条 Requirement」⇒ ⇒ 放 MODIFIED 会被 R3 静默不合并 | ✅ 已改 **ADDED** + AC 明写「delta 里 MUST 落 `## ADDED Requirements`」 |
| 11 | 歧义 | 🟢 `R1–R9` 引用口径不稳（脚本头注称 R1–R8 · `:552` 又有 R9） | ✅ AC **不用**它当判据（改用 `git diff --stat` 直证）⇒ ⇒ ⇒ 不制造第四种写法 |
| 12 | AI slop | 🔴 **`EmptyRootArtifactUnchanged` 是自相矛盾的 AC**（无名根下孩子端口名**必然**从 `leaf_*` 变 `unnamed_leaf_*`）⇒ ⇒ 要么恒红、要么名不符实恒绿 | ✅ 已**删除**该 AC ⇒ ⇒ 拆成 `NamedRootArtifactsUnchanged`（仓内 .v 的 `git diff --stat` == 0）+ `UnnamedRootArtifactPinned`（钉住新形态） |
| 13 | AI slop | 🟡 「在数学上不可能改变任何现有产物」是**过度声称** | ✅ 已删（见第 4 条）⇒ ⇒ 改为「仓内**当前**不存在 root 名为 `unnamed` 的设计」—— 一个**会过期**的经验事实 ⇒ ⇒ 由 AC 变成可复跑判据 |
| 14 | AI slop | 🟡 1 行改动配 10 条 AC + 跨域残差登记 ⇒ ⇒ 且 sister 草案**也**在登记**同一条**残差 ⇒ ⇒ 双向 1:1 护栏有**双重登记**打红的实际风险 | ✅ 残差登记段已写「**沿用** sister 草案的**同一条** ⇒ ⇒ **本项不新增行**」⇒ ⇒ ⇒ 免得双重登记 |
| 15 | AI slop | 🟡 P3 第三条「空名哨兵语义 MUST 明写」是**不可违反**的 MUST（写不出违反样例）⇒ ⇒ 违反 sister 草案自定的纪律 | ✅ 已**删除**该 MUST ⇒ ⇒ P3 只保留**一条可违反**的 Requirement ⇒ ⇒ ⇒ 哨兵语义**只作事实登记**（§A.1② + §已知风险），不进 MUST |
| 16 | AI slop | 🟢 自我盖章形态复现（预留空的 metis 小节 + dangling 引用） | ✅ dangling 引用已删 ⇒ ⇒ 本节为 metis 的**真实输出** |
| 17 | 缺失验收 | 🔴 缺**函数级**判据（最便宜 · 不依赖 ctx / 组件树 / ConstructionFrame） | ✅ P2 ① 已加（**最便宜的一条** ⇒ ⇒ 优先写） |
| 18 | 缺失验收 | 🔴 缺**不变量**判据（任意深度） | ✅ P2 ② 已加（2～3 层 fixture） |
| 19 | 缺失验收 | 🔴 缺「产物层确实有判别力」的证明（`verify_hierarchical_rtl.sh` 对本改动**结构性失明**） | ✅ P2 ③ 已拆成两条 ⇒ ⇒ 「仓内 .v 的 `git diff --stat` == 0」**直证** ⇒ ⇒ ⇒ 明写「它全绿**不构成任何证据**」 |
| 20 | 缺失验收 | 🟡 缺下游行为变更的**登记落点** | ✅ **P4** 已加（CHANGELOG + `public-api-surface.md`「行为变更（非符号移除）」节 · 若该节不存在 ⇒ 如实登记） |
| 21 | 缺失验收 | 🟡 缺新 Requirement 的**违反样例** | ✅ P3 已写成**可违反**形态（把特判加回去 ⇒ ⇒ 违反） |
| 22 | 缺失验收 | 🟡 缺「残差与 sister 是同一条、不重复」的判据 | ✅ 残差登记段已加 |
| 23 | 缺失验收 | 🟡 缺 openspec 侧形态判据（`## ADDED Requirements`） | ✅ AC 已加 |
| 24 | 边界情况 | 🔴 用户把根真名取成 `"unnamed"` ⇒ ⇒ 修后「空名」与「真名 `unnamed`」在 `name()` 上**永久不可区分**（**代价原稿完全没登记**） | ✅ 已写进 §已知风险 + **P4 的 CHANGELOG MUST 明写这一条** ⇒ ⇒ ⇒ 彻底解决须另立 item（独立的「无名」标记）⇒ ⇒ **不在本项** |
| 25 | 边界情况 | 🟡 `ch_module<T>("")` 被静默接受（`include/module.h:35` 无 `CHREQUIRE`，而 `create_child` 有 `include/component.h:169`）⇒ ⇒ 它**不**触发本特判（父路径是 `top.unnamed`）⇒ ⇒ 但它是「哨兵合流」的**第二条入口** | ✅ 已写进 §不在本项（**独立 item** · scope creep · 会改公开行为）⇒ ⇒ ⇒ 且 A.1③① 已**修正**：它不是「特判从未命中」的证据（轴错了） |
| 26 | 边界情况 | 🟡 `Component` 默认实参路径**未测**（下游最常见写法） | ✅ A.1③④ 已列为**下游随时可达** ⇒ ⇒ P2 用例 MUST 覆盖它 |
| 27 | 边界情况 | 🟡 move-constructed 组件作为 parent（`name_` 为空）会命中 `empty()` 支 ⇒ ⇒ 「死支」不是可证死支 | ✅ A.1② 已写明 ⇒ ⇒ P1 已改口径（`empty()` 支**保留**） |
| 28 | 边界情况 | 🟡 测试里裸构造 unnamed 根**不走** `ConstructionFrame` ⇒ ⇒ 同 TEST_CASE 里后建的节点会被误归属 | ✅ P2 已写明：用例**必须**显式加 `ConstructionFrame`，或写进用例注释 |
| 29 | 边界情况 | 🟡 层次化产物对 unnamed 根也**不**逐字节不变（两条产物路径共用同一张全局表 ⇒ ⇒ 同时变） | ✅ Impact 表已改口径（「只在 root = `top` 时成立」）+ P2 ③ 拆两条 |
| 30 | 边界情况 | 🟢 端口局部名本身就是 `unnamed_input`/`unnamed_output` ⇒ ⇒ 修后出现 `unnamed_leaf_unnamed_input` ⇒ ⇒ 下游按 `leaf_` 前缀匹配的会断 | ✅ 已并入 P4 的下游登记（CHANGELOG） |
| 31 | 边界情况 | 🟢 同名兄弟去重按 **base name** 计数 ⇒ ⇒ 新增 `unnamed_` 前缀会改变 base 键 ⇒ ⇒ 「零影响」论证里**唯一没被枚举的一维** | ✅ A.4 已补该维度（仓内无无名根 ⇒ 不爆发） |

### ⚠️ metis 未能独立验证的项（如实登记）

- metis 的 `shell` 工具被权限拒绝 ⇒ ⇒ **未能实测构建与产物** ⇒ ⇒ 涉及「仓内零实例 / 产物零 diff」的结论**均以 `grep` / `read` 静态核对为准** ⇒ ⇒ ⇒ **本项的 AC 正是为了把这些静态结论变成可复跑的判据**（`NamedRootArtifactsUnchanged` 的 `git diff --stat` == 0）⇒ ⇒ ⇒ 而 `CTEST_TOTAL` 的数字 metis **未独立复核**（引用自根 `AGENTS.md` NOTES 的 CI 锚点）⇒ ⇒ ⇒ 由 planner 实测：`tests/CMakeLists.txt:1079` 确实注册了 `test_component_naming` ⇒ ⇒ ⇒ 落点成立 ✓
