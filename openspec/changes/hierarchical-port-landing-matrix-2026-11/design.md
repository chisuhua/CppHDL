# Design: 父侧落点形态矩阵的轴缺口补齐

> 配套 `proposal.md`（Why / What Changes / Impact / 验收标准）。
> **实施期可变的部分明确标注为「实测填入」** ⇒ ⇒ 不得凭推理写死矩阵格值。

## 1. 既有矩阵的结构（本 change 的扩展对象）

### 1.1 结构（`tests/test_hierarchy_verilog.cpp:2073` 起）

```cpp
enum class State { Ok, Reject, Na };
struct Cell { const char *site; const char *type; State state; const char *evidence; };
```

- **轴 A（site）= 跨层拓扑**：`row1`（父→子·直接）/ `row2`（子→父·直接）/ `third_axis`（父→非直接子）/
  `upward`（子→非直接父）/ `sibling`（兄弟边）/ `bubble`（冒泡）；
- **轴 B（type）= 子侧端点节点类型**：`ch_in` / `ch_out` / `ch_reg` / `ch_proxy` / `op` / `mem`；
- **三态**：`Ok` 正确处理 · `Reject` 响亮拒绝 · `Na` 结构性无此形态（**MUST 写理由**）；
- **上界显式**：`REQUIRE(cells.size() == 36)` + `REQUIRE(n_ok == 16)` + `REQUIRE(n_reject == 15)` +
  `REQUIRE(n_na == 5)` ⇒ ⇒ 「补全」**可判定**；
- **evidence tag 校验**：`Ok`/`Reject` 格的 `evidence` MUST 是**源文件里真实存在的 tag**
  （`src.find(c.evidence) != npos`）⇒ ⇒ 删掉对应测试会在这里转红 ⇒ ⇒ **矩阵不会静默缩水**。

### 1.2 既有覆盖（metis 第 1 轮核查 + architect 独立复核 ⇒ 两方一致）

| 族 | 既有覆盖 | 出处 | 备注 |
|---|---|---|---|
| ③（父表达式含子 output 且含运算） | ✅ 真实 DUT `ImmConsistencyDut` + `ch::hier_eq::emit(..., ForceLayer)` | `tests/test_rv32i_stage_dual.cpp:430-441` | ⚠️ 它断言的过时归因属**行 17** ⇒ ⇒ 本 change **不动** |
| ④（兄弟边 · 父侧网） | ✅ 逐条断言父 module 的 `logic [7:0] top_a_unnamed_output;` + `.top_a_unnamed_output(top_a_unnamed_output)` + `.top_b_unnamed_input(top_a_unnamed_output)` | `tests/test_hierarchy_verilog.cpp:1379-1406` | ⇒ ⇒ **逐拍等价**仍缺（见 §4.2） |
| ②（父→子 · 表达式纯父内） | ⚠️ `:1915` 是**行 1**（合成 `__if_0`），**不等于**族 ② | `tests/test_hierarchy_verilog.cpp:1915` | ⇒ ⇒ `row2_expr` 里的一格会**顺带**覆盖它 |
| ①（父读子 output · 父侧带运算） | ❌ **空白** | —— | ★ **本 change 的核心产出** |

## 2. 新增轴 A 取值：`row2_expr`

### 2.1 定义

> **父层读取子组件的某个端点，且父侧对该端点的使用**带**运算 / 表达式**（而不是直通）。

- 与 `row2`（子→父·直接）的区别：`row2` 的父侧使用是**直通**（`{"row2","ch_out",Ok}` = S1 直通）；
- 与 `row1`（父→子·直接）的区别：`row1` 的**子侧端点不是子端口**（⇒ 合成隐式端口），
  而 `row2_expr` 的**子侧端点是子端口**（⇒ 不合成，靠父侧网中转）；
- 与 `sibling` 的区别：`sibling` 的两端是**两个不同子实例**（共同祖先内一根中转线）；
  `row2_expr` 是**同一个子实例的输出**被父侧运算消费。

### 2.2 6 格的语义与**实测填入**的取值

| type（子侧端点） | 语义 | 取值（**实测填入**，不得预设） | 理由（`Na` 时必填） |
|---|---|---|---|
| `ch_out` | ★ **族 ①/③**：父读子 output 端口 + 父侧运算 | **实测填入** | —— |
| `ch_reg` | 父读子成员的 `ch_reg` + 父侧运算 | **实测填入** | —— |
| `ch_proxy` | 父读子成员的 bundle 字段（proxy）+ 父侧运算 | **实测填入** | —— |
| `op` | 父读子成员的运算节点 + 父侧运算 | **实测填入** | —— |
| `ch_in` | 子 input 是**输入**方向，父不能「读」它 | **实测填入**（预期 `Na`） | 方向非法 ⇒ 结构性无此形态 |
| `mem` | 跨模块 `ch_mem` | **实测填入**（预期 `Reject`） | 跨模块 mem 更早被拒 ⇒ 到不了本路径 |

⚠️ **实测方法**：先写 6 个 fixture ⇒ 逐个跑 `toVerilogHierarchical` ⇒ 记录 `ok` / 产物形态 / 错误 ⇒
⇒ **填进矩阵** ⇒ ⇒ **不得**用推理代替实测（仓内纪律：两后端一致不能当正确性证据 ⇒ 期望值须手推）。

### 2.3 与既有矩阵的一致性

- 格值语义（三态定义、`Na` 必写理由、evidence tag 必须在源文件中真实存在）⇒ **完全沿用**；
- 上界同步：`cells.size()` 由 36 → 42；`n_ok` / `n_reject` / `n_na` 按实测同步；
- 新格复用既有 tag `"[hierarchy][emit][matrix]"` ⇒ ⇒ **不新增 CTest 条目**。

## 3. fixture 设计

### 3.1 6 个 DUT（`MatRow2Expr*`）

- 每个带**类级 `CH_HIERARCHY_LAYER("...")`**（成层激活方式，与既有矩阵 fixture 一致）；
- **每个端口单一驱动**（⚠️ 探针/夹具纪律：C++ 侧 last-wins ⇒ 产物侧 `Duplicate pin connection`
  ⇒ Verilator `%Error` ⇒ ⇒ 会制造假信号）；
- 至少一个用 **`ch_bool` / 1-bit**（对齐真实站点 `hazard.io().ex_is_load <<= id_stage.io().is_load;`
  —— `rv32i_pipeline.h:520`）⇒ ⇒ 断言 `logic` 声明形态（`logic x;` vs `logic [0:0] x;`）；
- 至少一个**复刻真实站点写法** `select(cond, child_out, …)`（见 `rv32i_pipeline.h:398`
  `exmem_valid <<= select(exmem_en, id_stage.io().valid_out, ch_bool(false));`）⇒ ⇒
  ⚠️ 真实 riscv-mini 顶层在 `ForceLayer` 下**先**被跨模块 `ch_mem` 拒绝 ⇒ ⇒ 真实顶层**不能**直接当 fixture。

### 3.2 粒度负向对照（`MatInlineControl`）

- **不带** `CH_HIERARCHY_LAYER` ⇒ ⇒ 默认粒度（`RespectDeclarations`，`src/component.cpp:160`）
  ⇒ ⇒ 产出**单 module**（全 inline）⇒ ⇒ 断言：无跨层形态、无 `__if_` / `__x_`、父侧无网。

### 3.3 S1 直通（「不造网」显式断言）

- 既有 `row2 × ch_out` 用例已有 fixture（`T3`：`io().dout <<= mid.io().dout;`）⇒ ⇒ **只补断言**：
  `ok==true` + 产物**无**父侧 `logic` 网 + 子 output **直接**接父 output 端口
  （`.top_mid_unnamed_output(top_unnamed_output)`）⇒ ⇒ **注意**：这**不是**「未造网 = 缺陷」，
  而是「未造网 = 合法形态（Q2 收窄后的契约目标）」⇒ ⇒ 断言 MUST 表达后者。

## 4. 断言算法（可机械判定）

### 4.1 fail-closed（`Reject` 格）

```
REQUIRE_FALSE(r.ok);
REQUIRE_FALSE(file_exists(path));      // 产物 MUST NOT 落盘
REQUIRE_FALSE(r.error.empty());
REQUIRE(r.error.find("<子 module key>") != npos);   // 点名子 module（**非**整串断言）
```

⚠️ **MUST NOT** 断言诊断**整串**（行 17 的过时刻固化即此类；且 Q3 会改这段诊断 ⇒ 断言整串必然假红）。

### 4.2 作用域锚定（`Ok` 格）

⚠️ `all.find(...)` **不算**作用域锚定（会把子 module 里的同名串误判为命中）⇒ ⇒ MUST 用既有
`module_text(all, "<module>")`（`tests/test_hierarchy_verilog.cpp:96-104` 的文件局部 helper）提取 body：

```
const std::string parent = module_text(all, "top");
const std::string child  = module_text(all, "<child>");
// ① 父 body 内：网声明 + .port(net) + 父侧表达式读该网
// ② **显式反断言**：同一网名**不**出现在子 body 内（防止「在子 module 里碰巧同名」判绿）
```

⇒ ⇒ **网「存在」的断言 MUST 独立于 `ProductSet::signal_map`**（不能从同一张表自证）。

### 4.3 逐拍等价（族 ④）

- 位置：**既有** `tests/test_hierarchy_equivalence.cpp` 目标（已带 `verilator_e2e` 标签 ⇒ ⇒ 不新增条目）；
- API：`ch::hier_eq::emit(..., ch::Granularity::ForceLayer)` → `ProductSet` ⇒
  `ch::hier_eq::compare(ctx, work_dir, hier_path, ports, inputs, observations, stimulus)`；
- ★ **期望 trace MUST 以字面量表写在源码里** + **推导注释完整** ⇒ ⇒ 且与 `Result::flat_trace`
  **分开断言**（禁把 `flat_trace` 当期望值 ⇒ 那就是抄仿真输出）；
- ⚠️ 被观测值 MUST **引到顶层可观测** ⇒ ⇒ 否则 `ch::hier_eq` 会以 `observation-not-visible` 机械拒绝
  （`include/hierarchy_equivalence.h:43-57`）⇒ ⇒ 这本身就是「引到顶层」的机械护栏。

## 5. 判别力自证（变异表）

⚠️ 每条 MUST：**编译确认 → 重建 → 观测 → 还原 → 重建 → 复绿**（★ 「二进制也是状态」；
编译失败 ⇒ 跑的是旧二进制 ⇒ 假绿）。

| # | 变异 | 预期 | 覆盖的缺口 |
|---|---|---|---|
| M1 | 临时删发射器的**父侧网声明**（`print_decl` 一处） | 族 ④ 用例**红**（网缺失） | 「抓发射器回归」 |
| M2 | 临时把族 ④ 的 `.port(net)` 改成 `.port(parent_out)` | 族 ④ 用例**红**（形态不符） | 「抓形态漂移」 |
| M3 | 把 `row2_expr × ch_out` 的 fixture 换成 `row2_expr × ch_reg` 写法 | 该格**红** | **四族互斥性**（证明该格没被误写成别的族） |

⚠️ M1/M2 触及 `src/hierarchy_verilog.cpp` ⇒ ⇒ **实施后 MUST 还原 + 重建 + 复跑** ⇒ ⇒
`git diff --stat -- src include` MUST 为空（验收项）。

## 6. 实施期实测结论（2026-11-08 · 仓外探针 `probe_row2expr` + 仓内用例）

### 6.1 ★ 新发现：**「父侧运算消费子成员非端口端点」⇒ 放行但产物不可编译**（**新缺陷**）

实测（`ForceLayer`）：

| 形态 | 父侧写法 | `ok` | 产物 | Verilator lint |
|---|---|---|---|---|
| 父读子 **ch_out** 端口 + 父侧运算 | `select(leaf.io().dout > 3_d, leaf.io().dout, 0_d)` | ❌ 0 | —（无产物） | —— |
| 父读子 **ch_reg** + 父侧运算 | `leaf.instance().acc \| ch_uint<8>(1_d)` | ✅ 1 | 父 module 内 `assign top_or_or = top_leaf__reg \| 8'h1;` | ❌ **`Can't find definition of variable: 'top_leaf__reg'`** |
| 父读子 **bundle 字段**（proxy）+ 父侧运算 | `leaf.instance().strm.payload \| ch_uint<8>(1_d)` | ✅ 1 | 同上（引用 `top_leaf_payload`） | ❌ 同形态 |
| 父读子 **op 节点** + 父侧运算 | `leaf.instance().sum \| ch_uint<8>(1_d)` | ✅ 1 | 同上（引用 `top_leaf_bits`） | ❌ 同形态 |
| 父读子 **ch_reg**（**无**父侧运算 · 既有 `row2` 行） | `leaf.instance().acc` | ✅ 1 | 父侧读**合成端口名** ⇒ ⇒ 产物**干净** | ✅ 干净 |

⇒ ⇒ **触发条件唯一**：「父侧对子端点的使用**带运算**」⇒ ⇒ 父侧引用了**子 module 的内部信号名**
（而非**可引用的端口名**）⇒ ⇒ 父作用域无此标识符 ⇒ ⇒ **产物不可编译**。

⚠️ **C++ 侧合法**（实测无 CHREQUIRE 失败）⇒ ⇒ 这是**发射器的缺陷**，不是用户误用。

⚠️ **与 ADR-060 R2 的关系**：R2（`docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md:86-112`）
要求「**所有** `src(0)` 解析 / 节点穿透路径 MUST 先查重定向表」⇒ ⇒ 本路径**未覆盖** ⇒ ⇒
**违反 ADR-060 R2** ⇒ ⇒ 不是「未定义行为」，是**契约违反**。

⚠️ **与台账行 16 的关系**：这是「父侧落点」族的**第三条路径** ——
行 16 登记的是「子侧端点是 `ch_out` 端口 ⇒ 被拒」；本条是「子侧端点是**非端口端点** ⇒ 放行但产物坏」
⇒ ⇒ ⇒ **候选 B 的实现 MUST 同时覆盖两条路径**（否则 B 落地后仍有坏产物）⇒ ⇒ **候选 B 的范围比原估计大**。

⚠️ **严重度**：**中**。① 仓内**零实例**（族 ①/③ 的 `ch_out` 路径被拒 ⇒ ⇒ 不产生坏产物）；
② 但形态**极常见**（父层对子状态做运算）⇒ ⇒ 用户一旦写就中招；
③ **不是静默数据损坏**（Verilator lint 直接失败 ⇒ ⇒ 用户会看到）⇒ ⇒ 但**发射器本应 fail-closed**。

⇒ ⇒ **处置**：本 change **不修**（修它 = 行 16 的实施域 + 需 architect 裁定后立项）⇒ ⇒
本 change 只做三件事：① **矩阵第四态 `Broken` 显式记录**（不静默记成 `Ok`）；② **现状锁定断言**
（缺陷修复后预期转红）；③ **登记为新残留行**（建议 id **19** · 归属 architect · 复核条件 =
「父侧运算消费子非端口端点的产物 MUST 可编译（或响亮拒绝）」）。

### 6.2 「父侧带运算」这一维的完整实测矩阵（42 格中的 6 格）

| type（子侧端点） | 取值 | 理由 |
|---|---|---|
| `ch_out` | **Reject** | 父侧端点不是父 output 端口 ⇒ ⇒ **行 16 的族 ①/③**（★ 本轴唯一「Reject」格） |
| `ch_reg` | **Broken** | 放行 ⇒ ⇒ 产物引用子内部信号名 ⇒ ⇒ lint 失败（§6.1） |
| `ch_proxy` | **Broken** | 同上 |
| `op` | **Broken** | 同上 |
| `ch_in` | **Na** | `ch_in` 是输入方向 ⇒ ⇒ 父层不能「读」它 ⇒ ⇒ 结构性无此形态 |
| `mem` | **Reject** | 跨模块 `ch_mem` 更早被拒 ⇒ ⇒ 到不了本路径（复用既有 `[reject][naming]` tag） |

⚠️ **实测要点**：父侧运算 MUST 与子端点**同宽** ⇒ ⇒ 用 `|`（同宽按位或）；
用 `+` 会进位成 9-bit ⇒ ⇒ 撞上 ADR-060 W1 的宽度检查 ⇒ ⇒ 测的就不是「父侧带运算」了。

⚠️ **既有 `row2` 行的三个 `Ok` 格无误**（那三个是**直通**读子非端口端点 ⇒ ⇒ 父侧读**合成端口名** ⇒ ⇒ 产物干净）⇒ ⇒
⇒ **新轴与既有轴的差异只在「加不加运算」**。

### 6.3 矩阵第四态 `Broken` 的引入理由

`Ok` = 「正确处理」· `Reject` = 「响亮拒绝」· `Na` = 「结构性无此形态」⇒ ⇒
「**放行了但产物坏**」**三者都不是** ⇒ ⇒ 若只记 `Ok` ⇒ ⇒ 矩阵会把这个坏产物**记成正确处理**
⇒ ⇒ 静默放过 ⇒ ⇒ 故**必须**单列第四态 ⇒ ⇒（既有 36 格**不受影响** ⇒ ⇒ 只有新轴的 3 格是 `Broken`）。

## 7. 边界与已知限制

| 边界 | 处置 |
|---|---|
| **多层 / 孙组件** | 本 change 只测**单层**（父 → 直接子）⇒ ⇒ 孙组件形态属行 16 裁定范围外（见裁定文档 §7 末） |
| **跨模块 `ch_mem`** | 既有矩阵的 `mem` 行已覆盖（更早被拒）⇒ ⇒ 本 change 不新增 |
| **宽度不一致** | 属行 16 裁定文档的 V5（拍板后随行 16 实施）⇒ ⇒ 本 change 不覆盖 |
| **> 64-bit 端口** | `ch::hier_eq` 会以 `port-width-64-exceeded` fail-closed ⇒ ⇒ fixture **MUST** 用 ≤64-bit |
| **多消费点扇出（同一子 output 被父侧多处消费）** | 属 Q4（**仅** Q1=B 时是义务）⇒ ⇒ 本 change **不测**；⚠️ 「每端口单一驱动」是**夹具构造纪律**，与此**不是**一回事 |
| **`Verilator` 不可用** | 族 ④ 的逐拍等价属 `verilator_e2e` 目标 ⇒ ⇒ 无 verilator 的 job 不跑该目标（既有口径） |
| **行 16 若实施（B）** | `row2_expr × ch_out` 格从 `Reject` 变 `Ok` ⇒ ⇒ **预期转红** ⇒ ⇒ MUST 在矩阵注释写明「本矩阵是**活的现状快照**」 |
