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
      ⇒ ⇒ ⚠️ **2026-11-09 实测订正**（ADR-066 **v1.4**）：**预期 = 分析层已响亮拒绝**
      （**不是**「分析放行 + 发射期静默丢端口」⇒ ⇒ **无**契约-实现缺口）⇒ ⇒ ⚠️ **禁止**把
      「现状 = 已拒绝」表述为「新增了拒绝能力」
- [ ] ★ 探针的**实质交付物** = **(i) 与 (ii) 的归因分开**：(i) 说「mem 本体跨 module」·
      (ii) 说「仅实参视线跨层」⇒ ⇒ ⚠️ 探针**仍必要**（删掉会失去归因登记载体 + 不放松护栏）
- [ ] 结果登记进 `.rddf/checks/arch-residual-registry.md` 行 20 的**归因描述** + 台账 §7
      （★ 该归因描述 MUST 含 **上条**的 (i)/(ii) 区分）

```bash
git grep -n "mem_port_impl" -- src/hierarchy_analysis.cpp
```

## S2 判别程序（三桶 + 归属比对）

- [x] `design.md` §2.1 的**独立扫描**（(i) 判定）⇒ `src/hierarchy_analysis.cpp` 的 `mem_port_impl` 扫描
- [x] `design.md` §2.2 的**三桶**（(ii) 放行条件 + 相邻性）⇒ 闸门改为 `allow_ii`（实参槽 + 直接父子）
- [x] 注释三桶化（权威链首环）
- [x] **回归**：`ctest -R hier` **4/4 通过**（既有 42 格矩阵 / 分析层 / 等价性未回归）
- [ ] ⚠️ **负向用例**（字面 (i) / (iii) MUST 仍被拒）**尚未写** ⇒ ⇒ **AC1 未验**
      ⇒ ⇒ ★ **2026-11-09 已确证**：`(i)` 的既有用例**已存在**（`tests/test_hierarchy_analysis.cpp:666`
      的 `MemTop`/`MemUser` = **桶 (i)**，mem 在父 · 端口在子）⇒ ⇒ S2 的独立扫描对**两个方向**的
      (i) 走**同一条扫描路径 + 同一文案** ⇒ ⇒ **(i) 无需**重做用例
      ⇒ ⇒ 只补 **(iii) 非相邻** + 「不放松」断言（AC1）
      ⇒ ⇒ ⚠️ **2026-11-09 metis 订正（判别力）**：既有 (i) 用例**只断言子串**
      `find("cross-module ch_mem")` ⇒ ⇒ **pre-scan 消息与边闸门消息都含该子串**
      ⇒ ⇒ **改前改后都绿 · 归因改错也绿** ⇒ ⇒ **零判别力** ⇒ ⇒ MUST **强化**
      （断言持有 module 名 + 使用 module 名 + 不含同层提示）
- [ ] ⚠️ **修正 `364e213` 引入的错误注释**（`src/hierarchy_analysis.cpp:277-279` 声称
      「字面 (i) **不产生任何跨 module 边** ⇒ ⇒ 闸门**不可达**」）⇒ ⇒ 与 ADR-066 **v1.4** **直接矛盾**
      ⇒ ⇒ **MUST** 改（本 change 自己的 commit 引入的缺陷 ⇒ ⇒ 写进本 change 才会在归档前被勾）

```bash
cd build && ctest -R hierarchy --output-on-failure
```

## S3 `(ii)` 放行 + 登记义务

- [x] **放行已生效**（S2 的三桶闸门）⇒ ⇒ ⚠️ **2026-11-09 实测**：父侧落点网**由既有机制自动合成**
      （产物内已有 `logic [3:0] <驱动端点全局名>;` 声明 + 子 output `.port(net)` + mem 端口读该名
      ⇒ ⇒ **命名恒等**）⇒ ⇒ ★ **本步**不需要写产品代码 ⇒ ⇒ 交付物 = **产物级断言**
- [ ] **AC4 声明断言**：产物内 mem 实参引用的名字 MUST 在父 module **有 `logic` 声明**
      （⇒ 否则 = implicit wire 1-bit ⇒ 宽度截断 ⇒ 值错）
- [ ] **正/负向用例**（(ii) ⇒ 发射成功）⚠️ **尚未写**
- [ ] **值判据（V21）**：读 `addr` 与写 `wdata`/`en` **两侧**断言**精确期望值**（**手推** ⇒ 非 0 只是下界）

```bash
cd build && ctest -R hier --output-on-failure
```

## S4 `en` fail-closed + 归因订正 + 注释同步

- [ ] `design.md` §2.3（两处共用 helper）
- [ ] **归因订正按分支**（⚠️ **2026-11-09 metis 订正**：原判据「grep 中文串『把 mem 与使用者放同层』」
      = **恒真** —— 该中文串全仓为 0 ⇒ ⇒ MUST 改判据）：
      - [ ] **(i) 支**（mem 本体跨 module）⇒ ⇒ 诊断**含** `memory and its user to be in the same module`
            （**合法保留** ⇒ ⇒ **MUST NOT** 一刀切删除）
      - [ ] **(ii)/(iii) 支**（mem 本体与使用者**已在**同层）⇒ ⇒ 诊断**不含**该句
- [ ] `design.md` §2.4 注释三桶化

```bash
# ⚠️ 判据 = **消息级断言**（两个用例各断言自己那条 message）⇒ ⇒ **禁**用全文件 grep 代替
git grep -n "memory and its user to be in the same module" -- src   # 定位点（**非**判据）
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
