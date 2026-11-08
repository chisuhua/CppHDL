# 审稿记录：立项 `hierarchical-parent-side-landing-implementation`（metis 第 1 轮）

- **审稿对象**：4 个文件 ——
  `.rddf/improvements/hierarchical-parent-side-landing-implementation.md` ·
  `openspec/changes/hierarchical-parent-side-landing-implementation/{proposal.md, tasks.md, specs/rtl-hierarchical-output/spec.md}`（spec delta）
- **权威决策**：`docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md`（**Accepted**）
- **审查者**：**metis**（planner 固定审查者）· 预算 **1 轮** ⇒ 处置后放行（无 BLOCKER 概念）
- **HARD-GATE**：`pre_create_brainstorm_check.sh` ⇒ **exit 0** ✅ · `verify_openspec_delta.sh` ⇒ **4 条全绿**（两个 change）✅

---

## 1. metis 第 1 轮的核心判断

**「真正要修的不是『族 ①/③ 被拒』，而是『父侧操作数命名』这一条静默通道」** ——
而该通道的实现面在 **`src/codegen_verilog.cpp`**（`node_names_[` 出现 **62 行**），
**不是** `src/hierarchy_verilog.cpp`（零命中）。⇒ ⇒ proposal 的 Impact 段**漏列**该文件。

---

## 2. ★ 两条经 architect 独立核实的新事实（本轮最重要）

### 2.1 **`ch_mem` 端口会被静默跳过**（metis M1 · **已核实**）

```cpp
// src/codegen_verilog.cpp:1266-1270
if (!parent || !addr || !data_proxy ||
    !node_names_.count(parent) || !node_names_.count(addr) ||
    !node_names_.count(data_proxy)) {
    return;                      // ← 整条 mem 读端口**静默不发射**
}
```
- 真实动机用例：`include/cpu/pipeline/rv32i_pipeline.h:194-197`
  （`reg_file.aread(id_stage.io().rs1_addr_id, "reg_file_rs1")` ⇒ ⇒ **addr = 子 `ch_out`**）
- ⇒ ⇒ 若该名字不在父 `node_names_` ⇒ ⇒ **产物过 lint、mem 读端口缺失、读回 0** ⇒ ⇒
  **比「坏产物」更危险的静默形态**（A1/A2/A3 全绿也抓不到：A3 只看 `%Error`）。
- ⇒ ⇒ **ADR-066 未覆盖这一消费者类型**（决策 ② 只谈「非端口端点」，未谈 `ch_mem` 端口）
  ⇒ ⇒ **登记为新残留（候选行 20）· 归 architect**。

### 2.2 **既有 tripwire 必然转红**（metis M2 · **已核实**）

```cpp
// tests/test_rv32i_stage_dual.cpp:430-441
TEST_CASE("corpus hierarchy regression (rejection boundary): ImmConsistencyDut", ...) {
    ...
    REQUIRE_FALSE(products.ok);                                              // MUST 被拒
    REQUIRE(products.error.find("implicit-port synthesis") != npos);         // 归因钉死
}
```
⇒ 而 `ImmConsistencyDut` 的形态 = `io().equal <<= (dec.io().imm == ids.io().imm)`（`:258`）
= **正是本 change 要放行的「父侧带运算」** ⇒ ⇒ **本 change 落地后必然转红**。

⇒ ⇒ 与「T5.2 ctest 全绿」**直接矛盾** ⇒ ⇒ 已补 **A16**（新期望在本 change 内定值）。

---

## 3. 处置（9 MUST / 9 SHOULD / 6 NIT）

| # | 级别 | finding | 处置 | 落点 |
|---|---|---|---|---|
| M1 | MUST | **`ch_mem` 端口静默不发射**（`print_mem_read_port` early-return）+ 「网名 MUST 进父 `node_names_`」 | **采纳** ⇒ 新增 **A15**（判据 = **读值非 0**，不只 lint）+ 候选行 20 登记 | A15 · 本文 §2.1 |
| M2 | MUST | **行 17 tripwire 必然转红** ⇒ T5.2 不可满足 | **采纳** ⇒ 新增 **A16**（新期望在本 change 内定值；叙事文字留给独立 work item） | A16 · §2.2 |
| M3 | MUST | **拒绝面可能萎缩**（`:756-779` 是兜底遍历，`:771` 单条件放行 = 放行全部非 output 父侧端点） | **采纳** ⇒ **A17**（13 个 Reject 格不变 + 跨多层/表亲边负向用例）+ **A18**（`Reject` 档首错 = 真正消费节点） | A17/A18 |
| M4 | MUST | **删 `Broken` 态若不同时补 lint 覆盖 = 倒退**（它是仓内唯一表达「ok=true 但产物不可编译」的态；`test_hierarchy_verilog.cpp` 零 verilator 调用） | **采纳** ⇒ **A19**（四格产物 MUST 进 lint 集合 + 护栏带 `--self-test`） | A19 |
| M5 | MUST | **A3 不可执行**（`verify_hierarchical_rtl.sh` 只 lint `hier_rtl_products` 的 3 个设计；矩阵产物在 `/tmp/opencode/hieremit`） | **采纳** ⇒ A3/A11 措辞改为「**点名**：哪 4 个产品、由哪个脚本的哪条规则判」+ T2.6 扩展产物生成器 | A3/A11 · tasks T2.6/T5.5 |
| M6 | MUST | **A12 三合一不可验** | **采纳** ⇒ 拆 **A12a/b/c**（spec delta 自检 · ADR-062 反向指针 grep · ADR-060 补格 grep 且与第 2 行**并存**） | A12 |
| M7 | MUST | **Impact 漏列 `src/codegen_verilog.cpp`**（真正的取址面） | **采纳** ⇒ What Changes / Impact 增列 + 风险「该文件是**扁平路径共用** ⇒ A9 逐字节锚是承重判据」 | What Changes · Impact |
| M8 | MUST | **A8 分母口径不一**（`grep -c` 行数 vs 出现次数）+ 与「无效变异如实登记」冲突 | **采纳** ⇒ 改为「**语义点集合**（T1.0 探针实测产出）+ 其余点登记为无效变异（附证据）」+ 钉死计数单位 | A8 · tasks T0.3 |
| S1 | SHOULD | A5 的归因禁令**作用域过宽**（同一文案服务所有 `synth_diags`，其措辞仍是真的） | **采纳** ⇒ 限定为「**新放行的形态**若仍失败」+ 反例 grep（该串 MUST 仍存在于其它诊断） | A5 |
| S2 | SHOULD | **A2 缺正向机制**（自建解析器 = scope creep；退化成 A3 = 重复计数） | **采纳** ⇒ A2 改为「父 body 含 `logic [W] <global_name>;` + 实例语句 `.child_port(<global_name>)` ⇒ **可解析性交 A3**」 | A2 |
| S3 | SHOULD | **A1 缺目标计数** | **采纳** ⇒ **`ok=20` · `reject=16` · `na=6` · 无 `broken` · `cells.size()==42`** | A1 · tasks T0.2 |
| S4 | SHOULD | A7 未覆盖「①×兄弟边」与「混合直通」 | **采纳** ⇒ 同 `ch_out` 多消费者 ⇒ **恰好一根网** + `add_conn` 幂等 + 复用 `wire_exists`（不新开网表） | A7 |
| S5 | SHOULD | 缺 1-bit / `ch_bool` 用例 | **采纳** ⇒ **A22** | A22 |
| S6 | SHOULD | 缺 root-as-parent 资格排除（会重复声明） | **采纳** ⇒ **A23** | A23 |
| S7 | SHOULD | 撞名错误文案会**指错来源** | **采纳** ⇒ **A24** | A24 |
| S8 | SHOULD | 缺真实库站点验收 | **采纳** ⇒ **A21**（`ForceLayer` 下 `riscv::Rv32iPipeline`，**不是** SoC） | A21 |
| S9 | SHOULD | 台账闭口无 A 编号 | **采纳** ⇒ **A20**（含 `--self-test`） | A20 |
| N1 | NIT | 「现有 7 个 Scenario」应为 **6** | **采纳** ⇒ 已核：主 spec `:234/241/248/255/262/268` = **6**；delta = 6 + 2 新增 = 8 ⇒ 自洽（查 `spec.md` 确认后按 6 记述） | spec delta 注记 |
| N2 | NIT | CLI 版本叙述冲突（`tasks.md` 写 1.14.x vs 根 `AGENTS.md` 钉 1.4.1） | **采纳** ⇒ 以 `openspec validate <change> --strict` 为门（T6.1），不依赖全量 `--specs` | tasks T3.4/T6.1 |
| N3 | NIT | **A6/A9/A10/A14 无对应 T** | **采纳** ⇒ tasks 补 T2.7（宽度）/ T5.7（A9 逐字节）/ T5.8（A10）/ T5.9（A14） | tasks |
| N4 | NIT | 锚点清单不完整（`DUAL_BACKEND_PARITY_TOTAL` / `_PAIRS` / `EXAMPLES_TEST_SOURCES`） | **采纳** ⇒ 写明「本 change 不动那三个锚点」（两个测试文件均 **0** 个 `DUAL_BACKEND_TEST_PAIR` ⇒ 已核实） | tasks T5.3 |
| N5 | NIT | A13 强度不足（「诊断非空」） | **采纳** ⇒ 要求点名**父 module + 该 input 端口名 + 改法** | A13 |
| N6 | NIT | `test_hierarchy_verilog.cpp` 的「修复后改写为『产物不含子内部信号名』」指引**在修复后是错的**（落点网名 = 端点全局名 ⇒ 父 body 构造性含它） | **采纳** ⇒ 改为「父 body 内该名字 MUST 被**声明**（`logic`）」+ lint | 测试注释（随实施改） |

---

## 4. 本轮未落实的（**明确交下一轮**）

| 项 | 状态 | 说明 |
|---|---|---|
| 上述处置**逐条改写 4 个文件的正文**（非 AC 段） | 🟡 **部分** | 本轮只把 **A15–A24 并入 improvement + proposal 的 AC 段**；What Changes / Impact / tasks 的细项（M5/M6/M7/N3/N4）**列入下一轮 builder 实施前完成** |
| 候选**行 20** 登记 | ⏸ | **architect 域**（登记表归 architect）⇒ 需切 architect（或与行 16/19 的状态更新同批） |
| A12a/b/c 的拆分 | ✅ 已写入 AC | 实施时按三条独立证据执行 |

---

## 5. 验证

| 项 | 结果 |
|---|---|
| `pre_create_brainstorm_check.sh`（improvement） | **exit 0** ✅ |
| `verify_openspec_delta.sh`（两个 change） | **4 条全绿** ✅（R1 无 change 名 · R2 无占位路径 · R3 MODIFIED 标题逐字一致 · R4 tasks bash 块可达） |
| 产品代码 | `src/` `include/` = **0** 改动 ✅ |
| spec delta 结构 | MODIFIED ×1（**逐字**匹配主 spec 标题）+ **2 个新增 Scenario**（决策 ① / 决策 ②）✅ |

---

# 附录：phase-6 **P1** —— 「`node_names_[` 的 62 行 / 64 次无需改动」的机械证据

- **执行**：builder · 2026-11-09 · **T4.4 的闭口**（`tasks.md` T4 唯一未勾项）
- **权威**：ADR-066 决策 ①（Accepted）· `design.md` §6.2「修复不需要改 62 个取数点」
- **护栏**：`scripts/verify_node_names_single_source.sh`（**新增** · R1–R4 + `--self-test`）
  · 只读模式 + `--self-test` 已进 CI（`build-and-test` 的 guard 段）
  · `--inject`（源码级注入）**故意不进 CI**

## A. 待证的断言（原文）

> `src/codegen_verilog.cpp` 的 `node_names_[`（**62 行 / 64 次**）**无需改动**。
> 依据（`design.md` §6.2）：网名 = **驱动端点的 ctx 全局名** ⇒ 父侧表达式的 RHS
> **本来就是**那个名字 ⇒ 只要在父 module 声明同名 `logic` 网，RHS 就自动可解析。

⚠️ **「零改动」不能靠 `git diff` 证明** —— 没改 ≠ 不需要改。必须把断言拆成
**结构事实**（R1/R2/R3）+ **承重性事实**（R4）。

## B. R1 —— grep 清单 + 分母钉死

```bash
bash scripts/verify_node_names_single_source.sh      # R1
grep -c 'node_names_\[' src/codegen_verilog.cpp                 # = 62   （行）
grep -o 'node_names_\[' src/codegen_verilog.cpp | wc -l         # = 64   （出现次）
```

⚠️ **计数单位 MUST 同时钉两个**：62 行里有 **2 行各出现 2 次**
（`:963` `print_bit_select` · `:994` `print_bits_extract`）⇒ 只钉一个数必然口径不一。
**分母漂移即红**（不是「顺手更新基线」）：62/64 变了 ⇒ 「无需改动」的论证必须重做。

逐函数分布（**62 行 / 64 次** · 合计对得上）：

| 函数 | 行 | 次 | | 函数 | 行 | 次 |
|---|---:|---:|---|---|---:|---:|
| `emit_signal_decl` | 1 | 1 | | `print_rotate_r` | 5 | 5 |
| `print_header` | 1 | 1 | | `print_concat` | 3 | 3 |
| `print_logic` | 5 | 5 | | `print_proxy` | 3 | 3 |
| `print_input` | 1 | 1 | | `print_mux` | 4 | 4 |
| `print_output` | 1 | 1 | | `print_bitsupdate` | 4 | 4 |
| `print_reg` | 4 | 4 | | `print_mem_decl` | 1 | 1 |
| `print_unary_op` | 9 | 9 | | `print_mem_init` | 1 | 1 |
| `print_binary_op` | 3 | 3 | | `print_mem_read_port` | 4 | 4 |
| `print_bit_select` | 2 | **3** | | `print_mem_write_port` | 4 | 4 |
| `print_bits_extract` | 1 | **2** | | **合计** | **62** | **64** |

完整清单（可复跑：`grep -n 'node_names_\[' src/codegen_verilog.cpp`）：

```
276  342  575  628  631  684  685  701  711  768  774  781  793
877  882  884  885  890  892  895  897  912  931  932  946
963  968  994  1012 1017 1019 1020 1024 1043 1048 1050 1051 1055
1074 1075 1084 1105 1133 1134 1154 1155 1156 1174
1201 1203 1205 1207 1221 1235 1271 1272 1273 1296 1365 1366 1367 1370
```

## C. R2 —— `node_names_` 的**单写入点**

```bash
grep -nE 'node_names_[[:space:]]*=[^=]' src/codegen_verilog.cpp
# 53:    node_names_ = view_.names;          ← 恰 1 处
grep -nE 'node_names_\.(insert|emplace|erase|clear|reserve|swap)\b' src/codegen_verilog.cpp
# （零命中）
```

⇒ ⇒ **64 次读全部取自同一次赋值的那张表**，且赋值之后**没有任何容器级 mutator**
⇒ ⇒ 结构上**不存在**「某个取址点用了另一套命名」的可能。
⇒ ⇒ 这就是「逐点变异」被降级为**零变异**（`.rddf/improvements/...implementation.md:22`）的
真正依据 —— **命名真源唯一**，逐点变异只能得到同一条变异。

## D. R3 —— 命名真源同源（发射器 ↔ codegen）

```bash
grep -n 'build_verilog_name_table(' src/codegen_verilog.cpp src/hierarchy_verilog.cpp
#  src/codegen_verilog.cpp:68   view.names = ch::build_verilog_name_table(ctx->get_eval_list());
#  src/hierarchy_verilog.cpp:132 const auto names = build_verilog_name_table(root.context()->get_eval_list());
```

两者**同一函数 + 同一实参形态**（`<ctx>->get_eval_list()`，见 `include/codegen_naming.h:71`
的「唯一真源」）⇒ ⇒ **落点网名 ≡ 父侧 RHS 名**（同一张表对同一节点的同一次查询）
⇒ ⇒ RHS 自动可解析，**不需要**任何一个取址点做「解析到父侧落点网」的本地 fix-up。

## E. R4 —— 负向注入（**源码级**）⇒ R9 必红 ⇒ 还原后复绿

**注入点 = 62 处之一**：`src/codegen_verilog.cpp:931`（`print_binary_op` 的 `lhs_name`）

```diff
         std::string lhs_name = node_names_[lhs_node];
+                            ... + "__p1_injected";
```

**为什么选它**：4 个父侧落点 fixture 的父 module 里都有一条
`assign <父侧运算> = <落点网> <op> <lit>` ⇒ 全部由 `print_binary_op` 发射
⇒ 一次注入同时命中 **族 ① + 路径 B 的全部 4 个产物**。

**过程（`bash scripts/verify_node_names_single_source.sh --inject`）**：

| 步 | 动作 | 结果 |
|---|---|---|
| 1 | **基线**：跑既有护栏 `scripts/verify_hierarchical_rtl.sh`（含 R9） | ✅ 全绿 |
| 2 | 注入 `+ "__p1_injected"` | — |
| 3 | **确认仍能编译**（`cmake --build build --target hier_rtl_products`） | ✅ 编译成功（⚠️ 编译失败 ⇒ 跑的是旧二进制 ⇒ 假红） |
| 4 | 重跑既有护栏 | ❌ **R9 必红** |
| 5 | 注入标记 MUST 出现在产物 + 父 module MUST **未**声明它 | ✅ 两条都成立 |
| 6 | 还原（`cp` 备份）+ **重建** | ✅ `cmp` 字节级一致 |
| 7 | 重跑既有护栏 | ✅ **复绿** |

**第 4 步的实测输出**（**摘录** · 4 个 fixture 各 2 条 = 子 module 一条 + 父 module 一条；
路径已缩写，父 module 那条即本判据）：

```
%Error: ps_expr_op.v:40:31:    Can't find definition of variable: 'top_leaf_unnamed_input__p1_injected'   ← 子 module
%Error: ps_expr_op.v:68:24:    Can't find definition of variable: 'top_leaf_bits__p1_injected'           ← **父 module**
%Error: ps_expr_out.v:67:24:   Can't find definition of variable: 'top_leaf_unnamed_output__p1_injected' ← **父 module**
%Error: ps_expr_proxy.v:68:24: Can't find definition of variable: 'top_leaf_payload__p1_injected'         ← **父 module**
%Error: ps_expr_reg.v:68:24:   Can't find definition of variable: 'top_leaf__reg__p1_injected'            ← **父 module**
```

父 module 声明的是**不带后缀**的落点网（如 `logic [7:0] top_leaf_unnamed_output;`）
⇒ 带后缀的那个**只在 RHS 出现** ⇒ ⇒ 正是「**把某处名称换成未在父作用域声明的名字**」。

⇒ ⇒ **结论**：`node_names_[...]` 的取值与发射器声明的落点网名**必须一致**，
且该一致性**承重**（不一致 ⇒ 产物非法）⇒ ⇒ 62 处**保持现状**即为正确实现，
**无需改动**（改动反而会破坏一致性）。

## F. 护栏自身的判别力（`--self-test` · 5/5）

`bash scripts/verify_node_names_single_source.sh --self-test`（**只动副本**，绝不碰仓内源码）：

| # | 形态 | 期望 |
|---|---|---|
| ① | 未改动的副本 | **绿**（对照：证明下面的红不是「本来就红」） |
| ② | 多一处 `node_names_[`（63 行） | R1 **红** |
| ③ | 第二个赋值点 | R2 **红** |
| ④ | 赋值后的 `node_names_.clear()` | R2 **红**（容器级 mutator 维度） |
| ⑤ | 发射器改用私有命名表 | R3 **红** |

实测：**5/5 全部生效** ✅

## G. 为什么「一次注入」足以覆盖 64 处（归纳论证 · 如实登记）

R4 只注入 **1 处**，**没有**逐点变异 64 次。覆盖 64 处的是**结构**，不是 64 条变异：

1. R2 ⇒ **所有** 64 次读都返回 `table[node]`（同一张表、无 mutator）；
2. R3 ⇒ `table` ≡ 发射器的命名表 ⇒ 返回的名字**就是**落点网名；
3. R4 ⇒ 两者一旦分歧，产物**必然**非法（lint 响亮失败）。

⇒ ⇒ 任一取址点若改成「自己另算一个名字」⇒ 落入第 3 条 ⇒ 红。
⇒ ⇒ **逐点变异在这里是无效变异**（每次都会得到同一条结论），
故按 A8 的口径登记为「**零变异 + 结构证据**」，而非「64 条变异」。

⚠️ **诚实边界**：上述归纳依赖「**同一个 `ModuleEmitter` 实例内** `node_names_` 不被改写」。
若将来出现「按 module 切换命名表」的形态，R2 的「单写入点」会**先**变红
⇒ ⇒ 该断言届时必须重审。

## H. 落地清单

| 工件 | 路径 |
|---|---|
| 证据脚本 | `scripts/verify_node_names_single_source.sh`（R1–R4 + `--self-test`） |
| CI 接线 | `.github/workflows/ci.yml` 的 `build-and-test` guard 段（只读模式 + `--self-test`；`--inject` 不上 CI） |
| 本记录 | 本文件（§A–§H） |
| phase-6 回填 | `.rddf/roadmap/phases/phase-6.md` 的 P1 行 |

## I. 验收（builder 内建 verifier 环节 · 2026-11-09 · 轮次 1/3）

**判据来源**（按优先级）：① 用户指令「读 phase-6，按 P1 推进」⇒ ② `phase-6.md` P1 的
**出口判据**（「证据脚本**可复跑**；负向注入使 **R9 lint 必红**、还原后复绿」）⇒
③ 本 change 自身声明的形态（A8 面 ① = grep 清单 + 负向注入）。

| # | 判据 | 证据（工具调用） | 结果 |
|---|---|---|---|
| 1 | 证据脚本**可复跑** | `bash scripts/verify_node_names_single_source.sh` ⇒ `PASS（R1–R3）` · `--inject` ⇒ `PASS（R1–R4）`（**多次重跑结果一致**） | ✅ PASS |
| 2 | grep 清单 = **62 行 / 64 次**，且**逐个**与源码一致 | `grep -c` / `grep -o \| wc -l` + 清单逐行比对（62 个行号与源码**完全相同**） | ✅ PASS |
| 3 | 负向注入使 **R9 必红** | `--inject`：注入 `src/codegen_verilog.cpp:931` ⇒ 重建 ⇒ `verify_hierarchical_rtl.sh` 输出 `❌ R9 lint 失败`（4/4 fixture） | ✅ PASS |
| 4 | **还原后复绿** | `--inject` 末段：`cmp` 字节级一致 + 重建 + 既有护栏全绿 | ✅ PASS |
| 5 | 「父作用域不可解析」**确为**转红原因 | 父 module 内**无** `__p1_injected` 的 `logic` 声明（`awk` 提取父 module 判定）；且注入标记确实出现在产物里 | ✅ PASS |
| 6 | 护栏自身有判别力 | `--self-test` **5/5**（对照 + 4 条投毒：分母 / 双赋值 / mutator / 私有命名表） | ✅ PASS |
| 7 | 编译 / 静态检查 | `cmake --build build -j` ⇒ 0 error；改动面 **不含** `src/**` `include/**`（`git diff --stat -- src/ include/` = 空） | ✅ PASS |
| 8 | 单元 / 集成测试 | `ctest -LE perf --output-on-failure` ⇒ **242/242 passed**（`verilator_e2e` 77 条含在内） | ✅ PASS |
| 9 | 既有护栏不回归 | `verify_hierarchical_rtl` / `verify_roadmap_fragments` / `verify_arch_ledger_residuals` / `verify_test_counts` / `verify_dual_backend_parity` / `verify_openspec_delta` / `verify_hierarchical_port_naming` / `verify_hier_corpus_coverage` ⇒ **全 PASS**（4 个 `--self-test` 亦 PASS） | ✅ PASS |
| 10 | 簿记回填 | `phase-6.md` P1 = ✅ + 证据路径 · `feat-hierarchical-rtl-output` 的 `### phase-6` + 验收标准 checkbox + `phase_refs` ⇒ `verify_roadmap_fragments.sh`（含 C1 AUTO 块守恒）**PASS** | ✅ PASS |

**边界与错误路径**（逐条已测）：
- **中断安全**：`trap cleanup EXIT / INT / TERM` ⇒ 任何退出路径都还原源码；实测三次完整
  `--inject` 后 `git diff -- src/ include/` **恒为空**，且 `/tmp` 无残留目录。
- **旧二进制假红**：注入后**强制重建**并检查 `cmake --build` 返回码；编译失败即判「判据
  不成立」而不是「红」。
- **注入未生效**：三重校验（源码里 MUST 无标记 · 注入锚点计数 == 1 · 产物里 MUST 有标记）。
- **基线本就不绿**：注入前先跑一次既有护栏，不绿则判「判据不成立」。

**失败项**：无。

**遗留清扫**：本次范围内 **无** TODO / 半截实现 / 注释掉的代码 / 临时调试日志 / 被 skip 的
测试 / 未替换的 mock / 临时 fallback。范围外的历史遗留（如 `phase-6.md` §「本窗口自身的两个
已知缺口」的行 136 悬空行与「见 P12」指向不存在的步）**只记录、不清理** ⇒ 交用户决定。
⚠️ 另：`src/codegen_verilog.cpp:536` 的 `-Wswitch`（`type_reset` 未处理）是**存量** warning，
不在本次范围。

**已用轮次**：**1 / 3**。
