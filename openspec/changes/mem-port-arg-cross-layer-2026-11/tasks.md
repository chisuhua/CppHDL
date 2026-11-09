# Tasks: memo —— mem 端口实参跨层（S0–S6）

> 顺序与判据 = `proposal.md` §「实施顺序」· 实现落点 = `design.md` §2。

## S0 基线

- [ ] 记录锚点：`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `ctest -LE perf` 全绿

```bash
cd build && ctest -LE perf --output-on-failure
cd .. && bash scripts/verify_test_counts.sh
```

## S1 探针（V20 先行）

- [ ] 字面 (i) 的运行期探针（父持 `addr` + `child.mem.aread()` + 成层发射）⇒ **现状登记**
- [ ] 结果登记进 `.rddf/checks/arch-residual-registry.md` 行 20 的**归因描述** + 台账 §7

```bash
git grep -n "mem_port_impl" -- src/hierarchy_analysis.cpp
```

## S2 判别程序（三桶 + 归属比对）

- [x] `design.md` §2.1 的**独立扫描**（(i) 判定）⇒ `src/hierarchy_analysis.cpp` 的 `mem_port_impl` 扫描
- [x] `design.md` §2.2 的**三桶**（(ii) 放行条件 + 相邻性）⇒ 闸门改为 `allow_ii`（实参槽 + 直接父子）
- [x] 注释三桶化（权威链首环）
- [x] **回归**：`ctest -R hier` **4/4 通过**（既有 42 格矩阵 / 分析层 / 等价性未回归）
- [ ] ⚠️ **负向用例**（字面 (i) / (iii) MUST 仍被拒）**尚未写** ⇒ ⇒ **AC1 未验**

```bash
cd build && ctest -R hierarchy --output-on-failure
```

## S3 `(ii)` 放行 + 登记义务

- [ ] 正/负向用例（(ii) ⇒ 发射成功；落点网名字在父 module **有 `logic` 声明**）⚠️ **尚未写**
- [ ] **值判据（V21）**：读 `addr` 与写 `wdata`/`en` **两侧**断言**精确期望值**

```bash
cd build && ctest -R hier --output-on-failure
```

## S4 `en` fail-closed + 归因订正 + 注释同步

- [ ] `design.md` §2.3（两处共用 helper）
- [ ] 被拒诊断的 `hint` 不含「把 mem 与使用者放同层」（当 mem 本体已在同层时）
- [ ] `design.md` §2.4 注释三桶化

```bash
git grep -n "把 mem 与使用者放同层" -- src
```

## S5 变异自证（V22）+ V18 复算

- [ ] 变异：**不登记落点网** ⇒ 产物转红（值判据 / lint）⇒ 先编译确认 ⇒ **还原后重建**
- [ ] `awk '/^### 2.7/,/^---/' docs/adr/ADR-066-*.md | grep -c '维持.*响亮拒绝'` == 0

```bash
awk '/^### 2.7/,/^---/' docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md | grep -c '维持.*响亮拒绝' || true
```

## S6 spec delta + 归档 + 台账闭口

- [ ] `specs/rtl-hierarchical-output/spec.md`（MODIFIED / ADDED）
- [ ] 归档 **禁** `--skip-specs`
- [ ] 台账行 20：`已裁决 → 已收口` · `归属` → `—` · `闭口证据` = archive 路径

```bash
bash scripts/verify_openspec_delta.sh
bash scripts/verify_arch_ledger_residuals.sh
bash scripts/verify_hierarchical_rtl.sh
```
