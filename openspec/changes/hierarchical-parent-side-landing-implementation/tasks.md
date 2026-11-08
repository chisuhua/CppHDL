# Tasks: 父侧落点语义落地（ADR-066 决策 ①–⑥）

> TDD 5 步纪律：**先让矩阵格翻转的期望值落地（会红）→ 再实现 → 再跑**。
> AC 编号 = `proposal.md` 的 **A1–A14**（= improvement 的判据）。

## T0 — 前置

- [ ] T0.1 记录基线：`cd build && ctest -N | tail -1` 与 `ctest -N -L verilator_e2e | tail -1`
      ⇒ 与 `AGENTS.md` 的 `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` 相等
- [ ] T0.2 记录矩阵基线：`tests/test_hierarchy_verilog` 的 42 格三态/四态计数（16/17/6/**3**）
- [ ] T0.3 记录 grep 基线：`grep -c "node_names_\[" src/codegen_verilog.cpp`（= A8 面 ① 的变异点数上界）

## T1 — 判据先行（期望红）

- [ ] T1.1 矩阵：`row2_expr × ch_out` 由 `Reject` 改期望 `Ok`；`row2_expr × {ch_reg, ch_proxy, op}`
      由 `Broken` 改期望 `Ok` ⇒ **第四态 `Broken` 从 `enum` 与计数中移除**
- [ ] T1.2 新增用例骨架：产物形态（A2）· 路径 B（A3）· 逐拍等价（A4）· 诊断（A5）· 宽度（A6）·
      单连接 + ①×④ 重叠（A7）· 父 `input` 拒绝（A13）
- [ ] T1.3 跑测试 ⇒ **确认红**（且红在**期望的断言**上，不是编译/夹具错误）

## T2 — 发射器实现

- [ ] T2.1 父侧网合成（复用网登记 + `add_conn` + `print_decl` + `clashes_with_port`；
      **资格判定按 ADR-066 决策 ① 重写**，不复用兄弟边资格判定）
      ⇒ **落点网名 = 驱动端点 ctx 全局名**（MUST NOT 用 `names.at(e.to_ptr)` 的自体名）
- [ ] T2.2 **父侧操作数名解析守卫**：`print_unary_op` / `print_mux` 等直接取 `node_names_[` 的位置
      ⇒ 覆盖 A8 面 ① 的**全部**命中点
- [ ] T2.3 诊断重写：删 `which this emitter cannot synthesize` + 点名**真正消费节点** + 给可执行改法
- [ ] T2.4 连接去重（同 `(实例, pin)` 相同网名幂等 · 不同网名合并为一根网扇出）
- [ ] T2.5 父 `input` 端口消费 ⇒ **拒绝**（决策 ① 排除项）

## T3 — 契约同步

- [ ] T3.1 `openspec/specs/rtl-hierarchical-output/spec.md` 合并 MODIFIED Requirement（change 归档时由 CLI 做）
- [ ] T3.2 `docs/adr/ADR-060-...md` §2.1 **补格**（新增「子 → 父 · 父侧带运算」行 · 与第 2 行**并列存在**）
- [ ] T3.3 `docs/adr/ADR-062-...md` §5 该条 + spec 的 MODIFIED delta ⇒ 加反向指针
      `→ ADR-066 决策 ③ 为唯一权威`
- [ ] T3.4 `openspec validate --specs --strict` 通过（⚠️ 本仓 CLI 为 1.14.x ⇒ 既有 spec 的新规则 warning
      **与本 change 无关** ⇒ 只看本 change 的判定）

## T4 — 判别力自证（**两个面**各逐条）

- [ ] T4.1 **面 ①**：`node_names_[` 的**全部**取址点各一条变异（变异点数 == T0.3 的 grep 计数）
- [ ] T4.2 **面 ②**：ADR-060 R2 的**四条**路径（`print_output` proxy-chain walk · `print_input` driver walk ·
      `print_proxy` `src(0)` · `print_reg` `get_next()`）各一条变异
- [ ] T4.3 每条：**先确认仍能编译** → **重建** → 观测红 → 还原 → **重建** → 观测绿
- [ ] T4.4 记录变异表（改动位置 / 编译确认 / 重建命令 / 预期红 / 还原 + 重建 / 预期绿）

## T5 — 验证

- [ ] T5.1 `cmake --build build -j$(nproc)` ⇒ 0 error / 0 warning（新增/改动文件）
- [ ] T5.2 **全量重建后** `ctest -LE perf --output-on-failure` ⇒ 全绿
- [ ] T5.3 `ctest -N | tail -1` ⇒ 与 `CTEST_TOTAL`（若新增条目则同步更新锚点 + `verify_test_counts.sh`）
- [ ] T5.4 `ctest -N -L verilator_e2e | tail -1` ⇒ 同步
- [ ] T5.5 `bash scripts/verify_test_counts.sh` · `scripts/verify_hierarchical_rtl.sh` ·
      `scripts/verify_hierarchical_port_naming.sh` · `scripts/verify_hier_corpus_coverage.sh` ·
      `scripts/verify_arch_ledger_residuals.sh` ⇒ **全部转绿**
- [ ] T5.6 台账 §7：行 **16** `未决`→`已裁决`（证据 = ADR-066）· 行 **19** `未决`→`已收口`
      （证据 = 本 change 归档目录）⇒ 登记表同步 ⇒ 护栏转绿

## T6 — 归档

- [ ] T6.1 `openspec validate hierarchical-parent-side-landing-implementation --strict` ⇒ 通过
- [ ] T6.2 归档 **MUST NOT** 用 `--skip-specs`（本 change **有** spec 变更）
- [ ] T6.3 commit：按路径显式 `git add <path>`（★ 禁 `git add -A`）⇒ 且**等用户确认**
