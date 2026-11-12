---
evidence: probe:scripts/measure_verilator_e2e_wallclock.sh
---

# CI 墙钟实测：`pr-verilator-fast` 的 verilator 缓存 warm vs cold（2026-11-03）

- **测的人**：build agent（builder role）
- **属于**：C2（`verilator_e2e` 套件的 CI 墙钟账）
- **判据归属**：**结论与取值决策归 planner**；本文只交数字 + 复现命令 + 判别力边界。
- **口径** = CI 的同一条命令、同一个 2 分片串行方式：

```bash
cd build
ctest --output-on-failure --no-tests=error -L verilator_e2e -LE perf -I <shard>,,2   # shard ∈ {1,2}
```

⚠️ `-I <start>,,<stride>` 是 ctest 的**区间起点 + 步长**语法（`-I 1,,2` / `-I 2,,2` ⇒
两片不相交、并集 = 全集）。写成 `-I 0,2` / `-I 1,2`（区间 start,end）会**两片相同且只覆盖 2 个测试**
（历史教训，见 `pr-verilator-fast` 的 shard-cover 护栏）。

## 1. 数字

| 场景 | shard 1 | shard 2 | 合计 |
|---|---|---|---|
| **warm**（真实 `~/.cache/cpphdl/verilator`） | **61 s** | **403 s** | **464 s**（7.7 min）|
| **cold**（`HOME` 指向空目录 ⇒ 全量重编 Verilog） | **2267 s**（rc=8，2 个 timeout）| **2034 s**（rc=0，37/37 通过）| **4301 s**（**71.7 min**）|

- 单测试规模（cold，shard 2 实测）：多数 **21–54 s**；`test_axi4_full_burst` **130.7 s**；
  `test_axi_interconnect_4x4` **453.1 s**（warm 时 281–284 s）；`test_verilator_backend` **205.7 s**。
- **cold/warm ≈ 9×**（464 s → 4301 s）。

## 2. 两个**实测确认**的 CI 风险

### 风险 ①（**已撤回**）：cold「合计 71.7 min > 45 min」—— **我先前算错了**

🔴 **订正（2026-11-03，写这份文档的同一人）**：`pr-verilator-fast` **已经是两分片并行**
（`.github/workflows/ci.yml` 的 `strategy.matrix.shard: [1,2]` + `fail-fast: false` ⇒ 两个
runner 各跑一片）⇒ ⇒ **`timeout-minutes: 45` 是每个 job 的预算，不是两片之和**。

⇒ cold 的**每 job** 墙钟 = max(2267, 2034) ≈ **37.8 min < 45 min** ⇒ ⇒ **不构成超预算**
（余量 ~7 min）。本文初稿把两片**串行相加**再比 45 min ⇒ **口径错误**。

⚠️ 仍然成立的部分：cold 的触发条件 = GitHub Actions 缓存**未命中**（key 是
`cpphdl-verilator-${os}-${sha}` + `restore-keys` 前缀 ⇒ 只有「一个缓存都没有」时才 cold：
首次运行 / 缓存被驱逐（7 天未写、10 GB 上限））⇒ ⇒ 那一刻的余量只有 ~7 min（且本机 4 核
vs CI runner 2 核 ⇒ 生产只会更长）⇒ ⇒ **余量偏薄，但未越线**。

⚠️ **shard 1 的 2267 s 含测量期并发负载**（同机在跑全量重建 / `ctest` / samples 护栏 + 另一个
仓库的 build）⇒ 其中 2 个 timeout 是负载造成的 ⇒ 该片是**上界**。

### 风险 ②：`add_dual_backend_test` 的默认 `TIMEOUT 60` 在 cold 缓存下**必然**失败

隔离复测（**无并发负载**、`HOME` 全新）：

| 测试 | TIMEOUT | cold（直接跑二进制）| warm | 判定 |
|---|---|---|---|---|
| `test_cache_sync` | **60 s** | **96 s** | 1 s | ❌ **cold 必红** |
| `test_rv32i_pipeline_behavior` | 300 s | 204 s | — | ✅（shard 1 的红是负载）|

⇒ 不是「差一点」而是 **1.6×**；而 shard 2 里还有多个 **47–54 s** 的 dual 测试紧贴 60 s
（`test_cache_async_dual` 53.8 s、`test_stream_pipeline_dual` 54.0 s、
`test_axi_slave_byte_lane` 47.1 s）⇒ 在**更慢的 CI runner**（2 核）上同样会翻红。

⇒ ⇒ 即：**cold 缓存下会先有一批测试「超时失败」**（不是整个 job 被杀 —— 见风险 ① 的订正）。

### 风险 ② 的处置：✅ **已修**（2026-11-03，用户裁决「B + C 都做」）

**规则**：`add_dual_backend_test` 的 `TIMEOUT` MUST **≥ 2 × 该测试的冷缓存实测**（30 s 的倍数）。

| 改动 | 值 | 依据 |
|---|---|---|
| `add_dual_backend_test` **默认** | 60 → **180** | 冷实测最慢的「默认超时」用例只需 ≤ 60 s；180 覆盖实测最慢 96 s 的 1.9× |
| `test_cache_sync` | 60 → **210** | 冷实测 **96 s**（2× ⇒ 192 ⇒ 取 210）|
| `test_axi_lite_slave_conformance` | 300 → **450** | 冷实测 **224.4 s** |
| `test_rv32i_pipeline_behavior` | 300 → **420** | 冷实测 **204 s** |
| `test_axi_interconnect_4x4` | 600 → **930** | 冷实测 **453.1 s**（CI runner 更慢 ⇒ 原来只剩 1.3×）|

其余 54 个 dual 测试的冷实测占比都 **< 48%** ⇒ 现有 300 s 足够 ⇒ **不动**（改动最小化）。

**代价（如实登记）**：「挂死」的检测变慢（默认 60 s → 180 s）⇒ 判据强度**不降**（仍是超时即失败），
只是**发现得更晚**。

## 3. 选项与裁决结果（用户裁决「B + C 都做」，2026-11-03）

| 选项 | 状态 |
|---|---|
| **A. 提高 job 超时预算**（45 → 90）| ❌ **不需要** —— 风险 ① 是我算错了（分片本来就并行 ⇒ 每 job ~38 min < 45）|
| **B. 两分片并行 job** | ✅ **早已存在**（`strategy.matrix.shard: [1,2]`，2026-11-02 的 P2 lever ②）⇒ ⇒ 本次**无需改动**，只补了本文的订正 |
| **C. 放宽 per-test TIMEOUT** | ✅ **已实施**（规则见风险 ② 的处置表）|
| **D. 只保 cache 热** | ❌ **不选** —— C 已让 cold 不再假红，D 只是放弃鲁棒性 |

## 3.1 原可选项（保留作决策史）

| 选项 | 动作 | 代价 |
|---|---|---|
| **A. 提高超时预算** | `pr-verilator-fast` 的 `timeout-minutes` 45 → 90 | job 卡死时等更久；cold 一次 72 min 仍会挤掉后续 step 的余量 |
| **B. 两分片改并行 job**（matrix） | 现有两分片各跑一个 job | **墙钟 ≈ 38 min**（< 45）⇒ 预算不用动；但 runner 分钟数 ×2（并行不省总量） |
| **C. 放宽 per-test TIMEOUT** | `add_dual_backend_test` 默认 60 → 180（或按 pair 数分档） | 「挂死」的检测变慢（hang 现在要等 180 s）；判据强度不降（仍是超时即失败） |
| **D. 只保 cache 热** | 不改预算/超时，cold 时人工重跑 | 依赖「缓存基本不失效」这一**未经验证**的假设；驱逐一旦发生 CI 假红 |

⚠️ **C 与 B 正交**，可同时做。C 的取值建议用**实测上界**而不是猜：cold 单测试最慢
`test_cache_sync` 96 s / `test_axi_interconnect_4x4` 453 s（后者已有 `TIMEOUT 600`）⇒ 对
`TIMEOUT 60` 那批，**180 s** 有 ~1.9× 余量且能覆盖 shard 2 的 54 s 级。

## 4. 复现命令

⚠️ **已提交的可复算脚本**（护栏的 `evidence: probe:` 指向它）：

```bash
scripts/measure_verilator_e2e_wallclock.sh --warm   # 用真实缓存跑两片
scripts/measure_verilator_e2e_wallclock.sh --cold   # 空 HOME ⇒ 全量重编 Verilog
scripts/measure_verilator_e2e_wallclock.sh --both   # 两轮 + 「cold 耗时 / TIMEOUT」占比表
```

⚠️ **测量纪律**：测量期间**不得**有其它构建 / 测试在跑（否则数字是上界，不是真值）。

等价的裸命令（脚本内部用的就是它们）：

```bash
# warm（必须独占机器，否则数字无意义）
cd build && ctest --output-on-failure --no-tests=error -L verilator_e2e -LE perf -I 1,,2
cd build && ctest --output-on-failure --no-tests=error -L verilator_e2e -LE perf -I 2,,2

# cold（cherry：空 HOME ⇒ 缓存目录为空）
rm -rf /tmp/coldhome && mkdir -p /tmp/coldhome
cd build && HOME=/tmp/coldhome ctest --output-on-failure --no-tests=error \
    -L verilator_e2e -LE perf -I 1,,2

# 单个测试的真实 cold 时长（⚠️ `ctest --timeout` **盖不过**测试自身的 TIMEOUT 属性 ⇒
#   要测真实 cold 时长只能直接跑二进制）
rm -rf /tmp/coldhome2 && mkdir -p /tmp/coldhome2
HOME=/tmp/coldhome2 ./build/tests/test_cache_sync     # 实测 96 s
```

## 5. 判别力边界（如实登记）

- **测量受同机负载影响大**：cold shard 1 全程与全量重建/另一仓库 build 并发 ⇒ 其 2267 s
  是**上界**；shard 2 后半段基本独占 ⇒ 2034 s 更接近真值。
- **本机 4 核**；GitHub `ubuntu-22.04` runner 是 2 核 ⇒ 生产环境的 cold 只会**更长**，
  不会更短 ⇒ 两个风险的**方向是安全的**（不会由「本机够快」制造假警报）。
- **warm 的 464 s 不能直接当 CI 墙钟**：它不含 checkout / 装依赖 / 编 verilator /
  编 CppHDL 的时间（`pr-verilator-fast` 的 45 min 覆盖的是这些**全部**）。

---

## E6 段：`examples` 的**独立 RTL 仿真**（`examples-rtl-sim-coverage` · 2026-11-12）

- **属于**：`examples-rtl-sim-coverage` **E6**（CI 形态）
- **形态**：`pr-verilator-fast` job 的**一个 step**（`Verify examples RTL simulation
  (independent Verilog TB)`）⇒ ⇒ **不新建 job** ⇒ ⇒ 无第二次完整 build ⇒ ⇒
  增量成本 ≈ 「不含 build 的仿真时间」。
- **判据来源**：`.rddf/checks/examples-rtl-sim-exclusions.md` §4（本文只交数字）。

### 数字（本地实测 · 2026-11-12 · 10 个 TB）

| 场景 | 墙钟 | 备注 |
|---|---|---|
| **warm**（`~/.cache/cpphdl/rtl_sim` 命中） | **3 s** | E7 的内容哈希缓存命中 ⇒ 10 个 TB 全部秒级 |
| **cold**（清空缓存 ⇒ 10 个 TB 全量编译 + 跑） | **61 s** | ⇒ ⇒ CI 的 E6 step 增量 **cold ≈ 1 min / warm ≈ 3 s** |

⇒ ⇒ 对照：该 job 的 `timeout-minutes: 45` ⇒ ⇒ 增量占比可忽略 ✓

⚠️ **CI 侧的数字待首次运行后回填**（本机无法测 CI 的冷/暖差异：CI 的缓存是
**内容寻址 + restore-keys 前缀复用**）⇒ ⇒ 本地数字的**判别力边界**：
只覆盖「护栏本身的耗时」，**不覆盖** CI 的缓存命中行为。
