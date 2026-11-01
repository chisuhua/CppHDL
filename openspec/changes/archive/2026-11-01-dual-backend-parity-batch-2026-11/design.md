# Design: dual-backend-parity-batch-2026-11

## 1. 量化盘点（**T7 步骤 1**）⇒ ⇒ 回答 gap C

### 1.1 筛选（**逐字复用 capability R6 的 grep 模式**）

```bash
grep -l "ch_device\|ch_module" tests/test_*.cpp tests/chlib/test_*.cpp | \
  xargs grep -L "STATIC_REQUIRE" | \
  xargs grep -L "ch_out<ch_uint<[0-9]\{3,\}>>"
```

| 步骤 | 数 |
|---|---|
| 候选 | **37** |
| − 契约锁定（R「9 个 verilator 测试 MUST 保留原状」，其中 7 个在候选内） | 7 |
| − 已是 dual（含 `DUAL_BACKEND_TEST_PAIR(` 调用） | 1 |
| ⇒ **可转 dual** | **29** |

### 1.2 ⚠️ 与 2026-10-28 盘点的**两处订正**

| 项 | 旧盘点 | **实测** | 原因 |
|---|---|---|---|
| 可转 dual 条目 | 33 | **29** | 旧盘点把「**无 runtime 语义**」（无 `Simulator` 调用）的算在内 ⇒ ⇒ **实测 4 个**（旧盘点说 3）⇒ ⇒ **差 4** |
| 「Catch2 SECTION 重入」排除 | 列为排除 | ⚠️ **措辞可能已陈旧** | 宏是**两个独立 TEST_CASE** ⇒ ⇒ SECTION 在每个后端**各跑一次** ⇒ ⇒ **语义正确** ⇒ ⇒ **该排除项应复核**（**不在本 change 改契约**） |

### 1.3 ⇒ ⇒⇒ **可达上限（路线 A = 转换既有文件）= 29 + 29 = 58 < 60**

⇒ ⇒ ⇒ **T7 的「planner 决策门」触发** ⇒ ⇒ ⇒ **build 不得自行改判据**
⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 两条正当路线**：

| 路线 | 内容 | 上限 | 需改判据？ |
|---|---|---|---|
| **A** | 转换既有文件 | **58** | ❌ 单靠转换**到不了 60** |
| **B** | **新增有意义的 dual 文件** | 无上限 | ❌ **不需要**（objective §5 axis ③ 明确允许；§5 反例**只**禁「加标签凑数」） |

⇒ ⇒⇒ **本计划 = A 打底 + B 补足**（A 的 29 个文件本身有独立价值）。

## 2. 成本模型（**实测**）

| 场景 | 墙钟 |
|---|---|
| **冷缓存 · 小 DUT**（纯组合逻辑） | **13.5 s** |
| **冷缓存 · mem 类 DUT**（`ch_mem` + FIFO，假 HOME） | **72.0 s** |
| 热缓存 · 小 DUT | 0.6 – 3.0 s |
| 热缓存 · RV32I 流水线 | 7.7 s |

⇒ ⇒⇒ **两条结论**：
1. **冷编译是主成本** ⇒ ⇒ **CI 每次都是冷缓存** ⇒ ⇒ CI 墙钟 ≈
   **不同 DUT 类型数 × 平均编译成本**；
2. **同 DUT 的多个 TC 只编译一次**（缓存按 SHA-1 key）⇒ ⇒ ⇒
   **⇒ 必须按「DUT 聚合」组织批次**。

⚠️⚠️ **CI 硬约束**：verilator_e2e job **`timeout-minutes: 45`** ⇒ ⇒ 且
`RESOURCE_LOCK "verilator_backend"` ⇒ ⇒ **全部 dual 测试串行**
⇒ ⇒ ⇒ ⇒ ⇒ **累计墙钟直接吃这个预算** ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ 这正是 objective §9.2 的 No-Go 判据**（「PR feedback 墙钟持续 > 45 min
⇒ 需降级」）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 批次划分把「CI 墙钟」当【首要约束】**。

## 3. 批次清单（详见 `.rddf/plans/dual-backend-full-coverage-2026-11.md` §4）

| 批 | 文件数 | DUT 数 | 冷编译预估 | 风险 |
|---|---|---|---|---|
| **A**（✅ 本 change） | 1 | 1 | 13.5 s | 低 |
| B | 8 | 23 | ≈ 5 min | 低 |
| C（AXI） | 4 | 5 | ≈ 3 min | 中 |
| D（mem） | 3 | 11 | ≈ 13 min | 中 |
| E（bundle/cache） | 3 | 12 | ≈ 8 min | 中 |
| **F（大文件·多 DUT）** | 6 | 62 | ≈ 25 min | **高** |
| G（无 runtime 语义 ⇒ 须先补断言） | 4 | 4 | — | **高** |
| H（路线 B：新增有意义的 dual 文件） | ≥2 | ≥2 | ≈ 1 min | 中 |

⇒ ⇒ **Σ ≈ 56 min** ⚠️⚠️ **已超 CI 的 45 min** ⇒ ⇒ ⇒ ⇒ ⇒
**⇒ 必须逐批实测 + 回填墙钟账本 + 超标即停并报告**（不硬凑）。

## 4. 批次 A 的实施与判别力

### 4.1 判别力：verilator 半边**真跑**的证据

```
$ ./build/tests/test_blt_debug_behavior
- Verilator: Walltime 0.017 s (elab=0.001, cvt=0.006, bld=0.000)
All tests passed (2 assertions in 2 test cases)
```
⇒ ⇒ **2 = 2 × pair 数** ⇒ ⇒ ⇒ **verilator 半边真跑了**（若被 skip ⇒
只有 1 个 TEST_CASE ⇒ ⇒ 断言数只有 1 倍）⇒ ⇒ ⇒ **⇒ 这个「断言数 = 2×
pair 数」的核对 MUST 进每批的验收模板**（⇒ ⇒ 它是「假绿」的**唯一**探针）。

### 4.2 期望值**手推**

`BranchConditionUnit`：`branch_type = 4`（BLT）· `rs1 = 10` · `rs2 = 20`
⇒ ⇒ 有符号比较 `10 < 20` ⇒ ⇒ **true** ⇒ ⇒ `branch_condition == 1`
⇒ ⇒ 沿用原用例 `tests/test_blt_debug.cpp` 的 `REQUIRE(condition == 1)`
（⇒ ⇒ **不是**抄仿真输出 ⇒ ⇒ 是从原用例的**语义**推出的）。

### 4.3 ⚠️ 接线纪律（A4 的「假绿新形态」）

本 DUT 无 ready 端口 ⇒ ⇒ 该纪律在本 pair 尚不触发 ⇒ ⇒ 但**批次 C/D/E/F
都有 ready 端口** ⇒ ⇒ ⇒ **MUST 用 `as_master()` + `<<=`**
⇒ ⇒ **禁** `operator=` 喂常量 ⇒ ⇒ 否则会**恰好绕过**真实依赖。

## 5. 变更记录

| 日期 | 变更 |
|---|---|
| 2026-11-01 | 创建（量化盘点 + 成本模型 + 批次清单 + 批次 A 实施） |
