# ADR-039: cache complete 架构重写路径 — 用 ch_mem 替代 ch_reg 数组

- **Status**: Accepted (2026-10-18)
- **Date**: 2026-10-18
- **Deciders**: build-agent (architect role)
- **Supersedes / builds on**:
  - ADR-037 (`docs/adr/ADR-037-cache-stub-removal.md`) — 本 ADR 的父决策
  - Improvement `.rddf/improvements/cache-complete-doa-note.md` — 本 ADR 的方案来源
  - Root AGENTS.md §KNOWN LIMITATIONS (2026-10-18 audit 增补的 `*_complete.h` dead-on-arrival bullet)
  - 调研证据: `.rddf/improvements/cache-stub-physical-removal.research.md` (11 个 pre-existing build error 列表)
- **Related**:
  - `include/core/mem.h` — `ch_mem<T, N>::aread/sread/write` 原语
  - `examples/riscv-mini/src/{i_cache_prefetch.h, d_cache_write_buffer.h}` — 现有 wrapper（未编译过）

---

## 1. Context

`include/cpu/cache/{i,d}_cache_complete.h` 自 2026-04-10 写入以来**从未被任何 .cpp 实际编译过**（cmake build probe 验证）。2026-10-18 第一次尝试执行 ADR-037 物理删除 stub 时撞墙，揭示 11 个 pre-existing build error，**最严重是架构级**：

```cpp
// i_cache_complete.h:84-96
ch_reg<ch_uint<ICacheConfig::DATA_WIDTH>>
    data_sram[ICacheConfig::NUM_SETS][ICacheConfig::ASSOCIATIVITY];   // [128][2]
// i_cache_complete.h:125
auto tag_way0 = tag_sram[index][0];   // index = ch_uint<7> 运行时
```

**冲突**：`ch_reg` 是 HDL 寄存器类型，`operator[]` 只为编译期 `unsigned` 重载；运行时 `ch_uint<7>` 索引无法做 C 数组下标。`d_cache_complete.h` 镜像同样问题。

stub 时代没人 compile `_complete.h`（grep 验证 0 production 调用方），所以 bug 藏了 6 个月。

**架构问题本质**：cache 应当用**内存原语** `ch_mem<T, N>`（`include/core/mem.h`），它原生支持运行时 `ch_uint<addr_width>` 索引（`aread(addr)` 0-cycle 异步读，`sread(addr, enable)` 1-cycle 同步读，`write(addr, data, enable)` 写端口）。

## 2. Decision

**采用 ch_mem-based rewrite 路径**，替换 `*_complete.h` 中所有 SRAM 数组声明为 `ch_mem` 实例：

| 原 (broken) | 新 |
|------------|---|
| `ch_reg<ch_uint<128>> data_sram[128][2]` (C-style 数组) | `ch_mem<ch_uint<128>, 128> data_mems[2]` (2 个 way 实例存 C 数组) |
| `ch_reg<ch_uint<21>> tag_sram[128][2]` | `ch_mem<ch_uint<21>, 128> tag_mems[2]` |
| `ch_reg<ch_bool> valid_sram[128][2]` | `ch_mem<ch_bool, 128> valid_mems[2]` |
| `ch_reg<ch_bool> lru_sram[128]` | `ch_mem<ch_bool, 128> lru_mem` |
| `data_sram[index][0]` (C-array) | `data_mems[0].aread(index)` / `.sread(index, hit)` |

**2-way 选择器**通过 `select(hit_way1, way1_data, way0_data)` mux：每个 way 独立 ch_mem 读端口，read_port 隐式转 lnode 后参与 `select`。

**数据位宽**：保留原 `DATA_WIDTH=128`（16-byte line = 1 cache line 的 128-bit 块）。4KB 容量 = `128 sets × 2 way × 16 byte = 4096 byte`，与原 spec `CAPACITY=4096` 一致。

**LRU 1-bit**（保持原设计，2-way set-associative 用 1 bit LRU 就够）。

### 2.1 同步决策

cmake build probe 2026-10-18（`tests/chlib/test_cache_simple.cpp` 临时 probe，验证后已恢复）**确认**：
- ✅ `ch_mem<ch_uint<128>, 128> data_mems[2]` C-style 数组存 2 个 ch_mem 实例**编译通过**
- ✅ `data_mems[0].aread(port<ch_uint<7>>)` —— 接受 port 索引
- ✅ `data_mems[0].write(port, port, port)` —— 接受 port 地址+数据+enable
- ✅ read_port `<<=` 到 `ch_out` —— 通过隐式转 lnode

**注意**：probe 只验证**编译可行性**，**未验证 simulator 运行时行为**（aread 0-cycle / sread 1-cycle 时序、LRU 状态机、miss-fill 路径）。这是 builder 阶段必须跑 test 验证的。

### 2.2 R2 aread/sread 时序 spike — 用既有证据闭环（2026-10-18）

`.rddf/improvements/cache-complete-doa-note.md §6.1.R2` 要求"cache hit 通路用 aread 还是 sread"必须有 runtime probe 实证。**结论：cache hit 通路用 `aread`，证据已在 tree 中**：

`tests/test_mem_comb_read.cpp`（2026-10-12 commit `4c19d5c` 等价提交 `test(mem): pin same-cycle write+aread race`）的 4 个 TEST_CASE 已 pin 死以下事实：

| TEST_CASE 行号 | 行为 | 结论 |
|---|---|---|
| 行 135-177 `[mem][comb-read][sim]` | aread same-tick 可见（addr 改变后下一个 `tick()` 立即看到新值）| ✅ aread 在 Simulator 0-cycle |
| 行 179-210 `[mem][comb-read][sim][sread-regression]` | sread 1-cycle（`tick()` 后才可见）| ✅ sread 1-cycle |
| 行 216-277 `[mem][comb-read][codegen]` | Verilog: aread → `assign`、sread → `always_ff` | ✅ Verilog 端 aread=0-cycle 组合逻辑、sread=1-cycle 时序逻辑 |
| 行 286-331 `[mem][comb-read][sim][write-read]` | 写后下一周期 aread 见 post-write 值 | ✅ write→aread 跨周期正确 |
| 行 340-371 `[mem][aread][race]` | same-cycle write+aread 返回 **pre-write** 值 | ✅ race 语义锁定（cache miss-fill 需注意这点）|

`tests/test_mem_comb_read.cpp:4-31` doc-comment 把以上语义形式化为可读契约：
- "aread emits `assign` in Verilog (0-cycle); sread continues to emit `always_ff` (1-cycle)"
- "Both use sequential list [in Simulator], observed same-tick via trailing eval_combinational"
- "Same-cycle write+read race: Simulator aread returns pre-write value"
- "imem must remain on sread to keep fetch/pc_lag alignment"

**对 cache hit 通路的决策**：
- **cache hit 数据通路** = 纯组合读 SRAM → 用 `aread`（同 dmem 风格）
- **imem（instruction fetch）** = 必须 `sread`（与 pc_lag 1 周期对齐）
- icache 用 `aread` 后，下游需要 1 个寄存器 stage 来对齐 CPU pipeline 的 fetch 阶段（详见 §3 集成层）

**R2 缺口（已纳入 §6 Step 3 覆盖）**：
- ❌ **multi-port aread 隔离性**：2-way cache = 2 个独立 `aread` 端口，需要确认 read_port 不会因共享底层 proxyimpl 而互相污染。现有 `test_mem_comb_read.cpp` 只有 single-port，未直接覆盖。
- ❌ **写端口在 hit 时与 aread 端口的并发**：cache hit 时既要 `write` SRAM（miss-fill），又要 `aread` 数据通路读 SRAM。`test_mem_comb_read.cpp` 行 286-331 测了 cross-cycle 但不是同时钟 cycle。

**这两个缺口不是 R2 阻塞项**（基础原语已实证），而是 §6 Step 3 `tests/chlib/test_cache_complete.cpp` 必须覆盖的 cache-specific 行为 —— 在 builder 阶段实现。

### 2.3 不采用 ADR-037 §4 variant (a) 的 BYPASS mode

`*.research.md §A4.1` 已论证（4 个理由）：架构污染、bypass 跨 Hit/Miss 状态机、新测试等于搬 stub 断言、违反 ZERO-DEBT。**BYPASS mode 不在范围内**。

## 3. Why ch_mem (vs ch_reg 数组 + mux 降级)

`cache-complete-doa-note.md §5.1 风险矩阵` 列了 ch_mem 降级到 ch_reg + mux 的备选。**选 ch_mem** 因为：

| 维度 | ch_mem | ch_reg 数组 + mux |
|------|--------|------------------|
| **HDL 语义** | ✅ 内存原语，bit-accurate | ❌ 用 reg 模拟 SRAM，1-cycle latency 难对齐 |
| **时序模型** | aread=0-cycle, sread=1-cycle, write=posedge | reg 是 1-cycle 边沿触发，模拟 SRAM 需要额外状态机 |
| **代码量** | 2-way ch_mem = 2 个 .aread + 1 个 select | 128-way mux for 循环展开 = 爆炸性代码 |
| **根 AGENTS.md 兼容性** | ✅ 已知 pattern（rv32i imem 已经在用）| ⚠️ 引入新模式（reg-as-mem）|
| **可读性** | "data_mems[0].aread(index)" 直白 | 128 个 mux 难 review |

**关键约束**：根 AGENTS.md §KNOWN LIMITATIONS "imem must use sread" 提示 RV32I 集成路径里 `sread` 是关键时序原语，cache 也要兼容。

## 4. ADR-037 关系

| 时间点 | ADR-037 状态 | 本 ADR 状态 |
|--------|------------|------------|
| 2026-10-17 | Accepted, deadline 2027-04-17 删 stub | — |
| 2026-10-18 | §5 Status 增补：ch_mem rewrite 是 hard prereq | **Accepted（本文）** |
| 2027-04-17 前 | rewrite 必须落地 | rewrite 落地后,执行 ADR-037 §2 路径 |
| 2027-04-17 触发 | 删 stub + 改 test + archive | — |

**如果 rewrite 在 2027-04-17 前未完成**：
- 保留 stub（带 `[[deprecated]]`），不删
- 取消 ADR-037 deadline（推到下一周期）
- **不**接受"删 stub 但留 broken complete"（违反 ZERO-DEBT POLICY "no orphan files / no broken build"）

## 5. Consequences

### 5.1 正面
- `*_complete.h` 从 dead-on-arrival 变真正可用的 cache 实现
- ADR-037 物理删除 stub 路径重新可执行
- root AGENTS.md §KNOWN LIMITATIONS 少 1 个 bullet（dead-on-arrival 消失）
- riscv-mini 集成 path 不再依赖 stub 当 mock
- 11 个 pre-existing build error 一并消失（zero-debt 政策）
- rv32i imem 用 `sread`（pc_lag 1 周期对齐），icache/dcache hit 通路用 `aread`（纯组合读 SRAM），两种原语各司其职，与 `tests/test_mem_comb_read.cpp` 已 pin 的语义对齐

### 5.2 负面
- builder 阶段需要：
  - ~~验证 ch_mem aread/sread 在 simulator 的时序行为~~（**已被 §2.2 R2 闭环**：`tests/test_mem_comb_read.cpp` 5 个 test_case 已 pin 死）
  - 验证 2-way multi-port aread 隔离性（2 个独立 read_port 不互相污染）—— §6 Step 3 覆盖
  - 验证 LRU 状态机（hit_way1 / hit_way0 → lru bit update 路径）
  - 验证 miss-fill 路径（axi_resp_valid=1 时 tag/data/valid 写 + LRU 翻转）
- 工作量 3-5 天（5-8 commit），1 个 builder change
- `i_cache_prefetch.h` / `d_cache_write_buffer.h` wrapper 自身长期未编译，rewrite 后可能暴露 wrapper 内部 bug（**独立风险**，builder 阶段需 ad-hoc 处理）

### 5.3 范围外
- ❌ 不实现 BYPASS mode（`*.research.md §A4.1` 4 个理由）
- ❌ 不实现 multi-level cache / coherency
- ❌ 不实现 BTB / branch predictor
- ❌ 不改 stub（stub 维持 `[[deprecated]]`，等 2027-04-17 触发 ADR-037 重启）
- ❌ 不实现 write-back / write buffer（`DCacheWriteBuffer` wrapper 已存在，本 change 仅保证它能 build，不动其内部逻辑）

## 6. Implementation Plan (Future Builder Change)

6 步 TDD 5-step（每步独立 commit + 独立 ctest 验证）：

1. **修复 `i_cache_complete.h` 的非架构 bug**（10 个 typo / port-conv / 运算符）+ SRAM 声明改成 ch_mem 数组（2-way）→ 验证 `tests/test_cache_simple` 改用 ICacheComplete 能 build
2. **修复 `d_cache_complete.h` 同 10 个 bug** + 改 ch_mem → 验证同上
3. **新增 `tests/chlib/test_cache_complete.cpp`** — 至少 4 个 test_case（覆盖 §2.2 R2 缺口）：
   - **icache miss-fill-then-hit**：验证 2-way aread 在 miss 后 fill，下一周期 read 应 hit
   - **dcache read miss-fill**：同上但 write-through 路径
   - **2-way multi-port aread 隔离性**（§2.2 R2 缺口 1）：同时读 way0 和 way1 不同 set，确认 read_port 不互相污染
   - **同周期 hit-aread + miss-fill-write**（§2.2 R2 缺口 2）：一个 way 是 hit（aread），另一个 way miss（fill write），确认两条路径独立
   - → ctest -R test_cache_complete 通过
4. **riscv-mini 串接验证** — 加 `examples/riscv-mini/CMakeLists.txt` smoke target，实例化 `ICachePrefetch` + `DCacheWriteBuffer` → ctest -R riscv_mini_smoke 通过
5. **文档同步** — 改 root AGENTS.md §KNOWN LIMITATIONS 删 dead-on-arrival bullet + ADR-037 §5 Status 增补"rewrite 完成"+ `include/cpu/AGENTS.md` STRUCTURE 加 ch_mem 描述
6. **archive / 复审** — 复审 + 启动 ADR-037 §2 重启（docs-only 收尾变 active 执行）

## 7. Status

2026-10-18: Accepted. Builder change 待启动（推荐进入独立 session，3-5 天工作量）。

## 8. Supersedes

- ADR-037 §2 variant (a) BYPASS mode（不采用）
- ADR-037 §2 variant (b) "delete only test_cache_pipeline_integration.cpp"（推迟到 rewrite 落地后评估）
