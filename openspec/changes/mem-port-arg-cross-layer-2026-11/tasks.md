# Tasks: memo —— mem 端口实参跨层（S0–S6）

> 顺序与判据 = `proposal.md` §「实施顺序」· 实现落点 = `design.md` §2。

## S0 基线

- [ ] 记录锚点：`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `ctest -LE perf` 全绿

```bash
cd build && ctest -LE perf --output-on-failure
cd .. && bash scripts/verify_test_counts.sh
```

## S1 探针（V20 先行）

- [x] 字面 (i) 的运行期探针（父持 `addr` + `child.mem.aread()` + 成层发射）⇒ **现状登记**
      ⇒ ⇒ **2026-11-09 实测**：**现状 = 分析层已响亮拒绝**（改前二进制 `364e213^` 抛点
      `src/hierarchy_analysis.cpp:544`）⇒ ⇒ **无**契约-实现缺口（ADR-066 **v1.4** 已订正）
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
- [x] ⚠️ **负向用例**（字面 (i) / (iii) MUST 仍被拒）⇒ ⇒ **AC1 已验**
      （(i) = `tests/test_hierarchy_analysis.cpp:666` **强化后**；⇒ ⇒ (iii) =
      `tests/test_mem_port_arg_cross_layer.cpp` 的「non-adjacent operand MUST still be rejected」）
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

- [x] `design.md` §2.3（两处共用 helper）⇒ ⇒ 实现 = `ModuleEmitter::resolve_mem_enable`
      （`include/codegen_verilog.h` 声明 · `src/codegen_verilog.cpp` 实现 · **读/写两侧共用**）
- [x] **归因订正按分支**（⚠️ **2026-11-09 metis 订正**：原判据「grep 中文串『把 mem 与使用者放同层』」
      = **恒真** —— 该中文串全仓为 0 ⇒ ⇒ MUST 改判据）：
      - [ ] **(i) 支**（mem 本体跨 module）⇒ ⇒ 诊断**含** `memory and its user to be in the same module`
            （**合法保留** ⇒ ⇒ **MUST NOT** 一刀切删除）
      - [ ] **(ii)/(iii) 支**（mem 本体与使用者**已在**同层）⇒ ⇒ 诊断**不含**该句
- [x] `design.md` §2.4 注释三桶化（`src/hierarchy_analysis.cpp` 的独立扫描块注释已按 v1.4 订正）

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

---

## S5 变异登记（**AC7 / V22** · 2026-11-09 实测 · **含无效变异**）

> ⚠️ **纪律**：每条变异 MUST **先确认仍能编译**（⇒ ⇒ 否则跑的是旧二进制 ⇒ **假绿**）
> ⇒ ⇒ **还原后 MUST 重建**（★「二进制也是状态」）⇒ ⇒ **无效变异 MUST 如实登记**。

| 变异 | 内容 | 结果 | 判定 |
|---|---|---|---|
| **M1** | 禁用 `(ii)` 放行（`allow_ii = false`）⇒ ⇒ (ii) 误拒 | **转红**（`test_mem_port_arg_cross_layer` 产物形态用例） | ✅ **有效** |
| **M2** | 不登记落点网（**并**把边推入 `remaining`）⇒ ⇒ 发射失败 | **转红**（发射 `ok=false`） | ✅ 有效，但红形态 = **fail-closed**（非「值错」） |
| **M3** | 不登记落点网（**不**推入 `remaining`）⇒ ⇒ 意图制造 implicit wire | **转红** —— ⚠️ **红形态是 `emit-failed`**：`collect` 诊断把「子 → 父 的父侧端点不是父 output 端口」判为无法映射 ⇒ ⇒ **「静默 implicit wire」失败模式在当前实现下不可达** | ✅ 有效 · ★ **重要发现**（见下） |
| **M4** | 读侧地址换成**同值**的父层网（`top__unnamed_input`） | **恒绿** | ❌ **无效变异**（**行为未变** ⇒ 不改变可观测行为 ⇒ 如实登记） |
| **M4′** | 读侧地址换成 **8-bit** 网（宽度不匹配） | 转红，但红形态 = **verilator 编译失败** | ⚠️ 有效但形态非目标 |
| **M5/M5′** | DUT 把子 `addr_in` 接到**未驱动**输入 | 转红，红形态 = `no-varying-signal`（harness 先触发） | ⚠️ 有效但形态非目标 |
| **M6** | DUT 把 `addr` **偏移 1**（产物合法 · 两侧**同错**） | ★ **精确转红**：`cycle 0 · addr = 3 · expected = 0xd3 · got = 0xd4` ⇒ ⇒ **且 `res.equal` 仍为 `true`** | ✅ **有效 + 目标形态** |
| **M7** | (i) 支的**归因改错**（去掉 same-module 语义句） | **转红**（`test_hierarchy_analysis.cpp:691` 的归因断言） | ✅ **有效**（证明 AC1 的强化断言**有判别力**） |

### ★ M6 的**承重结论**（AGENTS.md 的「同构缺陷」警告得到实证）

`M6` 让**两侧一起错**（同一份 C++ 生成 ⇒ 扁平侧也偏移 1）⇒ ⇒ **「层次化 ≡ 扁平」的断言
仍为 `true`** ⇒ ⇒ ⇒ **手推基准**（`0xD0 + addr`）**是唯一**能抓住这一族缺陷的判据
⇒ ⇒ 这实证了 AGENTS.md 的「两后端一致不能当作正确性证据」⇒ ⇒ 也实证了
`test_mem_port_arg_cross_layer_values.cpp` 里**手推逐拍断言**的**必要性**（不是冗余）。

### ★ M3 的**承重结论**（ADR-066 v1.3 §5 V22 的措辞需订正）

V22 原文预期「不登记落点网 ⇒ 读侧 = `mem[<子侧全局名>]` ⇒ **`%Error`**」⇒ ⇒ **实测**：
`collect` 诊断会先把这条边判为「无法映射到 module 端口」⇒ ⇒ **发射直接失败**（`ok=false`）
⇒ ⇒ ⇒ 「静默 implicit wire ⇒ 宽度截断 ⇒ 值错」这一族**在当前实现下不可达**
⇒ ⇒ **V22 的正确形态** = 「移除登记 ⇒ 产物 MUST 转红（**fail-closed**）」+ 「值判据抓**值错**」
（后者由 M6 证明）⇒ ⇒ ⚠️ 该订正属 **architect 域**（ADR 修订）⇒ ⇒ 登记为待办。

---

## AC 状态（2026-11-09）

| AC | 状态 | 证据 |
|---|---|---|
| **AC1** | ✅ | (i) = `test_hierarchy_analysis.cpp:666`（**已强化**：断言持有/使用 module 名 + same-module 语义）· (iii) = `test_mem_port_arg_cross_layer.cpp` |
| **AC2** | ✅ | 判别程序 ①②③ 在 `src/hierarchy_analysis.cpp` 的独立扫描（注释已按 v1.4 订正） |
| **AC3** | ✅ | `test_mem_port_arg_cross_layer_values.cpp`（**产物级** · 逐拍手推）+ `test_mem_port_arg_cross_layer.cpp`（解释器级 · 读/写两侧） |
| **AC4** | ✅ | `test_mem_port_arg_cross_layer.cpp`（父 body 内 MUST 有显式 `logic` 声明 + mem 端口 MUST 读该网 + **反向** `_not_declared` 必不命中） |
| **AC5** | ✅ | `ModuleEmitter::resolve_mem_enable`（合法缺省 / fail-closed 两分支分开） |
| **AC6** | ✅ | (i) 支保留 same-module 句 · (iii) 支改用「operand crosses a module boundary」措辞 |
| **AC7** | ✅ | S5 变异登记（M1 / M3 / M6 / M7 有效 · **M4 无效已如实登记**） |
| **AC8** | 🟡 **部分** | 已覆盖：孙组件仍拒（(iii)）· 父层自身节点不误报（既有用例）· 读写不同侧（读/写两个 DUT）⇒ ⇒ **未覆盖**：多 `ch_mem` · `sread` 类 · **语料登记**（`hier_corpus_registry.md`）· **决策 ⑤ 共网用例** |
| **AC9** | ⬜ **未做** | spec delta + 归档（禁 `--skip-specs`） |
| **AC10** | 🟡 **部分** | `ctest -LE perf` **246/246 绿** · 4 锚点已同步（257 / 77 / 66 / 290）· 护栏全绿 ⇒ ⇒ **未做**：台账行 20 闭口（依赖 AC9 的 archive 路径） |
