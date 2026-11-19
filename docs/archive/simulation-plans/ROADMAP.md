# CppHDL 仿真性能优化实施路线图

> 基于实测性能数据，制定分阶段优化实施计划。

---

## 一、性能改进方案

### 1.1 总体目标

将 CppHDL 的仿真吞吐率提升 **10-50 倍**，达到接近 cash JIT 编译后端的性能水平。方案分为三个阶段：

- **Phase 1（速赢）**：消除主要热点，预计 3-5 倍提升
- **Phase 2（架构改进）**：引入编译优化，预计再 3-5 倍提升
- **Phase 3（高级优化）**：SIMD 和并行化，预计再 2-3 倍提升

### 1.2 Phase 1：消除主要热点（P0）

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
// ⚠️ 注意：此方案不可行。CppHDL 的 sdata_type 不支持 operator+
// 实际 eval 涉及位宽截断、符号扩展、溢出处理，无法简化为单行
```

**预期提升**：指令分派开销降低 10-20%（解释执行层面虚函数 vs 函数指针差距有限）。

#### P0-3: Trace 系统优化

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

### 1.3 Phase 2：架构改进（P1-P2）

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

### 1.4 Phase 3：高级优化（P3）

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

---

## 二、优先级总表

| 优先级 | 改进项 | 预计提升 | 实施难度 | 预估时间 | 状态 |
|--------|--------|----------|----------|----------|------|
| **P0** | **Trace 系统优化** | **2-4x** | 低 | 1-2 天 | 🆕 消除哈希+临时对象 |
| **P0** | 扁平数组替代 unordered_map | 1.5-2x | 低 | 1-2 天 | |
| **P0** | bitvector SSO 优化 | 1.2-2x | 中 | 2-3 天 | |
| **P1** | 函数指针替代虚函数 | 1.1-1.3x | 中 | 3-5 天 | ⚠️ 原 P0，降级；解释执行层面收益有限 |
| **P1** | DCE 死代码消除 | 1.1-1.3x | 中 | 2-3 天 | |
| **P1** | 常量折叠 | 1.1-1.2x | 中 | 1-2 天 | |
| **P1** | 代理链合并 | 1.1-1.2x | 中 | 1-2 天 | |
| **P2** | 批量 Tick 修复 | 1.2-1.5x | 中 | 2-3 天 | ⚠️ 待验证根因（可能是测量问题） |
| **P2** | CSE 公共子表达式消除 | 1.1-1.2x | 高 | 3-5 天 | |
| **P2** | JIT 编译后端 | 3-8x | 高 | **2-3 月** | ⚠️ 基于 Cash simjit.cpp ~3360 行 |
| **P2** | SIMD 向量化 | 1.5-4x（宽信号） | 高 | 1-2 周 | |
| **P3** | 缓存布局优化 | 1.05-1.15x | 中 | 1 周 | |
| **P3** | 寄存器分配 | 1.1-1.2x | 高 | 1-2 周 | |

**注**：函数指针优化在解释执行层面（虚函数表 vs 函数指针数组）收益有限（<20%），Cash 的高性能主要来自 JIT 编译而非函数指针。

---

## 三、综合预期效果

| 阶段 | 包含改进 | 预期加速比 | 说明 |
|------|----------|------------|------|
| **实测基线** | 当前 CppHDL | **1x** | TC-01: 265 ticks/sec (depth=1000) |
| Phase 1 完成 | P0: Trace + 扁平数组 + 函数指针 + SSO | **3-6x** | Trace 优化贡献最大 (82% → 20%) |
| Phase 2 完成 | + P1: DCE/CFO/PCX | **5-10x** | |
| Phase 2 + JIT | + P2: JIT 编译后端 | **20-40x** | 保守估计，因架构限制 |
| Phase 3 完成 | + P3: SIMD + 缓存优化 | **30-60x** | |

**注**：相比原估计（25-75x）大幅下调，因实测揭示了架构层面的根本限制（非简单优化可解决）。

**可达目标**：
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

### 4.3 优化验证清单

| 优化项 | 验证方法 | 通过标准 | 对应测试 |
|--------|---------|---------|----------|
| **P0: Trace 优化** | 运行 TC-04 | overhead 从 82% → **< 30%** | `perf_tests --tc=04` |
| P0-1: vector 替代 map | `valgrind --tool=cachegrind` | L1 cache miss 减少 >30% | TC-01, TC-02 |
| P0-2: 指令内联 | `objdump -d` 查看函数大小 | eval() 函数内联展开 | TC-01 |
| P0-3: 批量 tick 修复 | 运行 TC-06 | batch vs single **< 5%** | `perf_tests --tc=06` |
| P2: SIMD | `perf stat -e simd_instructions` | SIMD 利用率 >50% | TC-01 (宽信号) |

---

## 五、实施指南

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

## 六、相关文档

- [ARCHITECTURE.md](./ARCHITECTURE.md) - 仿真架构深入分析
- [PERFORMANCE_TESTS.md](./PERFORMANCE_TESTS.md) - 性能测试计划与基线数据

---

*文档版本: v1.1*
*创建日期: 2026-04-26*
*最后更新: 2026-04-26（Oracle 审查反馈：函数指针 P0→P1、方案 B 不可行、JIT 2-4周→2-3月、trace 4-6x→2-4x）*