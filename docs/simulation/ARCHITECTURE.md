# CppHDL 仿真架构与优化方案

> 综合调研 CppHDL、Cash、SpinalHDL 三大项目仿真架构，制定优化路线图。

---

## 一、架构概览与横向对比

### 1.1 三项目仿真模式对照

| 特性 | CppHDL | Cash | SpinalHDL |
|------|--------|------|-----------|
| **语言** | C++20 | C++17 | Scala (JVM) |
| **仿真引擎** | 内置解释型模拟器 | JIT 编译模拟器 (LLVM/LIBJIT) | Verilator JNI + VPI 后端 |
| **调度模型** | 指令列表分类执行 | 拓扑排序 + JIT 编译函数 | 事件链表 + delta-cycle |
| **信号表示** | `bitvector<uint64_t>` + `unordered_map` | `bitvector<block_type>` + 扁平内存数组 | Verilator 原生 C++ 类型 / VPI 句柄 |
| **并行支持** | 仅 elaboration 线程安全 | 无 | 测试线程池 + CPU 亲和性 |
| **优化机制** | 无编译期优化 | DCE/CSE/CFO/PCX/BRO/RPO 七遍优化 | Verilator O3 + C++ O2 编译优化 |

### 1.2 核心架构差异

#### CppHDL：解释型指令模拟器

CppHDL 采用两阶段架构：elaboration 构建 IR 图，模拟器编译为指令列表后逐 tick 执行。关键特征：

- **调度**：拓扑排序后将指令分为 `combinational_instr_list_`、`sequential_instr_list_`、`input_instr_list_` 等，tick() 中按序遍历
- **信号存储**：`std::unordered_map<uint32_t, sdata_type>` 按节点 ID 索引
- **执行模型**：每个指令是 `instr_base` 子类，虚函数 `eval()` 执行单节点操作
- **时间管理**：离散 tick 计数器，无绝对时间概念

#### Cash：JIT 编译模拟器

cash 在 CppHDL 类似架构之上增加了 JIT 编译层：

- **编译期优化**：七遍 IR 优化（DCE、CSE、常量折叠、代理合并、分支合并、寄存器提升）
- **JIT 后端**：将优化后的 eval list 编译为原生机器码（LLVM 或 LIBJIT），生成单一 `entry(sim_state_t*)` 函数
- **标量优化**：≤64 位节点映射到原生寄存器，避免 bitvector 开销
- **bypass 优化**：单时钟域设计在时钟未翻转时可跳过部分评估
- **信号存储**：预分配扁平 `sim_state_t.vars` 字节数组，JIT 代码直接偏移访问

#### SpinalHDL：Verilator 编译仿真

SpinalHDL 采用完全不同的策略——将 RTL 编译为优化的 C++ 模型：

- **后端**：Verilator 将 Verilog 编译为 O3 优化的 C++，JNI 绑定实现进程内直接调用
- **信号访问**：C++ 指针数组 `ISignalAccess*[]` O(1) 索引，值存储在 Verilator 原生类型中
- **事件调度**：Scala 层维护排序链表 `SimCallSchedule`，delta-cycle 机制处理组合逻辑收敛
- **线程模型**：JVM 线程池 + CPU 亲和性绑核，减少上下文切换开销

### 1.3 性能瓶颈对比

| 瓶颈来源 | CppHDL | Cash | SpinalHDL |
|----------|--------|------|-----------|
| **指令调度** | 虚函数调用 + unordered_map 查找 | JIT 原生代码，无虚函数开销 | Verilator 优化 C++，无虚函数 |
| **信号访问** | unordered_map 哈希查找 + bitvector 堆分配 | 扁平数组偏移访问 / 寄存器 | C++ 指针数组直接访问 |
| **组合逻辑求值** | 每次 tick 全量遍历 | bypass 优化可跳过 | Verilator 增量优化 |
| **内存分配** | 每 tick 无分配，但 bitvector 内部可能堆分配 | 预分配，零运行时分配 | 预分配，零运行时分配 |
| **时钟门控** | 无 | 单时钟 bypass | 无（Verilator 内部优化） |

---

## 二、CppHDL 仿真执行架构详解

### 2.1 核心组件关系

```
┌──────────────────────────────────────────────────────────────────┐
│                         ch::Simulator                           │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────────────────┐  │
│  │  context*   │  │  data_map_  │  │     指令列表（5类）       │  │
│  │  节点图     │  │  哈希存储   │  │  input / comb / seq      │  │
│  └─────────────┘  └─────────────┘  └──────────────────────────┘  │
│         │                │                    │                 │
│    拓扑排序          指针关联              eval() 执行          │
│  get_eval_list()   data_map_[id]    虚函数调用循环             │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 仿真生命周期

```
1. Simulator(ctx, trace_on)
   ├── ctx_swap 设置 thread_local ctx_curr_
   ├── load_trace_config_from_file()
   └── initialize()
       ├── ctx_->get_eval_list()       // 拓扑排序
       ├── data_map_.reserve(nodes)   // 分配缓冲区
       ├── node->create_instruction()  // 创建指令
       └── 分类: input/comb/seq/clock/reset 列表

2. tick() 循环 (per cycle)
   ├── eval_combinational()
   │   ├── input_instr_list_ → eval()  // 驱动输入
   │   └── combinational_instr_list_ → eval() // 组合逻辑传播
   │
   └── eval()  [if default_clock]
       ├── default_clock_instr_->eval() // 翻转时钟
       ├── other_clock_instr_ → eval()
       ├── if clk==0 → eval_combinational()  // 低电平组合
       └── if clk==1 → eval_sequential()   // 高电平时序

3. trace() (可选 per tick)
   ├── 遍历 signals_
   ├── 比较 data_map_[id] vs prev_values_
   └── memcpy 变化值到 TraceBlock
```

### 2.3 关键数据结构

#### data_map_t（性能瓶颈之一）

```cpp
struct data_map_t : public std::unordered_map<uint32_t, ch::core::sdata_type> {
    using std::unordered_map<uint32_t, ch::core::sdata_type>::unordered_map;
};
```

**问题**：
- 哈希查找 O(1) 但 Cache miss 高
- 节点 ID 不连续时哈希效率下降
- 无法利用数据局部性

#### 指令列表结构

```cpp
std::vector<std::pair<uint32_t, ch::instr_base *>> sequential_instr_list_;
std::vector<std::pair<uint32_t, ch::instr_base *>> combinational_instr_list_;
```

**问题**：
- `pair<uint32_t, instr_base*>` 的大小 = 16 字节（64位）
- 遍历时缓存不友好
- `uint32_t` 未使用（已有 instr_base*）

### 2.4 eval() 虚函数调用开销

```cpp
class instr_base {
    virtual void eval() = 0;  // 每节点每次 tick 虚调用
};

instr_reg::eval() {
    if (rst_buf_ && !rst_buf_->is_zero()) { ... }
    if (clk_en_buf_ && clk_en_buf_->is_zero()) { return; }
    if (clk_edge_->is_zero()) { return; }
    if (next_buf_) { current_buf_->assign_truncate(*next_buf_); }
}
```

**问题**：
- 间接跳转（vtable 查找）
- 编译器无法内联
- 分支预测失败率增加

### 2.5 CHDBG 日志开销（最大瓶颈）

```
src/simulator.cpp:     141 处 CHDBG/CHINFO/CHERROR/CHWARN 调用
src/ast/instr_*.cpp:    多处调用
src/core/context.cpp:   多处调用
```

即使在 Release 构建，`CHDBG_FUNC()` 等宏仍然存在：
- 函数名字符串化
- 参数格式化（若有）
- `std::cout` 输出流操作

### 2.6 跟踪系统开销

```cpp
void Simulator::trace() {
    for (uint32_t i = 0, n = signals_.size(); i < n; ++i) {
        auto node_id = signals_[i].first;
        auto &curr_value = data_map_.find(node_id)->second; // 又一次哈希
        auto &prev = prev_values_.at(i);

        if (prev.first) {
            // memcmp 比较
            if (curr_value == temp_data) continue;  // 无变化跳过
        }
        // memcpy 复制数据
        std::memcpy(dst_block + dst_offset, curr_value.bitvector().data(), ...);
    }
}
```

**问题**：
- 每次 tick 都遍历所有信号
- 哈希查找（data_map_.find）
- memcmp + memcpy 对变化信号

---

## 二.x 实测性能分析（2026-04-25）

> 数据来源：`perf_results.csv`，运行 `perf_tests --all`

### x.1 基准测试结果

| 测试 | 参数 | 实测值 | 文档目标 | 达标 |
|------|------|--------|----------|------|
| TC-01 | depth=10 | **29,840** ticks/sec | > 100K | ❌ |
| TC-01 | depth=100 | **3,120** ticks/sec | > 100K | ❌ |
| TC-01 | depth=1000 | **265** ticks/sec | > 100K | ❌ |
| TC-01 | depth=10000 | **20** ticks/sec | > 100K | ❌ |
| TC-02 | regs=10 | **121,874** ticks/sec | > 50K | ✅ |
| TC-02 | regs=1000 | **115,809** ticks/sec | > 50K | ✅ |
| TC-02 | regs=10000 | **114,569** ticks/sec | > 50K | ✅ |
| TC-04 | 100 signals | **82.88%** overhead | < 10% | ❌ |
| TC-06 | batch vs single | **-27.35%** (batch 更慢) | <= 5% | ❌ |

### x.2 关键发现

#### 发现 1：TC-06 batch tick 比单步更慢 — 架构问题

实测显示批量 `tick(N)` 比循环单步 `tick()` **慢 27%**，这完全违背了批量优化的初衷。

**可能根因**：

1. **循环开销反转**：当前 `tick(N)` 实现可能每次都重新初始化/清理状态，而非真正复用
2. **指令重建**：`sim.tick(N)` 内部每次都调用 `eval_combinational()` 和 `eval_sequential()`，与单步完全相同
3. **状态保存开销**：为支持 `set_input()` 中断恢复，每次 tick 可能保存完整状态

```cpp
// 当前实现（推测）的潜在问题：
void tick(uint64_t n = 1) {
    for (uint64_t i = 0; i < n; ++i) {
        // 每次循环都做完全相同的工作，没有任何缓存/复用
        eval_combinational();
        eval_sequential();
        if (trace_enabled_) trace();
    }
}
```

**行动项**：诊断 `Simulator::tick(N)` 实现，确认是否存在状态冗余操作。

#### 发现 2：TC-01 组合链性能随深度非线性崩溃

| depth | ticks/sec | ns/tick | ns/node/tick | 相对衰退 |
|-------|-----------|---------|---------------|----------|
| 10 | 29,840 | 33,513 | 3,351 | 1x |
| 100 | 3,120 | 320,546 | 3,205 | ~10x |
| 1000 | 265 | 3,779,300 | 3,779 | ~113x |
| 10000 | 20 | 50,703,200 | 5,070 | ~1500x |

**观察**：`ns/node/tick` 从 3,351 (depth=10) 下降到 5,070 (depth=10000)，说明单节点 eval 本身是常数复杂度。但 **总时间** 随深度超线性增长（10x → 113x → 1500x 而非 10x → 100x → 1000x）。

**可能根因**：

1. **拓扑排序退化**：depth=10000 时指令列表遍历存在 Cache thrashing
2. **指令调度开销放大**：每次 tick 重新 dispatch 大量指令，指令缓存命中率下降
3. **组合逻辑收敛迭代**：链式 `a = b ^ i` 结构在 eval 时可能存在伪依赖，导致多次迭代

**初步结论**：组合逻辑链 eval 本身是 O(n)，但 n=10000 时常数因子急剧增大，疑似 Cache/分支预测失效。

#### 发现 3：TC-02 寄存器 eval 异常高效

| regs | ticks/sec | ns/tick | ns/reg/tick |
|-------|-----------|---------|-------------|
| 10 | 121,874 | 8,205 | 820.5 ns |
| 100 | 107,796 | 9,277 | 92.8 ns |
| 1000 | 115,809 | 8,635 | 8.6 ns |
| 10000 | 114,569 | 8,728 | 0.87 ns |

**观察**：`ns/reg/tick` 随寄存器数增加反而**下降**（820.5ns → 0.87ns），这说明寄存器 eval 的开销主要在于**固定调度开销**（函数调用、循环），而非每增加一个寄存器线性增长。

**结论**：寄存器 eval 本身非常高效（约 0.87ns/reg），主要瓶颈在调度框架开销。

#### 发现 4：TC-04 trace 开销是最大性能杀手

82.88% 的 trace overhead 意味着开启 trace 后性能下降近 6 倍。

**根因分析**：
- 每次 tick 遍历所有 `signals_`
- 每次遍历都有 `data_map_.find(node_id)` 哈希查找
- `memcmp` + `memcpy` 对每个信号

**行动项**：这应该被列为 **P0 优先级**，而非原来的 P1/P2。

### x.3 基于实测的优先级调整

| 原优先级 | 优化项 | 新优先级 | 理由 |
|----------|--------|----------|------|
| P0-3 | 时钟门控 bypass | **降为 P2** | batch tick 本身已慢，先解决架构问题 |
| P0-2 | 函数指针替代虚函数 | **升为 P0** | 寄存器 eval 高效说明虚函数开销可控，但组合链需要 |
| **新增** | **trace 哈希优化** | **P0** | 82.88% 开销，必须立即优化 |
| P1-1 | DCE 死代码消除 | **维持 P1** | 预期 1.1-1.3x |

---

## 三、CppHDL 性能改进方案

### 3.1 总体目标

将 CppHDL 的仿真吞吐率提升 **10-50 倍**，达到接近 cash JIT 编译后端的性能水平。方案分为三个阶段：

- **Phase 1（速赢）**：消除主要热点，预计 3-5 倍提升
- **Phase 2（架构改进）**：引入编译优化，预计再 3-5 倍提升
- **Phase 3（高级优化）**：SIMD 和并行化，预计再 2-3 倍提升

### 3.2 Phase 1：消除主要热点（P0）

#### P0-1: 扁平数组替代 unordered_map

**现状**：`data_map_` 使用 `std::unordered_map<uint32_t, sdata_type>`，每次信号访问都要哈希查找。

**改进**：改用扁平数组，以节点 ID 为直接索引。

```cpp
// 当前：
// std::unordered_map<uint32_t, sdata_type> data_map_;
// auto& val = data_map_[node_id];  // O(1) 平均但哈希开销大

// 改进：
std::vector<sdata_type> data_array_;  // 预分配，直接索引
// auto& val = data_array_[node_id];  // O(1) 直接指针算术
```

在 `Simulator::initialize()` 中：
```cpp
data_array_.resize(node_storage_.size());
for (auto& [id, instr] : instr_map_) {
    instr->set_data_ptr(&data_array_[id]);
}
```

**预期提升**：信号访问延迟降低 50-70%（~10ns/lookup → ~1ns）。

#### P0-2: 函数指针替代虚函数

**现状**：每条指令通过 `instr_base::eval()` 虚函数调用执行。

**改进**：使用类型特化的函数指针或模板展开。

方案 A - 函数指针数组：
```cpp
// 替代虚函数调用
using eval_fn_t = void(*)(void* ctx);
std::vector<eval_fn_t> combinational_eval_fns_;
std::vector<void*> combinational_contexts_;

// 在 tick 中：
for (size_t i = 0; i < combinational_eval_fns_.size(); ++i) {
    combinational_eval_fns_[i](combinational_contexts_[i]);
}
```

方案 B - 内联指令执行（针对常见操作）：
```cpp
// 将 instr_op_binary<OpAdd>::eval() 内联到 eval_combinational()
// 使用 switch 分派而非虚函数
void Simulator::eval_op_inline(uint32_t dst, uint32_t src0, uint32_t src1, op_type op) {
    switch (op) {
        case OP_ADD: data_array_[dst] = data_array_[src0] + data_array_[src1]; break;
        case OP_AND: data_array_[dst] = data_array_[src0] & data_array_[src1]; break;
        // ...
    }
}
```

**预期提升**：指令分派开销降低 30-50%。

#### P0-3: Trace 系统优化（优先级提升）

> **⚠️ 实测发现**：TC-04 显示 100 信号时 trace overhead 高达 **82.88%**，是当前最大性能杀手。

**现状**：每次 tick 都遍历所有 `signals_`，每次都做 `data_map_.find(node_id)` 哈希查找。

**改进**：

```cpp
// 问题代码：
void trace() {
    for (auto& sig : signals_) {
        auto& curr = data_map_.find(sig.first)->second; // 哈希查找
        // ...
    }
}

// 优化方案 A：预计算信号索引
void trace_optimized() {
    // 在 initialize() 时建立 signal_id → data_array_ 索引映射
    for (size_t i = 0; i < signals_.size(); ++i) {
        auto node_id = signals_[i].first;
        signal_data_ptrs_[i] = &data_array_[node_id]; // O(1) 直接访问
    }
}

// 优化方案 B：变化检测优化
void trace_optimized() {
    for (size_t i = 0; i < signals_.size(); ++i) {
        auto* curr = signal_data_ptrs_[i];
        auto* prev = prev_values_[i];
        // 跳过未变化信号
        if (!value_changed(curr, prev)) continue;
        // 只拷贝变化信号
        memcpy(dst + offset, curr, width);
    }
}
```

**预期提升**：trace overhead 从 82.88% 降至 < 20%（约 4-5x 提升）。

#### P0-4: 批量 Tick 架构修复

> **⚠️ 实测发现**：TC-06 显示 `tick(N)` 比循环单步 `tick()` **慢 27%**，说明当前批量实现存在架构问题。

**现状**：当前 `tick(N)` 实现可能每次循环都重新执行相同的 eval 流程，无任何状态复用。

**根因分析**：
- `tick(N)` 内部循环与单步 `tick()` 完全相同
- 可能存在每次 tick 的状态保存/恢复开销
- 缺乏批量执行的优化空间

**改进思路**：

```cpp
// 优化方案：真正的批量 eval
void tick(uint64_t n = 1) {
    // 批量模式下跳过 trace 中间状态（只在最后记录）
    bool was_tracing = trace_enabled_;
    if (was_tracing && n > 1) {
        trace_enabled_ = false;  // 批量期间关闭 trace
    }

    for (uint64_t i = 0; i < n; ++i) {
        eval_combinational();
        eval_sequential();
    }

    if (was_tracing) {
        trace_enabled_ = true;
        trace();  // 只在最后 trace 一次
    }
}

// 或者：支持批量 trace
void trace_batch(uint64_t tick_start, uint64_t tick_end) {
    // 只记录变化时刻，而非每 tick 都记录
}
```

**注意**：此优化需先确认 `tick(N)` 的当前实现。**优先级降为 P2**，先解决 trace 和组合链问题。

#### P0-5: bitvector SSO 优化

**现状**：所有 `sdata_type` 都使用 `bitvector<uint64_t>`，即使 1 位信号也堆分配。

**改进**：实现 Small Signal Optimization (SSO)。

```cpp
class sdata_type {
    static constexpr size_t SSO_WORDS = 2;  // 128 bits inline

    union {
        uint64_t inline_data[SSO_WORDS];    // 小宽度直接使用
        uint64_t* heap_data;                 // 大宽度堆分配
    };
    uint32_t size_;
    bool is_inline_;

public:
    // 对于 <= 128 位的信号，无需堆分配
};
```

**预期提升**：小信号（<=64 位，占 90%+）的内存分配和访问降低 80%。

### 3.3 Phase 2：架构改进（P1-P2）

#### P1-1: 死代码消除 (DCE)

**实现思路**：从 `type_output` 和 `type_tap` 节点开始 BFS，标记所有可达节点，移除未标记节点。

```cpp
void eliminate_dead_code(context& ctx) {
    std::unordered_set<uint32_t> live_nodes;
    std::queue<uint32_t> worklist;

    // 从输出端口开始
    for (auto& node : ctx.get_output_nodes()) {
        worklist.push(node->id());
    }

    // 反向 BFS 标记活跃节点
    while (!worklist.empty()) {
        auto id = worklist.front();
        worklist.pop();
        if (live_nodes.insert(id).second) {
            for (auto src : ctx.get_node_sources(id)) {
                worklist.push(src);
            }
        }
    }

    // 移除非活跃节点
    ctx.remove_nodes_except(live_nodes);
}
```

**预期提升**：典型设计减少 10-20% 的指令数量。

#### P1-2: 常量折叠 (CFO)

**实现思路**：检测所有操作数均为 `type_lit` 的操作节点，编译期计算结果。

```cpp
void constant_fold(context& ctx) {
    for (auto& node : ctx.nodes()) {
        if (node->op_type() == OP_ADD &&
            node->src0()->is_literal() &&
            node->src1()->is_literal()) {
            auto result = node->src0()->literal_value() + node->src1()->literal_value();
            ctx.replace_node(node, ctx.create_literal(result, node->bit_width()));
        }
    }
}
```

**预期提升**：1.1-1.2x。

#### P1-3: 代理链合并 (PCX)

**实现思路**：检测连续的 proxy chain，合并为单一 proxy。

```cpp
void coalesce_proxies(context& ctx) {
    for (auto& node : ctx.nodes()) {
        if (node->type() == type_proxy) {
            auto src = node->src_node();
            if (src->type() == type_proxy) {
                // proxy(proxy(source, slice), concat) → 直接计算最终偏移
                auto final_offset = compute_cumulative_offset(src, node->offset());
                auto final_width = node->width();
                ctx.replace_proxy_chain(node, src->src_node(), final_offset, final_width);
            }
        }
    }
}
```

**预期提升**：1.1-1.2x。

#### P2-1: JIT 编译后端（可选）

**实现思路**：集成 asmjit，将优化后的指令列表编译为原生机器码。

```cpp
#include <asmjit/asmjit.h>

class JitCompiler {
public:
    using jit_fn_t = void(*)(uint64_t* data, uint64_t ticks);

    jit_fn_t compile(const std::vector<instr_base*>& instr_list) {
        asmjit::CodeHolder code;
        code.init(asmjit::Runtime::getCpuInstance()->environment());

        asmjit::x86::Compiler compiler(&code);
        compiler.addFunc(asmjit::FuncSignatureT<void, uint64_t*, uint64_t>(asmjit::CallConv::kIdHost));

        // 生成评估循环
        for (auto instr : instr_list) {
            emit_instruction(compiler, instr);
        }

        compiler.endFunc();
        compiler.finalize();

        return code.makeJitFunction<jit_fn_t>();
    }
};
```

**预期提升**：相比解释执行 5-10x（相比基线 30-100x）。

### 3.4 Phase 3：高级优化（P3-P4）

#### P3-1: SIMD 向量化

```cpp
#include <immintrin.h>

void bv_add_avx2(uint64_t* dst, const uint64_t* a, const uint64_t* b, size_t words) {
    size_t i = 0;
    for (; i + 3 < words; i += 4) {
        __m256i va = _mm256_loadu_si256((__m256i*)&a[i]);
        __m256i vb = _mm256_loadu_si256((__m256i*)&b[i]);
        __m256i vr = _mm256_add_epi64(va, vb);
        _mm256_storeu_si256((__m256i*)&dst[i], vr);
    }
    for (; i < words; ++i) dst[i] = a[i] + b[i];
}
```

**预期提升**：宽信号操作 4x。

#### P3-2: 批量评估列表

```cpp
struct batch_op {
    op_type op;
    std::vector<uint32_t> dst, src0, src1;

    void eval(std::vector<sdata_type>& data) {
        for (size_t i = 0; i < dst.size(); ++i) {
            data[dst[i]] = apply(data[src0[i]], data[src1[i]], op);
        }
    }
};
```

**预期提升**：减少循环开销和指令分派，5-15%。

### 3.5 实施优先级总表（基于实测调整）

| 优先级 | 改进项 | 预计提升 | 实施难度 | 预估时间 | 状态 |
|--------|--------|----------|----------|----------|------|
| **P0** | **Trace 系统优化** | **4-6x** | 低 | 1-2 天 | 🆕 新增，实测 82% 开销 |
| **P0** | 扁平数组替代 unordered_map | 1.5-2x | 低 | 1-2 天 | |
| **P0** | 函数指针替代虚函数 | 1.3-1.5x | 低 | 1 天 | |
| **P0** | bitvector SSO 优化 | 1.2-1.5x | 中 | 2-3 天 | |
| **P1** | DCE 死代码消除 | 1.1-1.3x | 中 | 2-3 天 | |
| **P1** | 常量折叠 | 1.1-1.2x | 中 | 1-2 天 | |
| **P1** | 代理链合并 | 1.1-1.2x | 中 | 1-2 天 | |
| **P2** | 批量 Tick 架构修复 | 1.2-1.5x | 中 | 2-3 天 | 🆕 原 P0-3，现降级 |
| **P2** | CSE 公共子表达式消除 | 1.1-1.2x | 高 | 3-5 天 | |
| **P2** | JIT 编译后端 | 5-10x | 高 | 2-4 周 | |
| **P2** | SIMD 向量化 | 1.5-4x（宽信号） | 高 | 1-2 周 | |
| **P3** | 缓存布局优化 | 1.05-1.15x | 中 | 1 周 | |
| **P3** | 寄存器分配 | 1.1-1.2x | 高 | 1-2 周 | |

### 3.6 综合预期效果（基于实测校正）

| 阶段 | 包含改进 | 预期加速比 | 说明 |
|------|----------|------------|------|
| **实测基线** | 当前 CppHDL | **1x** | TC-01: 265 ticks/sec (depth=1000) |
| Phase 1 完成 | P0: Trace + 扁平数组 + 函数指针 + SSO | **3-6x** | Trace 优化贡献最大 (82% → 20%) |
| Phase 2 完成 | + P1: DCE/CFO/PCX | **5-10x** | |
| Phase 2 + JIT | + P2: JIT 编译后端 | **20-40x** | 保守估计，因架构限制 |
| Phase 3 完成 | + P3: SIMD + 缓存优化 | **30-60x** | |

**注**：相比原估计（25-75x）大幅下调，因实测揭示了架构层面的根本限制（非简单优化可解决）。

**可达目标**（5年）：
- 短期（Trace + 扁平数组）：TC-01 从 265 → **800-1500 ticks/sec** (3-6x)
- 中期（+ 函数指针 + JIT）：TC-01 达到 **4000-10000 ticks/sec**
- 长期（+ SIMD）：接近 **20000 ticks/sec**

---

## 四、性能测试计划

### 4.1 测试用例（实测校正目标）

| 测试 | 变量 | 目的 | 原目标 | **实测基线** | 校正后目标 |
|------|------|------|--------|-------------|------------|
| TC-01 | depth=1000 | 组合逻辑传播开销 | > 100K ticks/sec | **265 ticks/sec** | **800-1500** (Phase 1) |
| TC-02 | regs=1000 | 时序逻辑更新开销 | > 50K ticks/sec | **115,809 ticks/sec** | ✅ 已达标 |
| TC-03 | 内存深度×宽度 | 内存读写性能 | > 30K ticks/sec | **未测量** | 待测 |
| TC-04 | 100 signals | trace 功能开销 | < 10% | **82.88% overhead** | **< 30%** (Phase 1) |
| TC-05 | 总节点数 100~10000 | 整体性能曲线 | 可预测非线性下降 | 待测 | — |
| TC-06 | `tick(N)` vs 循环 | 批量执行优化 | <= 5% 差异 | **-27.35%** (batch 更慢) | **修复后 < 5%** |

### 4.2 性能回归检测

```
优化前基线 vs 优化后：差异 > 10% → CI 失败
```

### 4.3 优化验证清单（基于实测更新）

| 优化项 | 验证方法 | 通过标准 | 对应测试 |
|--------|---------|---------|----------|
| **P0: Trace 优化** | 运行 TC-04 | overhead 从 82% → **< 30%** | `perf_tests --tc=04` |
| P0-1: vector 替代 map | `valgrind --tool=cachegrind` | L1 cache miss 减少 >30% | TC-01, TC-02 |
| P0-2: 指令内联 | `objdump -d` 查看函数大小 | eval() 函数内联展开 | TC-01 |
| P0-3: 批量 tick 修复 | 运行 TC-06 | batch vs single **< 5%** | `perf_tests --tc=06` |
| P2: SIMD | `perf stat -e simd_instructions` | SIMD 利用率 >50% | TC-01 (宽信号) |

---

## 五、三大项目详细对比

### 5.1 CppHDL（解释执行型）

**核心模式**：纯虚函数调用循环，每次 tick 遍历所有节点

**关键瓶颈**：
1. 每 tick 3 次组合逻辑遍历
2. `unordered_map` 哈希查找
3. 跟踪系统临时对象分配
4. 位宽操作逐位循环
5. 无内联/无并行

### 5.2 Cash（解释+JIT 双模式）

**关键设计**：

| 特性 | 实现 |
|------|------|
| **JIT 编译** | simjit 生成机器码，标量存 JIT 寄存器 |
| **标量优化** | ≤64bit 操作直接生成原生指令 |
| **内存布局** | sim_state_t 连续存储，Cache 友好 |
| **常量折叠** | CFO pass 编译时求值常量表达式 |

**性能声称**：simjit vs simref 加速 **30-100 倍**

**可借鉴点**：JIT 标量寄存器优化、CRTP 完全内联、多 pass 优化

### 5.3 SpinalHDL（多后端+协作多线程）

**关键设计**：

| 特性 | 实现 | 源码位置 [1] |
|------|------|-------------|
| **后端抽象** | SimRaw 接口统一 get/set/eval/sleep | `sim/src/main/scala/spinal/sim/SimManager.scala` |
| **Verilator JNI** | 直接指针访问 signalAccess[]，零拷贝 | `core/src/main/scala/spinal/core/sim/` |
| **编译缓存** | SHA-1 缓存 .so，避免重复编译 | `sim/src/main/scala/spinal/sim/CompiledApi.scala` |
| **CPU 亲和** | JVM Thread 绑定 CPU 核心 | `SimManager.scala` |

**可借鉴点**：编译缓存机制、Read Bypass、命令缓冲批处理

### 5.4 三项目横向对比总结

| 维度 | CppHDL | Cash [2] | SpinalHDL [1] |
|------|--------|----------|---------------|
| **执行模式** | 解释执行 | 解释 + JIT | 解释执行（外部仿真器） |
| **数据存储** | `unordered_map` | 连续内存 + 寄存器 | Signal 对象 + 共享内存 |
| **时钟模型** | 全局时钟翻转 | clock_driver 批量翻转 | 协作多线程 + delta cycle |
| **拓扑排序** | 初始化时 DFS | 编译时 merged_context | 编译时 |
| **跟踪机制** | TraceBlock 链 + memcpy | 编译时注入 | 厂商 VCD/FSDB |
| **优化手段** | 指令分类 | JIT + 多 pass 优化 | 编译缓存 + CPU 亲和 |
| **并行能力** | 无 | simref 无 / simjit 可选 | 多线程 + CPU 亲和 |

**源码引用**：
- [1] SpinalHDL: `/workspace/project/SpinalHDL/sim/src/main/scala/spinal/sim/`, `/workspace/project/SpinalHDL/core/src/main/scala/spinal/core/sim/`
- [2] Cash: `/workspace/project/cash/src/compiler/simjit.cpp`, `/workspace/project/cash/src/sim/simulatorimpl.cpp`

---

## 六、实施指南

### 第一步：建立基准测试

```bash
# 构建性能测试
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target perf_tests

# 运行单个测试
./build/tests/performance/perf_tests --tc=01

# 运行全部测试
ctest -R perf --output-on-failure
```

### 第二步：性能剖析

```bash
# Linux
perf record -g ./build/tests/performance/perf_tests
perf report
```

### 第三步：逐项实施并验证

每项改进实施后运行基准测试，确认性能提升。

---

*文档版本: v4.0*
*创建日期: 2026-04-25*
*最后更新: 2026-04-26（基于实测数据更新：新增 x.1-x.3 节，修正 P0 优先级，新增 trace 优化为 P0）*