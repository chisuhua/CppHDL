# 审稿记录：`p6-hierarchy-friendliness-diagnosis-2026-11`（P6 · 层次友好度诊断 API）

- **日期**：2026-11-06
- **审查者**：critic（profile `build` · **公开 API 面**）
- **轮次**：**2 / 2**（预算用尽）
- **最终结论**：✅ **APPROVE** · `mergeable: yes` · `scope_exceeded: false`

## 第 1 轮 ⇒ `REJECT`（`mergeable: no`）

**2 BLOCKER · 3 SHOULD_FIX · 1 NIT**

### BLOCKER 1 — `synthesized_port_count` 对**非相邻隐式边**系统性多算

诊断把共享原语施加于**全部** `pending_implicit_edges`，但发射器对非相邻边在
**行 1 / 行 2 分支之前**就分流：

| 形态 | 发射器分支 | 结果 |
|---|---|---|
| `driver_is_anc`（第三轴：祖父驱孙层） | `src/hierarchy_verilog.cpp:442-483` | `continue` ⇒ **不**合成 `__if_`/`__x_` |
| `target_is_anc`（向上 pass-through） | `:486-574` | `continue` ⇒ 只产 `__up_` 链 |
| 兄弟边 | `:657` | `continue` |

⇒ ⇒ 非相邻边 `child_drives_parent` 恒 false，而共享判据对 false 边只要求「`to_ptr` 是
reg/proxy + 驱动端在父 module 内」⇒ ⇒ 第三轴边与上穿边**都满足** ⇒ ⇒ 诊断每条计 1，
发射器计 0 ⇒ ⇒ **违反 delta Requirement 3**。

⚠️ critic 还指出该错法直接**掏空 intent 的核心价值**（「1 个端口 vs N 跳链」的代价
可见性：对上穿设计用户看到 1，真实代价是 N 跳链）。

### BLOCKER 2 — `scan_cross_module_mem` 空指针解引用（SIGSEGV）

`nearest_layer` 对**不在组件树内**的 owner 返回 `nullptr` ⇒ 该值进 `mod_of` ⇒
一端为 `nullptr` 且任一端是 mem 节点时 `vm->second->name()` **解引用空指针** ⇒ ⇒
崩溃**严于**「诊断 MUST 返回报告」这条契约。

可达路径：前置校验①（「节点归属的组件不在 root 子树里」—— ADR-061 决策⑥ 的**真实
用户错误类**）抛 `runtime_error` ⇒ 被捕获 ⇒ **预扫描仍在同一 ctx 上跑** ⇒ 崩。

### SHOULD_FIX

| # | 内容 |
|---|---|
| SF1 | 文档写「报出**真实**计数」，而代码注释自承「可能偏小 ⇒ MUST 在文档说明『尽力而为』」⇒ **代码给自己立的义务未兑现**，且文档说的是**反话** |
| SF2 | 文档「≈ 代价」措辞**醒目度不足**（即便修好 B1，报告对「N 跳链」成本仍**没有任何数量**） |
| SF3 | `unconsumed_child_output_count` 的 refinement 分支**当前不可达**（`into_child==false` 的 connection 与 `edges` 条**成对**产生 ⇒ ① 必先匹配）⇒ ⇒ **注释承诺的防线是假的**，后人会以为有防线 |

### NIT

`implicit_input_synthesizable` 对 `child_drives_parent==true` 边**不做 null 检查**
（与行 1 分支不对称）⇒ 今天 pending 边 ptrs 恒非空 ⇒ 安全，但是**留给后人的脚枪**。

## 第 2 轮（**只验 BLOCKER 闭合**）⇒ `APPROVE`

### BLOCKER 1 闭合核验（critic 原文要点）

- 分析层两处 pending 入口：兄弟/非相邻边（`:575-584`）**不填** `child_instance_name`；
  直接父子边（`:607-633`）在 `parent_drives_child || child_drives_parent` 守卫下填写
  `:629` ⇒ ⇒ 与诊断路由门 `if (e.child_instance_name.empty()) continue;` 的**集合吻合** ✓
- 发射器侧逐分支核实：非直接父子的三个分支（第三轴 `:442-483` · 向上链 `:486-574` ·
  兄弟线网 `:657`）**全部** `continue` ⇒ 到不了 `:663` 之后的行 1/行 2 合成 ✓
- 「可合成性 + 宽度 + 去重 key」两侧**现在走同一份实现**
  （`src/hierarchy_synth_count.h` · 发射器 `:714/:721/:729` vs 诊断 `:884-891`）
  ⇒ ⇒ **「同源」从口头口径升级为共享代码** ✓（这正是 Requirement 3 要的形态）
- 第三轴链端口名 = 源节点 Verilog 名（`plan_pass_chain` 逐级同名）· `__up_` 在 `:512`
  ⇒ 均**不**落入 `^__(x|if)_` 计数谓词 ⇒ 与 delta「按前缀分别计数、不含链」及新文档自洽 ✓
- 判别力：`ThirdAxisTop` / `UpwardTop` 断言 `implicit_edge_count >= 1 &&
  synthesized_port_count == 0` 且与发射产物条数相等 ⇒ 源码层面与机制一致 ✓

### BLOCKER 2 闭合核验

- `nearest_layer` 确对树外 owner 返回 `nullptr`（`:91`）且该值会进 `mod_of`（`:134`）
  ⇒ ⇒ 崩溃路径**真实存在** ✓
- 守卫（`:157-161`）之后**无其它泄漏路径**（`:162` 比较 · `:169` 解引用都在守卫后）✓
- ⚠️ **判别力缺口（如实记录）**：该路径**无 DUT 覆盖** ⇒ ⇒ critic 判定：
  「守卫对 `nullptr` 路径是**无条件**覆盖（不是概率性修复）⇒ **不影响闭合结论**；
  缺口登记为已知即可」✓

### 结论

**无新 BLOCKER · 无新 SHOULD_FIX · 无新 NIT。**

## 处置汇总

| 项 | 处置 |
|---|---|
| **B1** | ✅ 加路由门 + 补 2 个 DUT（第三轴 / 上穿）⇒ ⇒ **M6 变异（去掉路由门）转红** ⇒ 判别力自证 |
| **B2** | ✅ 加 null 守卫 ⇒ ⚠️ **无 DUT**（构造需仓内无先例的手工 build）⇒ 如实登记 |
| **SF1** | ✅ 公开头注释 + 用户文档**都**改为「**尽力而为**（可能偏小）⇒ MUST NOT 当精确值」 |
| **SF2** | ✅ 文档补「**只覆盖直接父子隐式边** ⇒ 不含 `__up_` 与第三轴链 ⇒ ⇒ **链长不可由本报告推导**」 |
| **SF3** | ✅ 注释改为如实：「保留为兜底，但 MUST NOT 被当成『多实例防线』；真正兜住多实例的是『端口名全局唯一』」 |
| **N1** | ⚠️ **未采纳**（记 backlog）：pending 边 ptrs **恒非空**（`:571-572` / `:615-616`）⇒ 现状安全 ⇒ 不改以免扩大 diff |

## 返工后的全量门（重跑）

`ctest -LE perf` **241/241** · samples **39/39** · examples **28/28** ·
9 个护栏全 PASS · 计数锚点 252 / 76 / 66 / 289 · `test_hierarchy_friendliness_diagnosis`
**13 用例 / 58 断言** ✓
