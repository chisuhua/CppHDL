# rv32i-decoder-pattern - RISC-V 指令译码器模式

**版本**: v1.0
**创建时间**: 2026-04-02
**用途**: 指导如何正确实现 RISC-V 指令译码器，避免立即数提取错误

---

## 触发条件

当需要实现 RISC-V 指令译码器时触发，尤其是：
- 需要解析 RV32I 基础指令集（40 条指令）
- 需要正确处理 5 种指令格式（R/I/S/B/U/J 型）
- 需要从 32 位指令字中提取立即数并进行符号/零扩展
- 在 CppHDL 框架中使用 `bits<>()` 操作符进行位域提取

---

## 使用方法

### 步骤 1: 理解 RISC-V 指令格式

RISC-V RV32I 使用固定 32 位指令长度，分为 5 种格式：

```
R-type (寄存器 - 寄存器运算):
  31    25 24    20 19    15 14    12 11     7 6      0
  | funct7 |  rs2  |  rs1  | funct3 |  rd   | opcode |

I-type (立即数/加载):
  31            20 19    15 14    12 11     7 6      0
  |  immediate[11:0]  |  rs1  | funct3 |  rd   | opcode |

S-type (存储):
  31            25 24    20 19    15 14    12 6      0
  |  imm[11:5]  |  rs2  |  rs1  | funct3 | imm[4:0] | opcode |

B-type (分支):
  31    25 24    20 19    15 14    12 11     7 6      0
  | imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | opcode |

U-type (上层立即数):
  31            12 11     7 6      0
  |  immediate[31:12]   |  rd   | opcode |

J-type (跳转):
  31            12 11     7 6      0
  | imm[20|10:1|19:12]  |  rd   | opcode |
```

### 步骤 2: 实现译码器结构

```cpp
// 示例：RV32I 译码器基本结构
#include "core/core.h"

namespace rv32i {

struct InstrDecoder : ch::component<InstrDecoder> {
    // 输入：32 位指令字
    ch::ch_in<ch::u32> instr;
    
    // 输出：译码结果
    ch::ch_out<ch::u5> opcode;
    ch::ch_out<ch::u5> rd;
    ch::ch_out<ch::u3> funct3;
    ch::ch_out<ch::u5> rs1;
    ch::ch_out<ch::u5> rs2;
    ch::ch_out<ch::u7> funct7;
    
    // 立即数输出（符号扩展后）
    ch::ch_out<ch::s32> imm;
    
    void operator()() {
        using namespace ch::literals;
        
        // 1. 提取固定位域
        opcode = bits<6, 0>(instr);
        rd     = bits<11, 7>(instr);
        funct3 = bits<14, 12>(instr);
        rs1    = bits<19, 15>(instr);
        rs2    = bits<24, 20>(instr);
        funct7 = bits<31, 25>(instr);
        
        // 2. 根据 opcode 选择立即数提取方式
        ch::s32 imm_i, imm_s, imm_b, imm_u, imm_j;
        
        // I-type: imm[11:0] 符号扩展
        imm_i = sext<32>(bits<31, 20>(instr));
        
        // S-type: imm[11:5] + imm[4:0] 拼接后符号扩展
        imm_s = sext<32>(cat(bits<31, 25>(instr), bits<11, 7>(instr)));
        
        // B-type: imm[12|10:5|4:1|11] 特殊拼接
        imm_b = sext<32>(cat(
            bits<31, 31>(instr),  // imm[12]
            bits<7, 7>(instr),    // imm[11]
            bits<30, 25>(instr),  // imm[10:5]
            bits<11, 8>(instr),   // imm[4:1]
            0_b                   // imm[0] = 0 (始终对齐)
        ));
        
        // U-type: imm[31:12] 左移 12 位
        imm_u = bits<31, 12>(instr) << 12;
        
        // J-type: imm[20|10:1|11|19:12] 特殊拼接
        imm_j = sext<32>(cat(
            bits<31, 31>(instr),  // imm[20]
            bits<19, 12>(instr),  // imm[19:12]
            bits<20, 20>(instr),  // imm[11]
            bits<30, 21>(instr),  // imm[10:1]
            0_b                   // imm[0] = 0 (始终对齐)
        ));
        
        // 3. 根据 opcode 选择最终立即数
        imm = ch::select(opcode == 0b0010011, imm_i,  // I-type (ALU)
             ch::select(opcode == 0b0000011, imm_i,   // I-type (Load)
             ch::select(opcode == 0b0100011, imm_s,   // S-type
             ch::select(opcode == 0b1100011, imm_b,   // B-type
             ch::select(opcode == 0b0110111, imm_u,   // U-type (LUI)
             ch::select(opcode == 0b1101111, imm_j,   // J-type
              0_s32))))));
    }
};

} // namespace rv32i
```

---

## 技术细节

### 立即数提取规则

| 指令类型 | 立即数位范围 | 拼接方式 | 扩展方式 |
|---------|------------|---------|---------|
| I-type | [31:20] | 直接使用 | 符号扩展 (sext) |
| S-type | [31:25] + [11:7] | cat(high, low) | 符号扩展 (sext) |
| B-type | [31] + [7] + [30:25] + [11:8] | cat(12, 11, 10:5, 4:1, 0) | 符号扩展 (sext) |
| U-type | [31:12] | 左移 12 位 | 零扩展 (隐式) |
| J-type | [31] + [19:12] + [20] + [30:21] | cat(20, 19:12, 11, 10:1, 0) | 符号扩展 (sext) |

### CppHDL `bits<>()` 操作符用法

```cpp
// 语法：bits<HIGH, LOW>(signal)
// 返回：从 signal 中提取 [HIGH:LOW] 位域

// 示例 1: 提取低 8 位
auto low_byte = bits<7, 0>(data);  // data[7:0]

// 示例 2: 提取单个位
auto flag = bits<15, 15>(status);  // status[15]

// 示例 3: 提取高 7 位
auto high = bits<31, 25>(instr);   // instr[31:25]

// 注意事项：
// 1. HIGH >= LOW（不能反向）
// 2. 索引从 0 开始（LSB = 0）
// 3. 返回值类型自动推断为 ch::u<N>，其中 N = HIGH - LOW + 1
```

### 符号扩展 `sext<>()` vs 零扩展 `zext<>()`

```cpp
// 符号扩展：保留符号位
ch::s32 signed_val = sext<32>(bits<11, 0>(instr));  // 12 位 → 32 位，符号扩展

// 零扩展：高位补 0
ch::u32 unsigned_val = zext<32>(bits<11, 0>(instr));  // 12 位 → 32 位，零扩展

// 使用场景：
// - 立即数运算：sext（大多数 RISC-V 立即数是有符号的）
// - 地址偏移：sext（PC 相对偏移可正可负）
// - 位掩码：zext（掩码始终为正）
```

---

## 示例代码

### 完整译码器实现

```cpp
// rv32i_decoder.h
#pragma once
#include "core/core.h"

namespace rv32i {

// 指令类型枚举
enum class InstrType : ch::u3 {
    R_TYPE = 0,
    I_TYPE = 1,
    S_TYPE = 2,
    B_TYPE = 3,
    U_TYPE = 4,
    J_TYPE = 5
};

// 译码结果结构
struct DecodedInstr {
    ch::u5 opcode;
    ch::u5 rd;
    ch::u3 funct3;
    ch::u5 rs1;
    ch::u5 rs2;
    ch::u7 funct7;
    ch::s32 imm;
    InstrType type;
};

// 主译码器组件
struct Decoder : ch::component<Decoder> {
    ch::ch_in<ch::u32> instr;
    ch::ch_out<DecodedInstr> decoded;
    
    void operator()() {
        using namespace ch::literals;
        
        // 提取基本字段
        decoded.opcode = bits<6, 0>(instr);
        decoded.rd = bits<11, 7>(instr);
        decoded.funct3 = bits<14, 12>(instr);
        decoded.rs1 = bits<19, 15>(instr);
        decoded.rs2 = bits<24, 20>(instr);
        decoded.funct7 = bits<31, 25>(instr);
        
        // 确定指令类型（根据 opcode）
        ch::u3 type_code = ch::select(
            decoded.opcode == 0b0110011, 0_u3,  // R-type
            ch::select(decoded.opcode == 0b0010011, 1_u3,  // I-type
            ch::select(decoded.opcode == 0b0100011, 2_u3,  // S-type
            ch::select(decoded.opcode == 0b1100011, 3_u3,  // B-type
            ch::select(decoded.opcode == 0b0110111, 4_u3,  // U-type
            5_u3)))));  // J-type
        
        decoded.type = ch::bit_cast<InstrType>(type_code);
        
        // 提取立即数（见下方详细实现）
        decoded.imm = extract_immediate(instr, decoded.type);
    }
    
private:
    ch::s32 extract_immediate(ch::u32 instr, InstrType type) {
        using namespace ch::literals;
        
        ch::s32 imm_i = sext<32>(bits<31, 20>(instr));
        ch::s32 imm_s = sext<32>(cat(bits<31, 25>(instr), bits<11, 7>(instr)));
        ch::s32 imm_b = sext<32>(cat(bits<31, 31>(instr), bits<7, 7>(instr),
                                      bits<30, 25>(instr), bits<11, 8>(instr), 0_b));
        ch::s32 imm_u = bits<31, 12>(instr) << 12;
        ch::s32 imm_j = sext<32>(cat(bits<31, 31>(instr), bits<19, 12>(instr),
                                      bits<20, 20>(instr), bits<30, 21>(instr), 0_b));
        
        return ch::select(type == InstrType::I_TYPE, imm_i,
               ch::select(type == InstrType::S_TYPE, imm_s,
               ch::select(type == InstrType::B_TYPE, imm_b,
               ch::select(type == InstrType::U_TYPE, imm_u,
               ch::select(type == InstrType::J_TYPE, imm_j,
               0_s32)))));
    }
};

} // namespace rv32i
```

### 测试示例

```cpp
// test_decoder.cpp
#include "riscv/rv32i_decoder.h"
#include "core/sim.h"

int main() {
    using namespace rv32i;
    
    // 测试用例 1: ADDI (I-type)
    // addi x1, x0, 42  => 0x02A00093
    ch::u32 test1 = 0x02A00093;
    auto imm1 = sext<32>(bits<31, 20>(test1));  // 应等于 42
    
    // 测试用例 2: SW (S-type)
    // sw x2, 100(x3)  => 0x0641A223
    ch::u32 test2 = 0x0641A223;
    auto imm2 = sext<32>(cat(bits<31, 25>(test2), bits<11, 7>(test2)));  // 应等于 100
    
    // 测试用例 3: BEQ (B-type)
    // beq x4, x5, -20  => 0xFF5718E3
    ch::u32 test3 = 0xFF5718E3;
    auto imm3 = sext<32>(cat(bits<31, 31>(test3), bits<7, 7>(test3),
                              bits<30, 25>(test3), bits<11, 8>(test3), 0_b));  // 应等于 -20
    
    // 测试用例 4: LUI (U-type)
    // lui x6, 0x12345  => 0x12345337
    ch::u32 test4 = 0x12345337;
    auto imm4 = bits<31, 12>(test4) << 12;  // 应等于 0x12345000
    
    // 测试用例 5: JAL (J-type)
    // jal x1, 0x1000  => 0x001000EF
    ch::u32 test5 = 0x001000EF;
    auto imm5 = sext<32>(cat(bits<31, 31>(test5), bits<19, 12>(test5),
                              bits<20, 20>(test5), bits<30, 21>(test5), 0_b));  // 应等于 0x1000
    
    return 0;
}
```

---

## 常见错误

### 错误 1: 立即数位域提取错误

```cpp
// ❌ 错误：B 型立即数位顺序错误
imm_b = sext<32>(cat(bits<7, 7>(instr), bits<31, 31>(instr), ...));
// 应该：bits<31, 31> 是 imm[12], bits<7, 7> 是 imm[11]

// ✅ 正确
imm_b = sext<32>(cat(bits<31, 31>(instr), bits<7, 7>(instr), ...));
```

### 错误 2: 忘记符号扩展

```cpp
// ❌ 错误：直接赋值为有符号数
ch::s32 imm = bits<11, 0>(instr);  // 12 位无符号 → 32 位有符号，可能出错

// ✅ 正确
ch::s32 imm = sext<32>(bits<11, 0>(instr));  // 显式符号扩展
```

### 错误 3: U-type 未左移

```cpp
// ❌ 错误：U-type 立即数未左移 12 位
imm_u = bits<31, 12>(instr);  // 只有 20 位

// ✅ 正确
imm_u = bits<31, 12>(instr) << 12;  // 放到 32 位正确位置
```

### 错误 4: J-type 位拼接顺序错误

```cpp
// ❌ 错误：J 型位顺序混乱
imm_j = sext<32>(cat(bits<19, 12>(instr), bits<31, 31>(instr), ...));

// ✅ 正确（按 imm[20:0] 顺序）
imm_j = sext<32>(cat(
    bits<31, 31>(instr),  // imm[20] - 符号位
    bits<19, 12>(instr),  // imm[19:12]
    bits<20, 20>(instr),  // imm[11]
    bits<30, 21>(instr),  // imm[10:1]
    0_b                   // imm[0] - 始终为 0
));
```

### 错误 5: `bits<>()` 索引反向

```cpp
// ❌ 错误：HIGH < LOW
auto wrong = bits<0, 7>(instr);  // 编译错误

// ✅ 正确
auto correct = bits<7, 0>(instr);  // HIGH >= LOW
```

---

## 参考文档

- RISC-V 官方规范：https://riscv.org/specifications/
- RV32I 指令集手册：`riscv-spec-v2.2.pdf` Chapter 2
- CppHDL 操作符文档：`/workspace/CppHDL/docs/operators.md`
- 相关实现：`/workspace/CppHDL/include/riscv/rv32i_decoder.h`

---

## 测试

### 单元测试方法

```cpp
#include "riscv/rv32i_decoder.h"
#include "core/sim.h"
#include <cassert>

void test_r_type() {
    // add x1, x2, x3 => 0x003100B3
    ch::u32 instr = 0x003100B3;
    assert(bits<6, 0>(instr) == 0b0110011);
    assert(bits<14, 12>(instr) == 0b000);
    assert(bits<31, 25>(instr) == 0b0000000);
}

void test_i_type() {
    // addi x1, x0, 100 => 0x06400093
    ch::u32 instr = 0x06400093;
    auto imm = sext<32>(bits<31, 20>(instr));
    assert(imm == 100);
}

void test_s_type() {
    // sw x2, 200(x3) => 0x0C218223
    ch::u32 instr = 0x0C218223;
    auto imm = sext<32>(cat(bits<31, 25>(instr), bits<11, 7>(instr)));
    assert(imm == 200);
}

int main() {
    test_r_type();
    test_i_type();
    test_s_type();
    // ... 更多测试
    return 0;
}
```

### 编译验证

```bash
# 编译测试
cd /workspace/CppHDL
make test_decoder

# 运行测试
./build/test_decoder
# 预期输出：All tests passed!
```

---

**维护者**: DevMate
**许可证**: MIT
