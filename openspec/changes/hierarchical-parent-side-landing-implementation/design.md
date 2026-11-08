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
