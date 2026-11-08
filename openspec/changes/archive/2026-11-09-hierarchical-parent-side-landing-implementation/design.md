# Design: 父侧落点语义落地（ADR-066 决策 ①–⑥）

> 配套 `proposal.md`（A1–A24）· `tasks.md`（T0–T6）· `specs/rtl-hierarchical-output/spec.md`（MODIFIED ×1）。
> **权威决策 = ADR-066**（Accepted）⇒ ⇒ 本文件只描述**怎么改**，不复述**为什么**。

## 1. 实施顺序（TDD 5 步）

| 步 | 内容 | 交付 |
|---|---|---|
| **S0 基线** | 记录 ctest 计数 / 矩阵计数（16/17/6/**3**）/ `grep -c "node_names_\[" src/codegen_verilog.cpp`（= **62**，A8 面 ① 的**上界**） | tasks T0 |
| **S1 判据先行** | 矩阵四格**翻转期望**（`row2_expr × ch_out` ⇒ `Ok`；`× {ch_reg, ch_proxy, op}` ⇒ `Ok`）+ **删第四态** + 新增用例骨架 ⇒ ⇒ **确认红（且红在期望断言上）** | T1 |
| **S2 合成** | 父侧网合成 + 操作数名解析守卫 | T2 |
| **S3 契约** | spec 合并 + ADR-060 补格 + ADR-062 反向指针 | T3 |
| **S4 判别力** | 两个面各逐条变异（编译确认 → 重建 → 红 → 还原 → 重建 → 绿） | T4 |
| **S5 验证** | 全量重建 + ctest + 5 护栏 + 2 个 `--self-test` | T5 |

## 2. 改动点清单（**含 metis 纠正的取数面**）

### 2.1 `src/hierarchy_verilog.cpp`（合成与诊断）

| 位置 | 现状 | 改为 |
|---|---|---|
| `:761-776` 父侧端点检查 | 父 `input`/非 `output` ⇒ 一律 `synth_diags` | **按消费点性质分派**：(a) 父 `output` ⇒ 不造网（决策 ③）· (b) 父体内节点 ⇒ **造网 + 放行**（决策 ①）· (c) 父 `input` ⇒ **仍拒**（决策 ① 排除项） |
| `:640-653` 网登记 + `add_conn` | 仅供兄弟边（资格判定 `:605-615` 要求两个直接子模块） | **新路径 MUST 不复用**该资格判定；复用**网登记 + `add_conn` + 撞名检查（`:629-642`）**（决策 ①） |
| 网名 | 兄弟边用 `names.at(e.from_ptr)`（驱动端点全局名） | **`row2_expr` 同款** ⇒ 决策 ① 的命名规则 · ⚠️ **MUST NOT** 用 `names.at(e.to_ptr)`（`:697-698` 的自体名 ⇒ 自指 + 双驱动） |
| `:780-796` 诊断 | 共享文案含 `which this emitter cannot synthesize` | **新放行形态**若仍失败 ⇒ MUST NOT 复用该归因；**其它** unsupported 形态**保留**（它仍是真的）（A5 作用域） |

### 2.2 `src/codegen_verilog.cpp`（★ **真正的取数面** · metis M7）

| 位置 | 现状 | 改为 |
|---|---|---|
| `node_names_` 取址点（**62 行**，`grep -c` 上界） | 父侧表达式直接取 `node_names_[src]` | **父侧操作数名解析守卫**：把「子端点」解析为**父侧落点网名**（不是子内部信号名）⇒ ⇒ A8 面 ① |
| `:1266-1270` `print_mem_read_port` | `!node_names_.count(addr)` ⇒ **early-return**（静默不发射） | **父侧落点网名 MUST 进 `node_names_`** ⇒ 否则 mem 端口静默消失（A15 · ★ metis M1） |
| `:516-519` 网只发 `logic` 声明 | 不进命名表 | **同时登记进 `node_names_`**（A15 的关键） |

### 2.3 契约（A12a/b/c 三条独立可 grep 的证据）

| 目标 | 判据 |
|---|---|
| **A12a** spec MODIFIED | `bash scripts/verify_openspec_delta.sh` 通过 + MODIFIED 标题**逐字**匹配主 spec |
| **A12b** ADR-062 反向指针 | `grep` 命中 `→ ADR-066 决策 ③ 为唯一权威` |
| **A12c** ADR-060 补格 | `grep` 命中新行「子 → 父 · 父侧带运算」**且**第 2 行仍在（两条并存） |

## 3. 三个最危险的坑（metis 挖出 + 已核实）

| # | 坑 | 后果 | 抓手 |
|---|---|---|---|
| **P1** | `print_mem_read_port` 的 early-return（`codegen_verilog.cpp:1266-1270`）⇒ 落点网名不在 `node_names_` ⇒ 整条 mem 端口**不发射** | 产物**过 lint、读值恒 0** ⇒ 比坏产物更危险（A3 只看 `%Error`） | **A15**（判据 = **读值非 0**）+ `rv32i_pipeline.h:194-197` 为动机用例 |
| **P2** | 删第四态 `Broken` 后，**没有任何判据表达「`ok=true` 但产物不可编译」** | 未来同类缺陷**静默记成 `Ok`**（该测试文件**一次 verilator 都不调用**） | **A19**（四格产物进 lint 集合 + 护栏 `--self-test`） |
| **P3** | `:761-776` 是**兜底遍历**（所有实例连接）⇒ 单条件放行 = 放行**全部**非 output 父侧端点 | 拒绝面**萎缩**（表亲边 / 第三轴 / 跨多层 被顺手放行） | **A17**（13 个 `Reject` 格不变 + 跨多层/表亲边负向用例）+ **A18**（`Reject` 档首错） |

## 4. 验证矩阵（A → 判据）

| AC | 机械判据 |
|---|---|
| A1 | 矩阵计数 = `ok 20 / reject 16 / na 6 / 无 broken` · `cells.size()==42` |
| A2 | `module_text(parent)` 含 `logic [W] <global_name>;` + 实例语句含 `.child_port(<global_name>)`；可解析性**交 A3** |
| A3/A11 | **点名**：`row2_expr` 四格产物进 lint 集合（扩展 `tests/hier_rtl_products.cpp` 或护栏 lint 矩阵输出目录）⇒ 零 `%Error` |
| A4 | `ch::hier_eq::compare` ⇒ `equal` · 期望 trace **源码内字面量表** · 被观测值引到顶层 |
| A5 | 诊断含真正消费节点 + 可执行改法；**新放行形态**不含过时归因（其它形态该串仍在 ⇒ 反例 grep） |
| A7 | 产物无 `Duplicate pin connection`；同 `(实例, pin)` 相同网名幂等 |
| A8 | **语义点集合**（S0 探针实测产出）各一条变异 + 其余登记为无效变异（附证据）⇒ 全部转红 |
| A15 | mem 端口读值**非 0**（不是只 lint） |
| A17 | 13 个 `Reject` 格断言逐条不变 |
| A19 | 四格产物过 lint（`Broken` 的永久替代） |
| A20 | `verify_arch_ledger_residuals.sh` + `--self-test` 转绿（行 16 → 已裁决 · 行 19 → 已收口） |

## 5. 不做（ADR-066 决策 ⑥）

跨多层（孙组件）· 跨模块 `ch_mem`（仍拒）· 宽度语义（沿用 W1）· `PortPolicy` 三档分化（行 15 独立）·
行 17 的叙事文字（归因重写随本 change 的 A16 只做**测试/断言**部分）。
---

## 6. ★ S2 的实现方案（2026-11-08 实测确定 · **代码未落地**）

### 6.1 族 ① 的实际产物结构（`probe_row2expr` · `ForceLayer`）

```verilog
module top (...);
    logic [7:0] top_or_or;                        // 父侧 op 节点**已**声明
    assign top_or_or = top_leaf__reg | 8'h1;      // ❌ RHS 引用**子 module 内部名**
    mat_leaf leaf (
        .top_leaf_unnamed_input(top_unnamed_input),
        .__x_0(top_or_or),                        // ❌ 子侧合成端口**驱动父侧 op 节点**（双驱动）
        .top_leaf_unnamed_output()
    );
endmodule
```

⇒ ⇒ **两个 bug**（不是一个）：
- **B1**：`assign` 的 RHS 引用**子 internal 名** `top_leaf__reg` ⇒ 父作用域无此声明 ⇒ Verilator `Can't find definition of variable`
- **B2**：`.__x_0(top_or_or)` 把子侧合成端口接到**父侧 op 节点名** ⇒ 该节点**已有自己的 `assign`** ⇒ **双驱动**

### 6.2 ★ 关键洞察：**修复不需要改 `codegen_verilog.cpp` 的 62 个取数点**

因为 ADR-066 决策 ① 规定「落点网名 = **驱动端点的 ctx 全局名**」
⇒ ⇒ 而父侧表达式的 RHS **本来就是**那个名字（`top_leaf__reg`）
⇒ ⇒ **只要在父 module 里把该名字声明成一根 `logic` 网，RHS 就自动可解析** ✅

⇒ ⇒ 故改动**收敛到 `src/hierarchy_verilog.cpp` 一处分支**：

| 改动 | 内容 |
|---|---|
| **C1** 父侧检查分派 | `child_drives_parent` 分支按**父侧端点性质**分派：(a) 父 `output` ⇒ 既有直通行为；(b) 父体内节点 ⇒ **合成父侧网**；(c) 父 `input` ⇒ **仍拒**（决策 ① 排除项） |
| **C2** 网名与声明 | 网名 = `names.at(e.from_ptr)`（**端点全局名**）⇒ `synth_wires[parent_key].push_back(...)` ⇒ 发射时自动发 `logic [W] <name>;` |
| **C3** 连接实参 | `add_conn(parent_key, inst, <__x_n>, wire, into_child=false)` ⇒ ⇒ **MUST NOT** 用 `names.at(e.to_ptr)`（那正是 B2） |
| **C4** 撞名 | 复用 `clashes_with_port` 检查（A24：撞名 MUST 报**自己的**诊断，不复用 sibling wire 文案） |
| **C5** 诊断跳过 | 新增 `std::set<std::string> synth_ok_parent_eps`（key = `parent_key + "\x1f" + inst + "\x1f" + names.at(e.to_ptr)`）⇒ 诊断阶段跳过 ⇒ ⇒ **A17「拒绝面不萎缩」的抓手**（P3） |

⇒ ⇒ **`src/codegen_verilog.cpp` 的改动面从「62 个取数点」降为「0（若 6.2 成立）」**
⇒ ⇒ ⚠️ **但 A15（`ch_mem` 端口）仍可能需改**：`print_mem_read_port` 的 `node_names_.count(addr)`
判据 ⇒ 若落点网名**未进 `node_names_`** ⇒ 该 mem 端口仍 early-return ⇒ ⇒ A15 的验证是**唯一判据**。

### 6.3 本轮未完成（**代码零改动** · 已回滚）

| 项 | 状态 | 原因 |
|---|---|---|
| C1–C5 的代码落地 | ❌ **未落地** | 两次行块定位编辑均误（697..702 不是目标块）⇒ **编译失败** ⇒ 已 `cp` 回滚 ⇒ `git diff -- src include` = **空** ✅ |
| 单元/集成验证 | ❌ | 依赖 C1–C5 |

⇒ ⇒ **S1 的测试改动保留在工作区**（`tests/test_hierarchy_verilog.cpp`，`:2439` 单点红 = 决策 ① 未实现）
⇒ ⇒ **下一轮**：按 C1–C5 落地 ⇒ `:2439` 转绿 ⇒ 再推进 A2–A24。

⚠️ **实施提示（避免重犯）**：`src/hierarchy_verilog.cpp` 的 `child_drives_parent` 分支
**结尾块**是「行 2 合成 + 连接 + `continue`」，其 `{ std::string err; ... }` 块**与行 1 分支的同名块同形** ⇒
⇒ 行号/短锚点定位**不可靠** ⇒ ⇒ MUST 用**唯一上下文**（如 `names.at(e.to_ptr), /*into_child=*/false, err)) {`）定位。
---

## 7. ★ S2 排查结果（2026-11-08 · 第二次尝试 ⇒ **代码已回滚 · 零改动**）

### 7.1 已确认可靠的三件事

| # | 事实 | 证据 |
|---|---|---|
| ① | `module_port_of(mk, node, is_input)` **按节点指针**匹配（`p.node == n`）⇒ ⇒ 对 op 节点可靠返回 `false` | `src/hierarchy_verilog.cpp:259-273` |
| ② | C++ 端用 `"\x1f"` 拼 key 是**正确**的（终端显示 `topleaftop_gt_gt` 只是 0x1F 不可见） | 调试输出（已删） |
| ③ | **`inst.connections` 与 `analysis.edges` 不是同一套记录** ⇒ ⇒ 诊断遍历的 `inst.connections` 里**存在没有对应 `e` 的连接** | 见 §7.2 |

### 7.2 ★ 新发现：**修复点不止一处**

第一次实现（C1–C5）只在 `for (e : analysis.edges)` 的 `child_drives_parent` 分支里合成父侧网
⇒ ⇒ 实测结果：

```
[DBG] diag key='top\x1fleaf\x1ftop_gt_gt'     hit=0
[DBG] diag key='top\x1fleaf\x1ftop_mux_select' hit=0
（且没有任何 [DBG] insert 输出 ⇒ 合成分支的 insert 一次都没执行）
```

⇒ ⇒ 解读：
- 诊断报的 `top_gt_gt` / `top_mux_select`（父侧 **op 节点**）**没有对应的 `e` 进入我的新分支**
  ⇒ ⇒ 它们只存在于 `inst.connections`（`into_child==false`）；
- 而我的分支**确实被执行过**（对另一条边：`e.to_ptr` = 父 `output` 端口 ⇒ `ep_is_output=true`）⇒ 但那条边不需造网。

⇒ ⇒ **结论**：`row2_expr` 形态的边**有两条来源**：
1. `analysis.edges`（合成阶段处理）— 可能只记录「子 `ch_out` 端口 → 父侧端点」的**主边**
2. `inst.connections`（诊断阶段遍历）— 额外记录了 **`select` 展开的派生节点边**
   （`top_gt_gt` = 比较结果 · `top_mux_select` = mux 选择信号）

⇒ ⇒ ⇒ **修复 MUST 同时覆盖两套记录**（否则：合成一侧造网/接线，诊断一侧仍报；或反之 ⇒ 产物与诊断不一致）。

### 7.3 下一轮的正确实施顺序（建议）

| 步 | 内容 | 判据 |
|---|---|---|
| **P1** | 先**只用诊断旁路**（对 `inst.connections` 里「父侧端点是父 module 体内节点」的连接不报），**不合成** ⇒ 跑 `[matrix]` | 应变成**绿**但**产物坏** ⇒ ⇒ ⚠️ **这一步只为定位**，MUST NOT 提交 |
| **P2** | 在 P1 的产物上跑 lint ⇒ 确认坏在哪（B1/B2） | 定位真实需要的合成点 |
| **P3** | 按 §6.2 的 C1–C5 合成，并**为 `inst.connections` 的派生节点补建对应关系**（关键未解之处：派生节点 `top_gt_gt` 的父侧落点网应是**它的操作数**对应的网 ⇒ 需确认是否同一根） | `[matrix]` 绿 + lint 干净 + A2/A7 |
| **P4** | A15（`ch_mem` 端口）单独验证 | 读值非 0 |

⚠️ **不得跳过 P1/P2**：直接凭 static reading 猜「哪些边需要造网」已被本轮证明不可靠。

### 7.4 本轮交付

| 项 | 状态 |
|---|---|
| C1–C5 代码 | ❌ **已回滚** ⇒ `git diff -- src include` = **空** ✅（不留「放行但产物坏」的中间态） |
| 排查结论 | ✅ 已写入本文件 §7.1–7.3（**下一轮的直接输入**） |
| S1 测试改动 | ✅ 保留（`:2439` 单点红 = 决策 ① 未实现） |

---

## 8. spec delta 的头部说明（**从 delta 文件移来** · CLI 归档不携带首个 `## ` 之前的文本）

> 修订单条 Requirement（标题与主 spec **逐字一致**）⇒ ⇒ 只改两处：
> ① 其「父层消费子输出 MUST 有连接网」Scenario ⇒ **收窄**为「消费点**不是**父 `output` 端口时」；
> ② **新增** Scenario「父侧带运算 MUST 合成父侧网」+「父侧运算消费子非端口端点 MUST 读父侧落点网名」。
> 其余 Scenario **逐字保留**（MODIFIED 语义 = 整条 Requirement 被替换 ⇒ 必须完整列出）。

⚠️ **CLI 约束（实测 · 2026-11-09）**：MODIFIED Requirement MUST **逐字保留**主 spec 里该
Requirement 的**全部既有 Scenario 标题** ⇒ ⇒ 收窄后的 Scenario **标题不变**（「父层消费子输出
MUST 有连接网」）⇒ ⇒ **收窄写在 Scenario 正文首行**（⚠️ 改标题会被 archive 判为「omits scenario」）。
