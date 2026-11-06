# 任务清单：`p6-hierarchy-friendliness-diagnosis-2026-11`

> 判据来源：proposal.md §Acceptance（摘要）· capability delta
> `hierarchy-friendliness-diagnosis`（**ADDED ×3**）· improvement 草稿 v3
> `.rddf/improvements/p6-hierarchy-friendliness-diagnosis-2026-11.md` §3（19 条）
> ⚠️ **新增公开 API 面**（`include/hierarchy_analysis.h`）⇒ ⇒ 归档前须过 **critic**。

## T1 与发射器**同源**（共享内部头）

- [x] 1.1 新增 `src/hierarchy_synth_count.h`（★ **内部头** ⇒ 不进 `include/`
      ⇒ **公开 API 面零扩张**；★ 仓内同款先例：`src/hierarchy_chain.h` 等 4 个）
      ⇒ ⇒ 提供两个**共享原语**：
      ① `synth_port_key(edge)` = 隐式边的**合成端口去重 key** = (子侧 module key, 子侧节点)
      ② `implicit_input_synthesizable(edge, analysis)` = 行 1（父驱子）的**可合成性**判定
      ③ ★ **追加** `synth_width_ok(desc, from, to, err)`（★ 发射器原本是**内联 lambda**
      ⇒ ⇒ 诊断无法复用 ⇒ ⇒ 「端口数相等」在**宽度不一致**的边上就不成立
      ⇒ ⇒ 搬进共享头 ⇒ ⇒ 消息措辞**逐字保留** ⇒ 行为不变）
      ⇒ ⇒ **已落**（`src/hierarchy_synth_count.h` · 3 个原语）
- [x] 1.2 `src/hierarchy_verilog.cpp` 的行 2（`:683-693`）与行 1（`:710-741`）
      改为**调用共享原语** ⇒ ⇒ **行为逐字不变**
      ⇒ ⇒ 判据 = 既有 3 个产物（`bubble.v` / `row1_implicit.v` / `three_layers.v`）
      **逐字节相同**（对照 HEAD 的 `cmp` = 0 · sha256 + 重建 + 复验）
      ⇒ ⇒ **已验**：`bubble.v` / `row1_implicit.v` / `three_layers.v` **3/3 逐字节相同**
      ⇒ ⇒ `DIFF_COUNT = 0` · 还原后 sha256 相同 + **全量重建** + 护栏 PASS
- [x] 1.3 共享原语 MUST 被**两侧**调用（★ 发射器 + 诊断）⇒ ⇒ grep 双向可查
      ⇒ ⇒ **已验**：`src/hierarchy_verilog.cpp` 与 `src/hierarchy_analysis.cpp` **都**调用这 3 个原语

## T2 公开 API（`include/hierarchy_analysis.h`）

- [x] 2.1 `enum class HierarchyFriendliness { kClean, kNeedsWork, kNotAnalyzable }`
      （★ **3 档** ⇒ 5 大项里 4 项是**正交事实** ⇒ 5 档是伪全序）
- [x] 2.2 `struct HierarchyModuleSize`（★ 三个数**分开给** ⇒ pass-through 层
      body=0 但端口很多 ⇒ 合并成单值必误导）
- [x] 2.3 `struct HierarchyDiagnosisDetail`（★ **value-only** ⇒ 不带 `lnodeimpl*`）
      ⇒ ⇒ `node` 字段 MUST 是 **Verilog 名**（★ 与产物**同一套**名字 ⇒ 用户能直接在
      产物里定位；⚠️ `lnodeimpl::name()` 是 `.` 分隔的**组件路径** ⇒ 搜不到产物）
- [x] 2.4 `struct HierarchyDiagnosis`（★ **value-only**）⇒ ⇒ 5 大项**全部**
      `std::optional` ⇒ ⇒ 「未测得」与「实测为 0」**结构上可区分**
      ⇒ ⇒ 含 `to_text()` 声明
- [x] 2.5 `HierarchyDiagnosis diagnose_hierarchy(Component& root);` 声明
- [x] 2.6 `#include <optional>` ⇒ ⇒ 头文件**自包含**（`verify_header_self_contained.sh` 判据）
      ⇒ ⇒ **已验**：`verify_header_self_contained.sh` PASS

## T3 实现（`src/hierarchy_analysis.cpp`）

- [x] 3.1 内部 MUST 用 `ImplicitPortHandling::Record`（★ 默认 `Reject` ⇒ 遇到第一条
      隐式边就抛 ⇒ 诊断退化为「不可分析」⇒ 与 §1.1 的痛点同构）
- [x] 3.2 **只捕 `std::runtime_error`** ⇒ ⇒ `out_of_range` / `bad_alloc` **重抛**
      （★ 框架 bug 不被伪装成「你的设计不可分析」）
- [x] 3.3 `implicit_edge_count` = `pending_implicit_edges.size()`（★ 精确）
- [x] 3.4 `synthesized_port_count` = 隐式边按**共享 key**去重后的数量
      （★ 用 T1.1 的共享原语 ⇒ 与发射器**必然相等**）
- [x] 3.5 `dangling_input_count` = `bubbled_inputs` 中**跨 module** 的部分
- [x] 3.6 `unconsumed_child_output_count` = §2.1 的**并集推导** + 四条排除规则
      + **按实例**判（★ MUST NOT 重遍历 IR）
- [x] 3.7 `cross_module_mem_count`：正常路径 = `0`；不可分析路径 = **独立预扫描**的
      真实计数（★ A1）⇒ ⇒ 预扫描按**组件归属**（`Component::node_owner()`）判定 ⇒ ⇒
      ⚠️ **口径**（inline 折叠后可能同层）MUST 写进文档 + 明细标注
- [x] 3.8 `verdict`：`kNotAnalyzable`（分析失败）· `kNeedsWork`（任一计数 > 0）·
      `kClean`
- [x] 3.9 `to_text()`：**确定性**排序（明细按 `(category, module, node, reason)`
      字典序 · `module_sizes` 按 module 字典序）⇒ ⇒ 两次调用**逐字节相同**

## T4 测试（`tests/test_hierarchy_friendliness_diagnosis.cpp` · 新增）

- [x] 4.1 **项 1b 与发射器对得上**：同一 ctx 下 `synthesized_port_count` ==
      emit 后 `port_provenance` 里 `origin == Synthesized` **且** 端口名匹配
      `^__(x|if)_[0-9]+$` 的条数（★ 按前缀**分别计数** ⇒ 排除 `__up_`）
- [x] 4.2 **边数 ≠ 端口数** 的 DUT（一个子内节点驱动两个父侧端点 ⇒ 边 2 / 端口 1）
      ⇒ ⇒ 这条才真正证明单位没搞错
- [x] 4.3 **项 3 三条消费路径**：直接父子消费（不计）· 隐式边消费（不计）·
      兄弟边消费（不计）+ 真未消费（计）
- [x] 4.4 **多实例**：两实例只有一个消费该输出 ⇒ 计数 MUST **按实例**判
- [x] 4.5 **`details` 名称可区分**：**两个**悬空 `ch_in`（★ **必须两个** ⇒ 只用一个时
      「互不相同」的比较循环**空转** ⇒ ⇒ 恒绿 ⇒ ⇒ 假绿）⇒ 两条明细的节点名**互不相同**
- [x] 4.6 **项 2 / 项 3 / 项 5** 各有专门 DUT 取到**非零值**
      （★ 项 5 用「三个数分别正确」当判据 ⇒ 不能用「非零」）
- [x] 4.7 **`kNotAnalyzable`**：构造会抛的设计 ⇒ MUST 返回报告（**不抛**）·
      4 项 `nullopt` ⇒ ⇒ 与「真实为 0」**可区分** ⇒ ⇒ 报告 MUST NOT 全是 0
- [x] 4.8 **跨模块 mem 的 A1 路径**：不可分析时 MUST 报出**真实**计数（**非** `nullopt`）
- [x] 4.9 **异常不吞框架 bug**：注入非 `runtime_error` ⇒ MUST **重抛**
- [x] 4.10 **只读（加强版）**：全节点 `(num_srcs, src(i)…)` 快照前后相同 +
      前后两次 `analyze_hierarchy().to_text()` 相同
- [x] 4.11 **确定性**：同一 DUT 连续两次诊断 ⇒ `to_text()` **逐字节相同**（多模块 DUT 上验）
- [x] 4.12 **粒度相关性**：同一 DUT 在 `ForceInline` 与 `ForceLayer` 下
      `cross_module_mem_count` **不同** ⇒ ⇒ 证明它是粒度相关量而非结构常量

## T5 判别力自证（变异对照 · 每条先确认仍能编译）

- [x] 5.1 变异 ① 删检测（`synthesized_port_count` 恒 0）⇒ 转红
      ⇒ ⇒ **实测转红**（`46 passed / 2 failed`）
- [x] 5.2 变异 ② 项 3 只查 `edges`（**不并** `pending_implicit_edges`）⇒ 转红
      ⇒ ⇒ ⚠️ **实测仍绿 ⇒ 无效变异**（★ **不是**判据缺口 —— 理由见「实施期发现」⑥）
- [x] 5.3 变异 ③ 分档改错（`kNeedsWork` 恒不出现）⇒ 转红
      ⇒ ⇒ **实测转红**（`44 passed / 1 failed`）
- [x] 5.4 变异 ④ `Record` → `Reject` ⇒ 转红
      ⇒ ⇒ **实测转红**（`33 passed / 3 failed`）
- [x] 5.5 变异 ⑤ `details` **只取局部名**（★ 不是 `node->name()` 整体 —— 那个是
      **组件路径 + 局部名** ⇒ **本就**可区分 ⇒ ⇒ 用它当变异是**无效变异**）⇒ 转红
      ⇒ ⇒ **实测转红**（`46 passed / 2 failed`）
- [x] 5.6 每条变异**还原后** MUST **逐字节比对 + 重建 + 复绿**
      ⇒ ⇒ **已验**：`src/hierarchy_analysis.cpp` sha256 相同 + 重建 + 复绿
      ⇒ ⇒ ⚠️ **变异脚本 MUST 检查 python 退出码**（★ 否则断言失败会**静默跳过**变异
      ⇒ ⇒ 误判「无判别力」⇒ ⇒ 本轮踩过 ⇒ 已加 `|| { ...; exit 1; }`
      （★ 还原源码 ≠ 复原 ⇒ 二进制也是状态）

## T6 零回归 · 文档 · 锚点

- [x] 6.1 **零回归**：`ctest -LE perf` 全绿 · samples · examples · 各护栏 PASS
- [x] 6.2 **既有产物逐字节不变**（T1.2）⇒ ⇒ `verify_hierarchical_rtl.sh` R1–R8 全绿
- [x] 6.3 **计数锚点**：`CTEST_TOTAL` 251 → **252** · non-perf 240 → **241** ·
      `tests/test_*.cpp` 143 → **144**
      ⇒ ⇒ **已验**：`verify_test_counts.sh` PASS（252 = 252 · 76 = 76）
      （★ `VERILATOR_E2E_TOTAL` **不变** ⇒ 新测试不跑真 verilator）
- [x] 6.4 **文档**：用法指南新增「§5.4-bis 层次友好度**诊断 API**」小节 ⇒ ⇒ **显式 include**
      （★ `hierarchy_analysis.h` **不在** `ch.hpp` 聚合器里）⇒ ⇒ 5 项含义 + 3 档处置
      + **粒度相关性** + **不覆盖**「体内无驱动的子输出」+ **静态析构期** +
      **FATAL 日志副作用** ⇒ ⇒ 示例 MUST 实际编译通过
- [x] 6.5 `verify_header_self_contained.sh` PASS（★ 新增 `<optional>` include）

## 审查（critic · 2 轮 · 公开 API 面）

### 第 1 轮 ⇒ **REJECT**（`mergeable: no`）· 2 BLOCKER + 3 SHOULD_FIX + 1 NIT

| # | 类型 | 内容 | 处置 |
|---|---|---|---|
| **B1** | BLOCKER | `synthesized_port_count` 对**非相邻隐式边**系统性**多算**：诊断把共享原语施加于**全部** `pending_implicit_edges`，但发射器对非相邻边在行 1/行 2 之前就分流（第三轴链 / 向上 pass-through 链 ⇒ 都 `continue` ⇒ **不**合成 `__if_`/`__x_`）⇒ ⇒ 违反 delta Req 3 | ✅ **已闭合**：诊断侧加**路由门** `if (e.child_instance_name.empty()) continue;`（★ 依据：分析层只在**直接父子**路径填实例名 ⇒ ⇒ 非空 ⇔ 会走到行 1/行 2）⇒ ⇒ 补 DUT `ThirdAxisTop`（祖父驱孙层 reg）+ `UpwardTop`（顶层读孙层 reg）⇒ 断言 `implicit_edge_count >= 1` **且** `synthesized_port_count == 0` 且与发射产物**相等**⇒ ⇒ 变异 **M6**（去掉路由门）⇒ **转红** ✓ |
| **B2** | BLOCKER | `scan_cross_module_mem` 在「MUST NOT 抛异常」的返回路径上可对**空指针**解引用（SIGSEGV）：`nearest_layer` 对**树外 owner** 返回 `nullptr` ⇒ ⇒ 原实现 `vm->second->name()` 直接解引用 | ✅ **已闭合**：加 `um->second == nullptr \|\| vm->second == nullptr` 守卫 ⇒ continue ⇒ ⇒ **无条件**覆盖该路径（不是概率性修复）⇒ ⚠️ **如实登记：该路径无 DUT 覆盖**（构造「组件在 root 子树外 + 节点同 ctx」需仓内无先例的手工 build）⇒ 只由**代码审查**验证（critic 判定不影响闭合） |
| SF1 | SHOULD_FIX | 文档写「报出**真实**计数」，而代码注释自承「可能偏小 ⇒ MUST 在文档说明『尽力而为』」⇒ 代码给自己立的义务未兑现 | ✅ 公开头 `cross_module_mem_count` 注释 + 用户文档**都**改为「**尽力而为**（可能偏小）⇒ MUST NOT 当精确值」 |
| SF2 | SHOULD_FIX | 文档「≈ 代价」措辞醒目度不足（即便修好 B1，报告对「N 跳链」成本仍**没有数量**） | ✅ 文档补「**只覆盖直接父子隐式边**（`__if_`/`__x_`）⇒ 不含 `__up_` 与第三轴链 ⇒ ⇒ **链长不可由本报告推导**」 |
| SF3 | SHOULD_FIX | `unconsumed_child_output_count` 的 refinement 分支**当前不可达**（`into_child==false` 的 connection 与 `edges` 条**成对**产生 ⇒ ① 必先匹配）⇒ 注释承诺的防线是**假的** | ✅ 注释改为如实：「本分支**保留为兜底**，但 MUST NOT 被当成「多实例防线」；真正兜住多实例的是『**端口名全局唯一**』」 |
| N1 | NIT | `implicit_input_synthesizable` 对 `child_drives_parent==true` 边不做 null 检查（与行 1 不对称） | ⚠️ **未采纳**（记 backlog）：分析层给 pending 边填的 ptrs **恒非空**（`:571-572` / `:615-616`）⇒ 现状安全 ⇒ 不改以免扩大本轮 diff |

### 第 2 轮（**只验 BLOCKER 闭合**）⇒ ✅ **APPROVE**（`mergeable: yes`）

- B1 闭合核验：分析层两处 pending 入口（兄弟边**不填**实例名 / 直接父子边**必填**）与诊断路由门**集合吻合**；发射器侧三个非直接父子分支**全部** `continue` ⇒ 到不了行 1/行 2 ⇒ 与被排除集合吻合；且「可合成性 + 宽度 + 去重 key」两侧**现在走同一份实现** ⇒ 「同源」从**口头口径**升级为**共享代码** ✓
- B2 闭合核验：`nearest_layer` 确对树外 owner 返回 `nullptr`（`:91`）且该值会进 `mod_of`（`:134`）⇒ 崩溃路径真实；守卫之后**无其它泄漏路径**（`:162` / `:169` 都在守卫后）⇒ ⇒ 「对 `nullptr` 路径是**无条件**覆盖 ⇒ 不影响闭合」⇒ ⇒ 判别力缺口**登记为已知**即可
- 无新 BLOCKER · 无新 SHOULD_FIX · 无新 NIT

### 返工后的全量门（重跑）

`ctest -LE perf` **241/241**（返工后重跑）· samples **39/39** · examples **28/28** ·
9 个护栏全 PASS（含 `verify_hierarchical_rtl.sh` R1–R8 与 R1.2 的「3 产物逐字节不变」）·
计数锚点 252 / 76 / 66 / 289

---

## 实施期发现（如实登记 · 7 条）

| # | 发现 | 处置 |
|---|---|---|
| ① | ⚠️ **「`node->name()` 恒为 `unnamed_input`」是错的** —— 那是 **`port::name()`**（IO 访问器）的行为；**`lnodeimpl::name()`**（IR 节点）= **父组件名 + "." + 创建时的名字**（`src/core/lnodeimpl.cpp:32`）⇒ ⇒ 两个不同组件里的 input **本就不同名** ⇒ ⇒ 「明细不可区分」的担忧**不成立** | ✅ 订正 proposal 附录 A.3 · design §2.2 · tasks T2.3 · **improvement 草稿追加订正注记**（★ 只增不改）⇒ ⇒ 「用 Verilog 名」的理由改为「**与产物同一套名字** ⇒ 用户能在产物里定位」；变异 ⑤ 改为「**只取局部名**」⇒ ⇒ 转红 ✓ |
| ② | ⚠️ **DUT 构造错 3 次**才拿到「边 2 / 端口 1」：写两次同一个 proxy ⇒ ⇒ **last-wins 覆盖** ⇒ ⇒ 只剩**一条**边；改用两个 `ch_uint` 局部变量 ⇒ ⇒ 未被消费 ⇒ ⇒ **被优化掉**；最终用**两个不同的父侧 proxy**（`dout` / `dout2`）⇒ ⇒ 两条边 | ✅ DUT 已改正（`TwoTargetTop`）· 判据现**实测转红**（变异 M1） |
| ③ | ⚠️ **「跨 module 边界」口径** ⇒ 顶层**直接子**的悬空 `ch_in` **不算**（折叠后 owner = 根）⇒ ⇒ 「两个不同名的悬空输入」在**顶层直接子**形态下**不可达** | ✅ DUT 改为**两个中间层各带一个悬空叶子**（`TwoDanglingMid` × 2）⇒ ⇒ 两条跨层明细 |
| ④ | ⚠️ Catch2 的 `REQUIRE` **不支持 `||`** ⇒ ⇒ 编译期报「chained comparisons」 | ✅ 先算布尔再断言（★ **AGENTS.md 已登记过这条** ⇒ 本轮又犯 ⇒ 记此） |
| ⑤ | ⚠️ **变异脚本缺 `set -e` 语义** ⇒ ⇒ python 断言失败会**静默跳过变异** ⇒ ⇒ 误判成「判据无判别力」 | ✅ `mutate()` 加退出码检查（`|| { ...; exit 1; }`）⇒ ⇒ 并**手工核实**过替换确实生效 |
| ⑥ | ⚠️ **变异 ② 是无效变异**（实测仍绿）：`pending_implicit_edges` 的 `from_node` 恒是**子内部节点**（**不是**端口）⇒ ⇒ 它不出现在 ports 列表里 ⇒ ⇒ 「并集」那条规则在**当前 DUT 集**下**不可达** ⇒ ⇒ **不是**判据缺口 | ✅ **如实登记为无效变异**（★ 不改变可观测行为）⇒ ⇒ 规则**保留**（防御性）⇒ ⇒ 「兄弟边消费」DUT 记 backlog |
| ⑦ | ⚠️ **发射器的 `width_check` 是内联 lambda** ⇒ ⇒ 诊断无法复用 ⇒ ⇒ 「端口数相等」在**宽度不一致**的边上就不成立 | ✅ 搬进共享头 `synth_width_ok`（★ 消息措辞**逐字保留** ⇒ 行为不变）⇒ ⇒ 3 个产物**逐字节不变**已复验 |

---

## 验证命令

```bash
cd /workspace/project/CppHDL
set -euo pipefail
cmake --build build -j"$(nproc)"
( cd build && ctest -LE perf --output-on-failure )
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
bash scripts/verify_hierarchical_rtl.sh
bash scripts/verify_header_self_contained.sh
bash scripts/verify_test_counts.sh
openspec validate p6-hierarchy-friendliness-diagnosis-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
