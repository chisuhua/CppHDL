# ADR-022: SpinalHDL 流管道操作实现分析

**状态**: ✅ 已采纳
**日期**: 2026-05-08
**决策人**: Sisyphus

---

## 1. 背景

议题 #16 原称"SpinalHDL 流缺失操作 — m2sPipe(), s2mPipe(), halfPipe() 未实现"。该议题源自 2026-03-30 架构差距分析文档（P0 级缺陷："Stream 管道操作符未实现"），被列入 ADR 讨论计划 #16。

## 2. 实际状态调查结果

对当前代码库进行全面调查后发现，**三个函数均已实现**：

### 2.1 已存在的实现

| 组件 | 文件 | 行数 | 实现内容 |
|------|------|------|---------|
| `stream_m2s_pipe()` | `include/chlib/stream_pipeline.h:28-49` | 22 行 | Master→Slave 管道：valid/payload 延迟 1 拍 |
| `stream_s2m_pipe()` | `include/chlib/stream_pipeline.h:67-88` | 22 行 | Slave→Master 管道：payload 0 拍，ready 延迟 1 拍 |
| `stream_half_pipe()` | `include/chlib/stream_pipeline.h:106-144` | 39 行 | 全信号（valid/payload/ready）寄存器 + toggle 节拍控制 |
| `ch_stream::m2sPipe()` | `include/bundle/stream_bundle_member_inlines.h:11-13` | 3 行 | 代理到 `stream_m2s_pipe()` |
| `ch_stream::s2mPipe()` | 同文件 :21-23 | 3 行 | 代理到 `stream_s2m_pipe()` |
| `ch_stream::halfPipe()` | 同文件 :26-28 | 3 行 | 代理到 `stream_half_pipe()` |
| `ch_stream::stage()` | 同文件 :16-18 | 3 行 | `m2sPipe()` 别名 |

### 2.2 已存在的测试

`tests/test_stream_pipeline.cpp` 包含 **8 个 TEST_CASE**：

| 测试 | 标签 | 类型 |
|------|------|------|
| stream_m2s_pipe basic | `[stream][pipeline]` | 行为(1-cycle delay) |
| stream_m2s_pipe structural | `[stream][pipeline][structural]` | 结构(宽度验证) |
| stream_s2m_pipe basic | `[stream][pipeline]` | 行为(0-cycle payload) |
| stream_s2m_pipe structural | `[stream][pipeline][structural]` | 结构(宽度验证) |
| stream_half_pipe basic | `[stream][pipeline]` | 行为(toggle 节拍) |
| stream_half_pipe structural | `[stream][pipeline][structural]` | 结构(宽度验证) |
| m2sPipe member | `[stream][pipeline][member]` | 成员函数 |
| s2mPipe member | `[stream][pipeline][member]` | 成员函数 |
| halfPipe member | `[stream][pipeline][member]` | 成员函数 |
| stage member | `[stream][pipeline][member]` | `stage()` 别名 |

CMake 注册于 `tests/CMakeLists.txt:126`：
```cmake
add_catch_test(test_stream_pipeline test_stream_pipeline.cpp)
```

## 3. 识别到的残余缺口

尽管函数已实现，但存在影响用户可访问性和正确性的缺口：

### G1：`stream_pipeline.h` 不在 `chlib.h` 聚合头中（🔴）

`include/chlib.h` 是 chlib 组件的官方聚合入口，但它没有包含 `stream_pipeline.h`。
当前包含路径：

```
chlib.h
├── chlib/stream.h       ✅ (包含 stream bundle、FIFO、仲裁器等)
├── chlib/pipeline.h     ✅ (通用流水线寄存器)
├── chlib/stream_operators.h  ❌ (不在聚合中)
│   └── stream_pipeline.h     ← 间接可达但不可用
└── chlib/stream_pipeline.h   ❌ (不在聚合中)
```

用户通过 `#include "chlib.h"` 无法获得管道函数。

### G2：`stream_bundle_member_inlines.h` 是孤件（🔴）

该文件定义了成员函数 `ch_stream<T>::m2sPipe()`、`s2mPipe()`、`halfPipe()`，但**没有任何聚合头或公共头包含它**。文件头注释明确要求：

```
// This file must be included AFTER both:
//   - bundle/stream_bundle.h
//   - chlib/stream_pipeline.h
```

该文件仅被测试文件直接引用，用户无法通过标准 `#include "chlib.h"` 使用成员函数形式的管道操作。

### G3：`stream_operators.h` 不在聚合头中（🟡）

`stream_operators.h` 是 `stream_pipeline.h` 在聚合链中的唯一包含者，但它本身也不在 `chlib.h` 中。它仅被 `test_stream_operators.cpp` 直接引用。

### G4：`halfPipe` 实现语义需确认（🟡）

当前 `halfPipe` 使用 toggle flip-flop 实现"隔一拍接收"：

```cpp
// Only fire when toggle was low (meaning we accept every other cycle)
ch_bool accept_data = toggle_reg && input_fire;
```

这与 SpinalHDL 的 `halfPipe()` 语义是否一致需要验证。SpinalHDL 的 `halfPipe()` 通常：
- 在 valid & ready 同时为高的下一拍将 valid 拉低
- 再下一拍重新置高 valid
- 实际效果是插入一个寄存器级，带宽不变但增加 1 拍延迟

**注意**：SpinalHDL 的 `halfPipe()` 是一个**流水线寄存器**（插入寄存器级），名称中的 "half" 并非指带宽减半，而是指 pipeline bubble/throughput 特性。当前 toggle 实现可能产生不同的行为。

### G5：测试编译状态未验证（🔴）

测试在 CMake 中已注册但尚未验证编译通过。需要确认：
1. `#define CATCH_CONFIG_MAIN` 与 `catch_amalgamated.cpp` 的链接是否正常
2. 行为测试的期望值是否正确（halfPipe toggle 行为）
3. 所有 8 个测试通过

---

## 4. 决议

### 4.1 当前状态认定

鉴于实现代码和测试均已存在于代码库中，该议题的**核心实现已完成**。残余缺口属于**聚合集成**和**验证确认**范畴，而非"未实现"。

### 4.2 修复建议（非本次 ADR 范围）

| 缺口 | 修复方案 | 工作量 | 优先级 |
|------|---------|--------|--------|
| G1 | 在 `chlib.h` 中 `#include "chlib/stream_pipeline.h"` | ~1 行 | 🔴 高 |
| G2 | 在 `chlib.h` 末尾 `#include "bundle/stream_bundle_member_inlines.h"` | ~1 行 | 🔴 高 |
| G3 | 考虑将 `stream_operators.h` 加入 `chlib.h` 或由 `stream.h` 包含 | ~1 行 | 🟡 中 |
| G4 | 对照 SpinalHDL 源码验证 `halfPipe` 行为 | ~2h | 🟡 中 |
| G5 | `cmake --build && ctest -R test_stream_pipeline` | ~5min | 🔴 高 |

G1+G2 的修复需确保包含顺序正确：
```cpp
// chlib.h
#include "chlib/stream.h"            // 包含 stream_bundle.h
#include "chlib/stream_pipeline.h"   // 自由函数
#include "bundle/stream_bundle_member_inlines.h"  // 成员函数（依赖前两者）
```

---

## 5. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：记录 SpinalHDL 流管道操作实际实现现状和残余集成缺口 | Sisyphus |

---

**相关链接**:
- `include/chlib/stream_pipeline.h` — 管道自由函数实现
- `include/bundle/stream_bundle_member_inlines.h` — 管道成员函数实现
- `include/chlib.h` — chlib 聚合头（缺少管道包含）
- `include/chlib/stream_operators.h` — 流操作符（包含 pipeline 但自身也不在聚合中）
- `tests/test_stream_pipeline.cpp` — 管道测试
- `tests/CMakeLists.txt:126` — 测试 CMake 注册
- `docs/architecture/2026-03-30-cpphdl-architecture-gap-analysis.md` — 原始差距分析（称"未实现"）
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #16
