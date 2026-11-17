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
- [ ] `## Purpose` **不写进 delta**（`openspec archive` 不合并 ⇒ 归 T17）

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
- [ ] **删除后 MUST 全量重建**（禁只重建被点名 target ⇒ 旧二进制假绿）

验证：

```bash
grep -rn -e 'UndrivenPolicy::Bubble' -e 'Origin::Bubbled' -e 'Origin::InlineFolded' \
     -e 'ensure_bubbled_port' -e 'BubbledInput' -e 'dangling' src/ include/ tests/ scripts/
bash scripts/verify_hierarchical_rtl.sh --self-test
bash scripts/verify_hierarchical_port_naming.sh --self-test
bash scripts/verify_flat_rtl_emit.sh
```

---

## B6 · T10 — 顶层端口判定的唯一来源

- [ ] `tb_stub.cpp:59` / `test_rtl_emit_contract.cpp:142` / `flat_rtl_emit_check.py:79-92`
      **只消费** manifest 的产品端口表
- [ ] 负向对照：任一处回到旧算法 ⇒ 必红

验证：

```bash
./build/tests/test_rtl_tb_stub
./build/tests/test_rtl_emit_contract
bash scripts/verify_flat_rtl_emit.sh
```

---

## B7 · T5 — 10 份独立 TB 的端口映射 + tsv / expectations 重登记

- [ ] 10 份 TB 按新端口面重连（`axi4_lite`：删 S8 场景 + 8 条 `logic s_*` 声明 + 8 条实例引脚；
      `riscv-mini/tb_rv32i_soc.sv`：删 96 根 sink）
- [ ] `.rddf/checks/examples-rtl-sim-targets.tsv`：`axi4_lite` `min_checks` 45 → **37** · `scenarios` 8 → **7**；
      两桶锚点**同 commit 重算**
- [ ] `riscv` 仍 `checks=24 fails=0` · `axi4_lite` `checks=37 fails=0`

验证：

```bash
bash scripts/verify_examples_rtl_sim.sh
```

---

## B8 · T11 + T13 + T16 + T9

- [ ] **T11** 用户面 / 架构文档同步：`05-component-design.md:552,680` ·
      `HIERARCHICAL-RTL-AUTHORING-GUIDE.md:49-55,88` · `PORT-NAMING-GUIDE.md:78`
      （+ `PORT-NAMING-GUIDE.md:106` 的 `rtl-emit-manifest-v1` 陈旧指称 → v2）
      ⇒ 必须出现 `ctx-only 入口：` 与 `带根入口：` 两个小标题
- [ ] **T13** 产品面声明 + `verify_examples_rtl_sim.sh` **新增判据⑧**（四件套）+ 正/负成对判别力自证
- [ ] **T16** 账本锚点同步（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` 等，值从实测派生）
- [ ] **T9** 公开面登记终稿（`public-api-surface.md` + `docs/CHANGELOG.md`）

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

- [ ] `CliOptions::emit_hygiene` 默认 on + 逃生口 `--no-emit-hygiene`
- [ ] 库层 `EmitOptions{}` / `verilogwriter(ctx)` **不动**
- [ ] 护栏里的显式 `--emit-hygiene` **保留**

验证：

```bash
./build/tests/test_rtl_emit_hygiene
grep -rn 'STATIC_REQUIRE(ch::EmitOptions{}.emit_hygiene == false)' tests/
```

---

## B10 · T8 — ADR-058 / ADR-061 的 dated addendum（**architect 域**）

> ⚠️ **需切 architect**（决策内容变更 · proposal 的 P-3 门亦写 `architect`）。
> ⚠️ `ADR-058:187-191` 的「既有路径 MUST 逐字节不变」需**第二个** addendum。

- [ ] `ADR-058` dated addendum（含 `:187-191` 的第二条）
- [ ] `ADR-061` dated addendum
- [ ] 审稿记录落 `.rddf/reviews/`

---

## archive

- [ ] `openspec archive rtl-productization-surface-2026-11`
- [ ] 归档后确认主 spec 的 6 条已逐字合入（`verify_openspec_delta.sh` 的等价证据 = 主 spec grep）

```bash
openspec archive rtl-productization-surface-2026-11 --yes
grep -c '^### Requirement:' openspec/specs/rtl-emit-portability/spec.md
grep -n 'port_table' openspec/specs/rtl-emit-portability/spec.md
```

---

## 归档后 · T14 / T17

- [ ] **T14** 残差 id=23 闭口：`.rddf/checks/arch-residual-registry.md` +
      `docs/architecture/2026-11-02-hierarchical-rtl-output.md §7` **两侧同批**，
      状态格由 `--render` 生成，闭口证据指向 archive 目录
- [ ] **T17** 两处 capability Purpose 手工写：`rtl-emit-portability` 占位符 → 实质 Purpose；
      `rtl-hierarchical-output:12-13` 「悬空子输入 MUST 向上冒泡」→「未驱动的**后代**子输入 MUST 被**响亮拒绝**」
- [ ] T17 的判据口径 = **「历史否定式除外」**（`MUST NOT …冒泡…` 形态允许命中）

验证：

```bash
bash scripts/verify_arch_ledger_residuals.sh
sed -n '1,8p' openspec/specs/rtl-emit-portability/spec.md
sed -n '10,16p' openspec/specs/rtl-hierarchical-output/spec.md
```
