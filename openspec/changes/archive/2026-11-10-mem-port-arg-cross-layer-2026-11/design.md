# Design: `(ii)` 的判定与产物形态（实现规格）

> 权威：ADR-066 **v1.2**（决策 ⑦ 边界 + 三桶 + 判别程序）· **v1.3**（决策 ⑧ 产物形态）·
> ADR-060 **v1.17 / v1.20**（§2.1 补格 + R2 第 5 行 + R1 防御性注记）。
> ⚠️ **v1.4 / v1.21**（2026-11-09 · **事实订正** · 决策零变更）= **实施前必读**
> （字面 (i) 的现状 = 已响亮拒绝 ⇒ ⇒ 无契约-实现缺口 ⇒ ⇒ 独立扫描的价值 = 独立于边结构 +
> 归因精确 + 三桶可判 ⇒ ⇒ `(ii)` 的落点网由既有机制自动合成 ⇒ ⇒ 本文件 §2 的登记**不需新代码**）。
> 本文件只给**实现落点**，不重述契约。

## 1. 现状（实测）

| 事实 | 位置 |
|---|---|
| 闸门是**单条件**、且先于父子关系判定 | `src/hierarchy_analysis.cpp:542-553`（`is_mem_node(u) \|\| is_mem_node(v)` ⇒ `CHFATAL_EXCEPTION`） |
| `is_mem_node()` **含** mem 读 / 写**端口**类型 | 同文件 `:43-47` |
| 节点 → **折叠后** module 归属表 | 同文件 `:263-268`（`mod_of[node] = owner==nullptr ? root : nearest_layer(owner, tree)`） |
| mem 端口有**公共基类** `mem_port_impl`（`parent()` / `addr()` / `enable()` / `has_*()`） | `include/ast/mem_port_impl.h:15-74` |
| 读端口的**数据侧**是 `data_output()` 代理（**也是**端口的 `src`）；写端口有 `wdata()` | 同文件 `:113` / `:151` |
| `en` 取不到名字时**静默回退** `1'b1` | `src/codegen_verilog.cpp` 的 `en_str` 初始化 + `if (enable && node_names_.count(enable))`（读 `:1294-1297` · 写 `:1368-1371`） |
| ⚠️ **2026-11-09 实测订正**：字面 (i) 的**现状 = 已响亮拒绝**（**不是**「闸门不可达」） | mem **端口**节点的 src 列表**含 mem 本体**（`include/ast/mem_port_impl.h:107` / `:145` 的 `add_src(parent)`）⇒ ⇒ 该边**跨 module** ⇒ ⇒ 边闸门**可达** · 改前二进制实测抛点 = `src/hierarchy_analysis.cpp:544` ⇒ ⇒ **无**契约-实现缺口（详见 ADR-066 **v1.4** §2.7） |

⇒ ⇒ **关键（⚠️ 2026-11-09 实测订正 · ADR-066 v1.4）**：字面 **(i)**（mem 本体在子层、父层读它）
**确实产生跨 module 边** —— mem **端口**节点的 src 列表**含 mem 本体**
（`include/ast/mem_port_impl.h:107` / `:145` 的 `add_src(parent)`）⇒ ⇒ **闸门对它可达**
⇒ ⇒ 改前二进制实测 = **已响亮拒绝**（抛点 `src/hierarchy_analysis.cpp:544`）⇒ ⇒ **无**契约-实现缺口。
⇒ ⇒ **但** (i) 的判定**仍然独立扫描** ⇒ ⇒ 理由 = 该判定的价值 = **独立于边结构**
（不依赖「mem 本体 → 端口」这条边继续存在）+ **归因精确**（点名持有 / 使用 module）
+ **三桶可判** ⇒ ⇒ **不是**「补一条缺失的拒绝」⇒ ⇒ **MUST 保留**（否则 ADR-066 判别程序 ①②③ 落空）。

## 2. 改动点（4 处）

### 2.1 新增 **归属比对扫描**（(i) 的判定）—— 放在连接表循环**之前**

```cpp
// ── (i) mem 本体跨 module ⇒ 响亮拒绝（ADR-066 决策 ⑦ v1.2 / 判别程序 ①②③）
for (const auto& owned : ctx->get_nodes()) {
    auto* port = dynamic_cast<mem_port_impl*>(owned.get());
    if (!port || port->parent() == nullptr) continue;
    auto it_a = mod_of.find(port->parent());   // ① 持有 module（mem 本体）
    auto it_b = mod_of.find(port);             // ② 使用 module（端口节点自身）
    if (it_a == mod_of.end() || it_b == mod_of.end()) continue;  // 防御：未登记 ⇒ 不误拒
    if (it_a->second != it_b->second) {
        CHFATAL_EXCEPTION(
            "hierarchical analysis: cross-module ch_mem access ('%s' owned by '%s' "
            "read by '%s' in '%s'). ADR-066 decision 7 (i): the ch_mem body and its user "
            "MUST live in the same module. Two legal remedies: (1) keep the ch_mem in the "
            "user's module and pass address/wdata down and rdata up through ports; "
            "(2) move the read/write ports into the module that owns the ch_mem.",
            port->parent()->name().c_str(), it_a->second->name().c_str(),
            port->name().c_str(), it_b->second->name().c_str());
    }
}
```

⚠️ **`mod_of` 用 `find` 而非 `at`**（未登记的节点 ⇒ 不误拒）。

### 2.2 闸门 → **三桶**（连接表循环内，替换 `:542-553`）

```cpp
if (is_mem_node(u) || is_mem_node(v)) {
    // (ii) 放行条件（ADR-066 决策 ⑦ v1.2 判别程序 ③ + ③' 相邻性）
    bool allow_ii = false;
    if (auto* port = dynamic_cast<mem_port_impl*>(v)) {
        const bool is_arg_slot =
            (u == port->addr()) || (u == port->enable()) ||
            (port->type() == mem_port_type::write &&
             u == static_cast<mem_write_port_impl*>(port)->wdata());
        const bool body_same_module = /* 2.1 已保证：否则早已抛 */ true;
        if (is_arg_slot && body_same_module) {
            // 相邻性：u 的 module 与端口 module 必须是**直接父子**
            Component* up = mod_of.at(u);
            Component* vp = mod_of.at(v);
            Component* vp_parent = (vp == &root) ? nullptr : nearest_layer(tree.parent.at(vp), tree);
            Component* up_parent = (up == &root) ? nullptr : nearest_layer(tree.parent.at(up), tree);
            allow_ii = (vp_parent == up) || (up_parent == vp);
        }
    }
    if (!allow_ii) {
        CHFATAL_EXCEPTION(/* 原消息 · 但 hint 修正为可行动的三条 */);
    }
    // allow_ii ⇒ 落到下面的常规跨 module 边处理（决策 ① 的落点网 / 或 (iii) 的相邻性拒绝）
}
```

⇒ ⇒ **不新造机制**：`(ii)` 放行后走**既有**「父侧消费子 `ch_out`」路径 ⇒ 决策 ① 的落点网 + 决策 ⑧ 的**登记义务**天然满足（⇒ 不再有 implicit wire）。
⇒ ⇒ **(iii)**（非相邻）⇒ `allow_ii=false` 的**另一条**路 ⇒ 由 `:563` 的既有相邻性规则拒绝（⇒ 与 ADR 一致）。

### 2.3 `en` 槽 **fail-closed**（`src/codegen_verilog.cpp` 两处）

```cpp
std::string en_str = "1'b1";                       // 合法缺省：enable == nullptr
if (enable != nullptr) {                           // ⚠️ 非 nullptr ⇒ MUST 有名
    auto it = node_names_.find(enable);
    if (it == node_names_.end()) {
        // fail-closed：MUST NOT 静默回退 1'b1（否则读侧恒使能 / 写侧恒写 ⇒ 值错且 lint 绿）
        CHERROR("hierarchical emission: mem port '%s' has an unnamed enable operand",
                node_names_.count(node) ? node_names_[node].c_str() : "<unnamed>");
        return;                                    // 与既有 print_* 的 fail-closed 风格一致
    }
    en_str = it->second;
}
```
⚠️ **两处**（`print_mem_read_port` · `print_mem_write_port`）⇒ 建议抽成一行的 helper（MUST 共用 ⇒ 免漂移）。
⚠️ `return` 的语义：`print_*` 内部静默 return ⇒ **不是**「产物错误」⇒ ⇒ 需与 §2.4 的**产物级**判据配合（AC4 的「名字 MUST 有 `logic` 声明」+ AC3 的值判据）；**理想** = 上层 `ok=false`（⇒ 见 §3 的开放点）。

### 2.4 注释同步（权威链首环）

`src/hierarchy_analysis.cpp:542` 的注释改为三桶表述（(i)/(ii)/(iii)）+ 指向 ADR-066 决策 ⑦/⑧。

## 3. 开放点（实施时 MUST 决定，**不允许**即兴）

1. **`en` fail-closed 的载体**：`print_*` 内 `CHERROR + return`（现状风格）vs **上层 `ok=false`**（ADR-058 §2.3 的错误模型）⇒ ⇒ 建议：**先**用 `CHERROR + return`（与既有 print_* 一致）**并**在 `HierarchyEmitResult` 侧加一条 `warnings` 级反馈 ⇒ ⇒ 若判定必须 `ok=false` ⇒ ⇒ 单独立项（改错误模型 = 跨文件）。
2. **`dynamic_cast` 成本**：每节点一次（小 ctx 可接受）；若担心 ⇒ 用 `type()` + `static_cast`（`mem_port_impl` 是**公共基类** ⇒ `static_cast` 安全）。
3. **`(ii)` 放行后是否还需 `parent_landing_wire` 的幂等键区分**：`(ii)` 与「父侧运算」共享同一子 `ch_out` ⇒ 决策 ⑤ 已要求**同一根网** ⇒ ⇒ 复用既有键（MUST NOT 新造）。

## 4. 风险与回滚

| 风险 | 缓解 |
|---|---|
| **(i)/(ii) 判反** ⇒ 真跨模块 mem 访问被静默放行 | AC1 负向用例（字面 (i) / (iii) MUST 仍红）+ AC2 判别程序 + AC7 变异 |
| **`wdata` 漏判** ⇒ 写侧错 | AC3 值判据**两侧** + AC4 |
| **`en` 回归**（改了读侧漏写侧） | 两处**共用 helper** + AC5 |
| **回滚** | 4 处改动**独立可回**（2.1 / 2.2 / 2.3 / 2.4 各自无依赖）⇒ ⇒ revert 任一不影响其余 |
