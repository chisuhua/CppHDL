# Design: verilator-reset-initial-value-mismatch

## Context

`strengthen-verilator-e2e-coverage` archive (2026-10-14) 留下了 2 个 D8 (b) descope SKIP：

1. `tests/test_verilator_trace_parity.cpp::TraceParity - BundleMultiFieldTwentyCycles`：fixture 用 `ch_reg(0_d)` workaround，注释明说 "D8 (b) descope"
2. `tests/test_verilator_complex_e2e.cpp::ComplexE2E - Rv32iShortInstructionTraceParity`：documented SKIP（RV32I 是另一个 change）

本次 change 修 (1)，留下 (2) 给 `verilator-rv32i-pipeline-e2e` follow-up。

Oracle 调查已诊断两处 bug：

### Bug A（致命）：try_initialize 缺复位脉冲

`src/core/verilator_backend.cpp:365-373`：

```cpp
// verilator-issue-25-fix: Verilator initializes signals to X, which
// would propagate through async-reset always_ff blocks (kept at 0).
// Force reset low (one direct toggle, not the 2-step clock pattern) so
// the 2-step clock toggle in eval_sequential is the only thing that
// drives reg updates going forward.
if (reset_field_ptr_ && top_instance_) {
    *static_cast<uint8_t *>(reset_field_ptr_) = 0;
    if (eval_fn_) eval_fn_(top_instance_);
}
```

注释自承"Force reset low"——这让 Verilog `always_ff @(posedge clk or posedge default_reset)` 块的 `if (rst) reg <= INIT` 分支**永远不会被触发**，因为 `@(posedge default_reset)` 需要 reset 的 0→1 沿。

`VerilatorBackend::reset()` (`src/core/verilator_backend.cpp:791-813`) 有正确的 1→eval→0→eval 脉冲序列，但 `Simulator::set_backend()` 与 `Simulator::tick()` **从不调用**。

对照 interpreter：`Regimpl::create_instruction` (`src/ast/ast_nodes.cpp:38-41`) 在建指令时 `current_buf->assign_truncate(*init_buf)`，寄存器天生带初值。

### Bug B（表面）：复位字面量宽度窄于 reg 宽

`src/codegen_verilog.cpp:604-610`：

```cpp
std::string reset_value = "1'b0";
auto *init_node = node->get_init_val();
if (init_node && init_node->type() == ch::core::lnodetype::type_lit) {
    auto *lit = static_cast<ch::core::litimpl *>(init_node);
    reset_value = get_literal_str(lit->value());   // ← 字面量自身位宽，非 reg 宽
}
```

`ch_reg<ch_uint<8>>(10_d)` 中 `10_d` 是 `ch_literal_impl<10, 4>`（宽 = `bit_width(10)` = 4）；`build_register` 用 `value.width()`=4 造 `litimpl`，**不与 reg 宽对齐**。生成的 Verilog：`reg <= 4'ha;`（系统零扩展到 8 bit = `8'h0a`，值正确，但字面量形态不稳定）。

## Decisions

### D1：initialize 末尾加 reset 脉冲

- 决策：在 `try_initialize()` 现有 force-low 块（lines 365-373）后追加一次 1→eval→0→eval 脉冲，与 `VerilatorBackend::reset()` 同款序列
- 实现：
  ```cpp
  // verilator-reset-initial-value-mismatch fix: pulse reset once so
  // @(posedge default_reset) loads ch_reg init values into the
  // Verilog always_ff reset branch (matches interpreter's
  // Regimpl::create_instruction time-of-init semantics).
  if (reset_field_ptr_ && top_instance_ && eval_fn_) {
      *static_cast<uint8_t *>(reset_field_ptr_) = 1;
      eval_fn_(top_instance_);
      *static_cast<uint8_t *>(reset_field_ptr_) = 0;
      eval_fn_(top_instance_);
  } else if (top_instance_ && eval_fn_ && reset_node_id_ != UINT32_MAX
             && set_input_fn_) {
      uint64_t hi = 1;
      set_input_fn_(top_instance_, reset_node_id_, &hi);
      eval_fn_(top_instance_);
      uint64_t lo = 0;
      set_input_fn_(top_instance_, reset_node_id_, &lo);
      eval_fn_(top_instance_);
  }
  ```
- 理由：
  - 与 `reset()` (lines 791-813) 共用相同的 4-step 序列，行为一致
  - t=0 时一次性脉冲，复位分支被一次性加载，**后续不再触发**（除非外部 reset 信号再被断言）
  - 0_d fixture 行为不变（复位值本来就是 `1'b0`，零扩展后还是 0，verilator 上电零也匹配）
  - `Simulator::set_backend` 重复 initialize 幂等（多次脉冲加载同样的 init 值）

### D2：print_reg 字面量按 reg 宽规范化

- 决策：把 `get_literal_str(lit->value())` 改为先用 `sdata_type(static_cast<uint64_t>(lit->value()), node->size())` 规范化，再 `get_literal_str`
- 实现：
  ```cpp
  if (init_node && init_node->type() == ch::core::lnodetype::type_lit) {
      auto *lit = static_cast<ch::core::litimpl *>(init_node);
      auto chsv_const_width = node->size();  // reg 宽
      // 规范化到 reg 宽（零扩展 / 截断），消除 init 字面量窄于 reg 宽
      sdata_type widened(static_cast<uint64_t>(lit->value()),
                         chsv_const_width);
      reset_value = get_literal_str(widened);
  }
  ```
- 理由：
  - 单一修复点，所有 init 字面量形态稳定为 `<reg_width>'h<value>`
  - 与 `96ca969` 的 mask 语义一致（值先 mask 到 reg 宽再 emit）
  - 不动 interpreter / JIT（这些走 `Regimpl` 创建时 init_buf，不经过这里）

### D3：非 type_lit init 加 CHWARN

- 决策：当前 `init_node` 不是 type_lit 时静默回落到 `1'b0`——掩盖 simulator 一侧的 init 路径。改为加 `CHWARN` 提示
- 实现：
  ```cpp
  } else if (init_node) {
      // 非 type_lit init：interpreter / JIT 可能走特殊路径，
      // Verilog 复位分支回落到 1'b0（语义不一致）。
      CHWARN("[print_reg] ch_reg '%s' init_node is not type_lit "
             "(type=%d); reset branch falls back to 1'b0",
             reg_name.c_str(), static_cast<int>(init_node->type()));
  }
  ```
- 理由：让未来的 simulator 一侧 init 演进（如 `Regimpl::eval` 用 `*rst_val_buf_` 而不是 assign_truncate）有可见的回归信号

### D4：移除 trace parity fixture workaround

- 决策：`tests/test_verilator_trace_parity.cpp` 的 `BundleMultiField<8>` fixture 把 `reg_a/reg_b/reg_c` 起始值改回 `0_d/10_d/20_d`（即 D8 (b) 注释里说明的设计意图）
- 实现：替换 `ch_reg(0_d)` 为 `ch_reg(0_d)/ch_reg(10_d)/ch_reg(20_d)`，去掉注释里 "All three registers start at 0 to avoid the VerilatorBackend ch_reg initial-value mismatch bug (D8 (b))" 的说明
- 同样：`tests/test_verilator_bundle_e2e.cpp::BundleAssignmentChain` 的 `ch_reg(0_d)` 保留——它的 5 cycle 后值（`reg == 5`）也是当前 case，改成非零初值属于 G2 的子集

### D5：不引入 runtime reset 路径

- 决策：本 change 只修 `try_initialize` 时刻的 reset 脉冲。**不**在 `Simulator::reset()` / `Simulator::tick()` 路径上引入运行时 reset。
- 理由：
  - interpreter 侧 reset 语义与 Verilog 不等价（`instr_reg::eval` rst 断言且 `rst_val_buf_ == nullptr` 时保持原值，`src/ast/instr_reg.cpp:33-39`）
  - 运行时 reset 一致性是 interpreter-level 的语义变化，超出本 change scope
  - t=0 reset 脉冲已足以让所有"上电即生效"的 ch_reg init 模式与 interpreter 对齐

## Risks / Trade-offs

| 风险 | 等级 | 缓解 |
|------|------|------|
| `try_initialize` 多 2 次 eval 增加后端初始化时间 | 低 | 2-3 ms 量级（与现有 force-low eval 相当），可忽略 |
| 现有 0_d fixture 行为被影响 | 低 | 0_d 的复位值本来就是 `1'b0`；regression gate 用 `test_verilator_e2e_harness.cpp` 等 6 个真仿真测试自动覆盖 |
| 多次 `Simulator::set_backend` 重复 initialize 累积 reset 脉冲副作用 | 低 | reset 脉冲对同一 reg 的最终态是确定性的（同 init 值加载 N 次仍等于 INIT |
| `sdata_type` 构造对极宽 reg（>64 bit）的兼容性 | 低 | `node->size()` 返回 `unsigned`，与 `sdata_type` 签名一致；现有 `get_literal_str` 已支持宽值 |
| `node->size()` 在 type_clock / type_reset 上是垃圾值（AGENTS.md ADR-035 R5 提及） | **高** | `print_reg` 只对 `type_reg` 节点调用，`type_clock`/`type_reset` 不走这条路径；现有安全 |
| CHWARN 可能让"非 type_lit init"已存在的合法场景打印噪音 | 弱 | 当前 `regimpl` 的 init_val 只支持 `type_lit`（`src/core/node_builder.h:160-200`），非 type_lit 路径实际不可达；WARN 是"防御性"，非现实风险 |

## 验证矩阵

| 验证项 | 命令 | 期望结果 |
|--------|------|----------|
| 编译 | `cmake --build build -j$(nproc)` | exit 0, 0 warnings on changed files |
| 现有真仿真不退化 | `ctest -E "perf_tests|perf_three_way"` | 158 个 + 旧名 alias 全过 |
| trace parity 非零初值 | `ctest -R test_verilator_trace_parity --output-on-failure` | 3/3 fixture 全过（含 non-zero init bundle fixture） |
| bundle assignment chain 5 cycle | `ctest -R test_verilator_bundle_e2e --output-on-failure` | 4/4 全过 |
| verilog 字面量宽度 | `grep "reg <= 8'h" build/tests/test_verilog_gen.*/top.v` | 字面量宽度 == reg 宽 |
| openspec validate change | `openspec validate verilator-reset-initial-value-mismatch --strict` | valid |
| openspec validate specs | `openspec validate --specs --strict` | 10/10 passed（+1 新 capability） |

## TDD 4 阶段骨架（详细在 tasks.md）

### 阶段 1 — RED：现有 fixture 失败基线
1. 验证 D8 (b) workaround 当前状态（reg 全 0_d，pass）
2. 临时把 `reg_b(0_d)` 改为 `reg_b(10_d)`，确认 trace parity 失败 — 钉死 bug 存在

### 阶段 2 — GREEN：backend 修 reset 脉冲
3. 改 `src/core/verilator_backend.cpp::try_initialize` 加 reset 脉冲
4. 改 `src/codegen_verilog.cpp::print_reg` 字面量宽度规范化
5. trace parity 测试 pass（fixture 改回非零初值）

### 阶段 3 — REFACTOR：fixture workaround 清理
6. `tests/test_verilator_trace_parity.cpp` fixture 改回 `reg(0_d/10_d/20_d)`
7. `tests/test_verilator_bundle_e2e.cpp::BundleAssignmentChain` 保留 `ch_reg(0_d)` —— 它本来就是 0 初值设计
9. AGENTS.md "VERILATOR BACKEND RULES" 加 "initialize 必须脉冲 default_reset"

### 阶段 4 — VERIFY：全量 ctest 0 回归 + archive
1. `ctest -E "perf_tests|perf_three_way"` 158 + alias 全过
2. `openspec validate` 10/10
3. `openspec archive` 成功

## 关联 ADR 引用

- **ADR-035 §M5 e2e tier**："VerilatorBackend 能跑出正确结果"为真仿真基础。本 change 是 §M5 的 reg init 覆盖完善。
- **ADR-035 §3.5** SHA-1 缓存：reset 脉冲改变 Vtop 内部 reg 上电态，但生成的 Verilog 不变（只有 `print_reg` 输出的字面量宽度变化），所以缓存 key 仍命中。**但 cache key 由 verilog + sim_main + verilator 推断，print_reg 输出变化 → verilog 内容变化 → cache key 变化 → 旧缓存失效，**但这是预期的（"内容敏感" 是 ADR §3.5 R2 修的语义）。
- **verilator-backend-e2e spec**：低层契约（dlopen / data_map 同步 / 3 步时钟）已确立，本 change 在其上**新增 reset 脉冲时序契约**。
- **verilator-error-flow spec**：8 个错误码定义，本 change 不动。