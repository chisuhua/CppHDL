# CppHDL bits<Hi, Lo>(signal) 操作符使用说明

## 函数定义

在 `/workspace/CppHDL/include/core/operators.h` 第 577-588 行定义了两个 bits 函数：

```cpp
// 定义1：指定 Hi 和 Lo 位的片选操作
template <unsigned MSB, unsigned LSB, typename T> auto bits(const T &operand) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(MSB < ch_width_v<T>, "MSB must be < operand width");
    static_assert(LSB <= MSB, "LSB must be <= MSB");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_bits(
        operand_node, MSB, LSB, "bits", std::source_location::current());

    return make_uint_result<MSB - LSB + 1>(op_node);
}

// 定义2：指定宽度和低位索引的片选操作
template <unsigned Width, typename T>
auto bits(const T &operand, unsigned lsb) {
    static_assert(HardwareType<T>, "Operand must be a hardware type");
    static_assert(Width <= ch_width_v<T>, "MSB must be < operand width");

    auto operand_node = to_operand(operand);

    auto *op_node = node_builder::instance().build_bits(
        operand_node, lsb + Width - 1, lsb, "bits",
        std::source_location::current());

    return make_uint_result<Width>(op_node);
}
```

## 调用模式

### 函数式调用（推荐）

1. **`bits<Hi, Lo>(signal)`** - 从 Hi 位到 Lo 位的切片选择
   - Hi (MSB): 最高位索引
   - Lo (LSB): 最低位索引
   - 结果宽度为 Hi - Lo + 1
   - 例如: `bits<4, 0>(operand)` 提取 operand 的 [4:0] 位, 返回 5-bit 结果

2. **`bits<Width>(signal, lsb)`** - 指定宽度的位切片选择
   - Width: 提取的位宽
   - lsb: 最低位索引
   - 结果宽度为 Width
   - 例如: `bits<5>(operand, 0)` 提取 operand 的 5 位, 从索引 0 开始

### 举例

以下是真实的使用示例:
- `bits<4, 0>(io().operand_b)` - 提取 operand_b 的 [4:0] 位 (5 位结果)
- `bits<31, 31>(io().operand_a)` - 提取 operand_a 的 [31] 位 (1 位结果) 
- `bits<6, 0>(io().instruction)` - 提取 instruction 的 [6:0] 位 (7 位结果)
- `bits<11, 7>(io().instruction)` - 提取 instruction 的 [11:7] 位 (5 位结果)

## 实际应用案例

1. **ALU 中的位切片**:
   ```cpp
   auto shift_amt = bits<4, 0>(io().operand_b);  // 将 operand_b[4:0] 用作移位量
   auto a_sign = bits<31, 31>(io().operand_a);   // 获取操作数符号位
   auto b_sign = bits<31, 31>(io().operand_b);   // 获取操作数符号位
   ```

2. **指令解码中的字段提取**:
   ```cpp
   op = bits<6, 0>(io().instruction);      // 操作码 [6:0]
   dest = bits<11, 7>(io().instruction);   // 目标寄存器 [11:7]  
   f3 = bits<14, 12>(io().instruction);    // 功能码3 [14:12]
   f7 = bits<31, 25>(io().instruction);    // 功能码7 [31:25]
   ```

## 常见使用错误

以下是**错误**的调用方式（不会编译通过）：
- `bits(signal, Hi, Lo)` - 参数顺序错误，第一个 Hi 和 Lo 需要是模板参数
- `extract_bits<Hi,Lo>(signal)` - 不存在此函数名
- `signal.bits<Hi,Lo>()` - 错误的调用语法

## 总结

**bits<Hi, Lo>(signal)** 是 CppHDL 中提取信号位段的标准函数调用方式，语法完全正确。
这个函数提供了类似 Verilog 中位切片 (例如 signal[Hi:Lo]) 的功能。