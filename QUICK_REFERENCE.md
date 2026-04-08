# CppHDL 开发快速参考

**版本**: v1.0  
**更新日期**: 2026-04-08  
**适用对象**: 新加入的开发人员、AI Agent  

---

## 1. 核心概念速查

### 1.1 CppHDL vs SpinalHDL

| 概念 | SpinalHDL | CppHDL |
|------|-----------|--------|
| 组件类 | `Component` | `ch::Component` |
| IO 端口 | `in`/`out` | `ch_in`/`ch_out` |
| Bundle | `Bundle` | `bundle_base<T>` |
| 寄存器 | `Reg` | `ch_reg<T>` |
| 字面量 | `U(8, 8)` | `ch_uint<8>(8_d)` |
| 连接 | `<>` | `<<=` |

### 1.2 类型系统

```cpp
// 基本类型
ch_bool          // 1 位布尔
ch_uint<N>       // N 位无符号整数
ch_sint<N>       // N 位有符号整数

// 复合类型
ch::ch_stream<T> // 流 Bundle (payload, valid, ready)
ch::ch_flow<T>   // 流 Bundle (无 ready 握手)

// 自定义 Bundle
struct MyBundle : public bundle_base<MyBundle> {
    ch_uint<8> data;
    ch_bool valid;
    CH_BUNDLE_FIELDS_T(data, valid)
    
    void as_master_direction() {
        this->make_output(data, valid);
    }
};
```

---

## 2. 常用操作模式

### 2.1 Component 定义

```cpp
class MyModule : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> io_data;
        ch_out<ch_uint<8>> io_result;
    )
    
    MyModule(ch::Component* parent = nullptr, 
             const std::string& name = "mymodule")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 硬件描述逻辑
        io().io_result = io().io_data + 1;
    }
};
```

### 2.2 Bundle 连接

```cpp
// 方式 1: 使用 connect 函数
ch::core::connect(src_bundle, dst_bundle);

// 方式 2: 使用 operator<<=
dst_bundle <<= src_bundle;

// 方式 3: 使用 Simulator API
sim.set_value(bundle.field, value);
auto val = sim.get_value(bundle.field);
```

### 2.3 模块实例化

```cpp
// 方式 1: ch_module (推荐)
ch::ch_module<MyModule> inst{"inst"};
inst.io().io_data <<= input;
output <<= inst.io().io_result;

// 方式 2: module 类 (已废弃)
CH_MODULE(MyModule, inst);  // ❌ 不支持模板参数
```

---

## 3. 测试模式

### 3.1 基本测试结构

```cpp
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"

TEST_CASE("测试名称", "[标签]") {
    // 1. 创建设备
    ch::ch_device<MyModule> dev;
    
    // 2. 创建仿真器
    ch::Simulator sim(dev.context());
    
    // 3. 设置输入
    sim.set_input_value(dev.io().input, value);
    
    // 4. 运行仿真
    sim.tick();
    
    // 5. 验证输出
    auto result = sim.get_port_value(dev.io().output);
    REQUIRE(result == expected);
}
```

### 3.2 常用 Catch2 断言

| 断言 | 用法 | 说明 |
|------|------|------|
| `REQUIRE` | `REQUIRE(x == y)` | 必须满足 |
| `CHECK` | `CHECK(x > 0)` | 检查但不中止 |
| `STATIC_REQUIRE` | `STATIC_REQUIRE(CONST_EXPR)` | 编译期检查 |

### 3.3 测试标签

| 标签 | 用途 | 运行命令 |
|------|------|----------|
| `[bundle]` | Bundle 相关测试 | `ctest -L bundle` |
| `[stream]` | Stream 测试 | `ctest -L stream` |
| `[multithread]` | 多线程测试 | `ctest -L multithread` |
| `[chlib]` | Chlib 库测试 | `ctest -L chlib` |

---

## 4. 调试技巧

### 4.1 编译错误排查

| 错误类型 | 常见原因 | 解决方案 |
|----------|----------|----------|
| `node_impl_ or src_lnode is not null` | 对已有节点的字段使用 `<<=` | 检查字段是否已调用 `as_master/as_slave` |
| `parameter packs not expanded` | C++20 fold expression 语法错误 | 使用捕获指针方式 |
| `template argument 1 is invalid` | 宏不支持模板参数 | 使用 `ch::ch_module<T>` |

### 4.2 运行时错误

| 错误 | 原因 | 解决方案 |
|------|------|----------|
| `No active context` | 缺少 `ctx_swap` | 添加 `ctx_swap` 作用域 |
| `SIGSEGV` | 空指针解引用 | 检查节点是否已创建 |

### 4.3 LSP 诊断

```bash
# 检查编译错误
lsp_diagnostics -s error file.cpp

# 查看符号
lsp_symbols -s document file.cpp
```

---

## 5. 项目结构

```
CppHDL/
├── include/                    # 核心头文件
│   ├── core/                   # 核心逻辑层
│   │   ├── bundle/             # Bundle 元编程
│   │   ├── uint.h              # ch_uint 定义
│   │   ├── context.h           # 上下文管理
│   │   └── operators.h         # 操作符定义
│   ├── component.h             # Component 基类
│   ├── device.h                # Device API
│   ├── simulator.h             # 仿真 API
│   └── ch.hpp                  # 统一入口
├── src/                        # 实现文件
├── tests/                      # 单元测试
├── samples/                    # 快速示例
├── examples/                   # 完整项目
└── docs/                       # 文档
    ├── *UsageGuide.md          # 使用指南
    ├── problem-reports/        # 问题报告
    └── archive/                # 归档文档
```

---

## 6. 关键 API 参考

### 6.1 Simulator API

| 方法 | 签名 | 说明 |
|------|------|------|
| `set_input_value` | `void set_input_value(PortT& port, T value)` | 设置输入端口值 |
| `set_value` | `void set_value(FieldT& field, T value)` | 设置字段值 |
| `get_port_value` | `T get_port_value(PortT& port)` | 获取输出端口值 |
| `get_value` | `T get_value(FieldT& field)` | 获取字段值 |
| `tick` | `void tick()` | 运行一个时钟周期 |

### 6.2 Bundle API

| 方法 | 签名 | 说明 |
|------|------|------|
| `as_master` | `void as_master()` | 设置为主设备 (输出数据) |
| `as_slave` | `void as_slave()` | 设置为从设备 (输入数据) |
| `connect` | `void connect(src, dst)` | 同类型 Bundle 连接 |
| `operator<<=` | `Bundle& operator<<=(Bundle& src)` | Bundle 连接符 |

---

## 7. 常见问题 (FAQ)

### Q: `ch_uint` 和 `uint32_t` 什么区别？

A: `ch_uint<N>` 是硬件描述类型，表示 N 位硬件信号；`uint32_t` 是软件类型，表示 32 位整数。不要混用。

### Q: 为什么 `=` 和 `<<=` 表现不同？

A: `=` 是 C++ 赋值，`<<=` 是硬件连接符号。在 Component `describe()` 中使用 `<<=`。

### Q: Bundle 字段如何访问？

A: 直接访问字段成员：`bundle.data`, `bundle.valid`。无需转义。

### Q: 线程安全如何保证？

A: 使用 `thread_local` 存储当前上下文，每个线程独立。

---

## 8. 学习路径

| 阶段 | 学习内容 | 资源 | 预计时间 |
|------|----------|------|----------|
| 入门 | 基本类型、Component 定义 | `README.md`, `samples/simple_io.cpp` | 2h |
| 进阶 | Bundle、Stream、FIFO | `Bundle_UsageGuide.md`, `samples/fifo_example.cpp` | 4h |
| 高级 | AXI 协议、RISC-V | `PHASE3` 系列报告 | 8h |

---

## 9. 工具链

| 工具 | 用途 | 命令 |
|------|------|------|
| `cmake` | 构建系统 | `cmake -B build && cmake --build build` |
| `ctest` | 测试运行 | `ctest --output-on-failure` |
| `clang-tidy` | 静态分析 | `clang-tidy src/*.cpp` |
| `clang-format` | 代码格式化 | `clang-format -i src/*.cpp` |

---

## 10. 相关文档

- [Bundle_UsageGuide.md](Bundle_UsageGuide.md) - Bundle 详细使用指南
- [CppHDL_UsageGuide.md](CppHDL_UsageGuide.md) - CppHDL 综合指南
- [docs/problem-reports/bundle-connection-issue.md](docs/problem-reports/bundle-connection-issue.md) - Bundle 连接问题分析
- [docs/plans/bundle-connection-fix-plan.md](docs/plans/bundle-connection-fix-plan.md) - Bundle 连接修复计划

---

**维护**: 项目核心开发团队  
**反馈**: 提交 Issue 或 PR
