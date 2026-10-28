# Design: perf-baseline-refresh-2026-10

> 本 change 交付**一份数据文件的重采集**（`tests/benchmark/perf_baseline.json`）
> + **一处契约裁决**（MODIFIED ×1）⇒ **有** spec 变更 ⇒ 归档**不得**用 `--skip-specs`。

## 1. 问题

`perf_regression`（`tests/benchmark/perf_regression.cpp`）是仓内唯一的**性能回归闸门**。
它的判据是「current / baseline > 该 backend 的阈值 ⇒ 报回归」，阈值从**权威源**
`kBackendThresholds` 读出（interpreter 2.5× / jit 1.6× / verilator 1.3×）。

⇒ 闸门的**检出力 = baseline 的时效性**：baseline 记录的机器状态越旧，
同一份代码跑出来的比值就越可能越过阈值 ⇒ **假红**，真回归被噪声淹没。

实测（2026-10-28，本 change 之前）：`ctest -L perf` **10/11**，唯一失败 = `perf_regression`
（`TC-08|regs=1000|verilator` 2338.13 → 3281.32 µs，比率 **1.403** > 阈 1.3）。

⇒ 本 change 的目标：**在一台空闲机器上重采集 baseline**，让闸门重新有意义。

## 2. ⚠️ 实施期发现：同一 capability 内两条 Requirement 互相矛盾

### 2.1 两条条款

| # | Requirement | 条款（要点） | 写入时间 |
|---|---|---|---|
| ① | `perf_main CTEST TIMEOUT accommodates wall-clock with safety margin` → Scenario「TC-09 subprocess timeout known limitation」 | 「this limitation is acceptable as long as **both baseline AND current** `perf_results.json` have TC-09 rows with `median_us=0`（so `perf_regression` **skips** the comparison — no false-positive）」 | F2 期（2026-06/07） |
| ② | `perf baseline 重生成 MUST 守恒（行集 / 锚点 / 变松披露）」 | 「基准里 `status=PASS` 且 `median_us > 0` 的**锚点 MUST 仍存在且仍 `> 0`**」 | 2026-10-25（随 C2 的守恒护栏一并加入） |

### 2.2 为什么它们**互斥**（实测）

仓内**已提交**的 baseline（2026-10-25 / `git_sha=7c5038e`）里，TC-09 的
**6 个 interpreter/jit 锚点**正是 `status=PASS` 且 `median_us > 0`
（从 2026-10-16 的 baseline 逐字恢复，打了 `note_restored` 标记）：

```
TC-09 depth=10   interpreter  median_us=82796.151   status=PASS   note_restored
TC-09 depth=10   jit          median_us= 4873.126   status=PASS   note_restored
TC-09 depth=100  interpreter  median_us=786479.762  status=PASS   note_restored
TC-09 depth=100  jit          median_us=45310.800   status=PASS   note_restored
TC-09 depth=1000 interpreter  median_us=8489177.9   status=PASS   note_restored
TC-09 depth=1000 jit          median_us=484189.400  status=PASS   note_restored
```

⇒ ① 要求这 6 行变成 `median_us=0`，② 禁止它们变成 0 ⇒ **同一批行，两条条款给出互斥指令**。

### 2.3 为什么「按 ① 执行」在实践上**不可行**

② 是**可执行**的契约：`scripts/verify_perf_baseline_anchors.sh` 的 C2 对
「旧 baseline 里 PASS/median>0 的行在新 baseline 变成 `median_us=0`」**fail-closed**
（报违规 ⇒ 非零退出码 ⇒ CI 必红），且 `--self-test` 的 fixture ⑤⑥ 钉住这一点
（「`note_restored` 不是降级许可证」「恢复的取值必须逐字相等」）。

⇒ 若按 ① 把 TC-09 降为 0 ⇒ **护栏必然报违规** ⇒ 本 change 的验收不可能通过
⇒ ⇒ 初稿「无 spec 变更 ⇒ 归档用 `--skip-specs`」的**前提不成立**。

### 2.4 裁决

**以 ② 为准**，并把 ① 改写为**两种形态都合法**：

| 形态 | baseline 侧 | current 侧 | `perf_regression` 的行为 | 何时必须用 |
|---|---|---|---|---|
| **Shape A（锚点保留）** | TC-09 锚点逐字保留（`median_us > 0` + `note_restored`） | `median_us = 0`（SKIPPED） | 比值 = 0 ⇒ **不报回归** | **当上一份 baseline 持有 TC-09 的 PASS 锚点时（本次即是）** |
| **Shape B（两侧皆空）** | `median_us = 0` | `median_us = 0` | `base <= 0` ⇒ **跳过比较** | 锚点从未有过测量时 |

⇒ 两条形态都满足 ① 的**目的**（no false positive），差别只在**机制**；
⇒ Shape A 额外保留了「未来的 TC-09 真回归**可见**」这一 ② 的核心价值
⇒ ⇒ 改写后的条款**同时**满足 ① 的目的与 ② 的义务 ⇒ 契约与实现**逐字一致**。

⚠️ **取舍如实登记**：Shape A 的锚点记录的是**旧机器状态**（2026-10-16）
⇒ 若机器再变慢，这 6 行会**严格报红**（宁可误报不可漏报）—— 这是**安全的方向**，
且 2026-10-25 的 baseline 已经这样做过一次（其 note 里已有同样的告警）。

## 3. 采集与守恒流程

### 3.1 采集

- **独占窗口**：共享 4 核主机 ⇒ 采集期间不得有其它 `ctest` / `cmake --build`
  （2026-10-28 的教训：并发构建把 `perf_tests` 拖到 TIMEOUT，那次 4 个测试全红
  是**环境**造成的）；
- 采集命令 = ctest 的 `perf_tests` 条目（**不是**手敲二进制）⇒ 编译选项、
  `--verilator=` 注入、`--report=json` 与 CI **完全一致**；
- ⚠️ 只跑 `perf_tests`（不跑整条 `-L perf`）⇒ 避免 `perf_three_way` 用它自己的
  18 行结果**覆盖** `perf_results.json`（两者共享 `RESOURCE_LOCK perf_results_file`）。

### 3.2 转换（`/tmp/opencode/make_baseline.py`，一次性工具，非入库产物）

1. 读采集结果 `perf_results.json`（53 行）；
2. 旧 baseline 里 PASS 且 `median_us > 0`、而新结果 `median_us == 0` 的行
   ⇒ **逐字恢复**旧行（保留全部字段）+ 打 `note_restored` 标记（**Shape A**）；
3. 逐行计算比率 ⇒ 超过 backend 阈值的行进 note 的**披露表**
   （格式 `<test> <params> <backend> old= new= ratio=`，**数值必须与实际一致**）；
4. 写出新的 `perf_baseline.json`（`version` / `timestamp` / `git_sha` / `runs` / `note`）。

⚠️ **披露表与「逐行比率表」必须用不同形状**：护栏的 C3 用**行形状**识别披露表行
（`<test> <params> <backend> old= new= ratio=`）⇒ 若逐行比率表也用同一形状，
删掉披露表行后护栏仍能在逐行表里找到同一行 ⇒ **披露缺失变成不可见**
⇒ 逐行表刻意用 `old_us=` / `new_us=` / `ratio_vs_old=`。

### 3.3 `git_sha` 的口径

`git_sha` = **采集时的代码状态**（= 采集时的 HEAD），**不是**写入 baseline 的那个
commit（后者此刻还不存在 ⇒ chicken-and-egg）。

实测旧 baseline 遵守同一口径：`git_sha=7c5038e`（采集时的 HEAD），
而把它写进仓的 commit 是 `90d335d`。

⚠️ 该口径与 `perf-test-isolation` 里「The regenerated baseline MUST use `git_sha`
matching the commit hash」**字面不同** ⇒ 但那条 Requirement 的触发条件是
「**方法学变更**」(a)-(e) ⇒ 实测 (a)-(e) 自 `7c5038e` 以来**全部未变**
（`perf_main.cpp` / `subprocess_runner.h` / `verilator_runner.h` /
`perf_regression.cpp` 阈值；`BUILD_VERILATOR` 仍 `ON`）⇒ 本次重采集
**不是**方法学变更触发的 ⇒ 该 Requirement 的 `git_sha` 条款**不适用**
⇒ 本次触发的是它的**守恒** Requirement（无条件适用：「WHEN perf_baseline.json
被重生成」）。

## 4. 实测证据

> 本节的数值**全部**来自本次采集与脚本输出（不手抄、不从仿真输出反推期望值）。

### 4.1 采集（3 个独立样本，同一条 ctest 条目 = 与 CI 同旗标）

| 样本 | 起始（UTC） | wall-clock | `perf_tests` 用时 |
|---|---|---|---|
| #1 | 2026-10-28T08:06:17Z | 15m09s | **908.90 s** |
| #2 | 2026-10-28T08:27:38Z | 15m00s | **899.87 s** |
| #3 | 2026-10-28T08:44:39Z | 14m23s | **863.20 s** |

每次采集前实测：load average 1.52/1.77/2.19（4 核）、`ps` 中**零**
`ctest`/`cmake`/`cc1plus`、cgroup `cpu.cfs_quota_us = -1`（无限额）且
`cpu.stat` 的 `nr_throttled = 0`、`/proc/stat` 采样 `steal = 0.00%`。

### 4.2 逐行中位数 vs 旧 baseline（唯一需披露的行）

| 行 | 旧 | 新（中位数） | 比值 | 阈值 | 判定 |
|---|---|---|---|---|---|
| `TC-11 regs=10 jit` | 49654.42 | **99906.55** | **2.012×** | 1.6× | ⚠️ **变松 ⇒ 已披露** |
| `TC-11 regs=10 interpreter` | 49816.16 | 93814.80 | 1.884× | 2.5× | 未超阈 |
| `TC-11 regs=100 jit` | 449882.02 | 494390.00 | 1.099× | 1.6× | 未超阈 |
| `TC-11 regs=1000 interpreter` | 4368541.53 | 6667267.20 | 1.526× | 2.5× | 未超阈 |
| `TC-07 depth=1000 interpreter` | 2359882.75 | 2462895.30 | 1.044× | 2.5× | 未超阈 |
| `TC-08 regs=1000 verilator` | 2338.13 | 2413.68 | 1.032× | 1.3× | 未超阈 |

⇒ **只有 1 行**超过阈值 ⇒ 披露表 1 行（护栏 C3 已核对 old/new/ratio 数值一致）。

### 4.3 ⚠️ 抖动行：**离散度 > 阈值**（2 行，且是**既有**性质）

| 行 | 三个样本 | 离散度 | 阈值 | 旧 baseline 值 | 新 baseline 值 |
|---|---|---|---|---|---|
| `TC-08 regs=100 verilator` | 2413.7 / 2409.8 / **3935.7** | **1.633×** | 1.3× | 2392.33 | 2413.68（1.009× 旧值） |
| `TC-11 regs=10 jit` | 99906.5 / 102630.8 / **54567.4** | **1.881×** | 1.6× | 49654.42 | 99906.55（2.012× 旧值） |

⇒ 这两行**双模**。⚠️ **「离散度」与「仍会报假回归」是两个不同的问题** ——
「仍会假回归」取决于**基线取在哪一��**（只有**高于**基线的采样才会触发门禁）：
- `TC-08 regs=100 verilator`：中位数落在**快模**（2413.7）⇒ 慢模 3935.7 的
  最坏比值 **1.631×** > 阈 1.3× ⇒ ⇒ **该行仍会报假回归**；
- `TC-11 regs=10 jit`：中位数落在**慢模**（99906.5）⇒ 快模 54567.4 的比值
  **0.546×**（< 1，不触发）⇒ ⇒ **该行不会报假回归**。

⇒ ⇒ **新 baseline 下仍会假回归的行 = 1 行**（`TC-08 regs=100 verilator`）。
⇒ ⚠️ 该行在**新旧 baseline 下的门禁行为几乎相同**（旧值 2392.3 → 新值 2413.7
= 1.009×）⇒ ⇒ **该抖动是既有性质**（阈值表假定「verilator 噪声 ≈ JIT 噪声」，
实测不成立）⇒ **改阈值是本 change 的 Non-Goal** ⇒ 已在 baseline 的 `note` 与
`tests/benchmark/CMakeLists.txt` 的注释块里**如实登记**，
并已立项 `.rddf/improvements/perf-verilator-threshold-recalibration-2026-10.md`。

⇒ **中位数为什么落在多数模**（这是选择中位数而非 min 的**理由**，不是偏好）：
`TC-11 regs=10 jit` 的多数模是**慢模**（中位数 99906.5）⇒ 快模给出 0.546×、
慢模给出 1.027× ⇒ **两种模都不报回归** ⇒ 闸门对该行**不抖**。
（若取快模 54567.4 作基线 ⇒ 慢模的比值 1.881× > 1.6× ⇒ **必然假回归**。）

### 4.4 两个被**证伪**的推断（保留在案，避免后人重走）

1. 「样本 #1 的 `TC-11 regs=10` 是离群值」⇒ **被样本 #2 证伪**（#2 也测到
   93814.8 ⇒ 慢模**可复现**）⇒ 后来样本 #3 又测到快模 54567.4 ⇒ 真相是**双模**，
   不是「某个样本离群」。
2. 「独立跑 `perf_tests --tc=11` 与 `--all` 测的不是同一个东西」（因为 `--all`
   在 TC-11 之前 in-process 跑了 TC-01/02/04/06 并派生了 9 个子进程 ⇒ 父进程
   状态不同）⇒ **被样本 #3 证伪**（`--all` 同样测到 53.5 ms ⇒ 独立跑只是恰好
   落在快模 ⇒ **命令不是判别因子**）。
   ⇒ 但仍**全部**用 CI 的同一条 ctest 条目采集 ⇒ 无需任何路径等价的推断。

### 4.5 守恒验收（实测）

```
$ bash scripts/verify_perf_baseline_anchors.sh
  ::notice C3 使用的阈值：interpreter=2.50, jit=1.60, verilator=1.30（来源：权威源 …）
  ::notice 锚点变慢的行均已在 note 的披露表中逐行点名且数值一致（1 行）
  OK：行集守恒 + 锚点守恒 + 变松披露 全部通过
✅ verify_perf_baseline_anchors: PASS
```

⇒ **C1 行集**：53 = 53 ⇒ 集合逐项相同；
⇒ **C2 锚点**：旧 baseline 的 **30** 个 PASS 锚点全部仍 `median_us > 0`
（其中 6 个 TC-09 锚点为**逐字恢复** ⇒ 取值与旧 baseline 完全相同）；
⇒ **C3 披露**：1 行已点名且数值一致；
⇒ **零值行数**：旧 **15** → 新 **15**（**未增加** ⇒ 比较集合未缩小）。

## 5. 已知边界

| 边界 | 说明 |
|---|---|
| **TC-09 仍不可测** | `subprocess_runner.h:168` 的 `timeout_sec = 180` 对 `ch_uint<32>` 算术链仍不够 ⇒ 6 个锚点走 Shape A（逐字保留）⇒ 记录的是 2026-10-16 的机器状态 ⇒ 机器变慢时**偏严**（宁可误报不可漏报） |
| **TC-11 的 verilator 3 行** | `verilator harness not implemented` ⇒ 长期 SKIPPED（`median_us=0`）⇒ 不在比较集合内 |
| **TC-10 的 9 行** | `ch_float not implemented` ⇒ UNSUPPORTED ⇒ 不在比较集合内 |
| **本机是共享 4 核** | 采集的**独占**只能保证「没有并行的 ctest / build」⇒ 机器**上的其它租户**无法排除 ⇒ 采集后 MUST 核对逐行比率是否**同向**（真代码回归会同向且稳定；环境漂移是波动） |
| **抖动行（离散度 > 阈值）** | 2 行：`TC-08 regs=100 verilator`（1.633×）与 `TC-11 regs=10 jit`（1.881×）⇒ 但**仍会假回归的只有前者**（后者的中位数落在慢模 ⇒ 快模比值 0.546 < 1）⇒ 前者属**既有**性质（阈值表假定 verilator 噪声 ≈ JIT 噪声，实测不成立）⇒ **改阈值是 Non-Goal** ⇒ 只如实登记 + 已立项 `perf-verilator-threshold-recalibration-2026-10` |
| **TC-09 的锚点记的是 2026-10-16 的机器状态** | 机器再变慢时那 6 行会**严格报红**（宁可误报不可漏报）⇒ 安全方向 |
