# SpinalHDL 移植示例

本目录包含从 SpinalHDL 移植到 CppHDL 的经典示例。

## 目录结构

```
examples/spinalhdl-ported/
├── README.md                 # 本文件
├── testbench_template.cpp    # 测试平台模板
├── counter/                  # 计数器示例
│   ├── counter.cpp           # 完整版（带 enable/clear 输入）
│   ├── counter_simple.cpp    # 简化版（自由运行）
│   └── CMakeLists.txt
├── fifo/                     # FIFO 示例（待实现）
├── uart/                     # UART 示例（待实现）
└── ...
```

## 快速开始

### 1. 构建示例

```bash
cd /workspace/CppHDL/build
cmake ..
make counter_simple_example
```

### 2. 运行示例

```bash
./examples/spinalhdl-ported/counter/counter_simple_example
```

### 3. 查看生成的 Verilog

```bash
cat counter_simple.v
```

## 使用测试平台模板

创建新示例时，复制 `testbench_template.cpp` 并修改：

```cpp
// 1. 定义被测模块
class MyModule : public ch::Component {
public:
    __io(
        ch_in<ch_bool> enable;
        ch_out<ch_uint<8>> output;
    )
    
    void describe() override {
        // 实现逻辑
    }
};

// 2. 在 Top 中实例化
class Top : public ch::Component {
    void describe() override {
        CH_MODULE(MyModule, my_module);
    }
};

// 3. 在 Testbench 中编写测试
void run_test() {
    // 测试逻辑
}
```

## 已实现示例

### Counter（计数器）

| 文件 | 描述 | 状态 |
|------|------|------|
| `counter_simple.cpp` | 自由运行计数器 | ✅ 完成 |
| `counter.cpp` | 带 enable/clear 控制 | 🟡 部分完成 |

**SpinalHDL 原代码**:
```scala
class Counter extends Component {
  val io = new Bundle {
    val value = out UInt(8 bits)
  }
  val counterReg = RegInit(U(0, 8 bits))
  counterReg := counterReg + 1
  io.value := counterReg
}
```

**CppHDL 移植**:
```cpp
template <unsigned N = 8>
class Counter : public ch::Component {
public:
    __io(ch_out<ch_uint<N>> value;)
    
    void describe() override {
        ch_reg<ch_uint<N>> counter_reg(0_d);
        counter_reg->next = counter_reg + 1_d;
        io().value = counter_reg;
    }
};
```

## 待实现示例

| 示例 | 优先级 | 预计工作量 | 依赖 |
|------|--------|-----------|------|
| FIFO | P0 | 4 小时 | ch_mem |
| UART TX | P1 | 4 小时 | 状态机 DSL |
| UART RX | P1 | 4 小时 | 状态机 DSL |
| GPIO | P1 | 2 小时 | - |
| PWM | P2 | 3 小时 | 比较器 |
| SPI Controller | P2 | 6 小时 | 状态机 |

## 测试规范

### 测试期望值计算

CppHDL 仿真语义：`tick()` **后**读取值为新值

```cpp
// 正确：Cycle 0 对应计数值 1
for (int i = 0; i < 10; i++) {
    simulator.tick();
    auto val = simulator.get_value(device.instance().io().value);
    uint64_t expected = i + 1;  // tick 后计数值 = 周期数 + 1
}
```

### 测试通过标准

1. **功能正确**: 输出值符合预期
2. **边界正确**: 溢出/欠载行为正确
3. **Verilog 生成**: 无错误
4. **DAG 生成**: 无错误

## 参考资料

- [CppHDL 架构差距分析](../../docs/architecture/2026-03-30-cpphdl-architecture-gap-analysis.md)
- [SpinalHDL 移植架构](../../docs/architecture/2026-03-30-spinalhdl-port-architecture.md)
- [官方 Counter 示例](../../samples/counter.cpp)
