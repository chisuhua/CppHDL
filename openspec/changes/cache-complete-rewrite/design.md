# design: cache-complete-rewrite

## Why the *_complete.h was dead-on-arrival

`include/cpu/cache/{i,d}_cache_complete.h` 自身的设计假设是"C-style `ch_reg<T>` 数组 + 运行时 `ch_uint`/`ch_bool` 索引"。这个假设在 `ch_reg` 实现层是不成立的：

- `ch_reg::operator[]` 是 `constexpr` 化的（用于 DAG 静态展开），仅接受 compile-time `unsigned` 字面量作为索引
- 当 `cache` 用 `ch_uint<7> index`（从 `bits<...>(io().addr)` 派生）作为下标时，编译器**接受**这个调用（C++ 模板对 runtime value 没有静态拒识能力），但展开出来的 `ch_reg<T>` 实例化方式是 N=128 个独立 DAG 节点全部连接到同一个内部 proxy —— 索引值被丢弃
- 结果：`tag_sram[index][0]` 实际总是返回 `tag_sram[0][0]`，所有 128 sets 共享同一组数据，cache 退化成"1 个 tag + 1 个 data 反复覆盖"

这就是 DOA 的根因：编译能过（语法层 `ch_reg[128][2]` 是合法 C-style 数组），但语义层完全失效（runtime index 失效），且**没有任何测试捕获** —— 因为 `*_complete.h` 从未被任何 CMake target 实例化（grep 0 instances in `.cpp`），build error 始终是 latent 状态。

ADC 2026-10-18 R2 spike（commit `bfb3960`）闭环了 5 个 mem read port 时序 test（`tests/test_mem_comb_read.cpp`），确认 `ch_mem::aread` / `ch_mem::sread` 的语义契约可用；本变更直接基于此契约重写 SRAM。

## Why ch_mem for SRAM (not another approach)

候选方案评估（ADR-039 §2 决策表）：

| 方案 | 实现复杂度 | 与 Verilog codegen 兼容性 | 与 Simulator 兼容性 | 选中？ |
|---|---|---|---|---|
| **ch_mem<T, N>** | 中（用 aread/sread/write） | ✅ 已被 codegen 路径覆盖（`mem-read-timing` spec 已开） | ✅ 已被 R2 spike 验证 | ✅ |
| ch_reg<T>[N] + runtime index | 不可行（rdd 上） | ❌ 不可行（DAG 静态展开） | ❌ 索引失效 | ❌ |
| 手写 RTL class 模拟 SRAM | 高 | 需手写 codegen | 需手写 sim eval | ❌ scope 太大 |
| 多端口 ch_mem<>[N] | 高（需 codegen 加 multi-port） | ❌ 需要 verilog-e2e-coverage spec 变更 | ❌ R2 测过仅 1 路 | ❌（本期不做） |

**结论**：`ch_mem<T, N>` 是唯一可行且已被 spec 覆盖的路径。

## Why cache hit uses aread, imem uses sread (R2 closure)

R2 spike 闭环于 commit `bfb3960`（ADR-039 §2.2）。R2 实测结论：

- **cache hit 通路**：读 cache 自己存的 tag/data，**消费端立刻需要**（同 tick 内判 hit + 输出 data）→ 必须 0-cycle → `ch_mem::aread`
- **imem（RV32I 取指）**：CPU 取指是例行为，`pc` 寄存器和返回的 `instr` 之间需要 `pc_lag` 对齐 → 1-cycle registered → `ch_mem::sread`

两个路径不互通：**cache hit 通路不能用 sread**（sread 1-cycle 滞后破坏 hit/miss 决策的同 tick 性），**imem 不能用 aread**（aread 0-cycle 导致 `pc_lag` 错位 1 cycle，RV32I 跑 `riscv-mini` smoke 时 commit_count=0）。

详见 `openspec/specs/verilator-e2e-coverage/spec.md` §R1 invariant 3。

## Why deprecation rewrite order matters (ADR-036 ↔ this change)

ADR-036（`deprecate-hdl-logical-operators`）已在 2026-10-16 完成全量迁移，8 个 `&&`/`||` overload 全 `[[deprecated]]`。本变更内的 4 个 `&&`/`||` on ch_bool（i:131-133、d:134-136）和 4 个 mixed `&&`/`||` on ch_bool+port（i:155、i:163、d:162、d:190）都是 ADR-036 范围内的调用点 —— **当前树已经 0 个 `-Wdeprecated-declarations` warning**（handoff §6.1.R3 R3 hard-gate 前置条件已满足）。

迁移策略（参考 `deprecate-hdl-logical-operators-redo` archive 模式）：

| 调用模式 | 改法 | DAG / Verilog 影响 |
|---|---|---|
| ch_bool `&&` ch_bool | `&`（bit-identical） | DAG node 同 `ch_op::and_`，零影响 |
| ch_bool `\|\|` ch_bool | `\|` | 同上 |
| mixed ch_bool `&&` port | `select(cond, cond, ch_bool(false)) & port` 或先 `ch_bool(port)` | 1 个 mux + no selection |
| mixed ch_bool `\|\|` port | `select(cond, cond, port)` | mux + value mux |

迁移后 DAG / Verilog emission / JIT lowering 与改前完全等价（除 ch_bool+port 模式增加 1 个 mux node，但 Verilog codegen 路径已稳定支持）。

## Why write-through strategy preserved (no write-buffer in scope)

`DCacheComplete` 当前是 write-through（`d_cache_complete.h:152-171`）：写操作同时更新 cache 和 AXI 总线。`DCacheWriteBuffer` wrapper（`examples/riscv-mini/src/d_cache_write_buffer.h`）引入 write buffer 是为了把 cache miss 写转成 batched AXI 写，**不在本变更 scope 内**（handoff §6 "不实现 write-back / write buffer"）。

本变更保证 `DCacheWriteBuffer` wrapper 能 build（实例化），不动其内部逻辑。

## Files touched (impact radius)

```
include/cpu/cache/i_cache_complete.h        # 重写
include/cpu/cache/d_cache_complete.h        # 重写
include/cpu/cache/i_cache_stub.h             # UNCHANGED (维持 [[deprecated]])
include/cpu/cache/d_cache_stub.h             # UNCHANGED (维持 [[deprecated]])
tests/chlib/test_cache_complete.cpp         # 新增（~250 行）
tests/CMakeLists.txt                        # +1 行 add_catch_test
examples/riscv-mini/tests/test_cache_smoke.cpp  # 新增（~50 行）
examples/riscv-mini/CMakeLists.txt          # +1 行 add_catch_test
openspec/changes/cache-complete-rewrite/{proposal,design,tasks}.md  # 新增
openspec/specs/cpu-cache-subsystem/spec.md  # 新增 capability
openspec/changes/archive/2026-XX-XX-cache-complete-rewrite/  # archive 后
AGENTS.md                                   # KNOWN LIMITATIONS 删 bullet
docs/adr/ADR-037-cache-stub-removal.md      # §5 Status 更新
include/cpu/AGENTS.md                       # STRUCTURE 加一行
.rddf/improvements/cache-stub-physical-removal.md  # status 改回 Active
```

**未触及**：`ch.hpp` / `chlib` / `axi4` / `jit` / `verilator_backend` —— 本变更 scope 严格在 cache subsystem + 子包。

## Cross-spec dependencies

本变更新增 capability `cpu-cache-subsystem`（首次出现），依赖以下现有 spec：

- `mem-read-timing/spec.md` —— ch_mem `aread`/`sread` 时序契约（R2 spike 闭环来源）
- `verilator-e2e-coverage/spec.md` §R1 invariant 3 —— imem 必须 sread
- `chlib-aggregator/spec.md` —— `include/cpu/cache/` 是 `chlib` 子集，依赖 `chlib` aggregator pattern

## CI guard (optional, recommended but not in scope)

后续可选：把 `*_complete.h` 的 `__attribute__((deprecated))` 加回去（与 stub 一致）以防"再次 DOA 化"，但本期不做（2027-04-17 ADR-037 重启流程一并处理 stub + complete 的整体命运）。

## See also

- `docs/adr/ADR-039-cache-complete-rewrite-via-ch-mem.md` §2 决策表（commit `bfb3960`）
- `.rddf/improvements/cache-complete-doa-note.md` §6.1 R1-R5 hard-gate
- `.rddf/improvements/cache-complete-rewrite-session-handoff.md` §3 6 步 TDD checklist
- `openspec/specs/mem-read-timing/spec.md` —— `aread`/`sread` 时序契约
- `openspec/specs/verilator-e2e-coverage/spec.md` —— imem 必须 sread 的 §R1 invariant 3
- `openspec/changes/archive/2026-10-15-2026-10-16-deprecate-hdl-logical-operators-redo/design.md` —— deprecation rewrite 顺序参考