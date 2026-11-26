# 验收报告 · `rv32i-dual-backend-obs-vector-guard-2026-11`（步 1 = 护栏先行）

> **角色**：builder 自验（本 change 的审查者 = `verifier` 角色内建；无 subagent 审查者）
> **判据来源**（按优先级）：① 用户 2026-11-26「直接按步 1 开第一个 change（护栏先行）」
> ② `proposal.md` 的 `## 验收标准`（AC1–AC8）③ change 自身声明 ④ 无 ADR 适用
> **已用轮次**：**1**（本轮 = 轮 1）
> **结论**：**PASS（8/8 AC）** · 遗留 = T4.3 归档（待用户确认）

---

## 1. 判据来源与前提

| 项 | 值 |
|---|---|
| 验收对象 | `scripts/lib/rv32i_obs_vector.py`（新）· `scripts/verify_dual_backend_parity.sh`（改）· `.rddf/checks/rv32i-obs-vector-registry.md`（新）· spec delta |
| 基线 | 无远端 ⇒ 以工作树 HEAD = `ad9fa2a` 为基线；本 change 全部为未提交改动 |
| 判据面 | **可观测输出**（脚本 4 行结论 + 退出码 + 自检输出 + 篡改实验），不采信「代码看起来对」 |

---

## 2. 逐条判定（每条都带工具证据）

### AC1 主路径 PASS 且输出 4 行判据结论 —— ✅

```
$ bash scripts/verify_dual_backend_parity.sh ; echo EXIT=$?
PARITY_COUNT=67
PARITY_PAIRS=297
...
=== 第二判据：跨后端观测向量 vs 核顶层 ch_out 端口面（rv32i-obs-vector）===
  ✅ C1 守恒：向量 28 + 登记 11 == 核顶层 ch_out 39
  ✅ C2 登记有效：11 条全部存在于核顶层 ch_out
  ✅ C3 登记不 stale：11 条均不在向量里
  ✅ C4 归属：向量 ∪ 登记 == 核顶层 ch_out（39 根）
  ✅ C2b 登记完整性：11 条的「为什么未纳入」/「归位条件」均非空
✅ rv32i-obs-vector: PASS（C1–C4 + C2b · 观测集合对着端口面守恒）
EXIT=0
```

### AC2 5 类投毒逐条必红 —— ✅

```
$ bash scripts/verify_dual_backend_parity.sh --self-test
self-test: 12/12 fixture 全绿          ← 既有分析器（原头注释写「10 项」已订正为 12）
  ✅ 基线 fixture 全绿（P=4 · V=2 · R=2）
  ✅ P1 向量删一项 ⇒ 红（命中 C1、C4）
  ✅ P2 登记编造端口 ⇒ 红（命中 C2）
  ✅ P3 登记删一行 ⇒ 红（命中 C1、C4）
  ✅ P4 新增端口未登记 ⇒ 红（命中 C1、C4）
  ✅ P5 补齐未清账 ⇒ 红（命中 C3）
✅ rv32i-obs-vector --self-test: PASS（基线 + 5/5 投毒各自必红）
```

### AC3 登记表与核顶层 `axil_*` 输出逐字一致 —— ✅

```
$ grep -cE '^\s*ch_out<' examples/riscv-mini/src/rv32i_core_top.h      → 39
$ python3 scripts/lib/rv32i_obs_vector.py --root . --check
  ✅ C1 守恒：向量 28 + 登记 11 == 核顶层 ch_out 39
  ✅ C4 归属：向量 ∪ 登记 == 核顶层 ch_out（39 根）      ← 集合相等（不是只比条数）
```
双向可判：删一行 ⇒ C1+C4 红（AC4-M2）；加编造名 ⇒ C2 红（AC2-P2）。

### AC4 篡改实验（三条 · **手工在真实文件上做 · 每次复原 · 串行**）—— ✅

| # | 投毒 | 结果 | 复原后 |
|---|---|---|---|
| **M1** | 向量删一项 `obs_data_addr` | `M1_EXIT=1` · `❌ C1 守恒失败：向量 27 + 登记 11 = 38 ≠ 39` · `❌ C4 …：obs_data_addr` | `EXIT=0` |
| **M2** | 登记表删一行 `axil_rready` | `M2_EXIT=1` · `❌ C1 …：28 + 10 = 38 ≠ 39` · `❌ C4 …：axil_rready` | `EXIT=0` |
| **M3** | `axil_awaddr` 进向量但**不清账** | `M3_EXIT=1` · `❌ C1 …：29 + 11 = 40 ≠ 39` · `❌ C3 登记 stale：axil_awaddr 已在向量里` | `EXIT=0` |

> ⚠️ **过程失误如实登记**：首次把 M2/M3 放在**两条并行 shell** 里跑 ⇒ 两者同时改同一批文件
> ⇒ 输出串味（M2 日志里出现 M3 的 C3 命中）。**已重做为串行**（上表即串行结果）。
> ⇒ ⇒ 教训：篡改实验 MUST 串行（同一批文件不可并发投毒）。

### AC5 锚点零变化 —— ✅

```
$ bash scripts/verify_test_counts.sh ; echo $?        → 0
（主路径仍 PARITY_COUNT=67 / PARITY_PAIRS=297；CTEST_TOTAL / VERILATOR_E2E_TOTAL /
  EXAMPLES_TEST_SOURCES 未动 —— 零新增测试、零新增 macro 调用）
```

### AC6 可达性零变化 —— ✅

```
$ bash scripts/verify_guard_reachability.sh
✅ R1 引用存在 + 可执行位 + gate 对齐 全绿（58 个被引用脚本 · 登记表双向 1:1）
✅ verify_guard_reachability: PASS
```
（**无新脚本** ⇒ 58 不变 ⇒ 台账无需重生成）

### AC7 文档链接 —— ✅

```
$ bash scripts/verify_docs_links.sh
✅ docs-links: PASS（L1 47 篇 · 链接 197 条 · 引用 178 处 · L2/L3 477 篇不扫）
```

### AC8 spec delta 合法 —— ✅

```
$ openspec validate rv32i-dual-backend-obs-vector-guard-2026-11 --strict
Change 'rv32i-dual-backend-obs-vector-guard-2026-11' is valid

$ bash scripts/verify_openspec_delta.sh
  R1 ✓ delta 正文无 change 名
  R2 ✓ delta 无占位符路径
  R3 ✓ MODIFIED 标题与主 spec 逐字一致      ← 关键：MODIFIED 必须逐字对齐
  R4 ✓ tasks.md 的 bash 块语法可达
OpenSpec delta 自检通过（4 条全绿）✓
```

---

## 3. 边界与错误路径（第 4 步）

| 路径 | 探测方式 | 结果 |
|---|---|---|
| **解析为空 ⇒ fail-closed** | 实跑（开发期）：向量解析 0 条时 `SystemExit("::error::fail-closed：向量解析到 0 条")` | ✅ 不是 PASS |
| **数组花括号残缺** | 实跑（开发期 M1 的首个形态：删 `};`）⇒ 旧非贪婪正则会吃到**别处**的 `}`（抓到 63 条无关串）⇒ 已改**花括号配平扫描** | ✅ 修后 = 返回 `[]` ⇒ fail-closed |
| **`//` 注释含 `/*`** | 实测：`test_rv32i_soc_dual_parity.cpp:10` 含 ``tests/**`` ⇒ 朴素「先剥块注释」会把声明整段吃掉 | ✅ 已改**单趟 tokenizer** |
| **登记表列空** | 判据 C2b（本轮新增） | ✅ 实测 11/11 非空 |
| **缺逻辑载体** | `[ -f "$OBS_ANALYZER" ] || exit 2` | ✅ fail-closed |

---

## 4. 遗留清扫（**本次范围内**）

| 项 | 状态 |
|---|---|
| TODO / 半截实现 | **无**（新文件内 grep `TODO|FIXME|XXX` = 0 命中） |
| 注释掉的代码 | **无** |
| 临时调试输出 | **无**（`--self-test` 的 `print` 是产品输出，不是调试残留） |
| 被 skip 的测试 | 不适用（零测试改动） |
| 未替换的 mock | 不适用 |
| 死代码 | 发现并**删除** 1 处：`VECTOR_RE`（改配平扫描后无消费者） |
| 范围外历史遗留 | 本 change 不碰（上一个 change 已收口 `tests/hier_corpus_registry.md` 等） |

**本次不做、已交用户**：T4.3 `openspec archive`（0 产品代码 + spec MODIFIED 需合并 ⇒ 何时归档由用户决定）。

---

## 5. 失败项与路由

**无失败项**（8/8 AC 全绿）。
