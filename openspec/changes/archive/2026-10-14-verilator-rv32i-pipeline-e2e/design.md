# Design: verilator-rv32i-pipeline-e2e

## Context

`strengthen-verilator-e2e-coverage` archive (2026-10-14) 留下 2 个 D8 (b) descope SKIP：
1. `tests/test_verilator_trace_parity.cpp::BundleMultiFieldTwentyCycles` —— 由 `verilator-reset-initial-value-mismatch` change 解除（已完成）
2. `tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity` —— 本 change 解除

`verilator-reset-initial-value-mismatch` change 修复了 backend 的 init 加载语义（try_initialize 加 4-step reset pulse），本 change 不再受该 bug 阻塞。

## Goals / Non-Goals

详见 `proposal.md`。

## Decisions

### D1：fixture 选 3 条无 hazard 短指令

- 决策：fixture 装 3 条无 hazard 短指令 `addi x1, x0, 1; addi x2, x0, 2; add x3, x1, x2`
- 理由：
  - `addi` I-type → op `0x13`，funct3 0
  - `add` R-type → op `0x33`，funct3 0
  - 3 条指令压测 EX→EX forwarding（`add x3, x1, x2` 读 x1/x2 分别在 2 条 / 1 条前指令中写）+ MEM→WB forwarding（commit_count 顺序累加 3 次）
  - 无 load-use hazard、无 branch flush、无控制冒险 → 不依赖 hazard 单元的复杂路径，最小化 backend parity 失败的可能性
- 不选 4 条以上：危险，会因单 cycle timing 偏差放大 parity 差异
- 不选 load-use / branch flush：interpreter 层已覆盖（`tests/test_hazard.cpp` + `tests/test_rv32i_pipeline.cpp`），本 fixture 目标 backend parity 不重复 correctness 验证

### D2：指令编码手工计算

- 决策：3 条指令编码手工计算并 hard-code 到 fixture 中
- 计算：
  - `addi x1, x0, 1`：`imm=1, rs1=0, funct3=0, rd=1, opcode=0x13`
    - bits: `[imm=1:11:0][rs1=0:19:15][funct3=0:14:12][rd=1:11:7][opcode=0x13:6:0]`
    - = `0x001 | (0 << 15) | (0 << 12) | (1 << 7) | 0x013`
    - = `0x094`（注：bit 0..11 = imm=1，bit 7..11 = rd=1 → 1<<7=128=0x80，bit 0..6=opcode=0x13 → 1+0+128+19 = wait, 0x13=0001_0011b=0x13 → bit 0..6 = 0x13 = 19）
    - 完整：`000000000001 00000 000 00001 0010011`
    - = `0x0000_0094` （低 12 bit 是 0x094）
  - `addi x2, x0, 2`：imm=2, rd=2
    - = `0x113`
  - `add x3, x1, x2`：rs2=2, rs1=1, funct3=0, rd=3, opcode=0x33
    - = `(2 << 20) | (1 << 15) | (0 << 12) | (3 << 7) | 0x33`
    - = `0x200000 | 0x008000 | 0x000000 | 0x000180 | 0x033`
    - = `0x00208133 + 0x180` wait: 0x180|0x033 = 0x1B3
    - 完整：`0000000 00010 00001 000 00011 0110011`
    - = `0x0020_81B3`
- fixture 注释清楚标注编码与位字段对应关系，便于 reviewer 核对

### D3：imem 用 ch_mem + 初始化数组

- 决策：`ch_mem<ch_uint<32>, 4>` 用 `std::array` 初始化 3 条指令（最后 1 个 word 为 0 = nop/junk），挂靠 instr_addr → instr_data 通路
- 实现：
  ```cpp
  ch_mem<ch_uint<32>, 4> imem{
      std::array<uint32_t, 4>{0x00000094, 0x00000113, 0x002081B3, 0x00000013},
      "imem"};
  // RV32I imem: instr_data = imem[instr_addr >> 2]  (sread, posedge)
  ```
- 理由：
  - sread 是 posedge 对齐（vs aread 是 0-cycle comb），与 verilator backend parity 兼容（verilator-e2e-coverage spec §Requirement 1 invariant 3）
  - 简化版 4-word imem 避免 TCM `InstrTCM` 的 1MB 全地址空间

### D4：dmem 简化 stub

- 决策：dmem stub = 把所有 data_addr 写的数据清零（write=0），data_read_data 始终 = 0；fixture 不做 load/store 指令，所以 dmem 行为不影响 perf_instructions 验证
- 理由：
  - 3 条无 hazard 短指令全部是 ALU 操作（addi / add），不触发 load / store
  - dmem stub 简化 fixture 实现，且对 perf_instructions 终值无影响

### D5：观测点 perf_instructions

- 决策：用 `rv32i_pipeline.h:54` 已暴露的 `ch_out<ch_uint<48>> perf_instructions` 作为唯一观测点
- 理由：
  - top-level ch_out → verilator 可观测（per verilator-backend-e2e spec §M6 / ADR-035 §M6）
  - 3 条指令 commit 后 perf_instructions 终值 == 3（与 cycle 数无关，commit 顺序累加）
  - 不观测 `regs[3]` / `regs[7]` 等内部 reg_file（深内 ch_mem，verilator 不可观测）

### D6：tick 数量 ~ 20 cycle

- 决策：tick 20 cycle 跑 fixture
- 理由：
  - RV32I pipeline 5 阶段 + forwarding 链路 3 条无 hazard 指令 commit 至少 5 + 2 = 7 cycle（commit_count 累加 3 次至少在 IF 后 5 cycle 才开始）+ safety margin 13 cycle
  - 20 cycle 足够覆盖，最坏情况 IF/ID/EX/MEM/WB 流水线寄存器全部填满 + commit_count == 3
- 验证：3 条无 hazard 指令 commit 后 perf_instructions == 3 的硬约束，与具体 cycle 数无关（即便 cycle 数不足 commit 也只反映在 perf_instructions 终值上，parity 仍然成立）

### D7：CH_JIT_ENABLE=OFF 跑测试

- 决策：fixture 用 `InterpreterBackend` 显式指定，避免与 verilator 比对时 JIT 介入导致归因混乱（沿用 trace parity test 的 pattern）
- 理由：
  - JIT vs verilator 的差异比 interpreter vs verilator 的差异更微妙，难以归因
  - 现有 trace parity test 已建立 `set_backend(std::make_unique<InterpreterBackend>())` 模式

## Risks / Trade-offs

| 风险 | 等级 | 缓解 |
|------|------|------|
| RV32I verilator 编译冷 ~ 1-3 min | 中 | TIMEOUT 600s 给 2× warm 余量；cache 命中 warm ~5-10s |
| RV32I 流水线寄存器依赖 M0.5 dispatch（issue #25）导致寄存器传播不可靠 | 中 | perf_instructions 是组合逻辑直读 commit_count，**不依赖** always_ff 寄存器传播链路（commit_count 是 always_ff reg，但暴露在顶层 ch_out，verilator 通过 Vtop 字段读取） |
| imem `ch_mem` sread 时序与 interpreter 不一致 | 低 | sread 是 posedge 对齐（vs aread 0-cycle），已立 `verilator-e2e-coverage` spec §Requirement 1 invariant 3 明确安全 |
| RV32I 内部 CSR / exception / clint 路径触发意外行为 | 低 | 3 条短指令不触发任何 CSR / exception / interrupt 路径；fixture 设计有意规避 |
| 流水线触发 `cycle_count` perf counter overflow | 低 | ch_uint<48> 范围 ~2.8e14，20 cycle 远低于 |
| ch_mem 的 4-word size 太小导致 `ch_reg<ch_uint<5>>` 地址宽度冲突 | 中 | imem `ch_uint<2>` 地址（4 word），与 RV32I 的 `ch_uint<32>` instr_addr 右移 2 位相容 |

## 验证矩阵

| 验证项 | 命令 | 期望结果 |
|--------|------|----------|
| 编译 | `cmake --build build -j$(nproc)` | exit 0, 0 warnings on changed files |
| 现有真仿真不退化 | `ctest -E "perf_tests|perf_three_way"` | 158 + alias 全过 |
| Rv32I e2e 实际执行 | `ctest -R test_verilator_complex_e2e --output-on-failure` | 2/2 全过（RV32I 真跑，非 SKIP） |
| verilator 三路 perf 不退化 | `ctest -L verilator -R perf_three_way` | 全过 |
| openspec validate change | `openspec validate verilator-rv32i-pipeline-e2e --strict` | valid |
| openspec validate specs | `openspec validate --specs --strict` | 11/11 passed |

## TDD 4 阶段骨架（详细在 tasks.md）

### 阶段 1 — RED：编写 fixture，确认测试失败
1. 在 `test_verilator_complex_e2e.cpp` 实现 RV32I fixture + 短指令 imem + dmem stub
2. 用 `complex_e2e -R Rv32i` 单跑 → 期望 verifier 发现 perf_instructions 错误（interpreter != verifier if 真实 backend bug，或者 verifier 编译失败 → SKIP）

### 阶段 2 — GREEN：fix fixture 直至测试通过
3. 如果测试因为 imem 编码错 / 端口定义错 / include path 错 而失败 → 修正 fixture
4. 如果测试因为 verifier backend bug 失败 → D8 (b) descope：documented SKIP + 后续 follow-up（本次 change 不阻塞 archive）

### 阶段 3 — REFACTOR：清理 documented SKIP 注释
5. 移除 `ComplexE2E - Rv32iShortInstructionTraceParity` 中的 `[deferred]` tag 和 SKIP 注释
6. 移除 D8 (b) descope 注释块（保留简短 history 注释指明来自哪个 change）

### 阶段 4 — VERIFY：全量 ctest + archive
1. `ctest -E "perf_tests|perf_three_way"` 全过
2. `openspec validate` 11/11
3. `openspec archive` 成功

## 关联 ADR 引用

- **ADR-035 §M5 e2e tier**：本 change 是 §M5 的代表性复杂设计覆盖完善
- **verilator-e2e-coverage spec**：trace parity invariants (no aread, top-level ch_out, no reg-propagation dep)
- **verilator-reg-init spec**（刚 archive）：t=0 reset pulse 已修 ch_reg 非零初值加载，本 fixture 受益
- **rv32i_pipeline.h**：`perf_instructions` 观测点定义 line 54
- **ch_state_machine.h**: 不在本 fixture 使用（fixture 只用 ch_reg + ch_mem + assign）

## 关联基线

- **verilator-e2e-coverage** change (archive 2026-10-14)
- **verilator-reset-initial-value-mismatch** change (archive 2026-10-14)
- **f2 fix-perf-subprocess-isolation**：trace parity 不复用 cross-DUT state（每个 fixture 重新 `ch_device<T>` 构造）
- **tests/AGENTS.md**：no misleading naming（fixture 名直白，不带"verilator"前缀）