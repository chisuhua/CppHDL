# Change: fix-codegen-ch-reg-no-next-hold-state — ch_reg 无显式 next 时 emit hold-state always_ff

## Why

**现状**（oracle audit + pilot 触发 2026-10-20）：

`src/codegen_verilog.cpp::print_reg` 在 `next_node == nullptr` 时仅输出 warning comment 而**不 emit any always_ff**。这意味着任何 `ch_reg<T>(init_value)` 但**未调用 `->next = ...`** 的寄存器（如 `ch_reg<ch_bool> tx_empty(ch_bool(true))` 在 `axi_uart.h:75`、`ch_reg<ch_bool> busy(ch_bool(false))` 在 `axi_uart.h:66`、`ch_reg<ch_bool> rx_valid(ch_bool(false))` 在 `axi_uart.h:76`）**在 VerilatorBackend 下 init value 永远不应用**——reset pulse 期间 `top_reg <= 1'b0`，永远不是 init value。

**interpreter 端行为**：interpreter backend 通过 `ast_nodes.cpp:38-41` 在 `regimpl::create_instruction()` 时 `current_buf->assign_truncate(*init_buf)`（读 `data_map[init_val_->id()]`），init value 应用正确。所以**dual parity不一致**——interpreter 端 init value 1, Verilator 端 0。

**真实代价**：

| 历史痛点 | 暴露点 |
|---|---|
| `axi_uart.h:75` `tx_empty(ch_bool(true))` 期望 `io().uart_tx = select(tx_empty, true, false) = true` | ortate path: dual parity 失败, uart_tx 应=1 实=0 |
| `axi_uart.h:66` `busy(ch_bool(false))` 期望 reset 后 busy=0 | 已知 init=true/false → 实际上 busy 永远=1 (init 0 没应用), state machine 错误状态 |
| `axi_uart.h:76` `rx_valid(ch_bool(false))` 期望 reset 后 rx_valid=0 | 同样问题 |

**预期收益**：

1. **dual parity 恢复**: interpreter + verilator 在 `ch_reg<ch_bool>(init)` 时 init value 一致
2. **axi_uart behavior dual parity 完整可测**: 之前 5/14 测试 fail (`tx_empty` / `status` / `uart_tx` 初值); 修后 14/14 PASS
3. **架构层修复**: 所有 `axi4/peripherals/*.h` 7 个 peripherals 的 13 个 ch_reg<bool> init value 现在正确应用
4. **CI 收益**: 修复后为 `dual-backend-axi4-peripherals` change 扫除最大障碍 (选项 X)

**预期成本**：

- 1 行 spec verification (no -Werror / -Wall clean)
- 1 commit（codegen_verilog.js + tasks.md 双列表）
- 0 新测试 wallclock
- ~30 min 改造（如果无意外）

**Out of Scope**（明确划线）：

- ❌ 改 axi4/peripherals/*.h 头文件（保留 interpreter-only baseline）
- ❌ 改 `ch_reg` 构造路径（init_val_ vs next_val 区分已正确）
- ❌ 改 `get_next()` 返回值语义（line 57 当前正确返回 src(0)，保留）
- ❌ `test_axi_uart_behavior.cpp` dual 化（独立 change `dual-backend-axi4-peripherals`）

## What Changes

### 1. `src/codegen_verilog.cpp` print_reg: else 分支修复

**修改前**：
```cpp
} else {
    // If next is not found or not named, print a warning
    out << "    // Warning: Register '" << node_names_[node]
        << "' has no named 'next' source.\n";
}
```

**修改后**：
```cpp
} else {
    // If next is not found or not named, emit a hold-state
    // always_ff so the register's init value is still applied
    // at the reset pulse (otherwise uart_tx etc. never see
    // their declared init value).
    std::string reg_name = node_names_[node];
    out << "    always_ff @(posedge default_clock or posedge default_reset) begin // "
        << "Register update (no next) for " << reg_name << "\n";
    out << "        if (!default_reset)\n";
    out << "            " << reg_name << " <= " << reg_name << ";\n";
    out << "        else\n";
    out << "            " << reg_name << " <= " << reset_value << ";\n";
    out << "    end\n";
}
```

**关键变化**：hold-state always_ff 在 reset pulse (`else`) 时用 `reset_value`（之前计算的 init value 字面量），在 data update (`if (!default_reset)`) 时 hold（reg <= reg 自我赋值）。

### 2. CHWARN 增强诊断

line 694 `CHWARN("[print_reg] ch_reg init_node is not type_lit ...")` 增加 `init_node->size()` + `node->name()` 字段，诊断更易：

```cpp
CHWARN("[print_reg] ch_reg init_node is not type_lit "
       "(type=%d, size=%u, name=%s); reset branch falls back to 1'b0",
       static_cast<int>(init_node->type()),
       init_node->size(),
       node->name().c_str());
```

## Acceptance

#### Scenario: ch_reg<ch_bool>(true) init value 1 应用到 Verilator

- **WHEN** `axi_uart.h:75 ch_reg<ch_bool> tx_empty(ch_bool(true))` 经 codegen 生成
- **THEN** always_ff 块的 else 分支 (reset pulse) 含 `top_reg <= 1'b1`
- **AND** interpreter 端 `data_map_[tx_empty_id] = 1` 一致

#### Scenario: 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误

#### Scenario: ctest 全量不退化

- **WHEN** `ctest -LE perf --output-on-failure`
- **THEN** 156/156 PASS（同 baseline）

- **WHEN** `ctest -L verilator_e2e -LE perf`
- **THEN** 20/20 PASS（verilator_e2e 数不变）

#### Scenario: 不破反约束

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `include/axi4/peripherals/*.h`（保留 interpreter-only baseline）
- **AND** MUST NOT 改 `tests/*.cpp`（无测试代码变动）
- **AND** MUST NOT 改 `tests/CMakeLists.txt`
- **AND** MUST NOT 改 `src/core/verilator_backend.cpp`（verilator 端无需修改，init value 经 always_ff reset 分支自动应用）
- **AND** MUST NOT 改任何 spec 文件（no-spec-changes change）

#### Scenario: openspec validate 通过

- **WHEN** `openspec validate fix-codegen-ch-reg-no-next-hold-state --strict`
- **THEN** valid

## Capabilities

本 change 是 no-spec-changes，**不新增 capability requirement**。已存在的 `dual-backend-test-coverage/spec.md` R1-R13 + `verilator-reg-init/spec.md` 已被遵循：change 让 `ch_reg<T>(init)` 的 init value 在 VerilatorBackend 正确应用，对应 `verilator-reg-init/spec.md §R1`（reset 期间 init 应用）。

#### Scenario: 与现有 spec 对齐

- **WHEN** reviewer 阅读 `verilator-reg-init/spec.md` §R1
- **THEN** 本 change 直接服务该 R1（reset pulse 期间 init value 应用）
- **AND** `dual-backend-test-coverage/spec.md` R1（interpreter 双保险）+ R3（per-cycle REQUIRE parity）受益

#### Scenario: 防止 silent drift 复发

- **WHEN** `ch_reg<T>(init)` 未调用 `->next = ...` 的 code pattern 出现
- **THEN** codegen MUST emit hold-state always_ff 而非 warning comment
- **AND** 否则 dual backend parity 在 init value 不一致的 fixture 上 fail