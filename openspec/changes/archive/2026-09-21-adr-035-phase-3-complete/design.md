# Design: ADR-035 Phase 3 完整实现

## 1. 决策: Generated Per-Design Accessor (Phase 3.3 Port Binding)

### 1.1 候选方案对比

| 方案 | 编译开销 | 运行开销 | 可维护性 | SpinalHDL 对齐 |
|------|---------|---------|---------|--------------|
| **(b) Generated accessor** | sim_main.cpp 增加 ~200 行 inline function | 1 次 memcpy per port per cycle | 静态可查 | ✅ 完全对齐 |
| (a) VPI runtime lookup | 0 | 每次 VPI handle 解析（µs 级）| 动态难查 | ❌ 不对齐 |
| (c) 直接 reinterpret_cast Vtop struct | 0 | 0 | 极脆弱（依赖 Vtop field layout 不变）| ❌ 不可移植 |

**采纳 (b)**：SpinalHDL VerilatorBackend 模式 = generated accessor + ISignalAccess*[] 表。

### 1.2 sim_main.cpp emit 模式

当前 sim_main.cpp 模板（line 215-238）emit 4 个 extern "C" 符号：
```cpp
void* new_Vtop() { return new Vtop{ctx, "TOP"}; }
void eval_Vtop(void* top) { static_cast<Vtop*>(top)->eval(); }
void final_Vtop(void* top) { static_cast<Vtop*>(top)->final(); }
void delete_Vtop(void* top) { delete static_cast<Vtop*>(top); }
```

**扩展 emit 5 个符号**：
```cpp
// New Phase 3.3 symbols
void set_input_Vtop(void* top, uint32_t port_id, const void* bits) {
    auto* v = static_cast<Vtop*>(top);
    switch (port_id) {
        case 0: v->in_0 = *reinterpret_cast<const CData*>(bits); break;
        case 1: v->in_1 = *reinterpret_cast<const SData*>(bits); break;
        // ... generated per port
    }
}
void get_output_Vtop(void* top, uint32_t port_id, void* bits) {
    auto* v = static_cast<Vtop*>(top);
    switch (port_id) {
        case 0: *reinterpret_cast<CData*>(bits) = v->out_0; break;
        // ...
    }
}
uint8_t* get_field_ptr_Vtop(void* top, uint32_t port_id) {
    auto* v = static_cast<Vtop*>(top);
    switch (port_id) {
        case 0: return reinterpret_cast<uint8_t*>(&v->in_0);
        // ...
    }
}
```

**字段名到 Vtop 字段的映射**：从 `ctx_->get_eval_list()` 的 type_input/type_output 节点读取 `node->name()` → emit 时按 port_access_ 顺序匹配 Vtop 字段名（Verilator 用 snake_case：`default_clock`, `default_reset`, `io_*`）。

**emit 时机**：`VerilatorBackend::generate_verilog()` 调用 `ch::toVerilog(top.v, ctx)` **之后**追加 sim_main.cpp（因为 Verilator 编译后才生成 Vtop.h，需要从 Vtop.h 读字段名）。

### 1.3 字段 offset 解析

**关键问题**：Vtop.h 的字段在编译期确定，但 `verilator_backend.cpp` 不知道字段偏移。

**方案**：dlopen obj_dir/Vtop 后，在 `dlopen_top()` 调用后通过 `dlsym("get_field_ptr_Vtop")` 获取字段指针访问函数。`field_ptr` 不再是字面 offset，而是 `get_field_ptr_Vtop(port_id)` 返回的运行时计算指针。

```cpp
void VerilatorBackend::build_port_access_table() {
    port_access_.clear();
    clock_node_id_ = UINT32_MAX;
    if (!ctx_) return;

    // Resolve get_field_ptr_Vtop symbol
    using FieldPtrFn = uint8_t*(*)(void*);
    auto get_field_ptr = reinterpret_cast<FieldPtrFn>(
        dlsym(dl_handle_, "get_field_ptr_Vtop"));
    if (!get_field_ptr) {
        CHWARN("get_field_ptr_Vtop missing — port access table empty");
        return;
    }

    auto nodes = ctx_->get_eval_list();
    uint32_t port_id = 0;
    for (auto *node : nodes) {
        if (!node) continue;
        using ch::core::lnodetype;
        if (node->type() == lnodetype::type_clock) {
            clock_node_id_ = node->id();
            continue;
        }
        bool is_input = (node->type() == lnodetype::type_input);
        bool is_output = (node->type() == lnodetype::type_output);
        if (!is_input && !is_output) continue;

        VerilatorPortAccess pa;
        pa.field_ptr = get_field_ptr(top_instance_);  // runtime offset
        pa.bitwidth = node->size();
        pa.is_input = is_input;
        port_access_[node->id()] = pa;
        port_id++;
    }
}
```

**注意**：Vtop instance 必须先创建（`factory()` in `dlopen_top`），再调 `build_port_access_table`。

### 1.4 sync_inputs / sync_outputs 实装

```cpp
void VerilatorBackend::sync_inputs_to_vtop() {
    if (!data_map_ || !top_instance_ || !eval_fn_) return;
    using SetInputFn = void(*)(void*, uint32_t, const void*);
    auto set_input = reinterpret_cast<SetInputFn>(
        dlsym(dl_handle_, "set_input_Vtop"));
    if (!set_input) return;

    for (auto& [id, pa] : port_access_) {
        if (!pa.is_input) continue;
        uint64_t val = data_map_->at(id);
        set_input(top_instance_, id, &val);
    }
}

void VerilatorBackend::sync_outputs_from_vtop() {
    if (!data_map_ || !top_instance_ || !eval_fn_) return;
    using GetOutputFn = void(*)(void*, uint32_t, void*);
    auto get_output = reinterpret_cast<GetOutputFn>(
        dlsym(dl_handle_, "get_output_Vtop"));
    if (!get_output) return;

    for (auto& [id, pa] : port_access_) {
        if (pa.is_input) continue;
        uint64_t val = 0;
        get_output(top_instance_, id, &val);
        data_map_->at(id) = val;
    }
}
```

## 2. 决策: 3-Eval/Tick Clock Model (Phase 3.4)

ADR-009 仿真求值顺序: `eval_combinational → eval → eval_combinational` (3 次 eval = 1 cycle)。

```cpp
void VerilatorBackend::eval_sequential(
    ch::data_map_t & /*data_map*/,
    const std::vector<std::pair<uint32_t, ch::instr_base *>>
        & /*sequential_instr_list*/) {
    if (!top_instance_ || !eval_fn_) return;

    // Phase 3.4: simulate posedge via set_input(default_clock, 1) before eval
    if (clock_node_id_ != UINT32_MAX) {
        using SetInputFn = void(*)(void*, uint32_t, const void*);
        auto set_input = reinterpret_cast<SetInputFn>(
            dlsym(dl_handle_, "set_input_Vtop"));
        if (set_input) {
            // Find default_clock port id from port_access_
            auto it = port_access_.find(clock_node_id_);
            if (it != port_access_.end()) {
                uint8_t clk_high = 1;
                set_input(top_instance_, clock_node_id_, &clk_high);
                eval_fn_(top_instance_);
                uint8_t clk_low = 0;
                set_input(top_instance_, clock_node_id_, &clk_low);
            } else {
                eval_fn_(top_instance_);
            }
        } else {
            eval_fn_(top_instance_);
        }
    }
}
```

**Simulator 调用顺序**（由 Simulator::tick() 编排）：
```
tick():
    eval_combinational()    // sync_inputs + eval_fn_()
    eval_sequential()        // posedge eval_fn_ + clock_low eval_fn_
    eval_combinational()    // sync_outputs
```
1 cycle = 3 eval_fn_ calls。sequential 信号在第 2 个 eval 后翻转。

## 3. 决策: VCD Trace Per-Cycle Dump (Phase 3.6)

**Verilator `--trace` flag** 生成 `vl_trace_filename` 与 `VerilatedVcdC` API。

```cpp
// sim_main.cpp emit (when enable_vcd)
#include "verilated_vcd_c.h"
void init_trace_Vtop(void* top, const char* filename) {
    auto* v = static_cast<Vtop*>(top);
    auto* tfp = new VerilatedVcdC;
    v->trace(tfp, 99);
    tfp->open(filename);
    // Store tfp in global map for later flush
}
void dump_cycle_Vtop(void* top, uint64_t time) {
    auto* v = static_cast<Vtop*>(top);
    // ... lookup global tfp ...
    tfp->dump(time);
}
void close_trace_Vtop(void* top) {
    // ... lookup global tfp ...
    tfp->close();
    delete tfp;
}
```

**调用时机**：`VerilatorBackend::dump_vcd(time)` 在 `eval_sequential()` 后调用，output 已同步后 dump。

**限制**：Verilator VCD 仅 dump `vl_signal*` 注册的信号，不递归所有 internal signal。本 spec 仅要求 enable_vcd() + N cycle 后 .vcd 非空，**不**要求完整 signal coverage（避免范围蔓延）。

## 4. 风险与缓解

| 风险 | 等级 | 缓解 |
|------|------|------|
| Vtop.h 字段名变化（Verilator 版本升级） | 🟡 中 | sim_main.cpp emit 时用 `grep "VL_IN\|VL_OUT\|VL_SDATA\|VL_UREG"` 解析 Vtop.h 自动生成 case 语句，不硬编码字段名 |
| dlopen 同一 .so 多次 | 🟢 低 | `RTLD_LOCAL` 防止符号污染全局；`dl_handle_` 缓存复用 |
| 132 ctest 回归（verilator 后端开启影响非 verilator 测试） | 🟢 低 | `BUILD_VERILATOR=OFF` 是 CI 默认；新增 verilator 测试用 `[verilator]` tag 隔离 |
| SpinalHDL reference ISignalAccess 签名差异 | 🟢 低 | 本设计 emit 自定义 set_input/get_output，不复用 SpinalHDL 接口，避免兼容负担 |

## 5. 数据流图 (1 Cycle)

```
[t=0] data_map_ values
      ↓
      eval_combinational():
        sync_inputs_to_vtop()  → set_input_Vtop(port_id, data)
        eval_fn_()             → Vtop combinational logic
        sync_outputs_from_vtop() → get_output_Vtop(port_id) → data_map_
      ↓
      eval_sequential():
        set_input(default_clock, 1)  → simulate posedge
        eval_fn_()                  → Vtop sequential logic (clocked)
        set_input(default_clock, 0)  → falling edge
      ↓
      eval_combinational():  // re-evaluate with new sequential values
        eval_fn_()                  → Vtop combinational logic
      ↓
[t=1] data_map_ values (1 cycle elapsed)
```

## 6. 测试矩阵

| 测试 ID | 验证 |
|--------|------|
| `verilator_e2e_counter_simulator` | samples/counter.cpp 50 cycle → counter_value == 50 |
| `verilator_e2e_dlopen_real_symbols` | dlopen obj_dir/Vtop + dlsym 5 symbols (含 set/get) + factory() + eval() |
| `verilator_port_binding_rw` | data_map -> Vtop -> data_map 双向同步 |
| `verilator_clock_3_eval_model` | 3 evals/cycle, sequential 翻转 1 次 |
| `verilator_sha1_cache_hit_skips_compile` | 第二次 initialize < 100ms |
| `verilator_vcd_dump_writes_file` | enable_vcd() + 100 cycle → .vcd > 1KB |

## 7. 不采纳替代

| 替代方案 | 理由 |
|---------|------|
| 不 emit set_input/get_output，让 C++ 直接访问 Vtop struct | 依赖 Vtop field layout，与 ABI 强耦合，Verilator 升级会 break |
| 用 Verilator DPI (Direct Programming Interface) | 编译开销 > extern "C"，且需在 Verilog 侧写 DPI export task，增加 1 层 |
| Phase 3.3 用 VPI runtime | 每次 sync µs 级开销，10×~100× 慢于 memcpy |

## 8. 后续 (Out-of-Scope)

- ChipForge Phase 6d.5 E8 (openspec/changes/phase-6d-verilator-sim/) 实装 `tools/verilator_runner/main.cpp` 调用本 backend
- ChipForge Phase 6d.8 Harness 迁移（pb.run() → VerilatorBackend::eval）
- 未来 Verilator 5.020 → 6.x 升级时重新跑本 spec 132 ctest + 28 ported 验证
