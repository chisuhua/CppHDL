# RV32I Branch Pattern Skill

**技能名称**: `rv32i-branch-pattern`  
**版本**: 1.0  
**最后更新**: 2026-04-03  
**适用领域**: RISC-V RV32I 指令集、分支跳转指令、CppHDL 实现  

---

## 1. 技能描述

本技能沉淀 RISC-V RV32I 基础指令集中**分支跳转指令**的实现模式，涵盖 B-type 分支指令和 J-type 跳转指令的位域解析、立即数拼接、条件判断及 CppHDL 实现要点。

**核心价值**:
- 统一分支指令实现风格，避免位域拼接错误
- 提供经过验证的 CppHDL 代码模板
- 总结常见陷阱与调试经验
- 支持快速复用至其他 RISC-V 指令实现

---

## 2. 分支指令格式 (B-type)

### 2.1 位域分布

```
B-type 指令格式 (32 位):
┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
│  31:25  │  24:20  │  19:15  │  14:12  │  11:8   │   7     │  6:0    │
│ imm[10:4]│  rs2    │  rs1    │ funct3  │ imm[4:1]│ imm[11] │ opcode  │
│ (B-type)│         │         │         │ (B-type)│ (B-type)│(1100011)│
└─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘

立即数拼接规则:
imm[12|10:5|4:1|11] = instr[31|30:25|11:8|7]
                    ↓
imm[12]  = instr[31]  (符号位)
imm[10:5]= instr[30:25]
imm[4:1] = instr[11:8]
imm[11]  = instr[7]

最终 13 位有符号立即数 (bit12 是符号位):
branch_offset = {imm[12], imm[11], imm[10:5], imm[4:1], 1'b0}
                ↓
                左移 1 位 (PC 对齐到 2 字节边界)
```

### 2.2 CppHDL 位域提取代码

```cpp
// B-type 指令解析示例
struct BTypeInstr {
    ch_uint<7> opcode;    // [6:0]
    ch_uint<3> funct3;    // [14:12]
    ch_uint<5> rs1;       // [19:15]
    ch_uint<5> rs2;       // [24:20]
    ch_bits<13> imm;      // 拼接后的 13 位立即数
    
    static BTypeInstr decode(ch_uint<32> instr) {
        BTypeInstr result;
        result.opcode = bits<6, 0>(instr);
        result.funct3 = bits<14, 12>(instr);
        result.rs1 = bits<19, 15>(instr);
        result.rs2 = bits<24, 20>(instr);
        
        // 关键：B-type 立即数拼接 (注意位序重排)
        ch_bit imm12 = bits<31, 31>(instr);  // imm[12] = instr[31]
        ch_bit imm11 = bits<7, 7>(instr);    // imm[11] = instr[7]
        ch_uint<6> imm10_5 = bits<30, 25>(instr);  // imm[10:5] = instr[30:25]
        ch_uint<4> imm4_1 = bits<11, 8>(instr);    // imm[4:1] = instr[11:8]
        
        // 拼接：{imm[12], imm[11], imm[10:5], imm[4:1], 1'b0}
        result.imm = concat<13>(imm12, imm11, imm10_5, imm4_1, ch_bit(0));
        
        return result;
    }
};
```

---

## 3. 跳转指令格式 (J-type & I-type)

### 3.1 JAL 指令 (J-type)

```
J-type 指令格式 (32 位):
┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
│  31     │  30:21  │  20     │  19:12  │  11:1   │  6:0    │
│ imm[20] │ imm[10:1]│ imm[11]│ imm[19:12]│ (0x00)│ opcode  │
│(J-type) │(J-type) │(J-type) │ (J-type) │         │(1101111)│
└─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘

立即数拼接规则:
imm[20|10:1|11|19:12] = instr[31|30:21|20|19:12]
                       ↓
imm[20]  = instr[31]  (符号位)
imm[10:1]= instr[30:21]
imm[11]  = instr[20]
imm[19:12]= instr[19:12]

最终 21 位有符号立即数:
jump_offset = {imm[20], imm[19:12], imm[11], imm[10:1], 1'b0}
              ↓
              左移 1 位 (20 位范围，按 2 字节对齐)
```

### 3.2 JALR 指令 (I-type 变体)

```
I-type 指令格式 (32 位):
┌─────────┬─────────┬─────────┬─────────┬─────────┐
│  31:20  │  19:15  │  14:12  │  11:7   │  6:0    │
│  imm[11:0]│  rs1   │ funct3  │   rd    │ opcode  │
│(I-type) │        │ (000)   │         │(1100111)│
└─────────┴─────────┴─────────┴─────────┴─────────┘

跳转目标计算:
target = (rs1 + imm) & ~1
         ↓
         强制清零 bit 0 (确保地址对齐到 2 字节)

关键差异:
- JAL: PC-relative (PC + offset)
- JALR: Register-relative (rs1 + imm), 且必须清零 bit 0
```

### 3.3 CppHDL 跳转计算代码

```cpp
// JAL 跳转目标计算
ch_uint<32> calculate_jal_target(ch_uint<32> pc, ch_bits<21> offset) {
    // sign_extend 将 21 位扩展为 32 位有符号数
    return pc + sign_extend<32>(offset);
}

// JALR 跳转目标计算 (注意 & ~1)
ch_uint<32> calculate_jalr_target(ch_uint<32> rs1_value, ch_bits<12> imm) {
    ch_uint<32> sum = rs1_value + sign_extend<32>(imm);
    // 清零 bit 0: sum & ~1
    return sum & ch_uint<32>(0xFFFFFFFE);
}

// 链接寄存器 (rd != 0 时写入 PC+4)
void link_return_address(ch_uint<5> rd, ch_uint<32> pc_plus_4, RegisterFile& regfile) {
    if (rd != 0) {
        regfile.write(rd, pc_plus_4);
    }
}
```

---

## 4. 比较标志实现

### 4.1 三路比较器 (Three-way Comparator)

```cpp
// 分支比较结果结构
struct BranchCompare {
    ch_bool equal;        // rs1 == rs2
    ch_bool less_than;    // rs1 < rs2 (有符号)
    ch_bool less_than_u;  // rs1 < rs2 (无符号)
    
    // 派生条件
    ch_bool not_equal() const { return !equal; }
    ch_bool greater_equal() const { return !less_than; }      // >= (signed)
    ch_bool greater_equal_u() const { return !less_than_u; }  // >= (unsigned)
};

// 比较器实现
BranchCompare compare(ch_uint<32> rs1, ch_uint<32> rs2) {
    BranchCompare result;
    
    // 1. 相等判断 (直接比较)
    result.equal = (rs1 == rs2);
    
    // 2. 有符号小于判断
    // 方法 1: 符号位 + 减法结果 (推荐，避免溢出问题)
    ch_bit rs1_sign = bits<31, 31>(rs1);
    ch_bit rs2_sign = bits<31, 31>(rs2);
    ch_uint<32> diff = rs1 - rs2;
    ch_bit diff_sign = bits<31, 31>(diff);
    
    // 符号位不同：rs1 负 rs2 正 → rs1 < rs2
    // 符号位相同：diff 为负 → rs1 < rs2
    result.less_than = (rs1_sign & !rs2_sign) | 
                       (!(rs1_sign ^ rs2_sign) & diff_sign);
    
    // 3. 无符号小于判断 (直接比较)
    result.less_than_u = (rs1 < rs2);  // ch_uint 默认为无符号比较
    
    return result;
}
```

### 4.2 符号位判断详解

**问题**: 直接 `rs1 - rs2` 可能溢出，导致符号位错误。

**解决方案**: 分情况讨论

| rs1 符号 | rs2 符号 | 结论 | 条件表达式 |
|---------|---------|------|-----------|
| 0 (正) | 0 (正) | 看 diff 符号 | `!rs1_sign & !rs2_sign & diff_sign` |
| 0 (正) | 1 (负) | rs1 > rs2 | `!rs1_sign & rs2_sign` → false |
| 1 (负) | 0 (正) | rs1 < rs2 | `rs1_sign & !rs2_sign` → true |
| 1 (负) | 1 (负) | 看 diff 符号 | `rs1_sign & rs2_sign & diff_sign` |

**简化公式**:
```cpp
result.less_than = (rs1_sign & !rs2_sign) | 
                   (!(rs1_sign ^ rs2_sign) & diff_sign);
// 第一项：符号不同，rs1 负 rs2 正 → 肯定小于
// 第二项：符号相同，看差值符号
```

---

## 5. 分支条件判断表

| 指令 | funct3 | 条件表达式 | CppHDL 实现 |
|------|--------|-----------|------------|
| **BEQ** | 000 | `rs1 == rs2` | `cmp.equal` |
| **BNE** | 001 | `rs1 != rs2` | `!cmp.equal` |
| **BLT** | 100 | `rs1 < rs2` (signed) | `cmp.less_than` |
| **BGE** | 101 | `rs1 >= rs2` (signed) | `!cmp.less_than` |
| **BLTU** | 110 | `rs1 < rs2` (unsigned) | `cmp.less_than_u` |
| **BGEU** | 111 | `rs1 >= rs2` (unsigned) | `!cmp.less_than_u` |

### 5.1 分支条件解码器

```cpp
// 根据 funct3 选择分支条件
ch_bool evaluate_branch_condition(ch_uint<3> funct3, const BranchCompare& cmp) {
    // 使用 select() 函数进行条件选择
    ch_bool condition;
    
    switch (funct3) {
        case 0b000:  // BEQ
            condition = cmp.equal;
            break;
        case 0b001:  // BNE
            condition = !cmp.equal;
            break;
        case 0b100:  // BLT
            condition = cmp.less_than;
            break;
        case 0b101:  // BGE
            condition = !cmp.less_than;
            break;
        case 0b110:  // BLTU
            condition = cmp.less_than_u;
            break;
        case 0b111:  // BGEU
            condition = !cmp.less_than_u;
            break;
        default:
            condition = ch_bool(false);  // 非法指令
    }
    
    return condition;
}

// 或使用 select() 链式调用 (更符合 CppHDL 风格)
ch_bool evaluate_branch_condition_select(ch_uint<3> funct3, const BranchCompare& cmp) {
    return select(funct3 == 0b000, cmp.equal,
           select(funct3 == 0b001, !cmp.equal,
           select(funct3 == 0b100, cmp.less_than,
           select(funct3 == 0b101, !cmp.less_than,
           select(funct3 == 0b110, cmp.less_than_u,
           select(funct3 == 0b111, !cmp.less_than_u,
                  ch_bool(false)))))));
}
```

---

## 6. CppHDL 实现要点

### 6.1 核心函数使用

**`select()` 条件选择**:
```cpp
// 语法：select(condition, true_value, false_value)
ch_uint<32> next_pc = select(branch_taken, target_pc, pc + 4);

// 嵌套使用 (多路选择)
ch_uint<32> alu_result = select(op == ADD, rs1 + rs2,
                        select(op == SUB, rs1 - rs2,
                        select(op == AND, rs1 & rs2,
                               rs1 | rs2)));
```

**`bits<>()` 位域提取**:
```cpp
// 语法：bits<high, low>(value)
ch_uint<5> rd = bits<11, 7>(instr);
ch_bit sign_bit = bits<31, 31>(instr);

// 提取后立即扩展
ch_bits<12> imm12 = bits<31, 20>(instr);
ch_int<32> imm_extended = sign_extend<32>(imm12);
```

**`concat()` 位拼接**:
```cpp
// 语法：concat<total_bits>(part1, part2, ...)
// B-type 立即数拼接示例
ch_bits<13> branch_imm = concat<13>(
    bits<31, 31>(instr),  // imm[12]
    bits<7, 7>(instr),    // imm[11]
    bits<30, 25>(instr),  // imm[10:5]
    bits<11, 8>(instr),   // imm[4:1]
    ch_bit(0)             // imm[0] = 0 (左移 1 位)
);
```

### 6.2 符号扩展

```cpp
// 模板函数：将 N 位有符号数扩展为 M 位
template<int M, int N>
ch_int<M> sign_extend(ch_bits<N> value) {
    ch_int<M> result;
    // 高位用符号位填充
    for (int i = N; i < M; i++) {
        result[i] = value[N-1];  // 符号位复制
    }
    for (int i = 0; i < N; i++) {
        result[i] = value[i];
    }
    return result;
}

// 使用示例
ch_int<32> offset_32 = sign_extend<32, 13>(branch_imm);
ch_uint<32> target_pc = pc + static_cast<ch_uint<32>>(offset_32);
```

### 6.3 类型转换注意事项

```cpp
// ch_bits → ch_int (有符号)
ch_bits<13> branch_imm = ...;
ch_int<32> signed_offset = sign_extend<32, 13>(branch_imm);
ch_uint<32> unsigned_offset = static_cast<ch_uint<32>>(signed_offset);

// ch_uint → ch_bits (位提取后)
ch_uint<32> rs1_value = regfile.read(rs1);
ch_bit sign = bits<31, 31>(rs1_value);  // 自动转换为 ch_bit

// 避免隐式转换警告
ch_uint<32> a = ...;
ch_uint<32> b = ...;
ch_bool is_less = (a < b);  // ✅ 无符号比较
// ch_bool is_less = (static_cast<int>(a) < static_cast<int>(b));  // ❌ 错误：强制转有符号
```

---

## 7. 常见错误与调试

### 7.1 B-type 立即数符号扩展错误

**错误表现**: 分支偏移量计算错误，负偏移变成正偏移。

**错误代码**:
```cpp
// ❌ 错误：直接拼接，未考虑符号位
ch_bits<13> branch_imm = concat<13>(
    bits<31, 31>(instr),  // imm[12]
    bits<7, 7>(instr),    // imm[11]
    bits<30, 25>(instr),  // imm[10:5]
    bits<11, 8>(instr),   // imm[4:1]
    ch_bit(0)
);
ch_uint<32> target = pc + branch_imm;  // ❌ 未符号扩展！
```

**正确代码**:
```cpp
// ✅ 正确：拼接后符号扩展
ch_bits<13> branch_imm = concat<13>(...);
ch_int<32> signed_offset = sign_extend<32, 13>(branch_imm);
ch_uint<32> target = pc + static_cast<ch_uint<32>>(signed_offset);
```

**调试技巧**:
```cpp
// 打印关键信号进行验证
if (opcode == 0b1100011) {  // BRANCH opcode
    println("Branch instruction detected");
    println("  imm[12] = ", bits<31, 31>(instr));
    println("  imm[11] = ", bits<7, 7>(instr));
    println("  imm[10:5] = ", bits<30, 25>(instr));
    println("  imm[4:1] = ", bits<11, 8>(instr));
    println(" 拼接后 imm = ", branch_imm);
    println(" 符号扩展后 = ", signed_offset);
}
```

### 7.2 JALR 地址未对齐处理

**错误表现**: 跳转到奇数地址，导致 hardware trap。

**错误代码**:
```cpp
// ❌ 错误：忘记 & ~1
ch_uint<32> target = rs1_value + sign_extend<32, 12>(imm);
```

**正确代码**:
```cpp
// ✅ 正确：强制清零 bit 0
ch_uint<32> target = (rs1_value + sign_extend<32, 12>(imm)) & ch_uint<32>(0xFFFFFFFE);
```

**测试用例**:
```cpp
// 测试 JALR 地址对齐
TEST(JALR, AddressAlignment) {
    // rs1 = 0x1001 (奇数), imm = 0
    // 期望结果：0x1000 (清零 bit 0)
    ch_uint<32> rs1 = 0x1001;
    ch_uint<32> target = calculate_jalr_target(rs1, ch_bits<12>(0));
    ASSERT_EQ(target, 0x1000);
    
    // rs1 = 0x1000 (偶数), imm = 0
    // 期望结果：0x1000 (保持不变)
    rs1 = 0x1000;
    target = calculate_jalr_target(rs1, ch_bits<12>(0));
    ASSERT_EQ(target, 0x1000);
}
```

### 7.3 有符号/无符号比较混淆

**错误表现**: BLT 和 BLTU 行为错误，负数比较结果不正确。

**错误代码**:
```cpp
// ❌ 错误：BLTU 使用了有符号比较
ch_bool less_than = (static_cast<int>(rs1) < static_cast<int>(rs2));  // 有符号
branch_condition = select(funct3 == 0b110, less_than, ...);  // BLTU 应该用无符号！
```

**正确代码**:
```cpp
// ✅ 正确：严格区分
BranchCompare cmp = compare(rs1, rs2);
branch_condition = select(funct3 == 0b100, cmp.less_than,    // BLT (signed)
                   select(funct3 == 0b110, cmp.less_than_u,   // BLTU (unsigned)
                          ...));
```

**边界测试**:
```cpp
TEST(Compare, SignedVsUnsigned) {
    // rs1 = 0xFFFFFFFF (-1 signed), rs2 = 1
    ch_uint<32> rs1 = 0xFFFFFFFF;
    ch_uint<32> rs2 = 1;
    
    BranchCompare cmp = compare(rs1, rs2);
    
    // 有符号：-1 < 1 → true
    ASSERT_TRUE(cmp.less_than);
    
    // 无符号：0xFFFFFFFF > 1 → false
    ASSERT_FALSE(cmp.less_than_u);
}
```

---

## 8. 完整实现示例

### 8.1 分支执行单元

```cpp
// 分支执行单元主函数
ch_uint<32> execute_branch(
    ch_uint<32> pc,
    ch_uint<32> instr,
    ch_uint<32> rs1_value,
    ch_uint<32> rs2_value,
    ch_bool& branch_taken
) {
    // 1. 指令解码
    ch_uint<3> funct3 = bits<14, 12>(instr);
    ch_bits<13> branch_imm = concat<13>(
        bits<31, 31>(instr),  // imm[12]
        bits<7, 7>(instr),    // imm[11]
        bits<30, 25>(instr),  // imm[10:5]
        bits<11, 8>(instr),   // imm[4:1]
        ch_bit(0)             // imm[0]
    );
    
    // 2. 比较操作
    BranchCompare cmp = compare(rs1_value, rs2_value);
    ch_bool condition = evaluate_branch_condition(funct3, cmp);
    
    // 3. 计算跳转目标
    ch_int<32> signed_offset = sign_extend<32, 13>(branch_imm);
    ch_uint<32> target_pc = pc + static_cast<ch_uint<32>>(signed_offset);
    
    // 4. 决定是否跳转
    branch_taken = condition;
    ch_uint<32> next_pc = select(condition, target_pc, pc + 4);
    
    return next_pc;
}
```

### 8.2 跳转执行单元

```cpp
// 跳转执行单元主函数
struct JumpResult {
    ch_uint<32> next_pc;
    ch_uint<32> link_addr;  // 写入 rd 的值 (PC+4)
    ch_bool write_link;     // 是否写入链接寄存器
};

JumpResult execute_jump(
    ch_uint<32> pc,
    ch_uint<32> instr,
    ch_uint<32> rs1_value,
    RegisterFile& regfile
) {
    JumpResult result;
    ch_uint<5> rd = bits<11, 7>(instr);
    ch_uint<3> opcode = bits<6, 0>(instr);
    
    // 链接地址 (JAL 和 JALR 都需要)
    result.link_addr = pc + 4;
    result.write_link = (rd != 0);
    
    if (opcode == 0b1101111) {  // JAL
        // J-type 立即数拼接
        ch_bits<21> jal_imm = concat<21>(
            bits<31, 31>(instr),  // imm[20]
            bits<19, 12>(instr),  // imm[19:12]
            bits<20, 20>(instr),  // imm[11]
            bits<30, 21>(instr),  // imm[10:1]
            ch_bit(0)             // imm[0]
        );
        
        ch_int<32> signed_offset = sign_extend<32, 21>(jal_imm);
        result.next_pc = pc + static_cast<ch_uint<32>>(signed_offset);
        
    } else if (opcode == 0b1100111) {  // JALR
        // I-type 立即数 (高 12 位)
        ch_bits<12> jalr_imm = bits<31, 20>(instr);
        
        result.next_pc = calculate_jalr_target(rs1_value, jalr_imm);
    }
    
    // 写入链接寄存器
    if (result.write_link) {
        regfile.write(rd, result.link_addr);
    }
    
    return result;
}
```

### 8.3 测试用例

```cpp
// 测试套件：分支指令
TEST(Branch, BEQ_Taken) {
    // rs1 = rs2 = 5, 应跳转
    ch_uint<32> pc = 0x1000;
    ch_uint<32> instr = 0x00500063;  // BEQ x0, x5, offset=0
    ch_bool taken;
    ch_uint<32> next_pc = execute_branch(pc, instr, 5, 5, taken);
    
    ASSERT_TRUE(taken);
    ASSERT_EQ(next_pc, 0x1000);  // offset=0, 跳回当前地址
}

TEST(Branch, BNE_Not_Taken) {
    // rs1 = rs2 = 3, 不应跳转
    ch_uint<32> pc = 0x2000;
    ch_uint<32> instr = 0x003001e3;  // BNE x0, x3, offset=4
    ch_bool taken;
    ch_uint<32> next_pc = execute_branch(pc, instr, 3, 3, taken);
    
    ASSERT_FALSE(taken);
    ASSERT_EQ(next_pc, 0x2004);  // PC+4
}

TEST(Branch, BLT_Negative) {
    // rs1 = -5 (0xFFFFFFFB), rs2 = 3, 应跳转
    ch_uint<32> pc = 0x3000;
    ch_uint<32> instr = 0x00301063;  // BLT x0, x3, offset=8
    ch_bool taken;
    ch_uint<32> next_pc = execute_branch(pc, instr, 0xFFFFFFFB, 3, taken);
    
    ASSERT_TRUE(taken);
    ASSERT_EQ(next_pc, 0x3008);
}

TEST(Branch, BLTU_Unsigned) {
    // rs1 = 0xFFFFFFFF (4294967295), rs2 = 1
    // 无符号：rs1 > rs2，不跳转
    ch_uint<32> pc = 0x4000;
    ch_uint<32> instr = 0x00106263;  // BLTU x0, x1, offset=16
    ch_bool taken;
    ch_uint<32> next_pc = execute_branch(pc, instr, 0xFFFFFFFF, 1, taken);
    
    ASSERT_FALSE(taken);
    ASSERT_EQ(next_pc, 0x4004);
}

// 测试套件：跳转指令
TEST(Jump, JAL_Forward) {
    ch_uint<32> pc = 0x5000;
    ch_uint<32> instr = 0x0000006F;  // JAL x0, offset=0
    RegisterFile regfile;
    
    JumpResult result = execute_jump(pc, instr, 0, regfile);
    
    ASSERT_EQ(result.next_pc, 0x5000);  // PC+0
    ASSERT_FALSE(result.write_link);    // rd=x0, 不写入
}

TEST(Jump, JALR_Align) {
    ch_uint<32> pc = 0x6000;
    ch_uint<32> instr = 0x00008067;  // JALR x0, x1, offset=0
    ch_uint<32> rs1 = 0x7001;  // 奇数地址
    RegisterFile regfile;
    
    JumpResult result = execute_jump(pc, instr, rs1, regfile);
    
    ASSERT_EQ(result.next_pc, 0x7000);  // 0x7001 & ~1 = 0x7000
}
```

---

## 9. 技能使用指南

### 9.1 何时使用本技能

**适用场景**:
- ✅ 实现 RISC-V RV32I 分支指令 (BEQ/BNE/BLT/BGE/BLTU/BGEU)
- ✅ 实现 RISC-V RV32I 跳转指令 (JAL/JALR)
- ✅ 调试分支偏移计算错误
- ✅ 审查分支指令测试用例覆盖率
- ✅ 复用 CppHDL 位操作模式到其他指令

**不适用场景**:
- ❌ RV64 扩展指令 (需要处理 64 位寄存器)
- ❌ 压缩指令 (C 扩展，16 位指令格式)
- ❌ 非 RISC-V 架构的分支实现

### 9.2 快速检查清单

在提交分支指令实现前，逐项检查：

```markdown
## 实现检查清单

- [ ] B-type 立即数拼接正确：`{imm[12], imm[11], imm[10:5], imm[4:1], 1'b0}`
- [ ] J-type 立即数拼接正确：`{imm[20], imm[19:12], imm[11], imm[10:1], 1'b0}`
- [ ] JALR 地址已与 `~1` (清零 bit 0)
- [ ] BLT/BLTU 区分有符号/无符号比较
- [ ] 符号扩展使用 `sign_extend<>()` 模板函数
- [ ] 分支条件判断覆盖全部 6 种 funct3 组合
- [ ] 测试用例包含边界值 (最大正偏移、最小负偏移)
- [ ] 测试用例包含符号位变化场景
```

---

## 10. 参考资料

- RISC-V Reader: RV32I Base Instruction Set
- RISC-V 手册：一本开源指令集的入门指南
- CppHDL 文档：位操作与条件选择 API
- 项目实现：`src/rv32i/decoder.cpp`, `src/rv32i/execute_branch.cpp`

---

## Changelog

| 版本 | 日期 | 变更 | 作者 |
|------|------|------|------|
| 1.0 | 2026-04-03 | 初始版本，完成 RV32I 分支跳转指令模式总结 | DevMate |

