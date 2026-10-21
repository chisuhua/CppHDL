# Change: dual-backend-mem-read-timing-parity-2026-10 — mem 读端口时序纳入双后端 parity 矩阵

## Why

`sim-mem-read-timing-align-2026-10` 把 Simulator 的 `ch_mem::aread` 归入组合求值
（0-cycle），与生成的 Verilog `assign proxy = mem[addr];` 完全对齐。**该 change 的
核心主张就是「interpreter == Verilog」**，但落地后**没有任何跨后端测试验证这条主张**：

1. capability `dual-backend-test-coverage` 的 R6「适用范围排除清单」第 3 条明确
   排除 **aread-based 设计**，理由是「verilator 0-cycle 与 simulator posedge 设计
   drift 导致必然 parity 失败」（`openspec/specs/dual-backend-test-coverage/spec.md:99`）。
   → **排除项本身建立在已消除的 drift 上**，现在既无必要，也挡住了验证。
2. `tests/dual_backend_fixture.h:49` 的「Out of scope」清单同样写着
   「aread-based designs (mem-read-timing spec 1-cycle skew)」。
3. `tests/test_verilator_trace_parity.cpp:27-29` 写着「fixtures MUST NOT contain
   `ch_mem` `aread` read ports … parity fail by design」。

⇒ 结果：31 个 `verilator_e2e` 套件全绿，但**没有一个跑过 `aread`** → 本次时序
语义变更的「与 Verilog 对齐」这一主张**只被单后端测试间接覆盖**。

> ⚠️ **这不是理论风险**：在 `sim-mem-read-timing-align-2026-10` 落地**之前**，
> 任何 aread 的双后端 parity 测试都**必然红**（interpreter 滞后 1 拍 vs verilator
> 0-cycle）。也就是说，那条排除项是在**掩盖一个真实的 parity 缺口**，而不是在
> 保护一个「本来就没测」的东西。

## What Changes

### D1: 解锁 R6 排除清单第 3 条（aread-based 设计）

`dual-backend-test-coverage` 的排除清单删去「aread-based 设计」一项，理由改为
「aread 的 0-cycle 语义已与 Verilog 对齐，不再存在必然的 parity 失败」。

配套：删除「排除清单由 spec 引用 grep 模式定义」Scenario 里
`xargs grep -L "aread"` 这一过滤环节（否则新的候选名单会被该 grep 静默排除）。

### D2: 解锁 cache Sync/Async 分类要求

「cache 双后端 MUST 按 Sync/Async 分类（Sync 走 parity / Async 走 golden-value）」
这条 Requirement 的**理由**（「`mem-read-timing` spec §"aread 0-cycle vs posedge
1-cycle skew" 已锁定 Async 设计在两后端有设计 drift」）已不成立。

裁决：**Async（aread）cache 现在 MUST 走 parity**（与 Sync 同）；golden-value 路径
保留为**可选**（用于刻意比较不同 cycle 数的场景），不再是 Async 的**唯一**路径。

> 本次**不**把 cache 的 Async 双后端测试改造成 parity —— 那是 cache 子系统的
> 工作量，且 `ICacheAsync` / `DCacheAsync` 的内部时序需要单独评估。
> 本 change 只**解除禁令**，让后续工作不再被 spec 挡住。

### D3: 新增「mem 读端口时序 MUST 有双后端 parity 覆盖」Requirement

新增 Requirement，规定：

- `ch_mem::aread` 的 **0-cycle 语义** MUST 有双后端 parity 测试覆盖
  （interpreter 与 verilator 在同一 cycle 观测到**同一个值**）；
- 该测试 MUST 用**寄存器驱动**的读地址（输入驱动的地址在修复前后都同拍可见，
  区分不了 0-cycle 与 1-cycle → 锁不住任何东西）；
- 该测试 MUST 对「interpreter 侧退回 posedge 求值」敏感（非空转）。

### D4: 清理三处已失效的注释/文档

- `tests/dual_backend_fixture.h` 的「Out of scope」清单：删去 aread 项
- `tests/test_verilator_trace_parity.cpp` 的 fixture 约束注释：改为「aread 现在
  可以进 dual fixture（drift 已消除）」
- `openspec/specs/mem-read-timing/spec.md` 中两处**行号引用**漂移
  （`tests/test_mem_comb_read.cpp:62-108` 的 fixture 实际已是 62-126；
  「TEST_CASE at lines 216-277」的实际行号已变）→ 改为**按名称/标签引用**，
  不写行号（与项目「行号引用改为函数名」的约定一致）

## Impact

| 项 | 影响 |
|---|---|
| `openspec/specs/dual-backend-test-coverage/spec.md` | R6 排除清单 MODIFIED；cache Sync/Async 分类 Requirement MODIFIED；新增 mem 读端口时序 parity Requirement |
| `openspec/specs/mem-read-timing/spec.md` | 两处行号引用 → 名称引用（**非**行为变更） |
| `tests/test_mem_read_timing_dual.cpp` | **新增**双后端 parity 测试（1 个 `DUAL_BACKEND_TEST_PAIR` = 2 个 TEST_CASE） |
| `tests/CMakeLists.txt` | 新增 `add_dual_backend_test(test_mem_read_timing_dual ...)` |
| `tests/dual_backend_fixture.h` | 「Out of scope」清单删去 aread 项 |
| `tests/test_verilator_trace_parity.cpp` | fixture 约束注释更新 |
| 根 `AGENTS.md` | `CTEST_TOTAL` 180 → **181**（新增 1 个 ctest target）；`ctest -L verilator_e2e` 31 → **32** |
| 风险 | ⚠️ **低**。① 新测试只读不改行为；② spec 解锁是**放宽**（把「禁止」改成「允许」），不放松任何既有断言；③ `mem-read-timing` 主 spec 的**行为契约不变**（只改行号引用） |

## Non-Goals

1. **不改** `mem-read-timing` 的任何行为契约（0-cycle / 1-cycle / 竞态语义均不动）
2. **不**把 cache 的 Async 双后端测试改造成 parity（只解除 spec 禁令）
3. **不**改 `dual-backend-test-coverage` 的其它 Requirement（macro 语义、CTEST_TOTAL
   同步机制、AXI peripheral 分类等一律不动）
4. **不**修 `samples/fifo_bundle` 的既有 bundle 方向缺陷（与本 change 无关，
   根因是 `fifo_bundle::as_slave_direction()` 把 `push` 声明为输出 + sample 未接
   read data，属共享 bundle API 的独立缺陷）

## 附录：本次审计发现的其余问题（已在本 change 内处理或明确排除）

| # | 问题 | 处置 |
|---|---|---|
| 1 | 双后端无 aread 覆盖（核心主张未验证） | ✅ 本 change D3 + 新测试 |
| 2 | R6 排除清单第 3 条 / `dual_backend_fixture.h:49` / `test_verilator_trace_parity.cpp:27-29` 三处过时 | ✅ D1 + D4 |
| 3 | `mem-read-timing` spec 的行号引用漂移 | ✅ D4 |
| 4 | `sread` 的 **enable 拉低**路径无覆盖（读端口改写自身槽位后，该路径承载了「sync 恒读 0」陷阱的语义） | ✅ 新增 interpreter-only 用例（enable 拉低 → 端口保持上次值；与 Verilog `always_ff` 的 hold 语义一致） |
| 5 | `samples/fifo_bundle` 既有失败 | ⛔ 明确排除（Non-Goal #4）——共享 bundle API，需独立立项 |
