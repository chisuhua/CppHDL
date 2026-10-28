# 提案：两个 riscv-mini cache wrapper MUST 真正发射分支逻辑

- **架构依据**：无（**修 bug**，不改架构决策；不引入新机制 ⇒ 不需要 ADR）
- **5 段草案**：`.rddf/improvements/riscv-mini-cache-wrapper-live-logic-2026-10.md`
  （⚠️ 原草案文件名是 `...-dead-branch-2026-10` ⇒ **已改名**统一为 `...-live-logic-2026-10`
  ⇒ 与 `.rddf/plans/2026-10-26-phase5-followup-execution-plan.md` 的 T8 命名一致）
- **实施设计**：`design.md`（**范围裁决** + 驱动表 + 变异计划）
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

⇒ 后果（实测）：两个 wrapper 的 `ready` / `axi_valid` / `axi_addr` / `axi_wdata` /
`axi_we` **恒为未驱动**（读到 0）⇒ 两个 wrapper 目前**功能上是死的**
（只能证明「不 SIGSEGV」）。

⚠️ 仓内**全部 5 处**这样的 `if`（见 `design.md` §1 的清单）⇒ 命中 spec delta 的
grep 护栏 ⇒ 本 change MUST 把它们**全部**清掉。

**Why now**：T8 是 `feat-riscv-mini-rtl-debt` 的两个剩余项之一；
且 C1（`riscv-mini-test-triage-2026-10`）已把 `test_cache_smoke` 的断言改成**可证伪**
⇒ 现在修实现有**判别力**基础。

## What Changes

1. **修实现**（`design.md` §2.4 的驱动表）—— 把 C++ `if` 改成 `select(...)` 驱动的
   **无条件**组合赋值（`<<=`）：
   - ⚠️ **不只改 `if`** —— 只改 `if` 会让 `axi_addr` / `axi_wdata` / `axi_we`
     **仍然未驱动**（现状：前两者的驱动是**注释掉的**，第三者**从未出现过**）
     ⇒ 会发出「**地址 0、无写使能**」的 AXI 请求 ⇒ **比「恒未驱动」更危险**
     ⇒ 必须补齐（`design.md` §2.2）；
   - ⚠️ **AXI 主端 MUST 转发内层**（`io().axi_addr <<= icache.io().axi_addr`、
     `io().axi_valid <<= icache.io().axi_valid`）⇒ **不**在 wrapper 层重推导
     （理由三条见 `design.md` §2.4：AXI 的 VALID **不得**等 READY /
     内层输出不留死逻辑 / 值同源）；
   - ⚠️ **`DCacheWriteBuffer` 的 `axi_valid` MUST NOT 含 `io().axi_ready`**
     ⇒ 否则 `axi_ready` 为低时该写**根本不出现在 AXI 总线上**，而本 wrapper
     **无状态/队列**（`WriteBufferEntry buffer[4]` 是死数据）⇒ **静默丢写**；
     配套：`ready` = `req & we & axi_ready`（「本次写已被 AXI 接受」），
     消费方 MUST 保持 `req`/`we`/`addr`/`wdata` 直到 `ready` 拉高；
   - 并把内层 cache 的 **5 个输入端口**（`addr`/`req`/`axi_ready`/`axi_data`/
     `axi_resp_valid`）**全部驱动** ⇒ 现状下内层 `miss` 恒为「冷 miss」
     ⇒ 接上后 `hit`/`miss` 才**可区分**；
2. **连带断言处置**（**同 commit**，ADR-048 D11 同型纪律）：
   `examples/riscv-mini/tests/test_cache_smoke.cpp` 里钉住「未驱动 ⇒ 读 0」的两条断言
   **必须反转** ⇒ ⚠️ **不是**把 `== false` 改成 `== true`（那只证明「有驱动」）
   ⇒ 改为「**同一 TEST_CASE 内依次改变输入** ⇒ 逐个输出断言跟随自己的输入变化」；
   ⚠️ 该文件的**头部注释**也钉住了旧事实 ⇒ **MUST 同步更新**；
3. **spec**：
   - `cpu-cache-subsystem` **MODIFIED ×1** —— 把两条 Requirement 里**过时的路径**
     （`examples/riscv-mini/src/i_cache_prefetch.h` / `d_cache_write_buffer.h`）
     订正为现路径（`include/cpu/cache/…`，ADR-046 收敛后的真位置）
     ⚠️ MODIFIED 会**整体替换** ⇒ MUST 保留原标题与全部条目
     （⚠️ `ICacheComplete IO signature` 的 6 个端口 MUST **逐字**保留
     ⇒ 漏抄会与新增的「内层 cache 输入 MUST 全部驱动」**自相矛盾**）；
   - `cpu-cache-subsystem` **ADDED ×1** —— 「wrapper 的组合逻辑 MUST 以 `select` 发射
     ⇒ 输出 MUST 随输入变化（MUST NOT 恒为未驱动）」+ 5 个 Scenario。

## Acceptance

### 功能性

- [ ] 两个 wrapper 的输出**随输入变化**（不再是恒未驱动）
- [ ] `axi_addr` / `axi_wdata` / `axi_we` **被驱动**（不得仍未驱动）
- [ ] `axi_addr` 的**值正确**（块地址 / 直通 `addr`），不只是「非零」
- [ ] `ICachePrefetch` 的 AXI 主端**转发**内层（`axi_addr` / `axi_valid`）
- [ ] `ICachePrefetch` 的 `addr`/`req` **真正接到**内层 cache（⇒ `hit`/`miss` 可区分）
- [ ] 内层 cache 的 **5 个输入端口全部被驱动**
- [ ] `test_cache_smoke` 的两条「未驱动 ⇒ 读 0」断言**已反转**且转绿
      ⇒ ⚠️ 反转方式是「**同 TEST_CASE 内改输入 ⇒ 逐输出断言变化**」
      ⇒ **不是**「改成 `== true`」（后者只证明「有驱动」）
- [ ] ⚠️ 实测确认 `ICachePrefetch` 的 `miss == true` 断言（接上 `addr`/`req` 后）**仍绿**
- [ ] 双后端一致（两个 wrapper 进 dual 用例 ⇒ interpreter + verilator 全等）
      ⇒ ⚠️ CMake 三属性（`verilator_e2e` 标签 + `RESOURCE_LOCK` + PATH 注入）MUST 落地
- [ ] ⚠️ `ctest` 命令 MUST 用**真实 target 名** `riscv_mini_cache_smoke`
      ⇒ `ctest -R test_cache_smoke` **匹配 0 个测试且 exit 0**（静默「验过」什么也没跑）

### 可判别性

- [ ] **变异验证**：M1 把 `select(...)` 退回 C++ `if` ⇒ **断言转红**
      （⚠️ 这条是本 change 的核心判据：它证明「建图期 C++ `if`」这个形态本身被钉住）/
      ⚠️ M2 **把 `axi_addr` 的驱动改成常量 `0`** ⇒ 转红
      （初稿写「**删掉** `axi_addr` 的驱动」**前提不成立** —— 驱动是**新增**的，
      现在**没有**驱动可删）/ M3 把 `ready` 的条件取反 ⇒ 转红 /
      M4 断开内层 cache 的 `addr`/`req` 驱动 ⇒ 转红 /
      ⚠️ **M5 把 `axi_valid` 的条件加上 `io().axi_ready`** ⇒ 转红
      （**AXI「VALID 不得等 READY」** ⇒ 堵住「静默丢写」；
      ⚠️ 现有用例把 `axi_ready` 恒驱 1 ⇒ 没有「READY 拉低」那条用例就**抓不到**）
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
- ⚠️ **不做真 FIFO 写缓冲**（`design.md` §2.3 ⇒ 独立立项）
- ⚠️ **不做真预取**（`prefetch_data`/`prefetch_addr`/`prefetch_valid` 三个 `ch_mem`
  缓冲区目前是死数据 ⇒ 独立立项）
