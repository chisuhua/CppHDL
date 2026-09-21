# ADR-035 Phase 3 完整实现（dlopen + port binding + clock model + cache + VCD）

## Why

`docs/adr/ADR-035-verilator-backend.md` Phase 表格声称 Phase 3.1-3.6 + 4.1 "✅ completed"（17 个 commits，58 个 assertions）。**实证检查代码后，声称与实现不同步**：

| Phase | ADR 声称 | 代码实际状态 | 缺口 |
|-------|---------|------------|------|
| 3.1 脚手架 | ✅ | ✅ 真完成（440 行 .cpp 骨架）| 无 |
| 3.2 sim_main + dlopen | ✅ `c5b7b1b` | ✅ 真完成（dlopen_top 40 行实装，dlsym 4 符号）| 无 |
| **3.3 port binding** | ✅ `1cfc1af` | ❌ **stub**（line 340 "Phase 3.3 follow-up: VPI or codegen"；field_ptr=nullptr；sync_inputs_to_vtop/sync_outputs_from_vtop 仅 CHINFO log）| **2w 缺口** |
| **3.4 时钟模型** | ✅ `5d0d4d4` | ❌ **stub**（eval_sequential 不读 clock_node_id_，不触发 posedge）| **0.5w 缺口** |
| 3.5 SHA-1 cache | ✅ `d0dc194` | ⚠️ 路径存在但 hit 后不会触发 invoke_verilator；路径已 check，cache miss 触发 invoke_verilator 路径已 OK | **0.5w 验证/补强** |
| 3.6 VCD | ✅ `c928dfe` | ❌ stub（enable_vcd 只存标志位，无实际 dump）| **0.5w 缺口** |
| 4.1 测试 | ✅ 17 个 58 assertions | ⚠️ 17 测试大多测脚手架生成（line 198 "Phase 3.2-3.3 will fill in the data_map <-> Vtop sync. For now, eval_combinational is a safe no-op"），端到端仿真测试缺失 | **1w 缺口** |
| 7.5 riscv-mini e2e | ❌ | ❌ | **1w 缺口**（作为下游 ChipForge 6d.5 E8 集成验证）|

**ADR 声称与代码不同步** = 这是 Phase 7.1.5+ 持续债务。**ChipForge Phase 6d.5 E8**（Verilator sim 跑 vendored RISC-V ELF）需要 Phase 3.3 + 3.4 + 4.1 e2e 真实现才能跑通。

**v2.0 必要性**：本 change 是 ADR-035 的"虚标 ✅ → 真完成"修正，建立**契约层（specs）+ 实现 + e2e 测试**的完整闭环。

## What Changes

### 实现层

| Phase | 文件 | 改动 |
|-------|------|------|
| 3.3 port binding | `src/core/verilator_backend.cpp:318-347` (build_port_access_table) + 349-361 (sync_inputs_to_vtop) + 363-372 (sync_outputs_from_vtop) | 决定方案：**(b) generated per-design accessor**（避免 VPI runtime 开销 + 兼容 SpinalHDL 模式）—— 在 `sim_main.cpp` 生成器里 emit `void set_input(uint32_t id, const void* bits)` + `void get_output(uint32_t id, void* bits)`，按 port_access_ 顺序遍历 id 写入/读出 Vtop 字段。field_ptr 在 build_port_access_table 时通过 `reinterpret_cast<uintptr_t>` + 偏移量从 Vtop 内部计算（依赖 Vtop.h 公开 field 偏移） |
| 3.4 clock model | `src/core/verilator_backend.cpp:401-415` (eval_sequential) | 1. 在 sync_inputs_to_vtop 前翻转 default_clock 字段；2. eval_sequential 在 clock_node_id_ 命中时调用一次 eval（实现 posedge 等价：1 次 eval = 1 cycle = 时钟沿 + 组合稳定） |
| 3.5 cache 验证 | `src/core/verilator_backend.cpp:158-176` | 写一个 unit test 验证 cache hit 后不触发 verilator 子进程（用 `setenv(CPPHDL_NO_VERILATOR=1)` 或类似机制拦截） |
| 3.6 VCD trace | `src/core/verilator_backend.cpp:374-385` (close_top) + 新增 `dump_vcd()` 方法 | VCD header dump + 每个 cycle 追加信号值，依赖 `verilator --trace` flag 生成 sim_main 内的 `vl_trace_filename` API（参考 Verilator manual "Tracing" 章节） |

### 测试层（`tests/test_verilator_backend.cpp` + 新增文件）

| 测试 | 内容 | Phase |
|------|------|-------|
| 现有 17 个测试 | 大多测脚手架（保留）| 4.1 |
| 新增 `verilator_e2e_counter_simulator` | `samples/counter.cpp` 跑 Verilator sim 50 cycle，与解释器对比 `counter_value == 50` | 7.3 Phase 3 验收 |
| 新增 `verilator_e2e_dlopen_real_symbols` | dlopen obj_dir/Vtop + dlsym 4 符号 + factory() 创建 Vtop + eval() 不 crash | 3.2 验证 |
| 新增 `verilator_port_binding_rw` | 写入 input field → eval() → 读回 output field，验证 data_map ↔ Vtop 双向同步 | 3.3 验证 |
| 新增 `verilator_clock_3_eval_model` | 3 次 eval = 1 cycle (comb-1 + posedge + comb-2)，验证 sequential 信号（如 4-bit counter）在第 3 eval 后翻转 | 3.4 验证 |
| 新增 `verilator_sha1_cache_hit_skips_compile` | 第二次 initialize() 不触发 verilator 子进程（用 spy mock 或 timing 验证 < 100ms） | 3.5 验证 |
| 新增 `verilator_vcd_dump_writes_file` | enable_vcd() 后跑 100 cycle，验证 .vcd 文件生成且非空 | 3.6 验证 |
| 新增 `verilator_riscv_mini_hello_world` | 集成 ChipForge 6d 5-stage CPU：跑 vendored rv32ui-p-add ELF 50 cycle，DMem[0x1000] == 1 | 7.5 集成验收 |

### 文档层

| 文档 | 改动 |
|------|------|
| `docs/adr/ADR-035-verilator-backend.md` | Phase 表格从"❌ ADR/code 不一致" → "✅ completed"（仅在本 change 真完成后）；新增 v2.0 章节记录 2026-09-21 修正审计 + 后续风险（R10 multi-dlopen `RTLD_LOCAL` + `--output-split-cfuncs 500`） |
| `docs/usage_guide/10-verilator-backend.md` | 更新 e2e 示例：从"脚手架 GA" → "端到端可仿真" |
| `CHANGELOG.md` | v1.5 段：Verilator Backend 端到端可用（如果 v1.5 还未发布则创建） |

### Capability 契约

新增 capability `verilator-backend-e2e`（specs/capabilities/）定义 `SHALL` 级契约：

- `SHALL` 跑通 `samples/counter.cpp` Verilator sim 与解释器结果一致（counter_value == cycle_count）
- `SHALL` dlopen obj_dir/Vtop + dlsym 4 符号不 crash
- `SHALL` data_map 输入字段变化后 eval() → 输出字段正确反映
- `SHALL` 3 次 eval = 1 cycle (sequential 翻转 1 次)
- `SHALL` SHA-1 cache hit < 100ms 不触发 verilator 子进程
- `SHALL` enable_vcd() 跑 N cycle 生成 .vcd 文件非空

## 实施路径

**估时**: 5w（与 ADR 表格"已完成"声称矛盾 — 实际是 5w 实工作）

| 阶段 | 内容 | 时间 | 估时 |
|------|------|------|------|
| **M1** (Phase 3.3) | port binding 实装 + 4 个新 unit test | 2w |
| **M2** (Phase 3.4) | clock model + 1 个 test_3_eval_model | 0.5w |
| **M3** (Phase 3.5) | cache 验证 + 1 个 test_sha1_cache_hit | 0.5w |
| **M4** (Phase 3.6) | VCD 实装 + 1 个 test_vcd_dump | 0.5w |
| **M5** (Phase 4.1 e2e) | counter + dlopen + port binding + 3_eval + cache + VCD 测试 | 1w |
| **M6** (Phase 7.5) | riscv-mini hello world + ChipForge 集成验证 | 0.5w |

**总计**: 5w（单人实装 + 自测）

## Scope 边界

### 在 scope

- ✅ `src/core/verilator_backend.{h,cpp}` 实装（3.3, 3.4, 3.6）
- ✅ `tests/test_verilator_backend.cpp` 补 e2e 测试
- ✅ `docs/adr/ADR-035-verilator-backend.md` v2.0 修订
- ✅ `docs/usage_guide/10-verilator-backend.md` 更新
- ✅ `CHANGELOG.md` v1.5 段
- ✅ 新增 `specs/capabilities/verilator-backend-e2e/spec.md` 契约

### 不在 scope

- ❌ `samples/counter.cpp` 修改（仅作为测试 fixture 验证 e2e）
- ❌ ChipForge 主仓代码（CppHDL ADR-035 是 sibling repo 工作，ChipForge 6d.5 E8 集成验证在 M6 阶段作为下游消费）
- ❌ Verilator 工具链升级（已验证 Ubuntu 24.04 + Verilator 5.020-1 可用）
- ❌ 多 simulator 并行优化（R10 风险，仅文档化）

## 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| 生成的 Vtop field layout 因 Verilator 版本变化 | 🟡 中 | 用 `Vtop__Syms.h` 公开 field offset，不依赖 struct layout 直接 cast |
| SpinalHDL-style per-design accessor 增加编译时间 | 🟢 低 | accessor 是 small inline function in sim_main.cpp，编译可忽略 |
| VCD 写出大量信号触发 IO 瓶颈 | 🟢 低 | 默认 VCD off，仅 enable_vcd() 时开启 |
| 3-eval/tick 模型与下游消费方语义不一致 | 🟡 中 | ADR-009 仿真求值顺序已定义；本 change 不修改 Simulator 主循环，仅 sync 在正确阶段 |

## 不采纳方案

| 方案 | 理由 |
|------|------|
| VPI runtime port binding | 运行时 VPI lookup 开销 > 编译期 accessor，且与 SpinalHDL 模式偏离 |
| 一次性 Sim + dlopen Vtop 静态链接 | dlopen 模式允许增量编译（Vtop 缓存命中），静态链接每次全 build |
| 跳过 Phase 3.6 (VCD) | Waveform dump 是 SpinalHDL/CppHDL 用户验证的标准工具，不应跳过 |

## 验收标准

参考 ADR-035 §7.3 + §7.4 + §7.5：

- [ ] `samples/counter.cpp` 跑通 Verilator sim，counter_value == 50（与解释器一致）
- [ ] 10 个代表 sample 在 Verilator sim 跑通
- [ ] SHA-1 cache hit < 100ms
- [ ] VCD trace 生成 .vcd 文件
- [ ] 132 ctest 全绿
- [ ] 28/28 ported tests 通过 `run_all_ported_tests.sh`
- [ ] `tests/test_verilator_backend.cpp` ≥ 25 个 [verilator] 测试 PASS
- [ ] **新增**：riscv-mini hello world 跑 Verilator sim 与 CppHDL sim 一致（Phase 7.5 集成）

## 上游消费者（下游集成）

**ChipForge Phase 6d.5 E8**：本 change archive 后，ChipForge 可启动 `openspec/changes/phase-6d-verilator-sim` change（已 proposal 化），实装 `tools/verilator_runner/main.cpp` + `tests/cpu/test_cpu_verilator_sim.cpp`，跑 vendored rv32ui-p-{add,addi,auipc,beq,jal} 5 ELF 验证 Verilator sim 与 CppHDL sim tohost=1 + cycle 数 ±10% 一致。
