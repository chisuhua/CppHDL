# Tasks: rtl-productization-surface-2026-11

> **task 定义与判据的唯一权威** = `proposal.md`（§A 映射表 · §B task 清单 · 验收标准）。
> 本文只做**执行序 + 勾选点**，不重述判据。
> **delta 的唯一权威** = `docs/adr/ADR-072-rtl-productization-surface-and-boundary.md` **§2.8**
> （逐条照抄，MUST NOT 自由发挥）。

## 执行序（用户 2026-11-17 排定）

`B1·T7` → `B2·T2` → `B3·T3` → `B4·T4` → `B5·T15+T6`（同批）→ `B6·T10` →
`B7·T5` → `B8·T11/T13/T16/T9` → `B9·T12` → `B10·T8` → `archive` → `T14/T17`

**硬约束**

1. **T7 MUST 先于 T2–T6** —— 契约先行：delta 是实现的输入，不是产物。
2. **T15 MUST 与 T6 同批**（同一 commit）—— 删枚举后 `test_component_granularity.cpp:248-254`
   的 `static_assert` 与 4 个 `[bubble]` 用例同时失效 ⇒ 中间 commit 编译即红。
3. **T12 MUST 晚于 T3** —— 否则产物字节改两次。
4. **T14 / T17 MUST 晚于 archive** —— 闭口证据须指向 `openspec/changes/archive/<date>-<name>/`。
5. **T8 属 architect 域**（`ADR-058` / `ADR-061` 的决策 addendum）⇒ B10 处 MUST **切 architect**，
   builder 不得单方面改已签批 ADR 的决策内容。
6. **P-2（公开面前置登记）在 T2 之前**：`proposal.md` 的门表写「开工前，不是事后补记」
   ⇒ ⇒ T9 的登记动作 MUST 在 B2 开工前落一版草案（T9 本身在 B8 补齐 CHANGELOG 与终稿）。

---

## B1 · T7 — 两份 capability 的 spec delta（6 条）

- [x] `rtl-emit-portability`：**REMOVED**（`rtl-emit-manifest-v1`）+ **ADDED**（v2 + `port_table[]`）
- [x] `rtl-hierarchical-output`：**4 MODIFIED**（`:32` 层级粒度 · `:98` 无归属节点 ·
      `:240` 成层且非根端口 · `:402` 跨层端口策略）+ **1 REMOVED + 1 ADDED**（`:190` 悬空子输入）
- [x] MODIFIED 是**整条替换**：全部 Scenario 都要带上（`:240` 有 **8 个**），
      `:336-344` 末句「未驱动子 input MUST 冒泡」已改
- [x] 4 条**不需 delta** 的标题零命中（`:478` / `:518` / `:529` / `:637`）
- [x] `## Purpose` **不写进 delta**（`openspec archive` 不合并 ⇒ 归 T17）⇒ 已由 T17 手工写

验证：

```bash
bash scripts/verify_openspec_delta.sh rtl-productization-surface-2026-11
openspec validate rtl-productization-surface-2026-11
for f in openspec/changes/rtl-productization-surface-2026-11/specs/*/spec.md; do printf '%s ' "$f"; grep -c '^### Requirement:' "$f"; done
```

---

## B2 · T2 — 新增带根公开入口（只增不改）

- [x] `toVerilog(path, Component&, EmitOptions)` + `make_flat_module_view(Component&, bool)`
- [x] **49 处 ctx-only 调用点签名零改动**（`git diff` 中这些行不出现）
- [x] 内部改动（`make_flat_module_view` / `ModuleView` / `ModuleEmitter`）按带根语义改端口选择
- [x] **P-2 前置**：新增符号已登记（`public-api-surface.md`「新增公开符号」节 + `CHANGELOG` 一行）

验证：

```bash
cmake --build build -j"$(nproc)"
grep -rn "toVerilog(" src/ tests/ samples/ examples/ | wc -l
```

---

## B3 · T3 — CLI 切到带根入口

- [x] `src/rtl_emit/cli.cpp` 的产品面走带根路径
- [x] 产物端口表 == 产品端口表（逐字逐序）· 计数 = `2 + 根自有 IO 数`
      ⇒ 实测（`tools/cpphdl-emit-rtl` 自带设计：root 3 个 io）⇒ 产物 **5** 端口
      = clock / reset / prog_we / prog_data / result（后代 `top_leaf_data_out` 降为体内 `logic`）

验证：

```bash
cmake --build build -j"$(nproc)"
build/rtl_emit_cli --help
```

---

## B4 · T4 — manifest schema v1 → v2

- [x] 新增产品端口表维度（`port_table[]`）· `ports[]` **不变**（仍列全量）
- [x] `is_top_io` 退为**消费方筛选信号**（不再定义发射器行为）
      ⇒ 实测同一设计：`ports` = **7** · `port_table` = **5**（= 产物端口表，逐字逐序）

验证：

```bash
cmake --build build -j"$(nproc)"
./build/tests/test_rtl_manifest
```

---

## B5 · T15 + T6（**同批 · 一个 commit**）

- [x] **T15**：物理删除 `UndrivenPolicy::Bubble`（默认改 `Reject`）· `HierarchyPortOrigin::Bubbled`
      / `InlineFolded` · `PortProvenanceOrigin::Bubbled` / `InlineFolded` · `ensure_bubbled_port` ·
      `BubbledInput` / `bubbled_inputs`（→ `UndrivenInput` / `undriven_inputs`）· `dangling` 向量
- [x] **T15**：断言红线同步（`test_component_granularity.cpp` 的 `static_assert` ·
      `test_hierarchy_verilog.cpp` 的 5 个用例 / 2 处 `static_assert` · `test_hierarchy_analysis.cpp`）
- [x] **T15**：公开面「移除 / 改名」登记（登记表 1 行 + 机器探针 7 条 · 锚点 24→25 / 39→46）
- [x] **T6**：`flat_rtl_emit_check.py` F3 = **只消费** `port_table[]`（逐字逐序）·
      `verify_flat_rtl_emit.sh` 的 F3 文案 · `verify_hierarchical_rtl.sh` R6/R8 期望集
- [x] **T6**：冒泡靶子 → **同层两兄弟正向设计**（`two_siblings.v` · 仍恰好 3 个 `.v`）
- [x] **T6**：`tests/test_hierarchy_verilog.cpp`（bubble 用例 → 拒绝用例；provenance 五类 → 三类）·
      `tests/test_hierarchy_analysis.cpp`（golden 2 份 · 人工核对：冒泡端口消失 + inline 折叠端口降为体内）·
      `tests/test_hierarchy_equivalence.cpp`（可见集 = 根自有 `ch_out` · 逐字节比对改用**产品**扁入口）·
      `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp`（④ 改锚 manifest `port_table`）
- [x] **T6**：`emit_rtl_soc.cpp` 的 `--check-products` **零改动**即通过（其实测 `顶层 14/14`）
- [x] **修了一处** `suppress_driver_assign` 的边界：**体内** `ch_out`（非端口）MUST 仍发
      `assign out = <驱动网>`（否则被声明但永不驱动 ⇒ RV32I `dbg_id_pc` 第 70 拍起分歧）
- [x] **删除后 MUST 全量重建**（禁只重建被点名 target ⇒ 旧二进制假绿）
      ⇒ 实测 `cmake --build build` 后 `ctest -LE perf` = **255/255 通过**

验证（**AC18 口径已订正 · 用户 2026-11-17 裁决** —— 原写法把 `-e dangling` 与退役符号
并列 ⇒ ⇒ **字面不可满足**：`dangling_input` 是**未退役**的公开诊断契约）：

```bash
grep -rn -e 'UndrivenPolicy::Bubble' -e 'Origin::Bubbled' -e 'Origin::InlineFolded' \
     -e 'ensure_bubbled_port' -e 'BubbledInput' src/ include/ tests/ scripts/   # 实测 0
grep -rn 'dangling' src/hierarchy_verilog.cpp                                # 实测 0
bash scripts/verify_hierarchical_rtl.sh --self-test
bash scripts/verify_hierarchical_port_naming.sh --self-test
bash scripts/verify_flat_rtl_emit.sh
```

---

## B6 · T10 — 顶层端口判定的唯一来源

- [x] `tb_stub.cpp` / `test_rtl_emit_contract.cpp` / `flat_rtl_emit_check.py`
      **只消费** manifest 的产品端口表（`port_table[]`）
      ⇒ ⇒ `is_top_level_port()` / `has_top_level_port()` 两处**自推**已删除；
      `tb_stub` 的内部 wire 现在连「注释里点名」都不出现（更强的负向对照）
- [x] 判据 ② 改比 **带根产品**（`toVerilog(path, root)`）vs `port_table[].size()`
      —— ctx-only 扁平产物是**遗留 / 非产品**路径 ⇒ 拿它比 `port_table[]` 必然多算
- [x] 判据 ③ 的变异靶从 `ports[]` 换成 `port_table[]`（否则变异不再影响骨架 ⇒ 假绿）
- [x] 负向对照（实测）：把 `tb_stub` 的端口来源改回「从 `ports[]` 推（clock/reset ∪
      output ∪ `is_top_io`）」⇒ `test_rtl_tb_stub` **转红**（`top_leaf_din` 出场）⇒
      还原 + 重建后复绿

验证：

```bash
./build/tests/test_rtl_tb_stub
./build/tests/test_rtl_emit_contract
bash scripts/verify_flat_rtl_emit.sh
```

---

## B7 · T5 — 10 份独立 TB 的端口映射 + tsv / expectations 重登记

- [x] 10 份 TB 按新端口面重连（实测：**9 份**有后代镜像端口需删）
      · `counter` / `counter_simple` / `pwm`：删后代 `ch_out` 引脚与对应「同线相等」断言
      · `uart_tx` / `uart_rx` / `gpio` / `fifo_stream` / `quadrature_encoder`：同上
      · `axi4_lite`：删 S8 场景（8 条断言）+ 8 条 `logic s_*` 声明 + 8 条实例引脚
      · `riscv-mini/tb_rv32i_soc.sv`：实例按 manifest 的 `port_table[]` **逐字逐序**重建
        （96 根后代端口 → 0；未用输出重新分桶成 `sink1/2/32`）
- [x] `.rddf/checks/examples-rtl-sim-targets.tsv`：`axi4_lite` `min_checks` 45 → **37** ·
      `scenarios` 8 → **7**；`fifo_stream` 62 → **56** · `uart_tx` 34 → **32** ·
      `uart_rx` 22 → **20**（每行都写明「删的是哪些断言、为什么主语不存在」）
- [x] `riscv` 仍 `checks=24 fails=0` · `axi4_lite` `checks=37 fails=0`
- [x] **工具链绕行（B7 实测挖出，非本 change 引入）**：`verify_examples_rtl_sim.sh` 加
      `--output-split 0` —— `third_party/verilator` 的 **CMake** 构建没定义
      `CFG_CXXFLAGS_PCH_I`（上游 autoconf 在 `configure.ac:704-712` 定义为 `-include`）
      ⇒ ⇒ GCC 分支拿到**空串** ⇒ PCH 被当 linker input ⇒ 任何**触发 output-split** 的设计
      C++ 构建必失败。判别力：给 `counter` 加 `--output-split 200` 可**同型复现**
      ⇒ ⇒ ⚠️ **前向注记（2026-11-17 · 归档后追加）**：该绕行**已被移除** ——
      `verilator-pch-toolchain-fix-2026-11` 在**仓内 CMake 层**注入 `CFG_CXXFLAGS_PCH_I`
      修好了工具链（零 submodule 改动），并把护栏改回**默认** split（真实覆盖 PCH 路径）。
      ⇒ ⇒ **别把 `--output-split 0` 加回来**（那会把缺陷重新变成潜伏 CI 红）；
      依据 = `docs/developer_guide/verilator-integration.md` §「PCH 与自动 output-split」·
      判据 = `scripts/verify_verilator_pch.sh`

验证：

```bash
bash scripts/verify_examples_rtl_sim.sh
```

---

## B8 · T11 + T13 + T16 + T9

- [x] **T11** 用户面 / 架构文档同步（实测落点）
      · `05-component-design.md` **§5 入口表改成 3 行** + **§5.2.1 `ctx-only 入口：`** /
        **§5.2.2 `带根入口：`**（各自写明「顶层端口 = 哪个集合」）· §5.3.3 事实 3 ·
        §5.3.4 根 module 端口集合 · §5.3.5 默认值 `Reject`
      · `HIERARCHICAL-RTL-AUTHORING-GUIDE.md` D2 拆成两条入口（`ctx-only 入口：` /
        `带根入口：`）· D4 的「冒泡」→「发射直接失败」· 删「零冒泡端口」等陈旧措辞
      · `PORT-NAMING-GUIDE.md:106` 的 `rtl-emit-manifest-v1` → **v2**（+ 指向 `port_table[]`）
- [x] **T13** 产品面声明（`05-component-design.md` **§5.9**）：四件套表 · ISA 边界
      （**不含特权级** / **冻结是终态**，两串逐字可 grep）· 独立 TB 收尾骨架
      （`RESULT: checks=<N> fails=0` + `$fatal(1,...)` 非零退出）
- [x] **T13** `verify_examples_rtl_sim.sh` **新增判据⑧**（四件套 = manifest `port_table[]`
      == 产物端口表 **逐字逐序**；其余三条腿 = ③ / ⑤⑥ / ⓪）⇒
      层次化产物（`kind=soc`）按**根 module 名**取端口表（`--module top`）
- [x] **T13** 判别力自证**正/负成对**：`--self-test` 投毒 6（篡改 `port_table` 一个端口名）·
      投毒 7（产物里多插一个下沉端口）⇒ **各自转红**（实测 `7 条投毒各自转红`）
- [x] **T16** 账本锚点同步：本次**未增删 TEST_CASE** ⇒ `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL`
      等锚点**值不变**（`verify_test_counts.sh` / `verify_dual_backend_parity.sh` 均绿）
- [x] **T9** 公开面登记终稿：`public-api-surface.md`（移除 1 行搬到**正确章节** · 新增符号 2 行 ·
      行为变更 1 行 = CLI 卫生产物默认档）+ `docs/CHANGELOG.md` 5 条（T2 / T4 / T15+T6 / T13 / T12）

验证：

```bash
bash scripts/verify_test_counts.sh
bash scripts/verify_dual_backend_parity.sh
bash scripts/verify_public_api_visibility.sh
grep -rn 'ctx-only 入口：' docs/usage_guide docs/developer_guide
grep -rn '带根入口：' docs/usage_guide docs/developer_guide
```

---

## B9 · T12 — 卫生进默认档（仅 CLI 层）

- [x] `CliOptions::emit_hygiene` 默认 on + 逃生口 `--no-emit-hygiene`
      ⇒ 实测：默认产物 `lint_off` 计数 **0** + 有 `default_nettype none`；
      `--no-emit-hygiene` ⇒ `lint_off` **2** + 无 `default_nettype`（旧形态）
- [x] 库层 `EmitOptions{}` / `verilogwriter(ctx)` **不动**（`STATIC_REQUIRE(... == false)` 仍绿）
- [x] 护栏里的显式 `--emit-hygiene` **保留**（`verify_examples_rtl_sim.sh` / `verify_flat_rtl_emit.sh`）
- [x] **顺带修了一处投毒退化**：`verify_flat_rtl_emit.sh` 的 M1 原本往 `ports[]`（全量清单）
      里塞端口 ⇒ T6 之后 F3 只消费 `port_table[]` ⇒ 该投毒**恒绿**（无判别力）
      ⇒ 已改投 `port_table[]`（`--self-test` 复绿）

验证：

```bash
./build/tests/test_rtl_emit_hygiene
grep -rn 'STATIC_REQUIRE(ch::EmitOptions{}.emit_hygiene == false)' tests/
```

---

## B10 · T8 — dated addendum（**architect 域**）

> ⚠️ **已切 architect**（2026-11-17 · 用户口令「我现在切 architect」）⇒ ⇒ 交付后**切回 builder** 收尾。
> ⚠️ `ADR-058:187-191` 的「既有路径 MUST 逐字节不变」需**第二个** addendum。

- [x] `ADR-058` dated addendum（**§7** · v2.18）—— 3 条：① §2.3「既有路径」的**指称**精确化
      （= ctx-only 一族；新增带根入口**不**构成改动）② 选项摘录 `// 默认 Bubble` 陈旧
      ③ §2.2/§5「与扁平逐字节一致」的**比较基准** = 带根产品产物
- [x] `ADR-061` dated addendum（**§7** · v1.6.6）—— 4 条 + §3/§4/§5 处置清单
- [x] `ADR-060` dated addendum（**§7** · v1.22）—— **实施期扩充**（它是冒泡机制的**权威所有者**）
- [x] `ADR-062` dated addendum（**§7** · v1.8）—— **实施期扩充**（引用**已删除**的
      `Bubbled` / `InlineFolded` 枚举 ⇒ 符号级死链）
- [x] `ADR-066` dated addendum（**§6** · v1.4）—— **实施期扩充**（两处指针指向**已退役**章节）
- [x] 审稿记录落 `.rddf/reviews/ADR-058-061-060-d1-h-dated-addendum.md`
      （**oracle 1 轮 + critic 1 轮** · arch 档 · critic = **APPROVE_WITH_CONDITIONS** · **0 BLOCKER**）
      ⇒ oracle 的 1 BLOCKER（ADR-060 §7.2 与 D1-f 界定表 / AC17 矛盾）**已闭合**；
      critic 的 5 SHOULD_FIX + 1 NIT **逐条处置**（版本号碰撞 ×2 · 审稿文件落盘 ·
      proposal T8 措辞同步 · 「四类」口径 · 变更记录行位置）
- [x] **决策内容零变更**：Status 五份**保持 Accepted** · 既有决策句**逐字保留**（含已陈旧处）
      ⇒ ⇒ 只追加 dated 段 + 元数据行 + 变更记录行

---

## archive

- [x] `openspec archive rtl-productization-surface-2026-11`
      ⇒ 归档为 `openspec/changes/archive/2026-11-17-rtl-productization-surface-2026-11/`
- [x] 归档后确认主 spec 的 6 条已逐字合入 ⇒ 实测：`rtl-emit-portability` **+1 / −1**（v2 标题 + `port_table[]`）；
      `rtl-hierarchical-output` **+1 / ~4 / −1**（`openspec archive` 输出）；`openspec validate --specs`（非 strict）
      = **49 passed / 0 failed**

```bash
openspec archive rtl-productization-surface-2026-11 --yes
grep -c '^### Requirement:' openspec/specs/rtl-emit-portability/spec.md
grep -n 'port_table' openspec/specs/rtl-emit-portability/spec.md
```

---

## 归档后 · T14 / T17

- [x] **T14** 残差 id=23 闭口：`.rddf/checks/arch-residual-registry.md` +
      `docs/architecture/2026-11-02-hierarchical-rtl-output.md §7` **两侧同批**
      ⇒ 状态 = `已收口`（`--render` 生成的规范句，逐字节前缀匹配）· 闭口证据 =
      `openspec/changes/archive/2026-11-17-rtl-productization-surface-2026-11`
      ⇒ `verify_arch_ledger_residuals.sh` **PASS（28 行）** + `--self-test` **11 条投毒全绿**
- [x] **T17** 两处 capability Purpose 手工写：`rtl-emit-portability` 占位符 → 实质 Purpose
      （10 字段 + `port_table[]` 唯一权威 + `is_top_io` 退为筛选信号 + 确定性）；
      `rtl-hierarchical-output` 的「悬空子输入 MUST 向上冒泡」→「**未驱动的后代子输入 MUST 被
      响亮拒绝**（`ok=false` + 点名 + 零文件）」
- [x] T17 的判据口径 = **「历史否定式除外」**（`MUST NOT …冒泡…` 形态允许命中）
      ⇒ 实测主 spec 剩余命中 = `:253` / `:342` / `:407` / `:770` 与 Purpose 里的「已退役」句
      ⇒ 全部为**历史否定式 / 约束式**，无陈述式残留

验证：

```bash
bash scripts/verify_arch_ledger_residuals.sh
sed -n '1,8p' openspec/specs/rtl-emit-portability/spec.md
sed -n '10,16p' openspec/specs/rtl-hierarchical-output/spec.md
```
