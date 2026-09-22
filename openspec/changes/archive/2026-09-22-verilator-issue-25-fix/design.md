# Design: Issue #25 — VerilatorBackend eval/sync 路径修复

## 1. 决策: 3 步时钟翻转 + 同步路径分离

**问题**(基于 Oracle 审计 A1 + 本会话 AXI e2e 实证):
当前 `src/core/verilator_backend.cpp:696-711` 的 `eval_sequential()` 只调用 1 次 `eval_fn_()`,而 Verilator 5.x 需要完整时钟序列才能正确触发 `always_ff` 块。同时 `sync_inputs_to_vtop()` 每次都把 `data_map_` 中的时钟值覆盖到 Vtop 字段,破坏了 Verilator 的 `__Vclklast__` 边沿检测状态。

**修复方案**:重写 `eval_sequential()` 为 3 步直接 Vtop 字段操作序列。

## 2. 结构体变更

### 2a. `VerilatorPortAccess` 新增 2 个字段

```cpp
// include/core/verilator_backend.h:31-35 (MODIFIED)
struct VerilatorPortAccess {
    void *field_ptr;
    uint32_t bitwidth;
    bool is_input;
    bool is_clock;   // NEW: type_clock node
    bool is_reset;   // NEW: type_reset node
};
```

**为什么加入 port_access_ 而不是 continue 跳过**:保留完整端口视图供 `port_access_snapshot()` 测试使用;sync 函数通过 `is_clock`/`is_reset` 过滤。

### 2b. `VerilatorBackend` 类新增 2 个成员

```cpp
// include/core/verilator_backend.h private: 区域 (MODIFIED, 紧邻 clock_node_id_)
uint32_t clock_node_id_ = UINT32_MAX;
uint32_t reset_node_id_ = UINT32_MAX;
void* clock_field_ptr_ = nullptr;    // NEW: 直接指向 Vtop->default_clock (via get_field_ptr_fn_)
void* reset_field_ptr_ = nullptr;    // NEW: 直接指向 Vtop->default_reset (via get_field_ptr_fn_)
```

**类型选择**: `void*` 与 `VerilatorPortAccess::field_ptr` 一致。解引用时用 `static_cast<uint8_t*>` (clock/reset 均为 1-bit, CData = uint8_t)。

**获取方式**: 不硬编码 `&Vtop->default_clock`,而是通过 `get_field_ptr_fn_(top_instance_, clock_node_id_)` 获取 — 这与现有 `build_port_access_table()` 的通用路径一致,且 Verilator 生成的字段名确实是 `default_clock`(context.cpp:108 创建 clock 节点时命名为 `"default_clock"`,codegen 将其作为 Verilog 端口名,Verilator 生成同名 Vtop 字段)。

## 3. eval_sequential() 新实现

```cpp
// src/core/verilator_backend.cpp:696-711 (REPLACE)
void VerilatorBackend::eval_sequential() {
    // 3-step clock sequence per Verilator 5.x requirement:
    //   step 1: clk=0 → eval (stable state)
    //   step 2: clk=1 → eval (posedge triggers always_ff)
    //   step 3: clk=0 → eval (cleanup, prevent double-trigger)
    //
    // Direct Vtop field access via clock_field_ptr_ avoids round-trip
    // through data_map_ (which would defeat VlClockSig edge detection).

    // Sync user inputs (NOT clock) before sequence
    sync_inputs_to_vtop(exclude_clock=true);

    if (clock_field_ptr_) {
        // Step 1: clk=0 (stable)
        *static_cast<uint8_t*>(clock_field_ptr_) = 0;
        eval_fn_();

        // Step 2: clk=1 (posedge triggers always_ff)
        *static_cast<uint8_t*>(clock_field_ptr_) = 1;
        eval_fn_();

        // Step 3: clk=0 (cleanup)
        *static_cast<uint8_t*>(clock_field_ptr_) = 0;
        eval_fn_();
    } else {
        // No clock model (combinational only) — single eval
        eval_fn_();
    }

    // Sync outputs AFTER all 3 evals, capturing final wire values
    sync_outputs_from_vtop();
}
```

**关键点**:
- `clock_field_ptr_` 在 `build_port_access_table()` 时通过 `get_field_ptr_fn_()` 获取并保存,**不经过 data_map_**
- 守卫条件只检查 `clock_field_ptr_`(不要求 `reset_field_ptr_` 非空 — 很多设计有 clock 无 reset)
- `sync_inputs_to_vtop(exclude_clock=true)` 排除时钟/复位字段,只同步 user IO
- `sync_outputs_from_vtop()` 在 3 步 eval 完成后执行一次

## 4. sync_inputs_to_vtop() / sync_outputs_from_vtop() 修正

**原代码问题**: `src/core/verilator_backend.cpp:574-596` 的 `sync_inputs_to_vtop()` 遍历 `port_access_` 中所有 `is_input=true` 的条目(包括 clock/reset),把 `data_map_` 值写入 Vtop 字段。这覆盖了 Vtop 的时钟字段,破坏了 `__Vclklast__` 边沿检测。

**修复**:

```cpp
// 签名变更: 新增 exclude_clock 参数,默认 true
void VerilatorBackend::sync_inputs_to_vtop(bool exclude_clock = true) {
    for (auto& [node_id, entry] : port_access_) {
        if (!entry.is_input) continue;
        if (exclude_clock && (entry.is_clock || entry.is_reset)) continue;
        auto it = data_map_->find(node_id);
        if (it == data_map_->end()) continue;
        uint64_t val = static_cast<uint64_t>(it->second);
        set_input_fn_(top_instance_, node_id, &val);
    }
}

// sync_outputs_from_vtop(): 新增 clock/reset 过滤
void VerilatorBackend::sync_outputs_from_vtop() {
    for (auto& [node_id, entry] : port_access_) {
        if (entry.is_input) continue;           // 已有: 跳过输入
        if (entry.is_clock || entry.is_reset) continue;  // 新增: 跳过 clock/reset
        uint64_t val = 0;
        get_output_fn_(top_instance_, node_id, &val);
        (*data_map_)[node_id] = ch::core::sdata_type(val, entry.bitwidth);
    }
}
```

**签名兼容性**: `exclude_clock` 默认值为 `true`,不影响现有调用点:
- `eval_combinational()` (L689): `sync_inputs_to_vtop()` → 默认排除时钟 ✅
- `eval_sequential()` (新实现): `sync_inputs_to_vtop(exclude_clock=true)` → 显式排除 ✅

**注意**: `eval_combinational()` 排除时钟是正确的 — Simulator 的 `tick()` 流程中,`eval_combinational()` 在 `eval()` 之前调用,此时时钟值还未切换。时钟的实际切换由 `eval_sequential()` 的 3 步序列完成。

## 5. build_port_access_table() 修正

**原代码问题**: `src/core/verilator_backend.cpp:534-550` 对 `type_clock`/`type_reset` 节点只保存了 `clock_node_id_`/`reset_node_id_`(uint32_t),没有保存直接 Vtop 字段指针。`sync_inputs_to_vtop()` 把时钟/复位当普通输入同步,覆盖了 Vtop 内部时钟状态。

**修复**: 保留 clock/reset 在 `port_access_` 中(标记 `is_clock`/`is_reset`),同时保存直接指针:

```cpp
// In build_port_access_table(), after computing pa:
if (node->type() == lnodetype::type_clock) {
    clock_node_id_ = node->id();
    pa.is_clock = true;
    clock_field_ptr_ = get_field_ptr_fn_
        ? get_field_ptr_fn_(top_instance_, node->id())
        : nullptr;
} else if (node->type() == lnodetype::type_reset) {
    reset_node_id_ = node->id();
    pa.is_reset = true;
    reset_field_ptr_ = get_field_ptr_fn_
        ? get_field_ptr_fn_(top_instance_, node->id())
        : nullptr;
}
port_access_[node->id()] = pa;  // 保留在 port_access_ 中
```

**与 `clear()` 的一致性**: `clear()` 方法需要同时重置新指针:
```cpp
void VerilatorBackend::clear() {
    close_top();
    port_access_.clear();
    ctx_ = nullptr;
    data_map_ = nullptr;
    clock_field_ptr_ = nullptr;   // NEW
    reset_field_ptr_ = nullptr;   // NEW
}
```

## 6. 测试强化

| 测试 | 改动 |
|------|------|
| `test_verilator_e2e_harness.cpp:166` | `CHECK(actual == 50)` → `REQUIRE(actual == 50)` |
| `test_verilator_e2e_harness.cpp:206` | `CHECK(actual == 50 % 16)` → `REQUIRE(actual == 50 % 16)` |
| `test_axi_lite_verilator_e2e.cpp:240-249` | `INFO(...)` → `REQUIRE(awready_v == 1)` / `REQUIRE(wready_v == 1)` / `REQUIRE(bvalid_v == 1)` |
| `test_axi_lite_verilator_e2e.cpp:265` | (新增) `REQUIRE(rdata_val == 0xDEADBEEF)` |
| `test_verilator_three_way.cpp` | 12 cases 维持(无改动) |

**RED → GREEN 路径**:
1. 先强化测试(REQUIRE)→ RED(确认 issue #25 阻断)
2. 实施 3 步 eval_sequential + sync 修正 → GREEN
3. 回归测试 base 100% PASS

## 7. 不做的事

- **不** 重写整个 VerilatorBackend:只改 eval_sequential + sync 2 函数 + build_port_access_table 1 处标记逻辑 + header 2 个字段 + clear() 2 行
- **不** 改 Verilator 生成器:90+ 行 sim_main.cpp 不需要改
- **不** 引入新依赖

## 8. 风险与回退

- **风险**: 3 步时钟翻转使 eval 调用从 1 次增加到 3 次,TC-07 depth=1000 verilator 行性能可能从 34.54 ticks/sec 下降
- **缓解**: `AC6` 明确 baseline 34.54,不允许 regress;若 regression 触发,把 fix 限定在 sync_outputs 路径,不动 eval 序列
- **回退**: git revert 一个 commit 即可;修复集中在 eval_sequential 一个函数 + sync 过滤逻辑,影响面小