# Design: strengthen-verilator-e2e-coverage

## Context

ADR-035 §M5 e2e tier 在 2026-09 月底确立（commit `e19230c` + `9ab7cb5`），当前已有 9 个真仿真 TEST_CASE（散在 3 个 test binary 中）：
- `test_verilator_e2e_harness.cpp`（4 个 TEST_CASE）：E2E CounterSimulatorPortAccess / RealFiftyCycleCounterSimulator / SamplesCounter4Bit50CyclesMod16 / LibVtopSoOnDisk
- `test_verilator_backend.cpp`（3 个 TEST_CASE）：E2E CounterFiftyCycles / TwoStepClockToggleIncrementOnePerTick / E2E PortBindingReadWrite
- `test_axi_lite_verilator_e2e.cpp`（2 个 TEST_CASE）：AxiLiteTopPortAccessSnapshot / WriteReadReg0ThroughBackend

但这 9 个 TEST_CASE **全部基于单一 fixture**（CounterFixture<N> 或 AxiLiteTop），都只验证**终值**（`data_map_[out_lnode] == 50` 或 `rdata == 0xDEADBEEF`），没有**逐 cycle**对比 interpreter 与 verilator 的 output trace。

本 change 在 e2e tier 之上**增加覆盖广度**与**trace parity 强约束**，不动 `VerilatorBackend` 实现一行。

## Goals / Non-Goals

详见 `proposal.md`。本设计文档补充技术决策。

## Decisions

### D1：trace parity 以 interpreter 为 ground truth

- 决策：trace parity 单测在**同一 fixture**上跑 interpreter 与 verilator 两个后端，逐 cycle 对比所有 `ch_out` port 的 `data_map_` 值
- 理由：
  - interpreter 是 project 内最成熟的仿真器，零争议的 ground truth
  - verilator 是被验证对象
  - 任何一个 cycle 不匹配 = fail，避免"verilator 整体偏移 N cycle"的静默 bug
- 实现（与 `test_verilator_e2e_harness.cpp:100-163` 逐行对齐，避免被 AI 抄残）：
  ```cpp
  // tests/test_verilator_trace_parity.cpp
  TEST_CASE("TraceParity - CounterFixture32TwentyCycles",
            "[verilator][e2e][trace-parity][m5]") {
      if (!tool_available("verilator")) { SKIP("verilator not on PATH"); }

      // 1) Interpreter ground truth（关键不变量 2：必须显式 set_backend）
      auto dev_a = std::make_unique<ch::ch_device<CounterFixture<32>>>();
      ch::Simulator sim_a(dev_a->context());
      sim_a.set_backend(std::make_unique<ch::core::InterpreterBackend>());

      // 2) Verilator side（单 device + 单 backend，跨 cycle 不重建）
      auto dev_b = std::make_unique<ch::ch_device<CounterFixture<32>>>();
      ch::Simulator sim_b(dev_b->context());
      ch::data_map_t data_map_b;
      auto backend = std::make_unique<ch::core::VerilatorBackend>(
          make_temp_dir("_parity"));
      if (!backend->initialize(dev_b->context(), data_map_b)) {
          SKIP("verilator compile failed");
      }
      if (backend->port_access_snapshot().empty()) {
          SKIP("dlopen did not populate port_access_");
      }
      sim_b.set_backend(std::move(backend));

      auto out_id = dev_a->instance().io().out.impl()->id();
      REQUIRE(out_id == dev_b->instance().io().out.impl()->id());

      // 3) 逐 cycle 对比（每次 tick(1) 后读 sim.data_map()，关键不变量 6：post-tick 视图）
      for (uint32_t cycle = 1; cycle <= 20; ++cycle) {
          sim_a.tick(1);
          sim_b.tick(1);
          const auto& map_a = sim_a.data_map();
          const auto& map_b = sim_b.data_map();
          auto it_a = map_a.find(out_id);
          auto it_b = map_b.find(out_id);
          REQUIRE(it_a != map_a.end());
          REQUIRE(it_b != map_b.end());
          uint64_t val_a = static_cast<uint64_t>(it_a->second);
          uint64_t val_b = static_cast<uint64_t>(it_b->second);
          REQUIRE(val_a == val_b);  // 必须 == cycle
      }
  }
  ```
- 关键不变量 1：interpreter 与 verilator 各持**单 device**（不每 cycle 重建）；interpreter 跑过的 context 不可复用给 verilator（per F2 JIT cross-DUT state pollution 教训）
- 关键不变量 2：ground truth 必须钉死 interpreter —— 默认 `Simulator` 在 `CH_JIT_ENABLE=ON`（默认）下实际走 JIT（`include/simulator.h:569`），parity 测试的 interpreter 侧 **MUST** 显式 `sim_a.set_backend(std::make_unique<InterpreterBackend>())`，否则失败时无法归因 JIT vs verilator
- 关键不变量 3：parity fixture **MUST NOT** 包含 `ch_mem` **`aread`** 读端口（已立 `openspec/specs/mem-read-timing/spec.md` 明确 aread 在 Verilog 是 0-cycle comb、Simulator 是 posedge，1-cycle skew，**parity 必失败**）；`sread` 路径两后端都是 posedge 对齐（parity 安全），可用于 RV32I imem 等场景。SyncFifoFixture 类 `ch_mem` 数据端口**只比 full/empty 序列 + 终值**，不逐 cycle 比 data。
- 关键不变量 4：parity fixture 的**观测点 MUST 为顶层 `ch_out` 端口**（已立 verilator-backend-e2e spec §M6 + ADR-035 §M6 拒绝深内 memory 观察；`data_map_[regs[3].id()]` 对 verilator 侧永远拿不到真值）。RV32I fixture 观测点用 `perf_instructions`（已暴露），SPI FSM 观测点用 `dbg_state` 等专用 debug `ch_out`
- 关键不变量 5：parity fixture **MUST NOT** 依赖"always_ff 寄存器传播"（`test_axi_lite_verilator_e2e.cpp:23-27` 记录 issue #25：M0.5 dispatch 下寄存器更新可能不传播，最终断言用 CHECK 而非 REQUIRE；依赖寄存器传播的 REQUIRE-parity 必 flaky）。AxiLite 阶段 1.4 与 AxiFull 阶段 6.5 fixture 只比较**组合逻辑直连**的 output port（awready / wready / bvalid 等由 assign 驱动），**不比较** rvalid/rdata 等寄存器驱动的 port
- 关键不变量 6：cycle 采样边界钉死为「tick 返回后读 `sim.data_map()`，post-tick 视图」；不依赖 3-eval/tick 的中间态（`Simulator::set_backend` 注释明确 eval 迁移是 deferred）

### D2：bundle 仿真 fixture 选择

- 决策：4 个场景（API 以 `tests/test_bundle.cpp:18-29` 的 CRTP 模式为准；`ch_bundle` 模板不存在）
  1. **单层多字段**：`struct AbcBundle : public bundle_base<AbcBundle> { ch_uint<8> a,b,c; CH_BUNDLE_FIELDS_T(a,b,c) };` → 字段赋值 → 绑独立 `ch_out` → verilator 仿真
  2. **双层嵌套**：`InnerBundle`（同为 `bundle_base` 派生 struct）作为 `OuterBundle` 字段，递归 `<<=` 由 `bundle_base.h:137-143` 支持 → 嵌套字段访问 → verilator 仿真
  3. **bundle 数组**：`XBundle bundles[3];` C 数组 → 索引访问 → verilator 仿真（无既有测试先例，先在 interpreter 下验证再进 verilator）
  4. **bundle `<<=` 链**：同型 `A <<= B; B <<= C;` + `C.field <<= reg` 输出（`operator<<=` 仅接受同型 bundle 左值，`bundle_base.h:119`；不接受 reg/literal）→ verilator 仿真验证 A 字段
- 端口约束：所有场景观测点 **MUST** 为独立 `ch_out` 端口（遵守 AGENTS.md "bundle 不作 IO，字段绑独立端口"约定）
- 理由：
  - 覆盖 chlib/bundle/ 目录核心 API
  - 任何"verilator 字段名错"（R5 风险）的回归会立刻被 catch
  - 4 个场景在 ~150 行内可实现，cost 合理

### D3：Stream/FIFO 仿真选择

- 决策：2 个场景
  1. **handshake 单笔**：producer 发 1 笔 → consumer 在 cycle N 收 → verilator 验证 valid/ready/payload 时序
  2. **back-pressure 多笔**：consumer 偶数 cycle 不 ready → producer 持续发 → verilator 验证 stall 行为
- 理由：
  - Stream 是 SpinalHDL-style API 的核心 handshake 模式
  - back-pressure 行为是 FIFO 设计的灵魂，覆盖一次胜过 5 个 happy path
  - 复用 `samples/stream_handshake_example.cpp` 的 `StreamFifoComponent` 模式（现存唯一 ch_stream fixture；`examples/spinalhdl-ported/fifo/` 的 `FiFo` 用裸端口），cost 最低

### D4：lint 升级策略（40+ → 8+ 个 TEST_CASE）

- 决策：从 `test_verilog_{arith,compare,shift,gen}` 各选 1-2 个**最有可能 catch codegen 回归**的 TEST_CASE 升级为真 lint
- 候选：
  - `VerilogGen - ArithAdd`（add 操作符模板）
  - `VerilogGen - ArithMul`（mul 操作符模板，宽位截断）
  - `VerilogGen - CompareEq`（eq 操作符模板）
  - `VerilogGen - Shl` / `VerilogGen - Shr`（移位操作符模板）
  - `VerilogGen - RegisterWithComplexLogic`（寄存器 always_ff 模板）
  - `VerilogGen - AndReduce`（reduction 操作符）
  - `VerilogGen - Zext` / `VerilogGen - Sext`（zero/sign extend 模板）
- 实现：在每个被选 TEST_CASE 末尾追加：
  ```cpp
  // Lint 升级：把生成的 verilog 喂给 verilator --lint-only
  if (tool_available("verilator")) {
      auto tmp = write_to_tempfile(verilog, ".v");
      ToolResult r = run_tool("verilator --lint-only -Wno-fatal -Wno-WIDTH " + tmp + " 2>&1");
      REQUIRE(r.exit_code == 0);
  }
  ```
- 复用 `test_verilog_external.cpp` 的 `tool_available` / `run_tool` / `write_to_tempfile` helper（提取为共享 header `tests/external_tool_helpers.h`）

### D5：test_verilator_three_way 改名

- 决策：文件改名 `tests/test_verilator_three_way.cpp` → `tests/test_codegen_literal_mask.cpp`
- CMake 改动：
  - `add_catch_test(test_verilator_three_way test_verilator_three_way.cpp)` → `add_catch_test(test_codegen_literal_mask test_codegen_literal_mask.cpp)`
  - 追加旧名 alias（CTest 无 ALIASES 测试属性，用第二个 `add_test` 指向同一可执行文件实现）：
    `add_test(NAME test_verilator_three_way COMMAND test_codegen_literal_mask)`
    `set_tests_properties(test_verilator_three_way PROPERTIES LABELS "base")`，保留历史 ctest 命令兼容
- 文件内容 0 改动（除了 `// tests/test_codegen_literal_mask.cpp` 顶行）
- 理由：AGENTS.md "禁用'误导命名'" + 当前注释 17 行已自承"不依赖 verilator"，改名是诚信修复

### D6：不修改 VerilatorBackend

- 决策：`src/core/verilator_backend.cpp` / `include/core/verilator_backend.h` 在本 change 中 diff 为 0
- 理由：
  - scope 控制：测试侧 change 不应混入实现侧
  - 现有 6 个 e2e 测试已验证 VerilatorBackend 正确性，本 change 关注**测试覆盖广度**而非**后端正确性**
  - 若新测试发现 VerilatorBackend bug，是 follow-up change，本 change 不预设

### D7：代表性 Memory 与复杂设计选择标准

- 决策：阶段 6/7 选 4 个 fixture（Sync FIFO / AXI4-Full / RV32I Pipeline / SPI FSM）作为 35 种设计类型中的**代表性**
- 理由：
  - **Sync FIFO**：ch_mem + pointer + full/empty 是 memory 设计的最小复刻；验证 verilator 对 `ch_mem` 时序的支持
  - **AXI4-Full Slave**：复用 AxiLiteTop 模式，最小成本扩展 burst 验证；5-channel + burst 是 SoC interconnect 基础
  - **RV32I 5-stage Pipeline**：CPU 代表性设计，IF/ID/EX/MEM/WB 跨 cycle 数据流；验证 verilator 对 forwarding 路径的支持（本 fixture 选 **3 条无 hazard 指令**压 EX→EX / MEM→WB forwarding；**不压** load-use / branch flush——后者由 interpreter 层 `test_rv32i_pipeline` / `test_hazard` 覆盖）
  - **SPI FSM**：`ch_state_machine` DSL 验证 4-state 状态机；与 UART TX / I2C / WS2812 同模式（FSM + shift reg + clock div）
- 排除：
  - 不选 I-Cache/D-Cache：复杂度远超单测 cost；留作 perf + RV32I 集成测试
  - 不选 Sigma-Delta DAC：单寄存器无 FSM，无新覆盖价值
  - 不选 VGA / Quadrature Encoder：纯计数器组合，无 ch_stream/bundle/FSM 新构造

### D8：三档 descope 机制（解除 D6 + D7 + 暂停机制的结构性死锁）

- 决策：本 change 实施中如发现 backend bug，按三档处理（**archive 不被阻塞**）
- 三档定义：
  - (a) **fixture 自身 bug**（如缺少 `ctx_swap`、`ch_out` 顺序错、tag 错）→ 修 fixture，本 change 内继续
  - (b) **疑似 backend bug**（如寄存器不传播、port field 名错、verilator 编译错）→ 该 TEST_CASE 转 **documented SKIP**：
    - 源码注释：`SKIP("backend bug #N; see follow-up change <name>")`（与现有 `test_verilator_e2e_harness.cpp:107` 的 SKIP 模式同）
    - tasks.md 记录：标记该 task 为 `⏸️ Deferred to <follow-up-change-name>`
    - 关联 `openspec/changes/<follow-up>/` 创建并被 archive 后，本 change 才完成闭环
  - (c) **follow-up change** 修 backend + 移除 SKIP：与本 change 解耦 review，按 ADR-035 / verilator-error-flow spec 实施
- 理由：
  - D6（不动 backend）+ D7（选 RV32I 暴露 risk）+ 旧"暂停机制" = 任何 fixture 发现 backend bug 都无限期阻塞 archive
  - 改为 descope 后，本 change 完成 = 8 阶段 commit + `openspec validate` 8/8 + 唯一一次 archive；backend 修复在 follow-up change 内独立推进
  - 与 D6 完全兼容（backend diff 仍为 0）
- 影响：tasks.md 2.8/3.7/6.8/7.8 四处"暂停"措辞改为"documented SKIP + follow-up change"

## Risks / Trade-offs

| 风险 | 等级 | 缓解 |
|------|------|------|
| 新增 6 个 test binary 拉长 build 时间 | 中 | 每个新 test < 200 行；build 时间 +5-10s（vs 现状 ~3min） |
| 6 个新 test 在 PR matrix (无 verilator) 全 SKIP → vacuous pass | **高** | 矩阵需区分 executed/skipped 两列（proposal 后续补）；CMake 三件套 `CATCH_AMALGAMATED_CUSTOM_MAIN` 让 exit 0 而非 4（见 tasks 1.6/2.6/3.5/6.6/7.6）；CI matrix 已有 `verilator` label 单独跑 |
| trace parity 双倍 wall-clock | 中 | 关键不变量 1：interpreter 与 verilator 各持单 device，循环内各 tick(1)，不每 cycle 重建（vs 草案 5 阶段版本每 cycle 重建 O(n²)） |
| `ctest -L base` 拖慢日常 ctest | **高** | RESOURCE_LOCK 让 6 个新 test `-j` 不加速，cold 首次 ~3-7 min 增量；建议**不**给 6 个新 test 加 `base` label，单独 label `verilator-e2e`（任务 1.6/2.6/3.5/6.6/7.6 改为 `LABELS "verilator-e2e"`，日常 ctest 不跑） |
| lint 升级依赖 verilator on PATH | 低 | `tool_available("verilator")` SKIP 模式（与 `test_verilog_external.cpp` 同） |
| test_verilator_three_way 改名破坏外部 CI 引用 | 低 | 旧名 add_test alias 保留 |
| RV32I Pipeline verilator 编译慢 | 中 | `set_tests_properties(... TIMEOUT 600)`（`add_catch_test` 不支持 TIMEOUT 参数，必须单独 set_tests_properties）；仍超时视为环境性能问题，调 TIMEOUT 而非降 fixture |
| RV32I 依赖 TCM / DDR mock 内存 | 高 | fixture 提供简化 imem（`ch_mem` 装 3 条指令）+ dmem stub；与现有 riscv-mini example 模式一致 |
| AXI4-Full burst 长度参数化 | 低 | fixture 硬编码 burst=4 笔；如需参数化留 future change |
| 结构性死锁（D6 不动 backend + D7 选 RV32I 暴露 risk + 暂停机制 → 无限阻塞 archive） | **高** | 改为三档 descope 机制（见 D8）：(a) fixture bug → 修 fixture 本 change 内继续；(b) 疑似 backend bug → TEST_CASE 转 documented SKIP（含错误码 + follow-up change 链接），**archive 不被阻塞**；(c) follow-up change 修 backend 并移除 SKIP |
| bundle 仿真发现 VerilatorBackend bug | 中 | 同 D8 (b)：documented SKIP + follow-up change |
| Stream/FIFO 仿真 cycle 数 | 低 | 与现有 e2e 测试同量级（20-50 cycles），TIMEOUT 60s 足够 |
| `add_catch_test` 不支持 TIMEOUT 参数 | 中 | 必须单独 `set_tests_properties(test_xxx PROPERTIES TIMEOUT N)`（先例 `tests/CMakeLists.txt:120/123`） |

## 验证矩阵

| 验证项 | 命令 | 期望结果 |
|--------|------|----------|
| 编译 | `cmake --build build -j$(nproc)` | exit 0 |
| 现有 ctest 不退化 | `ctest -LE perf` | 154/154 仍通过（改名后 ctest 实际 +1 alias → 155；记数口径见 E4） |
| 新增 trace parity | `ctest -R test_verilator_trace_parity` | 全通过 |
| 新增 bundle e2e | `ctest -R test_verilator_bundle_e2e` | 全通过 |
| 新增 stream e2e | `ctest -R test_verilator_stream_e2e` | 全通过 |
| 新增 memory e2e | `ctest -R test_verilator_memory_e2e` | 全通过 |
| 新增 complex e2e | `ctest -R test_verilator_complex_e2e` | 全通过 |
| 升级的 lint 门 | `ctest -R test_verilog_` 全跑 | 至少 8 个新加 lint 断言全过 |
| 改名兼容性 | `ctest -R test_verilator_three_way` 仍命中 | exit 0（旧名 add_test alias 工作） |
| 例程 0 改动 | `ctest -R spinalhdl_ported` | 25/25 仍通过 |
| openspec validate | `openspec validate strengthen-verilator-e2e-coverage --strict` | `Change 'strengthen-verilator-e2e-coverage' is valid` |
| spec 一致性 | `openspec validate --specs --strict` | 8/8 passed（+1 新 capability） |

## TDD 7 阶段骨架（详细在 tasks.md）

### 阶段 1：RED — trace parity 单测
- 创建 `tests/test_verilator_trace_parity.cpp` + 3 个 TEST_CASE
- `ctest -R test_verilator_trace_parity` 全过

### 阶段 2：GREEN — bundle 仿真
- 创建 `tests/test_verilator_bundle_e2e.cpp` + 4 个 TEST_CASE
- `ctest -R test_verilator_bundle_e2e` 全过
- 如发现 VerilatorBackend bug → **暂停**，新开 change

### 阶段 3：GREEN — Stream/FIFO 仿真
- 创建 `tests/test_verilator_stream_e2e.cpp` + 2 个 TEST_CASE
- `ctest -R test_verilator_stream_e2e` 全过
- 如发现 VerilatorBackend bug → **暂停**，新开 change

### 阶段 4：REFACTOR — lint 升级 + 改名
- 提取 `tests/external_tool_helpers.h`
- 在 8 个被选 TEST_CASE 末尾追加 lint 断言
- `test_verilator_three_way.cpp` → `test_codegen_literal_mask.cpp`（旧名 add_test alias 保留）
- `ctest -R test_verilog_` 全过

### 阶段 5：VERIFY — 全量 ctest 0 回归
- `ctest -E perf_tests` 全过（154 → 160，+5 新 test binary + 1 旧名 add_test alias）
- `ctest -R perf_tests` 全过（已跑过的 perf_tests 单测）
- `openspec validate` 8/8 passed
- `openspec archive strengthen-verilator-e2e-coverage` 成功

### 阶段 6：GREEN — 代表性 Memory 设计仿真
- 创建 `tests/test_verilator_memory_e2e.cpp`
- fixture 1：Sync FIFO（简化自 `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp` 的 `FiFo`），用 `ch_mem` + rd/wr pointer + full/empty 标志，interpreter + verilator 多 cycle 对比 full/empty 状态 + 1 笔 data round-trip
- fixture 2：AXI4-Full Slave（简化自 `examples/axi4/axi4_full_example.cpp`），1 次 burst write 4 笔 + 1 次 burst read 4 笔
- trace parity: interpreter vs verilator 逐 cycle 对比 data_map_

### 阶段 7：GREEN — 代表性复杂设计仿真
- 创建 `tests/test_verilator_complex_e2e.cpp`
- fixture 1：RV32I 5-stage 流水线（简化自 `include/cpu/pipeline/rv32i_pipeline.h`），跑 3 条短指令（`addi x1,x0,1; addi x2,x0,2; add x3,x1,x2`），interpreter vs verilator 对比 `regs[3]` 终值
- fixture 2：SPI Controller FSM（简化自 `examples/spinalhdl-ported/spi/spi_controller.cpp`），4-state `ch_state_machine` IDLE → START → TRANSFER → FINISH，interpreter vs verilator 状态转移对比
- TIMEOUT 60-120s/测试（verilator 编译慢）；如遇不可逾越的 backend bug → SKIP + follow-up change（与 D6 一致）

## 关联 ADR 引用

- **ADR-035 §3.5**：dlopen + SHA-1 缓存 — trace parity 测试复用此机制
- **ADR-035 §5 R5**：IO 字段名错 — bundle 仿真测试直接 catch 此风险
- **ADR-035 §5 R7**：内存端口 verilator 不识别 — NG7 已声明本次不涉及
- **verilator-backend-e2e spec**：低层契约（dlopen / data_map 同步 / 3 步时钟）已确立，本 change 在其上**新增覆盖广度**
- **verilator-error-flow spec**：8 个错误码定义，本 change 新测试在 verilator 编译失败时 SKIP（同现有 e2e 模式）
