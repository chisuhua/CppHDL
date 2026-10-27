# 提案：两个 riscv-mini cache wrapper MUST 真正发射分支逻辑

- **架构依据**：无（**修 bug**，不改架构决策）
- **5 段草案**：`.rddf/improvements/riscv-mini-cache-wrapper-live-logic-2026-10.md`
  （⚠️ 原草案文件名是 `...-dead-branch-2026-10` ⇒ **已改名**统一为 `...-live-logic-2026-10`
  ⇒ 与 `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` 的 T8 命名一致）
- **roadmap 归属**：`feat-riscv-mini-rtl-debt` ①
- **规格影响**：**有**（`cpu-cache-subsystem`：1 条 MODIFIED + 1 条 ADDED）⇒ 归档**禁** `--skip-specs`
- **规模 / 风险**：**中**（⚠️ **既有断言会转红** ⇒ 改断言与修实现必须**同 commit**）

## Why

`include/cpu/cache/{i_cache_prefetch,d_cache_write_buffer}.h` 用**普通 C++ `if`** 包裹
`ch_bool` 条件来发射 RTL：

```cpp
if (io().req == ch_bool(true) && io().we == ch_bool(true)) {
    io().ready <<= ch_bool(true);      // ← 这行**从不发射**
}
```

`ch_bool` 有 `explicit operator bool()` ⇒ C++ `if` 拿到的是「**当前节点是否非零**」，
而 `describe()` 在**建图期**执行 ⇒ 该条件在建图期为**假** ⇒ **分支体永不发射**。

⇒ 后果（实测）：`DCacheWriteBuffer` 的 `ready` / `axi_valid` / `axi_addr` / `axi_wdata`
与 `ICachePrefetch` 的 `axi_valid` / `axi_addr` **恒为未驱动**（读到 0）
⇒ 两个 wrapper 目前**功能上是死的**（只能证明「不 SIGSEGV」）。

**Why now**：T8 是 `feat-riscv-mini-rtl-debt` 的两个剩余项之一；
且 C1（`riscv-mini-test-triage-2026-10`）已把 `test_cache_smoke` 的断言改成**可证伪**
⇒ 现在修实现有**判别力**基础。

## What Changes

1. **修实现**：`include/cpu/cache/{i_cache_prefetch,d_cache_write_buffer}.h` ——
   把 C++ `if` 改成 `select(...)` 驱动的**组合赋值**（`<<=`），
   并把输入真正接到内层 cache（如 `ICachePrefetch` 此前**从未**把 `addr`/`req` 接到内层
   ⇒ `miss` 恒为「冷 miss」）。
2. **连带断言处置**（**同 commit**，ADR-048 D11 同型纪律）：
   `examples/riscv-mini/tests/test_cache_smoke.cpp` 里钉住「未驱动 ⇒ 读 0」的两条断言
   **必须反转**（⇒ 改为「随输入变化」）⇒ 否则修复后**必然转红**。
3. **spec**：
   - `cpu-cache-subsystem` **MODIFIED ×1** —— 把两条 Requirement 里**过时的路径**
     （`examples/riscv-mini/src/i_cache_prefetch.h` / `d_cache_write_buffer.h`）
     订正为现路径（`include/cpu/cache/…`，ADR-046 收敛后的真位置）
     ⚠️ MODIFIED 会**整体替换** ⇒ MUST 保留原标题与全部条目；
   - `cpu-cache-subsystem` **ADDED ×1** —— 「wrapper 的组合逻辑 MUST 以 `select` 发射
     ⇒ 输出 MUST 随输入变化（MUST NOT 恒为未驱动）」+ Scenario。

## Acceptance

### 功能性

- [ ] 两个 wrapper 的输出**随输入变化**（不再是恒未驱动）
- [ ] `ICachePrefetch` 的 `addr`/`req` **真正接到**内层 cache（⇒ `hit`/`miss` 可区分）
- [ ] `test_cache_smoke` 的两条「未驱动 ⇒ 读 0」断言**已反转**且转绿
- [ ] 双后端一致（两个 wrapper 进 dual 用例 ⇒ interpreter + verilator 全等）

### 可判别性

- [ ] **变异验证**：M1 把 `select(...)` 退回 C++ `if` ⇒ **断言转红**
      （⚠️ 这条是本 change 的核心判据：它证明「建图期 C++ `if`」这个形态本身被钉住）/
      M2 删掉 `axi_addr` 的驱动 ⇒ 转红 / M3 把 `ready` 的条件取反 ⇒ 转红
- [ ] 每个断言带**反向期望**（`X_old != X_new`）

### 回归与工程

- [ ] `ctest -LE perf` 零回归 + `run_all_ported_tests.sh` 28/28 +
      `run_all_samples.sh` 35/35 + 护栏脚本全 exit 0
- [ ] 计数：`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` / `ctest -L verilator_e2e`
      —— **若**新增测试文件则 +1，**否则**不变 ⇒ 以实测为准并同步锚点

## Capabilities

- **`cpu-cache-subsystem`** —— **MODIFIED ×1**（路径订正）+ **ADDED ×1**（wrapper 活性契约）
- `dual-backend-test-coverage` —— 可能 MODIFIED（若新增 dual 用例的覆盖声明）

## Impact

- **文件面**：`include/cpu/cache/{i_cache_prefetch,d_cache_write_buffer}.h`、
  `examples/riscv-mini/tests/test_cache_smoke.cpp`、主 spec delta
- **风险**：**中** —— ⚠️ **既有断言会转红**（这是**预期**的 ⇒ 处置 MUST 与实现同 commit）
- **可回退**：单 commit

## Non-Goals

- 不改 cache 控制器本体（`i_cache_sync.h` / `d_cache_sync.h`）
- 不改 AXI 互连层
- 不做 wrapper 的**性能**优化（只求「逻辑真发射」）
