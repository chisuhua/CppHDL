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

### 风险 ①：cold 会**超过** `pr-verilator-fast` 的 `timeout-minutes: 45`

cold 合计 **71.7 min**（≈ 1.6 × 预算）。触发条件 = GitHub Actions 缓存**未命中**
（key 是 `cpphdl-verilator-${os}-${sha}` + `restore-keys` 前缀 ⇒ 只有「一个缓存都没有」时才
cold：首次运行 / 缓存被驱逐（7 天未写、10 GB 上限））。

⚠️ **shard 1 的 2267 s 含测量期并发负载**（同机在跑全量重建 / `ctest` / samples 护栏 + 另一个
仓库的 build）⇒ 其中 2 个 timeout（`test_cache_sync` 60 s、`test_rv32i_pipeline_behavior` 300 s）
**是负载造成的**；扣掉这部分后 cold 合计仍 ≈ **66–70 min** ⇒ 结论不变。

### 风险 ②：`add_dual_backend_test` 的默认 `TIMEOUT 60` 在 cold 缓存下**必然**失败

隔离复测（**无并发负载**、`HOME` 全新）：

| 测试 | TIMEOUT | cold（直接跑二进制）| warm | 判定 |
|---|---|---|---|---|
| `test_cache_sync` | **60 s** | **96 s** | 1 s | ❌ **cold 必红** |
| `test_rv32i_pipeline_behavior` | 300 s | 204 s | — | ✅（shard 1 的红是负载）|

⇒ 不是「差一点」而是 **1.6×**；而 shard 2 里还有多个 **47–54 s** 的 dual 测试紧贴 60 s
（`test_cache_async_dual` 53.8 s、`test_stream_pipeline_dual` 54.0 s、
`test_axi_slave_byte_lane` 47.1 s）⇒ 在**更慢的 CI runner**（2 核）上同样会翻红。

⇒ ⇒ 即：**cold 缓存下不只是整个 job 超时被杀，还会先有一批测试「超时失败」**。

## 3. 可选项（**交 planner 裁决**，build 未擅自改）

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
