# CppHDL 仿真架构分析

> 综合调研 CppHDL、Cash、SpinalHDL 三大项目仿真架构，深入分析当前实现瓶颈。

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

cash 在 CppHDL 类似架构之上增加了 JIT 编译层（使用 **LIBJIT/GNU Lightning**，非 LLVM）：

- **编译期优化**：七遍 IR 优化（DCE、CSE、常量折叠、代理合并、分支合并、寄存器提升）
- **merged context**：通过 `create_merged_context()` 将多模块 flatten 为单一 eval context，这是 Cash 能高效编译的关键
- **JIT 后端**：将优化后的 eval list 编译为原生机器码（**LIBJIT**），生成单一 `entry(sim_state_t*)` 函数
- **标量优化**：≤64 位节点直接映射到 x86 原生寄存器，生成 `add/and/or/xor` 等指令；>64 位才调用库函数
- **bypass 优化**：单时钟域设计在时钟未翻转时可跳过 sequential 节点评估
- **信号存储**：预分配扁平 `sim_state_t.vars` 字节数组，JIT 代码使用**编译期偏移量**直接访问

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

2. tick() 循环 (per cycle) - **注意：实际调用 eval() 两次**
   ├── eval_combinational()
   │   ├── input_instr_list_ → eval()  // 驱动输入
   │   └── combinational_instr_list_ → eval() // 组合逻辑传播
   │
   └── eval()  [if default_clock] — **调用两次**
       ├── 第一次 eval()：时钟翻转到 1 → eval_sequential()  时序锁存
       ├── 第二次 eval()：时钟翻转到 0 → eval_combinational() 组合逻辑收敛
       └── **实际流程**：tick() → eval() → eval() → trace()（per tick）

   **伪代码**（src/simulator.cpp）：
   ```cpp
   void tick() {
       eval_combinational();      // 1. 先执行组合逻辑
       if (default_clock_instr_) {
           eval();                 // 2. 第一次：时钟→1，时序评估
           eval();                 // 3. 第二次：时钟→0，组合评估
       }
       if (trace_on_) trace();    // 4. 每 tick 都 trace（可选）
   }
   ```

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

### 2.5 CHDBG 日志开销

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
            // ⚠️ 关键问题：每次变化信号都构造临时对象
            auto temp_data = ch::core::sdata_type(curr_value.bitwidth()); // 堆分配！
            std::memcpy(temp_data.bitvector().data(), prev.first, ...);
            if (curr_value == temp_data) continue;  // 无变化跳过
        }
        // memcpy 复制数据
        std::memcpy(dst_block + dst_offset, curr_value.bitvector().data(), ...);
    }
}
```

**问题**：
- 每次 tick 都遍历所有信号
- 哈希查找（data_map_.find）→ O(1) 但 Cache miss 高
- **临时对象构造**：每个变化信号都 `new sdata_type(bitwidth)` → **堆分配**
- memcmp + memcpy 对变化信号

**优化方向**：直接比较原始字节，避免临时对象构造：
```cpp
// 直接比较内存，避免 sdata_type 构造/析构
if (std::memcmp(curr_value.bitvector().data(), prev.first, byte_len) == 0) continue;
```

---

## 三、实测性能分析

> 数据来源：`perf_results.csv`，运行 `perf_tests --all`

### 3.1 基准测试结果

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

### 3.2 关键发现

#### 发现 1：TC-06 batch tick 比单步更慢 — 疑似测量问题

实测显示批量 `tick(N)` 比循环单步 `tick()` **慢 27%**。

**⚠️ 根因分析（待验证）**：从 `src/simulator.cpp:831-845` 代码看，`tick(N)` 实际就是简单循环调用 `tick()`：
```cpp
void Simulator::tick(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        tick();  // 就是简单循环！
    }
}
```

与手动单步循环的代码完全相同，理论上批量应略快（减少循环控制开销）。

**可能根因**：
1. **进度报告干扰**：`tick()` 内部每秒一次的 `[PROGRESS]` 日志可能在测量时产生不公平比较
2. **测试方法问题**：批量模式下时间测量可能包含了 `sim.tick(N)` 之外的初始化开销

**待验证**：需重新测试，排除进度报告干扰，确认批量是否有加速。

#### 发现 2：TC-01 组合链性能随深度非线性崩溃

| depth | ticks/sec | ns/tick | ns/node/tick | 相对衰退 |
|-------|-----------|---------|---------------|----------|
| 10 | 29,840 | 33,513 | 3,351 | 1x |
| 100 | 3,120 | 320,546 | 3,205 | ~10x |
| 1000 | 265 | 3,779,300 | 3,779 | ~113x |
| 10000 | 20 | 50,703,200 | 5,070 | ~1500x |

**观察**：`ns/node/tick` 从 3,351 (depth=10) 下降到 5,070 (depth=10000)，说明单节点 eval 本身是常数复杂度。但 **总时间** 随深度超线性增长。

**可能根因**：
- 拓扑排序退化：depth=10000 时指令列表遍历存在 Cache thrashing
- 指令调度开销放大：每次 tick 重新 dispatch 大量指令，指令缓存命中率下降
- 组合逻辑收敛迭代：链式 `a = b ^ i` 结构在 eval 时可能存在伪依赖

**初步结论**：组合逻辑链 eval 本身是 O(n)，但 n=10000 时常数因子急剧增大，疑似 Cache/分支预测失效。

#### 发现 3：TC-02 寄存器 eval 异常高效

| regs | ticks/sec | ns/tick | ns/reg/tick |
|-------|-----------|---------|-------------|
| 10 | 121,874 | 8,205 | 820.5 ns |
| 100 | 107,796 | 9,277 | 92.8 ns |
| 1000 | 115,809 | 8,635 | 8.6 ns |
| 10000 | 114,569 | 8,728 | 0.87 ns |

**观察**：`ns/reg/tick` 随寄存器数增加反而**下降**，这说明寄存器 eval 的开销主要在于**固定调度开销**（函数调用、循环），而非每增加一个寄存器线性增长。

**结论**：寄存器 eval 本身非常高效（约 0.87ns/reg），主要瓶颈在调度框架开销。

#### 发现 4：TC-04 trace 开销是最大性能杀手

82.88% 的 trace overhead 意味着开启 trace 后性能下降近 6 倍。

**根因分析**：
- 每次 tick 遍历所有 `signals_`
- 每次遍历都有 `data_map_.find(node_id)` 哈希查找
- `memcmp` + `memcpy` 对每个信号

---

## 四、三大项目详细对比

### 4.1 CppHDL（解释执行型）

**核心模式**：纯虚函数调用循环，每次 tick 遍历所有节点

**关键瓶颈**：
1. 每 tick 3 次组合逻辑遍历
2. `unordered_map` 哈希查找
3. 跟踪系统临时对象分配
4. 位宽操作逐位循环
5. 无内联/无并行

### 4.2 Cash（解释+JIT 双模式）

**关键设计**：

| 特性 | 实现 |
|------|------|
| **JIT 编译** | simjit 生成机器码，标量存 JIT 寄存器 |
| **标量优化** | ≤64bit 操作直接生成原生指令 |
| **内存布局** | sim_state_t 连续存储，Cache 友好 |
| **常量折叠** | CFO pass 编译时求值常量表达式 |

**性能声称**：simjit vs simref 加速 **30-100 倍**

**可借鉴点**：JIT 标量寄存器优化、CRTP 完全内联、多 pass 优化

### 4.3 SpinalHDL（多后端+协作多线程）

**关键设计**：

| 特性 | 实现 | 源码位置 [1] |
|------|------|-------------|
| **后端抽象** | SimRaw 接口统一 get/set/eval/sleep | `sim/src/main/scala/spinal/sim/SimManager.scala` |
| **Verilator JNI** | 直接指针访问 signalAccess[]，零拷贝 | `core/src/main/scala/spinal/core/sim/` |
| **编译缓存** | SHA-1 缓存 .so，避免重复编译 | `sim/src/main/scala/spinal/sim/CompiledApi.scala` |
| **CPU 亲和** | JVM Thread 绑定 CPU 核心 | `SimManager.scala` |

**可借鉴点**：编译缓存机制、Read Bypass、命令缓冲批处理

### 4.4 三项目横向对比总结

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

*文档版本: v1.1*
*创建日期: 2026-04-26*
*最后更新: 2026-04-26（Oracle 审查反馈：修正 tick() 描述、补充 trace 临时对象、修正 TC-06 根因、更新 Cash 描述）*