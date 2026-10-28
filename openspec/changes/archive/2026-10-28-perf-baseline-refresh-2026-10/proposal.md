# 提案：perf baseline MUST 与当前机器状态同步（重采集 + 守恒流程）

- **架构依据**：无
- **5 段草案**：`.rddf/improvements/perf-baseline-refresh-2026-10.md`
- **roadmap 归属**：工程纪律项
- **规格影响**：**有**（⚠️ **2026-10-28 实施期订正** —— 初稿写「无」⇒ **实测推翻**）
  —— MODIFIED ×1（`perf-test-isolation` 的「perf_main CTEST TIMEOUT accommodates
  wall-clock with safety margin」Requirement 的 TC-09 Scenario）⇒ 归档**不得**用
  `--skip-specs`。**订正的根因见下方「⚠️ 实施期发现：两条 Requirement 互相矛盾」**
- **规模 / 风险**：**中**（重采集会**抬高**阈值 ⇒ 靠锚点守恒 + 变松披露把「变松」显式化）

## Why

**实测证据（2026-10-27，四条）**：

1. **依赖闭包**：perf 源文件**零** `#include` 任何 `cpu/` 头，`.d` 也**零**命中
   `include/cpu` ⇒ perf 路径与被改的 `include/cpu/**` **完全无关**；
2. **分档形态**：回归只在 **regs=10（最小档）**；regs=100/1000 的比率仅 **1.06–1.15×**
   ⇒ 代码级回归会**每档同比劣化**；
3. **劣化集中在固定开销**：`build_us` **5198 → 16220 µs**（JIT 编译 / 进程启动）
   ⇒ 是**机器状态**；
4. **同二进制二次测量有抖动**：verilator 行 3070 → **2734 µs**（掉回 1.3× 阈值内）。

⇒ baseline（采集于 **2026-10-25 / git_sha 7c5038e**，另一台机器状态）**已脱节**
⇒ 当前 `perf_regression` 在最小档**恒假红** ⇒ **真回归会被噪声淹没**。

## What Changes

1. 在**空闲机器**上重采集 `tests/benchmark/perf_baseline.json`，走
   `scripts/verify_perf_baseline_anchors.sh` 的守恒流程：
   - **C1 行集守恒**（baseline 的行集合不得变）；
   - **C2 锚点守恒**：带 `note_restored` 标记的行 MUST 断言取值与旧基准**逐字相等**
     （⚠️ 该标记曾是「降级逃逸口」⇒ 加一个 JSON 字段就能把 PASS 锚点永久摘掉）；
   - **C3 变松披露**：新 baseline 相对旧 baseline **变慢**的行 MUST 在 `note` 里点名；
2. `note` 字段 MUST 记录：git_sha、采集日期、机器状态说明、**与旧 baseline 的比率**
   （沿用 2026-10-25 那次的做法：把「为什么这不是代码回归」的论证写进 note）。

⚠️ **纪律**：
- **单独批次** —— 不与任何其它改动同批 ⇒ 归因可分离；
- 采集期间**不得**有其它构建 / 测试在跑（本次实测教训：并发构建把 `perf_tests`
  拖到 **TIMEOUT**，那次 4 个测试全红是**环境**造成的）。
  ⇒ ⚠️ **本机是共享 4 核主机** ⇒ 采集窗口 MUST **串行独占**
  ⇒ 实施时 MUST 确认**没有**其它 `ctest` / `cmake --build` 在跑。

⚠️ **`git_sha` 的口径**（实施时 MUST 照此填，否则锚点会指向错误代码状态）：
baseline 与采集代码状态在**同一个 commit** 里 ⇒ `git_sha` 只能是**采集时的代码状态**
的 hash（即**父提交**的 hash）⇒ 与既有 baseline 一致
（现有 baseline 的 `git_sha = 7c5038e` = 采集时的代码状态，不是写入它的那个 commit）。
⇒ ⚠️ **不得**填「本 change 的 commit hash」（那个 commit 还没存在）。

## Acceptance

- [ ] `scripts/verify_perf_baseline_anchors.sh` exit 0（含其 `--self-test`）
- [ ] `ctest -L perf` **11/11**
- [ ] 新 baseline 的 `note` 含 git_sha / 日期 / 与旧 baseline 的比率 / 环境说明
- [ ] **行集守恒**：baseline 的 **(test_name, params, backend) 集合不变**
      ⇒ 当前 **53 行**（PASS 30 / UNSUPPORTED 9 / LEGACY 8 / SKIPPED 6）⇒ 集合不变
- [ ] **锚点守恒**：旧 baseline 里 **PASS 且 `median_us > 0`** 的 **30** 行
      ⇒ 新 baseline 里仍 `median_us > 0`
      ⇒ ⚠️ 当前有 **15 行 `median_us == 0`**（6 SKIPPED + 9 UNSUPPORTED）
      ⇒ `perf_regression` 的 `if (base <= 0.0) continue;` 会**跳过**它们
      ⇒ **这 15 行不可再增加**（增加 = 比较集合缩小 = 未来真回归不可见）
- [ ] `perf_regression` 仍比较 **18** 行且 **0 回归**
      ⇒ ⚠️ **口径澄清**：**18** 是 **perf_results 侧**的比较行数
      （TC-07 / TC-08 × interpreter/jit/verilator × 3 档规模 = 2×3×3），
      **不是** baseline 的行数（53）⇒ 初稿把两者混为一谈 ⇒ 已订正

## Capabilities

- `perf-test-isolation` —— ⚠️ **2026-10-28 实施期订正**：初稿判定「**无需 MODIFIED**
  （该 capability 已有 Requirement「perf baseline 重生成 MUST 守恒（行集 / 锚点 /
  变松披露）」⇒ 本次「重采集 + 走守恒流程」正是执行既有契约 ⇒ 不改契约）」⇒
  **该判定基于一个假前提**：它只看了「守恒流程已有契约」，没发现**同一 capability 内
  另有一条条款与之互斥** ⇒ 详见下方「⚠️ 实施期发现」⇒ 结论改为 **MODIFIED ×1**。

### ⚠️ 实施期发现：两条 Requirement 互相矛盾（2026-10-28 实测）

| # | Requirement | 条款 | 写入时间 |
|---|---|---|---|
| ① | `perf_main CTEST TIMEOUT accommodates wall-clock with safety margin` → Scenario「TC-09 subprocess timeout known limitation」 | 「this limitation is acceptable as long as **both baseline AND current** `perf_results.json` have TC-09 rows with `median_us=0`（so `perf_regression` **skips** the comparison）」 | F2 期（2026-06/07） |
| ② | `perf baseline 重生成 MUST 守恒（行集 / 锚点 / 变松披露）」 | 「基准里 `status=PASS` 且 `median_us > 0` 的**锚点 MUST 仍存在且仍 `> 0`**」 | 2026-10-25（随 C2 的守恒护栏一并加入） |

⇒ **实测事实**：仓内**已提交**的 baseline（2026-10-25 / `7c5038e`）里，TC-09 的
**6 个 interpreter/jit 锚点**就是 `PASS` 且 `median_us > 0`（从 2026-10-16 逐字恢复）
⇒ ① 要求它们变成 0，② 禁止它们变成 0 ⇒ **同一批行，两条条款给出互斥指令**。

⇒ 且 ② 是**可执行**的：`scripts/verify_perf_baseline_anchors.sh` 的 C2 对
「锚点降级为 0」**fail-closed**（`--self-test` fixture ⑤⑥ 钉住）⇒ 若按 ① 把 TC-09
降为 0，护栏必然报违规 ⇒ 「无 spec 变更 ⇒ 归档用 `--skip-specs`」的前提**不成立**。

⇒ **裁决**：以 ② 为准（它是**可执行**的契约；且锚点保留是**安全方向** —— 宁可误报
不可漏报），把 ① 的 TC-09 条款**改写为两种形态都合法**：

- **Shape A（锚点保留）**：baseline 侧 TC-09 锚点逐字保留（`median_us > 0` +
  `note_restored`），current 侧 `median_us = 0` ⇒ 比值为 0 ⇒ **无假阳性**；
- **Shape B（两侧皆空）**：两侧都 `median_us = 0` ⇒ 比较被跳过 ⇒ 无假阳性；

⇒ 并明确「若上一份 baseline 持有 TC-09 的 PASS 锚点 ⇒ **必须**走 Shape A」。

## Impact

- **文件面**：`tests/benchmark/perf_baseline.json`（+ 可能 `perf_regression.cpp` 的注释）
- **计数**：**不变**
- **风险**：**中** —— 重采集本身会**抬高**阈值 ⇒ MUST 靠 C2 锚点守恒 + C3 披露
  把「变松」**显式化**，避免把真回归一起放过

## Non-Goals

- 不改 perf 基准的**内容**（TC-01..TC-11 的定义不动）
- 不改阈值（1.6× / 2.5× / 1.3× 不动）
- 不在同批做任何**其它**改动
