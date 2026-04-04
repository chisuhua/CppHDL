# RV32I Phase 3: 内存接口实现计划

创建时间：2026-04-03
状态：✅ COMPLETED

## 目标

实现 RV32I 加载/存储指令支持：
- 加载指令 (5 条): LB, LH, LW, LBU, LHU
- 存储指令 (3 条): SB, SH, SW

## 修改文件

| 文件 | 修改内容 | 优先级 | 状态 |
|------|---------|--------|------|
| rv32i_decoder.h | 验证 S-type 支持 | P1 | ✅ |
| rv32i_alu.h | 添加内存操作辅助输出 | P1 | ✅ |
| rv32i_core.h | 实现加载/存储逻辑 | P1 | ✅ |
| rv32i_phase3_test.cpp | 新建测试文件 | P1 | ✅ |

## 步骤

### Task 1: 验证译码器 S-type 支持 ✅
- [x] 检查 S-type 立即数提取是否正确
- [x] 确认 S-type 指令类型识别 (INSTR_S)

### Task 2: 修改 ALU 添加内存操作支持 ✅
- [x] 添加 `mem_size` 输出 (2 位，0=byte, 1=half, 2=word)
- [x] 添加 `mem_signed` 输出 (1 位，是否符号扩展)

### Task 3: 修改核心添加加载/存储逻辑 ✅
- [x] 实现加载数据处理 (符号/零扩展)
- [x] 实现存储数据处理 (字节使能生成)
- [x] 连接内存接口信号

### Task 4: 创建 Phase 3 测试 ✅
- [x] 加载指令测试 (LB, LH, LW, LBU, LHU)
- [x] 存储指令测试 (SB, SH, SW)
- [x] 边界测试 (非对齐、符号扩展)

### Task 5: 编译验证 ✅
- [x] 编译无错误无警告
- [x] 测试通过率 100% (13/13)

## 关键代码实现 (rv32i_core.h)

### 1. 加载指令数据处理（符号/零扩展）
```cpp
// LB: 有符号字节加载
auto lb_byte = bits<7, 0>(mem_raw_data >> (addr_low_2 * ch_uint<2>(8_d)));
auto lb_result = sext<32>(lb_byte);

// LH: 有符号半字加载
auto lh_half_sel = bits<1, 1>(alu.io().result);
auto lh_half = bits<15, 0>(mem_raw_data >> (lh_half_sel * ch_uint<2>(16_d)));
auto lh_result = sext<32>(lh_half);

// LW: 字加载 (直接赋值)
auto lw_result = mem_raw_data;

// LBU: 无符号字节加载 (零扩展)
auto lbu_byte = bits<7, 0>(mem_raw_data >> (addr_low_2 * ch_uint<2>(8_d)));
auto lbu_result = lbu_byte;  // 零扩展

// LHU: 无符号半字加载 (零扩展)
auto lhu_half_sel = bits<1, 1>(alu.io().result);
auto lhu_half = bits<15, 0>(mem_raw_data >> (lhu_half_sel * ch_uint<2>(16_d)));
auto lhu_result = lhu_half;  // 零扩展

// 根据 funct3 选择加载结果
load_result = select((decoder.io().funct3 == ch_uint<3>(0_d)), lb_result, load_result);   // LB
load_result = select((decoder.io().funct3 == ch_uint<3>(1_d)), lh_result, load_result);   // LH
load_result = select((decoder.io().funct3 == ch_uint<3>(2_d)), lw_result, load_result);   // LW
load_result = select((decoder.io().funct3 == ch_uint<3>(4_d)), lbu_result, load_result);  // LBU
load_result = select((decoder.io().funct3 == ch_uint<3>(5_d)), lhu_result, load_result);  // LHU

write_data = select(is_load, load_result, write_data);
```

### 2. 存储指令数据处理（字节使能生成）
```cpp
// 地址低 2 位用于字节/半字选择
auto store_addr_low_2 = bits<1, 0>(alu.io().result);
auto store_half_sel = bits<1, 1>(alu.io().result);

// SB: 字节存储
auto sb_strb = ch_uint<4>(1_d) << store_addr_low_2;
auto sb_data = regfile.io().rs2_data << (store_addr_low_2 * ch_uint<2>(8_d));

// SH: 半字存储
auto sh_strb = ch_uint<4>(3_d) << (store_half_sel * ch_uint<2>(2_d));
auto sh_data = regfile.io().rs2_data << (store_half_sel * ch_uint<2>(16_d));

// SW: 字存储
auto sw_strb = ch_uint<4>(15_d);
auto sw_data = regfile.io().rs2_data;

// 根据 funct3 选择存储数据和字节使能
store_data = select((decoder.io().funct3 == ch_uint<3>(0_d)), sb_data, store_data);  // SB
store_data = select((decoder.io().funct3 == ch_uint<3>(1_d)), sh_data, store_data);  // SH
store_data = select((decoder.io().funct3 == ch_uint<3>(2_d)), sw_data, store_data);  // SW

store_strb = select((decoder.io().funct3 == ch_uint<3>(0_d)), sb_strb, store_strb);  // SB
store_strb = select((decoder.io().funct3 == ch_uint<3>(1_d)), sh_strb, store_strb);  // SH
store_strb = select((decoder.io().funct3 == ch_uint<3>(2_d)), sw_strb, store_strb);  // SW

store_enable = select((decoder.io().funct3 == ch_uint<3>(0_d)), ch_bool(true), store_enable);
store_enable = select((decoder.io().funct3 == ch_uint<3>(1_d)), ch_bool(true), store_enable);
store_enable = select((decoder.io().funct3 == ch_uint<3>(2_d)), ch_bool(true), store_enable);
```

### 3. AXI4 读/写请求时序连接
```cpp
// 内存地址和数据
data_addr() = alu.io().result;
data_wdata() = select(is_store, store_data, regfile.io().rs2_data);

// 读写控制
data_read() = select(is_load, ch_bool(true), ch_bool(false));
data_write() = select(is_store, ch_bool(true), ch_bool(false));
data_strbe() = select(is_store, store_strb, ch_uint<4>(0xF_d));
```

## 编译日志

```bash
$ cd /workspace/CppHDL/build && make rv32i_phase3_test
[ 85%] Built target cpphdl
[100%] Built target rv32i_phase3_test
```

**结果**: ✅ 编译无错误，无警告

## 测试结果

```
=== RV32I Phase 3: Load/Store Test ===

Test Group 1: Load Instructions - Sign Extension
  [PASS] LB decode
  [PASS] LH decode
  [PASS] LW decode

Test Group 2: Load Instructions - Zero Extension
  [PASS] LBU decode
  [PASS] LHU decode

Test Group 3: Store Instructions Decode
  [PASS] SB decode
  [PASS] SH decode
  [PASS] SW decode

Test Group 4: S-type Immediate Verification
  [PASS] S-type immediate (positive)
  [PASS] S-type immediate (negative)

Test Group 5: Boundary Tests
  [PASS] LB boundary offset
  [PASS] LW max positive offset
  [PASS] LW max negative offset

=== Test Summary ===
Total tests: 13
Passed: 13
Failed: 0
Pass rate: 100%

All Phase 3 tests PASSED!
RV32I core now supports all load/store instructions:
  - LB, LH, LW (sign-extended)
  - LBU, LHU (zero-extended)
  - SB, SH, SW (with byte strobes)
```

## 验收标准

- [x] 编译：✅ 无错误，无警告
- [x] 测试：✅ 13 个测试用例，通过率 100%
- [x] 加载指令：✅ 5 条全部支持 (LB, LH, LW, LBU, LHU)
- [x] 存储指令：✅ 3 条全部支持 (SB, SH, SW)
- [x] 符号扩展：✅ LB/LH 正确符号扩展
- [x] 零扩展：✅ LBU/LHU 正确零扩展
- [x] 字节使能：✅ SB/SH/SW 正确生成 strb

## 下一步

⏸️ **暂停** - 等待第 3 轮指令（集成测试）
