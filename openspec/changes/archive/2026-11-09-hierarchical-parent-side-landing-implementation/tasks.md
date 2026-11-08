# Tasks: 父侧落点语义落地（ADR-066 决策 ①–⑥）

> TDD 5 步纪律：**先让矩阵格翻转的期望值落地（会红）→ 再实现 → 再跑**。
> AC 编号 = `proposal.md` 的 **A1–A24**（= improvement 的判据）。
> ⚠️ **T2 拆成两批**（`T2-α` / `T2-β`）⇒ 依据 = `design.md §7`（两桶结构：分析层把跨 module 边
> 分成 `pending_implicit_edges` 与 `analysis.edges` ⇒ **两者的合成通路不同**）⇒ ⇒ **顺序 MUST 为
> α → β**（反序 ⇒ 路径 B 的 `Ok` 格与坏产物并存 ⇒ 矩阵说谎）。

## T0 — 前置（✅ 已完成 · commit `d2c5903`）

- [x] T0.1 记录基线：`ctest -N | tail -1` = **253** · `ctest -N -L verilator_e2e | tail -1` = **77**
- [x] T0.2 记录矩阵基线：`tests/test_hierarchy_verilog` 的 42 格计数 = **16 / 17 / 6 / 3**
- [x] T0.3 记录 grep 基线：`grep -c "node_names_\[" src/codegen_verilog.cpp` = **62 行 / 64 次**
      （A8 面 ① 的**上界**；⚠️ 计数单位 = **出现次**）

## T1 — 判据先行（✅ 已完成 · 期望红已确认）

- [x] T1.1 矩阵：`row2_expr × ch_out` 由 `Reject` 改期望 `Ok`；`row2_expr × {ch_reg, ch_proxy, op}`
      由 `Broken` 改期望 `Ok` ⇒ **第四态 `Broken` 从 `enum` 与计数中移除**
- [x] T1.2 新增用例骨架：产物形态（A2）· 路径 B（A3）· 逐拍等价（A4）· 诊断（A5）· 宽度（A6）·
      单连接 + ①×④ 重叠（A7）· 父 `input` 拒绝（A13）
- [x] T1.3 跑测试 ⇒ **确认红**（红在 `REQUIRE(r.ok)` = 决策 ① 未实现）

## T2-α — 路径 B 的父侧落点（决策 ② · ✅ 已完成）

> 形态 = 父侧带运算消费子**非端口端点**（`ch_reg` / bundle 字段 / op 节点）⇒ ⇒ 走
> `pending_implicit_edges` 的「行 2」分支。

- [x] T2-α.1 按**父侧端点性质**分派：(a) 父 `output` ⇒ 直通不造网（决策 ③）· (b) 父 `input`
      ⇒ 响亮拒绝（决策 ① 排除项）· (c) 父体内节点 ⇒ **造网 + 放行**
- [x] T2-α.2 落点网名 = **驱动端点的 ctx 全局名**（MUST NOT 用 `names.at(e.to_ptr)` 自体名
      ⇒ 自指 + 双驱动）⇒ 父侧声明同名 `logic` ⇒ 父侧表达式 RHS **自动可解析**
- [x] T2-α.3 父侧网登记（`register_parent_wire`）：幂等 + 撞名报**自己的**诊断（A24）
- [x] T2-α.4 产物判据：三格（`ch_reg` / `ch_proxy` / `op`）成功 + 父侧声明落点网

## T2-β — 族 ①/③ 的父侧落点（决策 ① · ✅ 已完成）

> 形态 = 父侧带运算消费子 `ch_out` 端口 ⇒ ⇒ 这类边在**分析层落在 `analysis.edges`**
> （子侧**已就绪**）⇒ ⇒ 发射器此前**完全看不到它们** ⇒ ⇒ 「被拒」只是**诊断 pass 的缺省**。

- [x] T2-β.1 新增 pass：遍历 `analysis.edges` ⇒ 驱动端 MUST 是子 module 的 **output 端口**
      · 消费点 MUST 在父 module 内 ⇒ ⇒ 直通（父 `output`）⇒ 不造网 · 父 `input` ⇒ 拒绝 ·
      其余 ⇒ 造网
- [x] T2-β.2 父侧网 + `.port(net)` 连接（**按 pin 归并** ⇒ 决策 ⑤ 的扇出）
- [x] T2-β.3 **改接**分析层为该 pin 记的行（否则同一 pin 两次连接 ⇒ `Duplicate pin connection`）
      ⇒ ⇒ 父 `output` 端口改由显式 `assign` 取网的值（①×④ 重叠）
- [x] T2-β.4 诊断 collect MUST 跳过已合成的 pin ⇒ ⇒ 「拒绝面不萎缩」（A17）

## T3 — 契约同步

- [x] T3.1 spec delta 的**措辞订正**（Q6）：「MUST NOT 引用子 module 的内部信号名」
      ⇒ ⇒ 「MUST NOT 引用**仅在**子 module 内声明的名字」（旧措辞与「落点网名 = 端点全局名」**自相矛盾**
      ⇒ ⇒ 逐字执行会把正确实现判成违规）
- [x] T3.2 `docs/adr/ADR-060-...md` §2.1 **补格**（新增「子 → 父 · 父侧带运算」行 · 与第 2 行**并列存在**）
- [x] T3.3 `docs/adr/ADR-062-...md` §5 该条 + §2.6 落点表 ⇒ 加反向指针 `→ ADR-066 决策 ③ 为唯一权威`
- [x] T3.4 ADR-066 的**裁定修订**（Q2 拆批 / Q3 V8 判据 / Q4 + Q5 mem 范围 / Q6 措辞）
- [x] T3.5 `openspec validate --specs --strict` 通过（⚠️ 本仓 CLI 为 1.14.x ⇒ 既有 spec 的新规则 warning
      **与本 change 无关** ⇒ 只看本 change 的判定）

## T4 — 判别力自证（**两个面**各逐条）· ⚠️ **未全闭**：面 ① 的 62 处**零变异**
（命名真源唯一 ⇒ codegen 侧无需改动 ⇒ 「无需改动」的机械证据待补 ⇒ **登记为待办**）

- [ ] T4.1 **面 ①**：**名称解析语义点**各一条变异（父侧分派 / 网名取 `from_ptr` / `add_conn` 目标 /
      撞名诊断分家 / 诊断跳过集 ⇒ **5 个决策点**）⇒ 每条：编译确认 → 重建 → 红 → 还原 → 重建 → 绿
- [ ] T4.2 **面 ②**：ADR-060 R2 的**四条**路径（`print_output` proxy-chain walk · `print_input`
      driver walk · `print_proxy` `src(0)` · `print_reg` `get_next()`）各一条变异
- [ ] T4.3 记录变异表（改动位置 / 编译确认 / 重建命令 / 预期红 / 还原 + 重建 / 预期绿）
- [ ] T4.4 「62 处无需改动」的机械证据（grep 清单 + 负向注入 ⇒ lint 必红）

## T5 — 验证

- [x] T5.1 `cmake --build build -j$(nproc)` ⇒ 0 error（改动文件无新增 warning）
- [x] T5.2 **全量重建后** `ctest -LE perf --output-on-failure` ⇒ 全绿
- [x] T5.3 `ctest -N | tail -1` ⇒ 与 `CTEST_TOTAL`（若新增条目则同步更新锚点 + `verify_test_counts.sh`）
- [x] T5.4 `ctest -N -L verilator_e2e | tail -1` ⇒ 同步
- [x] T5.5 `bash scripts/verify_test_counts.sh` · `scripts/verify_hierarchical_rtl.sh` ·
      `scripts/verify_hierarchical_port_naming.sh` · `scripts/verify_hier_corpus_coverage.sh` ·
      `scripts/verify_arch_ledger_residuals.sh` ⇒ **全部转绿**
- [x] T5.6 台账 §7：行 **16** `未决`→`已裁决`（证据 = ADR-066）· 行 **17** `未决`→`已收口`
      （证据 = 本 change 归档目录）· 行 **19** `未决`→`已收口` ⇒ 登记表同步 ⇒ 护栏转绿
- [x] T5.7 行 **20**（`ch_mem` 端口实参跨层）：按 **Q4/Q5 裁定**处置（不纳入 ⇒ 改记「已裁决（不纳入）」
      + 另立 improvement；纳入 ⇒ 另开一个 change）⇒ 复核条件 MUST 指向该立项

## T6 — 归档

- [x] T6.1 `openspec validate hierarchical-parent-side-landing-implementation --strict` ⇒ 通过
- [x] T6.2 归档 **MUST NOT** 用 `--skip-specs`（本 change **有** spec 变更）
- [x] T6.3 commit：按路径显式 `git add <path>`（★ 禁 `git add -A`）⇒ 且**等用户确认**
