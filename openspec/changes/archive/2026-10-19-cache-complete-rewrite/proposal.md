# cache-complete-rewrite

## Status: 2026-10-19 ready to archive

本文档是 2026-10-18 session 决议的落地提案：

- 用户决议 = **A1 启动 R2 spike + F 保留为 fallback + ADR-039 §2 按 R2 实测修订**
- R2 spike 已闭环于 commit `bfb3960`（ADR-039 §2.2）
- 本变更以 `bfb3960` 为起点，按 ADR-039 §6 6 步 TDD 落地

本变更**有 spec 变更**：新建 capability `cpu-cache-subsystem`（首次出现于 `openspec/specs/`），定义 `ICacheComplete` / `DCacheComplete` 的 ch_mem-based SRAM 实现契约。

## Spec merge status (2026-10-19)

主 spec `openspec/specs/cpu-cache-subsystem/spec.md` 已在 commit 91a0833 落地时与本 change 同步创建, 其内容与 change 包内子 spec `openspec/changes/cache-complete-rewrite/specs/cpu-cache-subsystem/spec.md` 一致 (含后续 ADR-041 增量: Sync/Async variant selection 4 个 Scenario).

**Archive 时用 `--skip-specs`**: 主 spec 已先于 change 包 archive 落地, 重新合并会触发 `ADDED failed for header "Requirement: SRAM storage uses ch_mem primitives" - already exists`. Per root AGENTS.md OPENSPEC WORKFLOW: "禁止 --skip-specs 除非该 change 显式声明 'no spec changes'". 本 change 显式声明: 主 spec 状态 = change 包 state, 无新合并 delta.

本文档是 2026-10-18 session 决议的落地提案：

- 用户决议 = **A1 启动 R2 spike + F 保留为 fallback + ADR-039 §2 按 R2 实测修订**
- R2 spike 已闭环于 commit `bfb3960`（ADR-039 §2.2）
- 本变更以 `bfb3960` 为起点，按 ADR-039 §6 6 步 TDD 落地

本变更**有 spec 变更**：新建 capability `cpu-cache-subsystem`（首次出现于 `openspec/specs/`），定义 `ICacheComplete` / `DCacheComplete` 的 ch_mem-based SRAM 实现契约。

## Why

`include/cpu/cache/{i,d}_cache_complete.h` 自 ADR-037（2026-10-17）标记 `*_complete` 类为"应替代 stub"的目标实现，但实际代码是 dead-on-arrival（DOA）：**11 个 pre-existing build error** 让任何 CMake target 包含这两个头都会编译失败。

按 `cache-complete-doa-note.md §6.1.R1`（arch 阶段 cmake build probe 实测）确认：

| # | 错误 | 类型 | 来源 |
|---|---|---|---|
| 1 | `ch_reg<T> data_sram[128][2]` + runtime `ch_uint<7>` 索引 | **架构级** | `ch_reg::operator[]` 仅接受 compile-time `unsigned` |
| 2 | `ch_uint<32>(io().addr)` 冗余 wrap | 调用语法 | port 自带 `operator ch_uint<W>()` |
| 3 | `ch_uint<DATA_WIDTH>(io().axi_data)` 类型不匹配（32→128） | 类型 | port `ch_in<ch_uint<32>>` 不可隐式扩展到 128 位 |
| 4 | `valid_way0 && (tag_way0 == tag)` 等 `&&`/`\|\|` on ch_bool | ADR-036 deprecation | handoff §3.1 改法 |
| 5 | `hit \|\| io().axi_ready` / `miss && io().req` mixed ch_bool+port | ADR-036 deprecation | 同上 |
| 6 | `0xFFFFFFF0_d` typo | 字面量 | 应为 `0xFFFFFFF0_h` |
| 7 | `lru_sram[index] = ch_reg<...>(...)` 写语法错 | 副作用 | C-style 数组不支持运行时赋值 |
| 8 | `tag_sram[index][victim_way] = ...` 类型错（`victim_way: ch_bool` 用作下标） | **架构级** | 同 #1，runtime `ch_bool` index |

**核心架构决策**（ADR-039 §2）：SRAM 改用 `ch_mem` 原语实现：
- 读访问用 `ch_mem::aread` —— combinational `assign mem[addr]`（0-cycle）
- 写访问用 `ch_mem::write(addr, data, enable)` —— 1-cycle registered
- **cache hit 通路用 aread**（消费端立刻需要 tag/data）
- **imem 用 sread**（RV32I `pc_lag` 对齐，详见 verilator-e2e-coverage spec §R1 invariant 3）

R2 spike（`tests/test_mem_comb_read.cpp`）实测闭环了 aread/sread 时序契约，本变更无需再做 arch 时序探查。

## What Changes

### A. 重写 2 个 cache header

| 文件 | 改动 | 改法 |
|---|---|---|
| `include/cpu/cache/i_cache_complete.h` | 11 bug fix + SRAM 改 ch_mem | ADR-039 §2 决策表 |
| `include/cpu/cache/d_cache_complete.h` | 同上（镜像） | 同上 |

`__io` 接口签名**完全保持**（外部消费者如 `ICachePrefetch` / `DCacheWriteBuffer` wrapper 无需改动）。
`ICacheConfig` / `DCacheConfig` 数值保持（CAPACITY=4096, ASSOCIATIVITY=2, LINE_SIZE=16, NUM_SETS=128, INDEX_BITS=7, OFFSET_BITS=4, TAG_BITS=21, DATA_WIDTH=128）。

### B. 新增 1 个 test 文件

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_cache_complete.cpp`（新文件，~250 行） | 4 个 `TEST_CASE` 覆盖 R2 缺口 G1/G2（2-way aread 隔离性 + 同周期 hit+fill-write 并发）|

`tests/CMakeLists.txt` 加 1 行 `add_catch_test(test_cache_complete ...)`，CTEST_TOTAL 从 159 → 160。

### C. 新增 1 个 riscv-mini smoke target

| 文件 | 内容 |
|---|---|
| `examples/riscv-mini/tests/test_cache_smoke.cpp`（新文件，~50 行） | 仅实例化 `ICachePrefetch` + `DCacheWriteBuffer`，验证 wrapper 能 build |

不动 wrapper 内部逻辑（runtime 阻塞不在 cache —— 是 `DCacheWriteBuffer` 自己尚未接入 RISC-V datatpath 的问题，单独 follow-up）。

### D. 文档同步（3 处）

- root `AGENTS.md §KNOWN LIMITATIONS`：删 `*_cache_complete.h` is dead-on-arrival bullet（保留 stub `[[deprecated]]` bullet）
- `docs/adr/ADR-037-cache-stub-removal.md §5 Status`：增补"rewrite 已完成"
- `include/cpu/AGENTS.md STRUCTURE 节`：加一行"`*_complete.h` 是 ch_mem-based 4KB L1 2-way set-associative"

### E. 不在 scope 内（划线，handoff §6）

- ❌ 不实现 BYPASS mode（ADR-037 §2 variant (a) 已废）
- ❌ 不实现 multi-level cache / coherency / BTB / branch predictor
- ❌ 不改 stub（stub 维持 `[[deprecated]]`，等 2027-04-17 触发 ADR-037 重启）
- ❌ 不实现 write-back / write buffer（`DCacheWriteBuffer` wrapper 仅保证能 build）
- ❌ 不改 ADR-039 的"aread vs sread"决策（R2 已闭环于 commit `bfb3960`）

## Acceptance

1. **零编译错/警告**
   ```bash
   cmake --build build -j$(nproc)  # 0 error 0 warning on new files
   ```
2. **零测试回归**
   ```bash
   ctest -LE perf --output-on-failure -j$(nproc)  # 148/148 PASS（基线）+ 1 new test_cache_complete
   ```
3. **新 test 全过**
   ```bash
   ctest -R test_cache_complete --output-on-failure  # 4/4 PASS
   ```
4. **riscv-mini smoke target 可 build**
   ```bash
   cmake --build build --target riscv_mini_cache_smoke
   ctest -R riscv_mini_cache_smoke --output-on-failure  # PASS
   ```
5. **CTEST_TOTAL 增量**：159 → 160（新增 `test_cache_complete`）
6. **文档同步完成**：3 处 AGENTS.md / ADR-037 已更新
7. **openspec validate 通过**
   ```bash
   openspec validate cache-complete-rewrite --strict
   openspec validate --specs --strict  # 13 → 14 capabilities
   ```
8. **stub 维持 `[[deprecated]]`**（本变更不动 stub，2027-04-17 由 ADR-037 重启流程处理）

## Why deferred from 2026-10-18 session

2026-10-18 session 完成：
- R2 spike 闭环（`tests/test_mem_comb_read.cpp`）
- ADR-039 §2.2 按 R2 实测修订（cache hit 用 aread；imem 用 sread）
- ADR-039 §6 Step 3 multi-port coverage（G1/G2 → Step 3 test 编号）

但未执行 6 步 TDD 实施。Session B（当前）承接 Step 1+2 起的全部实施工作。

预估（来自 handoff §7）：

| 步骤 | 工作量 | 累计 |
|---|---|---|
| Session B 启动（读 3 文件 + 验证 + worktree + openspec 包） | 1 h | 1 h |
| Step 1+2 合并 commit | 1 d | 1 d 1 h |
| Step 3 test_case 编写 + 调试 | 0.5 d | 1 d 5 h |
| Step 4 riscv-mini 串接 | 0.5 d | 2 d 1 h |
| Step 5 文档同步 | 0.5 h | 2 d 1.5 h |
| Step 6 archive + 重启 ADR-037 | 0.5 h | 2 d 2 h |
| **总计** | **2-3 天** | (handoff §7 估算) |

## See also

- `docs/adr/ADR-039-cache-complete-rewrite-via-ch-mem.md` (commit `bfb3960`) — 本变更的父决策
- `docs/adr/ADR-037-cache-stub-removal.md` — 父 ADR（6-month deadline 2027-04-17）
- `.rddf/improvements/cache-complete-doa-note.md` — 撞墙事件起源 + Re-entry Protocol 5 条 hard-gate
- `.rddf/improvements/cache-complete-rewrite-session-handoff.md` — 本变更的 handoff（§3 6 步 TDD checklist）
- `.rddf/improvements/cache-stub-physical-removal.md` — sibling（backlog，等本 rewrite 落地后重启）
- `openspec/specs/mem-read-timing/spec.md` — aread/sread 时序契约（R2 spike 闭环来源）
- `openspec/specs/verilator-e2e-coverage/spec.md` — imem 必须 sread 的 §R1 invariant 3