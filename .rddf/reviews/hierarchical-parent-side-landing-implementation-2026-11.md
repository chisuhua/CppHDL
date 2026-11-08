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
