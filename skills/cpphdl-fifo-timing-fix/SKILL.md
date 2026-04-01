# CppHDL FIFO 时序逻辑缺陷修复技能

## 触发条件

当 CppHDL 项目出现以下问题时激活：
- FIFO 数据无法正确写入
- 写使能条件判断行为异常
- `sync_fifo` 示例仿真结果与预期不符
- 编译或运行时发现 `.acf/status/arch-issues-found.md` 中记录的问题

## 问题根因

### 核心问题 (Issue #1)

在 `sync_fifo` 函数中，写使能逻辑存在**时序缺陷**：

```cpp
// ❌ 错误代码模式
write_enable = wren && !is_full(count);
```

**问题分析**:
- `count` 是**寄存器**，存储的是**上一周期的值**
- 写使能 `write_enable` 是**组合逻辑**，基于 `count` 的当前值（旧值）计算
- 导致写入判断基于过时的计数值
- **实际表现**: 数据无法正确写入 FIFO，或写入时机错误

### 时序图说明

```
时钟周期：   T0      T1      T2      T3
wren:       ────┐   ┌───────┐   ┌────
                │   │       │   │
count (旧):  [10]───[10]───[11]───[11]
                ↑           ↑
                │           └── 此时 count 已更新为 11
                └── 写使能判断时 count 还是 10

is_full:     [false]─[false]─[false]─[true]
                ↑
                └── 基于旧值判断，可能错误允许写入
```

## 修复方案

### 方案 1: 改为时序逻辑（推荐）

将写使能逻辑从组合逻辑改为时序逻辑：

```cpp
// ✅ 正确代码模式
// 在 describe() 中使用时序逻辑
ch_always_ff(clk) {
    if (reset) {
        write_enable <= false;
    } else {
        write_enable <= wren && !is_full(next_count);  // 使用下一周期值
    }
}
```

### 方案 2: 在 describe() 中明确时序

```cpp
// ✅ 正确代码模式
auto fifo = sync_fifo(...);

// 在模块描述中明确写使能的时序关系
describe([=] {
    ch_always_ff(clk) {
        if (wren && !fifo.is_full()) {
            fifo.write(data);
        }
    }
});
```

### 方案 3: 临时规避（使用 ch_mem）

如果 `sync_fifo` 问题无法立即修复，使用 `ch_mem` 直接实现：

```cpp
// ✅ 临时规避方案
ch_mem<ch_uint<32>, 16> memory("fifo_memory");
ch_uint<4> head("head"), tail("tail"), count("count");

ch_always_ff(clk) {
    if (reset) {
        count = 0;
        head = 0;
        tail = 0;
    } else {
        if (wren && count < 16) {
            memory.swrite(tail) <<= data;
            tail++;
            count++;
        }
    }
}
```

## 执行流程

### 1. 定位问题文件

```bash
cd /workspace/CppHDL

# 查找 sync_fifo 使用
grep -rn "sync_fifo" examples/ include/chlib/

# 查找 FIFO 相关实现
grep -rn "write_enable" include/chlib/fifo.h
```

### 2. 检查架构问题记录

```bash
cat /workspace/CppHDL/.acf/status/arch-issues-found.md
```

### 3. 应用修复

根据具体场景选择修复方案：
- **新设计**: 方案 1（时序逻辑）
- **现有设计修复**: 方案 2（明确时序）
- **紧急规避**: 方案 3（ch_mem 直接实现）

### 4. 验证修复

```bash
cd /workspace/CppHDL/build

# 重新编译
cmake .. && make -j$(nproc)

# 运行 FIFO 相关测试
ctest -R fifo -V

# 运行仿真示例
./examples/fifo_test
```

## 检查清单

修复后必须验证：
- [ ] 写使能逻辑在时序块中（`ch_always_ff`）
- [ ] 计数值使用正确的周期值
- [ ] 边界条件测试（空 FIFO、满 FIFO）
- [ ] 连续写入测试
- [ ] 读写同时发生测试
- [ ] 仿真波形与预期一致

## 相关架构问题

### Issue #1: sync_fifo 写使能逻辑缺陷
- **严重级别**: 🔴 高
- **影响范围**: 所有使用 `sync_fifo` 的设计
- **状态**: 🟡 分析中

### Issue #2: ch_mem 读端口 API 复杂
- **严重级别**: 🟡 中
- **建议改进**: 简化内存读 API

## 预防措施

### 设计审查清单

在审查 FIFO 相关设计时，强制检查：
```markdown
## FIFO 时序审查清单

- [ ] 写使能是否在时序逻辑中？
- [ ] 读使能是否在时序逻辑中？
- [ ] 空/满标志是否基于正确的计数值？
- [ ] 格雷码计数器用于跨时钟域？
- [ ] 有边界条件测试？
```

### 编码规范

在 `CppHDL` 项目中添加 FIFO 编码规范：
```markdown
## FIFO 编码规范

1. **时序优先**: FIFO 控制逻辑必须使用时序逻辑
   - ✅ `ch_always_ff(clk) { ... }`
   - ❌ 组合逻辑中使用寄存器值

2. **明确周期**: 明确区分当前周期和下一周期值
   - 使用 `next_count` 而非 `count` 进行判断

3. **边界测试**: 必须测试空、满、连续读写场景
```

## 测试用例模板

```cpp
// FIFO 边界条件测试
CH_TEST(fifo_boundary_test) {
    ch_context ctx;
    
    // 创建 FIFO
    auto fifo = sync_fifo<ch_uint<32>, 4>("test_fifo");
    
    // 测试 1: 空 FIFO 读取
    // ...
    
    // 测试 2: 满 FIFO 写入
    // ...
    
    // 测试 3: 连续读写
    // ...
    
    // 测试 4: 写使能时序验证
    // ...
}
```

## 相关技能

- `cpphdl-shift-fix`: 位移操作 UB 修复
- `cpphdl-mem-init-dataflow`: 内存初始化数据流
- `cpphdl-cdc-check`: 时钟域交叉检查

## 历史案例

### 2026-03-30: FIFO 示例移植失败

**问题现象**:
- `sync_fifo` 示例仿真时数据无法写入
- 波形显示写使能始终为 false

**根本原因**:
```cpp
// 在 include/chlib/fifo.h 中
write_enable = wren && !is_full(count);
// count 是寄存器，组合逻辑中是旧值
```

**修复方案**:
- 临时规避：使用 `ch_mem` 直接实现 FIFO
- 长期修复：重构 `sync_fifo` 为时序逻辑

**验证结果**: ✅ 临时规避成功，长期修复待完成

---

**技能版本**: v1.0  
**创建时间**: 2026-04-01  
**适用项目**: CppHDL, 及其他使用 CppHDL 框架的硬件设计项目  
**参考文档**: `.acf/status/arch-issues-found.md`
