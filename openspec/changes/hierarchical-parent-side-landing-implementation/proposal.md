# Proposal: 父侧落点语义落地（ADR-066 决策 ①–⑥）

> **权威决策**：`docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md`（**Accepted** · 用户拍板 Q1=B）
> ⇒ ⇒ 本 proposal **MUST NOT** 重述或改写 ADR 的决策内容或理由。
>
> **improvement 来源**：`.rddf/improvements/hierarchical-parent-side-landing-implementation.md`
> （7 段 · **A1–A14** 完整验收判据 · 已过 `pre_create_brainstorm_check.sh` HARD-GATE）· 审稿记录
> `.rddf/reviews/ADR-066-parent-side-landing.md`
>
> **本 change 有 spec 变更** ⇒ ⇒ `specs/rtl-hierarchical-output/spec.md` 存在
> （MODIFIED ×1 Requirement）⇒ ⇒ 归档时 **MUST NOT** 用 `--skip-specs`。

## Why

ADR-066 已 Accepted（2026-11-08）⇒ 契约已定。当前实现与契约在三处背离：

1. **族 ①/③（父侧消费点非父 `output` 端口）被拒** ⇒ ⇒ 而公共库内**7 处**正在用该写法
   （`include/cpu/pipeline/rv32i_pipeline.h:398`/`:401`/`:402`/`:506`/`:524` ·
   `examples/riscv-mini/src/rv32i_pipeline_top.h:112`/`:121`）⇒ ⇒ 库自己的写法不被自己的特性支持。
2. **路径 B（父侧运算消费子非端口端点）放行但产物不可编译** ⇒ ⇒ 父 module 引用**子 module 内部信号名**
   ⇒ Verilator `Can't find definition of variable` ⇒ ⇒ **违反 ADR-060 R2**（台账行 **19**）。
3. **契约层有两处需要修订**：ADR-062 §5 最后一条与 spec 的「父层消费子输出 MUST 有连接网」Scenario
   **无条件**要求造网 ⇒ ⇒ 但实测「消费点 = 父 `output` 端口」时**不造网且逐拍等价全等**
   ⇒ ⇒ ADR-066 决策 ③ 已裁定收窄 ⇒ ⇒ 本 change 执行该修订（取代流程）。

⇒ ⇒ 三处**共用一张重定向表** ⇒ ⇒ MUST **同批**落地（分两批 ⇒ 中间态仍产出坏产物）。

## What Changes

1. **发射器** `src/hierarchy_verilog.cpp`：
   - 父侧网合成（复用网登记 + `add_conn` + `print_decl` + `clashes_with_port`，**资格判定按 ADR-066 决策 ① 重写**）；
   - **落点网名 = 驱动端点 ctx 全局名**（MUST NOT 用被驱动节点自体名 ⇒ 自指 + 双驱动）；
   - **父侧操作数名解析守卫**（`print_unary_op` / `print_mux` 等直接取 `node_names_[` 的位置）；
   - 诊断重写（删过时归因 + 点名真正消费节点 + 给可执行改法）；
   - 连接去重（①×④ 重叠 ⇒ 扇出同一根网）。
2. **契约同步**：spec MODIFIED Requirement（见 `specs/rtl-hierarchical-output/spec.md`）·
   ADR-060 §2.1 **补格**（新增「子 → 父 · 父侧带运算」行 · 与第 2 行并列）· ADR-062 §5 该条 **反向指针**。
3. **测试**：矩阵 `row2_expr × ch_out` 由 `Reject` 变 `Ok`；`row2_expr × {ch_reg, ch_proxy, op}` 由
   `Broken` 变 `Ok`（**第四态 `Broken` 归零并移除**）；新增逐拍等价 / 诊断 / 宽度 / 单连接（含 ①×④ 重叠）/
   父 `input` 拒绝用例；变异自证**两个面**各逐条。

## Impact

- **产品代码**：`src/hierarchy_verilog.cpp`（主要）· 可能 `src/hierarchy_analysis.cpp`；
- **契约**：spec MODIFIED ×1 · ADR-060 补格 · ADR-062 反向指针；
- **测试/CI**：矩阵格翻转 + 新增用例；`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` 以实际新增条目为准；
- **下游**：台账行 **16**（`未决`→`已裁决`）· 行 **19**（`未决`→`已收口`，闭口证据 = 本 change 归档目录）
  ⇒ ⇒ **随本 change 归档同步**；行 17 在本 change 落地后由独立 work item 修订。

## 验收标准（= improvement 的 **A1–A24**，见 `tasks.md` 的 checkbox；A15–A24 = metis 第 1 轮补充）

- [ ] **A1** 矩阵四格翻转（`row2_expr × ch_out` ⇒ `Ok`；`× {ch_reg, ch_proxy, op}` ⇒ `Ok`）
      + **第四态 `Broken` 归零并从矩阵移除** + 上界/计数同步
- [ ] **A2** 产物形态（父侧网 + `.port(net)` + 父侧运算读该网）· 作用域锚定 =
      「父 body 内每个标识符在父 module 作用域内可解析」（**MUST NOT** 用「网名不在子 body」子串判据）
- [ ] **A3** 路径 B 的父 body 标识符 MUST NOT 是「仅在子 module 内声明」的名字 ⇒ R1 lint 零 `%Error`
- [ ] **A4** 逐拍等价（族 ①/③ + 路径 B）· 期望值**手推** · 被观测值引到顶层可观测
- [ ] **A5** 诊断可行动（点名真正消费节点 + 可执行改法 · MUST NOT 含过时归因）
- [ ] **A6** 宽度不一致响亮拒绝 + 点名两个宽度值
- [ ] **A7** 单连接不变式（含 ①×④ 重叠扇出一根网）· 产物无 `Duplicate pin connection`
- [ ] **A8** 变异两个面各逐条（面 ① `node_names_[` 全命中点 · 面 ② R2 四路径）
      ⇒ 每条：编译确认 → 重建 → 红 → 还原 → 重建 → 绿
- [ ] **A9** 「默认全 inline 产物与扁平逐字节一致」仍成立
- [ ] **A10** 默认粒度负向对照（单 module）
- [ ] **A11** 所有产物过 ADR-060 §2.5 lint 门（零 `%Error`）
- [ ] **A12** 契约同步（spec 合并 + ADR-062 反向指针 + ADR-060 补格）· `openspec validate --specs --strict` 通过
- [ ] **A13** 父 `input` 端口被拒（负向用例 + 诊断非空）
- [ ] **A14** 零孤儿（新增 `.cpp` 注册 CMake · 新增头被包含）

- [ ] **A15 ★ `ch_mem` 端口不得静默不发射**：父层 `ch_mem` 读/写端口的 `addr` / `en` / `data` 若取自子 `ch_out`
      ⇒ ⇒ 父 body MUST 出现对应的 `assign` / `always_ff` 语句 ⇒ ⇒ **判据 = 读值非 0**（ⓘ 只断言 lint 干净**不够**：
      `src/codegen_verilog.cpp:1266-1270` 在 `!node_names_.count(addr)` 时 **early-return**
      ⇒ ⇒ 整条 mem 读端口**静默不发射** ⇒ ⇒ 产物 **过 lint、值恒错**）；真实动机用例 =
      `include/cpu/pipeline/rv32i_pipeline.h:194-197`（`reg_file.aread(id_stage.io().rs1_addr_id, …)`）
- [ ] **A16 ★ 行 17 的 tripwire 必须在本 change 内定值**：`tests/test_rv32i_stage_dual.cpp:430-441`
      断言 `ImmConsistencyDut` **MUST 被拒**且错误串含 `implicit-port synthesis` ⇒ ⇒ 而该 DUT 的形态正是
      「父侧带运算」（`:258` `io().equal <<= (dec.io().imm == ids.io().imm)`）⇒ ⇒ 本 change 落地后
      **必然转红** ⇒ ⇒ 新期望 MUST 在本 change 内落地（`ok==true` + 产物过 lint，或改断言新归因）
- [ ] **A17 拒绝面不萎缩**：矩阵其余 **13 个 `Reject` 格**的格值与行为断言逐条不变；并新增负向用例
      **跨多层（孙组件）** 与 **表亲边** ⇒ ⇒ 二者 MUST 仍被拒（ADR-066 决策 ⑥ 边界）
- [ ] **A18 `PortPolicy::Reject` 档**：首错 MUST = **真正的父侧消费节点**（收窄兜底检查后 `front()` 的语义变了）
- [ ] **A19 ★ 删 `Broken` 态的替代不变量**（否则是**倒退**）：`row2_expr × {ch_out, ch_reg, ch_proxy, op}`
      四格产物 MUST 进入某个 **lint 集合**（扩展 `tests/hier_rtl_products.cpp` 或让护栏 lint 矩阵输出目录）
      ⇒ ⇒ 零 `%Error` ⇒ ⇒ 该护栏 MUST 带 `--self-test` 投毒
      （ⓘ `Broken` 是仓内**唯一**表达「`ok=true` 但产物不可编译」的态；只删态不补 lint 覆盖 ⇒
      未来同类缺陷会被记成 `Ok` ⇒ 而 `test_hierarchy_verilog.cpp` **一次 verilator 都不调用**）
- [ ] **A20 台账闭口**：§7 行 **16** `未决`→`已裁决`（证据 = ADR-066）· 行 **19** `未决`→`已收口`
      （证据 = 本 change 归档目录）⇒ 登记表同步 ⇒ `verify_arch_ledger_residuals.sh` **+ `--self-test`** 转绿
- [ ] **A21 真实库站点验收**：`ForceLayer` 下 `riscv::Rv32iPipeline`（**不是** SoC —— 避免跨模块 `ch_mem`
      提前拒绝）发射 `ok==true` + 产物过 lint ⇒ ⇒ 闭合 Why 的论证
- [ ] **A22 1-bit / `ch_bool`**：`logic` 声明形态（`logic` vs `logic [0:0]`）+ `==` / `|` 组合
      （真实动机：`rv32i_pipeline.h:398/401/402/506/524`）
- [ ] **A23 root-as-parent**：端点**已在父 module 本地可见**（inline 折叠 / 父自身端口）⇒ ⇒
      MUST NOT 造网（否则重复声明）⇒ 负向用例
- [ ] **A24 撞名诊断分家**：新路径撞名（`clashes_with_port`）MUST 报**自己的**诊断
      （MUST NOT 复用 `sibling wire … collides with an existing port` ⇒ 会**指错来源**）
