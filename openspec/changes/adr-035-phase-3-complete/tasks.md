# Tasks (修订 v2 — 合并 Oracle + Metis 双审查必做项)

## 0. M0.5 Simulator↔Backend Delegation（前置阻塞）— Oracle R2

- [ ] 0.1 `include/simulator.h` 添加 `BackendGuard` helper 类（`backend_ != nullptr` 时短路 eval，否则 fallback interpreter/JIT）
- [ ] 0.2 `src/simulator.cpp` `eval_combinational()` line 219: 在 JIT 分支前加 `if (backend_) { backend_->eval_combinational(data_map_, ...); return; }`
- [ ] 0.3 `src/simulator.cpp` `eval_sequential()` line 186: 同样在 JIT 分支前加 `if (backend_) { backend_->eval_sequential(data_map_, ...); return; }`
- [ ] 0.4 单元测试: `tests/test_simulator_backend_delegation.cpp` 新建 (或追加到 `test_simulator.cpp`)—
      用 spy mock backend 记录调用次数，验证 1 tick = 2×eval_combinational + 1×eval_sequential
- [ ] 0.5 验证: 现有 132 ctest 全部不回归（interpreter 路径未受影响）

## 1. Phase 3.3 Port Binding 实装 (M1)

### 1.0 M1 前置：dlopen 改为 .so（Oracle R1 / Metis §2.1 — 必做）

- [ ] 1.0.1 `invoke_verilator()` 命令行重构: `--cc --exe --build` → `verilator --cc -Mdir obj_dir -fPIC --build sim_main.cpp obj_dir/Vtop__ALL.a -o libVtop.so`
       （用 -shared -fPIC 产出 .so 让 dlopen 可加载；obj_dir main() 烟囱测试可选保留为单独 Vtop-main 二进制）
- [ ] 1.0.2 `dlopen_top()` 中 `compiled_so_path_` 默认改为 `obj_dir/libVtop.so`
- [ ] 1.0.3 验证: `nm -D obj_dir/libVtop.so | grep _Vtop` 能看到 `set_input_Vtop`, `get_output_Vtop`, `get_field_ptr_Vtop` 等符号

### 1.1 决定方案: generated per-design accessor (SpinalHDL-style)，避免 VPI runtime 开销

### 1.2 `generate_verilog()` 修改: emit sim_main.cpp 含 5 个 extern "C" 符号

- [ ] 1.2.1 `new_Vtop` / `eval_Vtop` / `final_Vtop` / `delete_Vtop` / `set_input_Vtop` / `get_output_Vtop` / `get_field_ptr_Vtop`（7 个符号，spec §ADDED Requirements 一致）
- [ ] 1.2.2 修正 Oracle §5#5 design.md signature bug: `get_field_ptr_Vtop(void* top, uint32_t port_id)` 不是 `(void*)`
- [ ] 1.2.3 端口 id 必须用真实 `node->id()`（不是 0..N-1 sequential），即 emit `case <node_id>:` by lnode id

### 1.3 accessor 实现

- [ ] 1.3.1 switch 遍历真实 port_id，`memcpy(field_ptr, bits, (bitwidth + 7) / 8)`
- [ ] 1.3.2 一次性解析所有符号到 members (`set_input_fn_`, `get_output_fn_`, `get_field_ptr_fn_`) — Oracle §5#5 dlsym-per-call 性能修复

### 1.4 `build_port_access_table()` 修改

- [ ] 1.4.1 不再 hardcode field_ptr=nullptr；改为 `pa.field_ptr = get_field_ptr_fn_(top_instance_, node->id())`
- [ ] 1.4.2 **新增时钟作为 input**（Oracle §5#4 / R3 修复）: type_clock 节点**不要** continue；改为 `is_input=true` 加入 port_access_。删掉 design.md §2 的 toggle 逻辑（clock 由 Simulator::tick 的 default_clock_instr_->eval() 在 data_map 中翻转，backend 仅 sync 输入）
- [ ] 1.4.3 `clock_node_id_ = UINT32_MAX` fallback：context 没有 type_clock 时 eval_sequential 走 eval_inputs + eval + eval_outputs 普通路径（与 eval_combinational 同）

### 1.5 `sync_inputs_to_vtop()` 实装

- [ ] 1.5.1 遍历 port_access_ 的 is_input，read `data_map_->at(id)`，调 `set_input_fn_(top_instance_, id, &val)`
- [ ] 1.5.2 bitwidth > 64 时 `CHREQUIRE(bitwidth <= 64)`（Oracle §1 width ceiling guard）

### 1.6 `sync_outputs_from_vtop()` 实装

- [ ] 1.6.1 遍历 port_access_ 的 !is_input，调 `get_output_fn_(top_instance_, id, &val)`，write `data_map_->at(id)`

### 1.7 Cache key 扩展（Oracle §5#6 提前到 M1，避免与新符号/flags 互踩）

- [ ] 1.7.1 `compute_cache_key()` 不再 hardcode "5.020"：改为 `verilator --version` 运行时查询并 hash（popen 一次性）
- [ ] 1.7.2 key 组分: `sha1(top.v + sim_main_template_hash + verilator_flags + runtime_version)`
- [ ] 1.7.3 `sim_main.cpp` emit 变化时（或 `--trace` 变化时）自动让旧 cache 失效

### 1.8 修正现有 test_verilator_backend.cpp 的 BuildPortAccessTable 断言（Oracle §5#7）

- [ ] 1.8.1 `REQUIRE(kv.second.field_ptr == nullptr)` → dlopen 成功后改为 `field_ptr != nullptr`，dlopen 失败分支保持 nullptr-tolerant

### 1.9 验证

- [ ] 1.9.1 132 ctest 全绿（含修正后的 BuildPortAccessTable）
- [ ] 1.9.2 28/28 ported tests 全过
- [ ] 1.9.3 4 个新 M1 unit test PASS:
      - `verilator_set_input_symbols_resolved`
      - `verilator_port_binding_rw` (单 round-trip data_map ↔ Vtop)
      - `verilator_clock_node_appears_as_input` (R3 修复证据)
      - `verilator_cache_key_includes_template_and_version`

## 2. Phase 3.4 Clock Model 实装 (M2) — R3 已在 1.4.2 修复，此处是测试与集成

- [ ] 2.1 `eval_sequential()` 实现: `sync_inputs_to_vtop() + eval_fn_(top_instance_) + sync_outputs_from_vtop()` （**完全等同 eval_combinational**，因为时钟已由 Simulator::tick 的 default_clock_instr_ 翻转）
- [ ] 2.2 `tests/test_verilator_backend.cpp` 新增 `verilator_clock_3_eval_model`: 跑一个 ch_uint<4> counter fixture，1 cycle (3 eval) 后 counter_value 增 1
- [ ] 2.3 Simulator::tick() 默认时钟分发已天然产出 3-eval/tick (comb + clock_instr + comb)；无需 VerilatorBackend 内部时钟切换

## 3. Phase 3.5 Cache Hit 验证 (M3)

- [ ] 3.1 `invoke_verilator_call_count_` 计数器成员 + 暴露给 test (Oracle §5 验证方式 C / Metis §2.5B)
- [ ] 3.2 新 unit test `verilator_sha1_cache_hit_skips_compile`: 第一次 initialize → invoke_verilator 触发；第二次 initialize（同 verilog）→ 计数器不增、返回 true、整个路径 < 100ms（timing 软断言）
- [ ] 3.3 文档: `docs/adr/ADR-035-verilator-backend.md` R7 (缓存) 风险复审

## 4. Phase 3.6 VCD Trace 实装 (M4)

- [ ] 4.1 `enable_vcd(true)` + `dump_vcd()` 新增方法: header dump + 每个 cycle signal value 行
- [ ] 4.2 `invoke_verilator()` 命令行添加 `--trace` 条件：当 `enable_vcd()` 开启时 emit `sim_main.cpp` 含 `VerilatedVcdC` API
- [ ] 4.3 cache key 加入 `--trace` flag（防止 trace on/off 缓存污染）
- [ ] 4.4 验证: enable_vcd(true) + 跑 100 cycle → `sim.vcd` 文件 size > 1KB 含 100 个 `#<time>` 时间戳 + 信号值行

## 5. Phase 4.1 端到端测试 (M5) — Oracle §5#9 测试分级

- [ ] 5.0 测试 tier 拆分 (Oracle §5#9):
  - `tests/test_verilator_backend.cpp` 已有 + 新增 [verilator] 测试 → 默认 SKIP-when-tool-missing 兼容 BUILD_VERILATOR=OFF
  - 新建 `tests/test_verilator_e2e.cpp` → 强制 REQUIRE when `CPPHDL_REQUIRE_VERILATOR=1` env 设
- [ ] 5.1 `verilator_e2e_counter_simulator`: 自建 ch_uint<32> counter fixture（**不修改 samples/counter.cpp**，4-bit wraps at 16，违反 §9.3 的 ==50 标准 — 改用 fixture），跑 50 cycle → counter_value == 50
- [ ] 5.2 `verilator_e2e_dlopen_real_symbols`: dlopen `obj_dir/libVtop.so` + dlsym 7 符号 + factory() + eval() 不 crash；`REQUIRE(eval_fn_ != nullptr)`
- [ ] 5.3 `verilator_port_binding_rw`: 写 input → eval → 读 output（双向同步已验证）
- [ ] 5.4 `verilator_clock_3_eval_model`: 3 eval / cycle，sequential 翻 1 次
- [ ] 5.5 `verilator_sha1_cache_hit_skips_compile`: invoke_verilator_call_count_ 不增 + < 100ms
- [ ] 5.6 `verilator_vcd_dump_writes_file`: enable_vcd(true) + 100 cycle → `.vcd` > 1KB
- [ ] 5.7 现有 20 + 新增 6 = ≥ 26 个 [verilator] tag 测试 PASS

## 6. Phase 7.5 集成验证 (M6) — Oracle §5#10 + Metis §2.2 descope

- [ ] 6.1 **重定义 M6 scope**：取消跨仓 ChipForge 集成（Proposal §Scope 边界 ❌）
- [ ] 6.2 改为 CppHDL 仓内 `examples/riscv-mini/src/rv32i_soc.h` 5-stage RV32I pipeline
- [ ] 6.3 集成观察点选择 (Oracle §M6 fail blocker): 由于 port sync 仅覆盖顶层 IO，**不接受 DMem[0x1000]==1** 这种深内 memory 观察；改用 UART TX output port 观察 "H" 字符输出（SoC 已有 UART MMIO at 0x40001000 — ch_out uart_tx）
- [ ] 6.4 固件加载方案 (Oracle §M6 memory backdoor): 在 codegen_verilog 验证 SoC memory 是否支持 `$readmemh` / initial block；若不支持，本 milestone descope 到 "fixture level"（不跑 RISC-V，仅跑 counter + dlopen + port + clock 全链路）
- [ ] 6.5 验证: 若 6.3 + 6.4 可行 → UART TX 在 N cycle 内输出 'H'；若不可行 → 6.4 descope 不阻塞 M1-M5 archive
- [ ] 6.6 文档 v2.0 修订（仅当 M6 真完成时）: `docs/adr/ADR-035-verilator-backend.md` Phase 表格更新

## 7. 文档同步

- [ ] 7.1 `docs/adr/ADR-035-verilator-backend.md` v2.0 修订:
  - Phase 表格更新（仅在 M1-M5 + M6 真完成后）→ ✅
  - 新增 v2.0 章节记录 2026-09-21 修正审计 + Oracle/Metis 双审查引用 (`ses_f3dca2620ffet62PA4ifSJHVPK`, `ses_f3dca0901ffeuoj8KHgAz8b9gv`)
  - 风险 R1 dlopen 静态 ELF 不可行、R3 时钟设计 R3 修复、R10 multi-dlopen `RTLD_LOCAL` + `--output-split-cfuncs 500` 文档化
- [ ] 7.2 `docs/usage_guide/10-verilator-backend.md` 更新 e2e 示例: 从"脚手架 GA" → "端到端可仿真"
- [ ] 7.3 `CHANGELOG.md` v1.5 段：Verilator Backend 端到端可用

## 8. Capability 契约 (specs/) — Oracle §5#1 路径修正

- [ ] 8.1 delta spec 已位于 `openspec/changes/adr-035-phase-3-complete/specs/verilator-backend-e2e/spec.md`；**archive 时由 openspec CLI 自动 merge** 到 `openspec/specs/verilator-backend-e2e/spec.md`（capability folder 结构符合 OpenSpec 1.4.0 + AGENTS.md §OPENSPEC WORKFLOW）
- [ ] 8.2 验证: archive 后跑 `openspec validate --specs --strict` 必须 PASS 且能力计 6 new SHALL requirements（与 proposal §Capability 契约一致）

## 9. 验收 (Acceptance Criteria) — Oracle §5#8 + #1 修正

- [ ] 9.1 132 ctest 全绿
- [ ] 9.2 28/28 ported tests 通过 `run_all_ported_tests.sh`
- [ ] 9.3 自建 ch_uint<32> counter fixture 跑 Verilator sim 50 cycle → counter_value == 50
       (**注：samples/counter.cpp 为 4-bit wraps at 16，不能直接套 ==50。** 见 §5.1 fixture)
- [ ] 9.4 10 个代表 sample 在 Verilator sim 跑通（含 ch_bool、ch_uint、ch_stream、ch_reg）
- [ ] 9.5 SHA-1 cache hit < 100ms + invoke_verilator_call_count_ 二次未增
- [ ] 9.6 VCD trace 生成 .vcd > 1KB 且含 ≥ 100 个信号值行
- [ ] 9.7 `tests/test_verilator_backend.cpp` ≥ 26 个 [verilator] 测试 PASS（含修正后的 BuildPortAccessTable）
- [ ] 9.8 M6 riscv-mini: 见 §6.3 + §6.4 接受标准；若不可行则 descope 不阻塞 archive
- [ ] 9.9 **`openspec archive adr-035-phase-3-complete`** — **禁止 --skip-specs**（违反 AGENTS.md 第 113 行规则）；delta spec 有 6 个 ADDED Requirements 必须 merge

## 10. 上下游集成 (Out-of-Scope)

- **ChipForge Phase 6d.5 E8**: 本 change archive 后启动（ChipForge 仓内）
  - `openspec/changes/phase-6d-verilator-sim/` proposal 已存在（ChipForge 仓）
  - 实装 `tools/verilator_runner/main.cpp` + `tests/cpu/test_cpu_verilator_sim.cpp`
  - 跑 vendored 5 ELF tohost=1 + cycle 数 ±10% 一致
  - **本 CppHDL change 不包含 ChipForge 工作**（proposal §Scope 边界 ❌）

## 11. 审计 trail (Oracle + Metis 双审查 2026-09-21)

**Oracle 审查 session_id**: `ses_f3dca2620ffet62PA4ifSJHVPK`（位于 CppHDL 仓）

**Metis 审查 session_id**: `ses_f3dca0901ffeuoj8KHgAz8b9gv`（位于 CppHDL 仓）

### Oracle 关键发现（必做阻塞）

| 编号 | 问题 | 必做 |
|------|------|------|
| **R1** | dlopen 静态 ELF 不可行；--cc --exe --build 产出非 dlopen-able | §1.0.1 改 .so |
| **R2** | Simulator::eval_combinational/eval_sequential 从不调用 backend_ | §0.1-0.5 加 delegation |
| **R3** | 时钟设计永远无法触发（clock continue 跳过 + port_access_ find 永远 miss）| §1.4.2 把 type_clock 当 input |
| R4 | cache key 不含 sim_main template / flags / 实际 verilator version | §1.7 扩展 key |
| R5 | 现有 SKIP-on-compile-fail 让 e2e vacuous green | §5.0 tier 拆分 + REQUIRE mode |
| R6 | 现有 test field_ptr==nullptr 断言会被 M1 打破 | §1.8 更新断言 |
| R7 | 4-bit counter vs counter==50 接受标准错误 | §5.1 / §9.3 用 ch_uint<32> fixture |
| R8 | M6 缺 memory-backdoor 或 $readmemh | §6.4 descope 或加 backdoor |

### Metis 关键发现

- M6 riscv-mini 应从本 change 移除（依赖外部 ELF + 跨仓）→ §6.1-6.5 descope 或子集
- 5w 不能在 single session 完成 → 本 session 实现 M0.5 + M1 + M2 + M3，M4-M5-M6 后续 session
- tasks.md §9.9 --skip-specs 违规 → §9.9 修正

### 修复路径

`tasks v2` 闭合所有 8 项 R1-R8 阻塞；archive 后 ChipForge Phase 6d.5 E8 可启动。
