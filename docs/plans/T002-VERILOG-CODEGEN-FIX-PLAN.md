# T002: Verilog 代码生成器改进计划

**任务编号**: PHASE4-T002
**创建日期**: 2026-04-09
**预计工期**: 4-6 小时
**状态**: 🟡 分析已完成，准备实施

---

## 🎯 问题描述

### 当前症状

**生成的 Verilog（有问题）**:
```verilog
module top (
    input [31:0] top_io,
    input [1:0] top_io_1,
    input  top_io_2,
    output  top_io_3,
    // ❌ 所有信号都被命名为 top_io_N
);
```

**期望的 Verilog**:
```verilog
module top (
    input [31:0] awaddr,
    input [1:0] awprot,
    output i2c_sda,
    output i2c_scl,
    // ✅ 保留原始字段命名
);
```

---

## 🔍 根因分析

### 问题链路

```
Bundle 定义 → Component IO → as_slave()/as_master() → 位切片节点创建 → Verilog 生成
                     ↓
            节点名称丢失原始字段名
```

### 具体原因

1. **Bundle 字段命名**：在定义时正确设置（如 `awaddr`, `i2c_sda`）
2. **as_slave()/as_master()调用**：创建位切片节点（`bits_op`）
3. **新节点名称问题**：位切片节点名称为通用名（`top_io`, `top_io_1`）
4. **Verilog 生成器**：使用节点名称生成端口名，导致名称丢失

---

## 💡 修复方案

### 方案 A：保留位切片的字段名称（推荐）

**修改位置**: `include/core/bundle/bundle_base.h`

**修改内容**: 在 `as_slave()`/`as_master()` 创建位切片节点时，保留原始字段名称。

```cpp
// 当前代码（问题所在）
field = field.slice(bits_op::create(size, offset));

// 修复后代码
auto sliced = field.slice(bits_op::create(size, offset));
sliced->set_name(field.name());  // 保留原始字段名称
field = sliced;
```

**优点**:
- ✅ 最小改动
- ✅ 保持语义清晰
- ✅ 不影响现有功能

**预计工时**: 2-3h

---

### 方案 B：Verilog 生成器特殊处理

**修改位置**: `include/codegen_verilog.h`, `src/codegen_verilog.cpp`

**修改内容**: 在生成端口名称时，查找节点的原始 Bundle 字段名称。

```cpp
std::string verilogwriter::get_port_name(lnodeimpl* node) {
    // 如果是位切片节点，尝试获取原始字段名称
    if (node->type() == lnodetype::type_bits) {
        auto* src = node->src(0);
        if (src && src->has_custom_name()) {
            return sanitize_name(src->name());
        }
    }
    return sanitize_name(node->name());
}
```

**优点**:
- ✅ 不修改核心逻辑
- ✅ 仅影响 Verilog 生成

**缺点**:
- ⚠️ 逻辑较复杂
- ⚠️ 需要追踪原始字段

**预计工时**: 3-4h

---

### 方案 C：组合方案（最完整）

结合方案 A 和方案 B：
- 方案 A：确保新创建的位切片节点保留名称
- 方案 B：添加备用逻辑处理遗留问题

**预计工时**: 4-6h

---

## 📋 实施计划（选择方案 C）

### 阶段 1: Bundle 位切片命名修复（2h）

**文件**: `include/core/bundle/bundle_base.h`

**步骤**:
1. 定位 `as_slave()` 和 `as_master()` 方法
2. 找到位切片创建代码
3. 添加 `set_name()` 调用，保留字段原始名称
4. 编译验证

**验收**:
- [ ] 代码修改完成
- [ ] 编译通过
- [ ] 现有测试通过

---

### 阶段 2: Verilog 生成器增强（2h）

**文件**: `src/codegen_verilog.cpp`

**步骤**:
1. 添加 `get_port_name()` 辅助函数
2. 处理位切片节点的备用命名逻辑
3. 更新端口打印代码使用新函数
4. 编译验证

**验收**:
- [ ] 代码修改完成
- [ ] 编译通过
- [ ] 现有测试通过

---

### 阶段 3: 测试验证（1-2h）

**步骤**:
1. 重新生成 I2C Verilog
2. 验证端口命名正确
3. 运行 `test_axi_i2c` 测试
4. 运行所有 AXI 测试确保无回归

**验收**:
- [ ] `test_axi_i2c` Verilog 生成测试通过
- [ ] 所有 AXI 测试通过
- [ ] 生成的 Verilog 可综合

---

## ✅ 验收标准

### 功能验收

| 测试 | 期望结果 | 状态 |
|------|---------|------|
| `test_axi_i2c` Verilog 生成 | 端口命名正确 | ⏳ 待验证 |
| `test_axi_spi` Verilog 生成 | 端口命名正确 | ⏳ 待验证 |
| `test_axi_pwm` Verilog 生成 | 端口命名正确 | ⏳ 待验证 |
| Bundle 模块 Verilog 生成 | 端口命名正确 | ⏳ 待验证 |

### 质量验收

| 指标 | 目标值 | 状态 |
|------|-------|------|
| 编译错误 | 0 | ⏳ 待验证 |
| 测试回归 | 0 | ⏳ 待验证 |
| 生成的 Verilog 可综合 | 100% | ⏳ 待验证 |

---

## 📊 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 位切片命名冲突 | 低 | 中 | 使用 sanitize_name() 确保合法 |
| 现有测试失败 | 中 | 高 | 保留备份，可快速回滚 |
| Verilog 生成退化 | 低 | 中 | 逐项验证所有生成测试 |

---

## 🔗 参考资源

### 相关文件

| 文件 | 说明 |
|------|------|
| `include/core/bundle/bundle_base.h` | Bundle 基础实现 |
| `include/codegen_verilog.h` | Verilog 生成器头文件 |
| `src/codegen_verilog.cpp` | Verilog 生成器实现 |
| `tests/test_axi_i2c.cpp` | I2C 测试及 Verilog 验证 |

### 相关文档

| 文档 | 说明 |
|------|------|
| `docs/tech/T401-TECHNICAL-DOC.md` | Simulator API 文档 |
| `usage_guide/06-bundle-patterns.md` | Bundle 使用模式 |

---

## 📝 实施检查清单

### 准备阶段

- [x] 问题分析完成
- [x] 修复方案确定
- [x] 实施计划制定

### 实施阶段

- [ ] Bundle 位切片命名修复
- [ ] Verilog 生成器增强
- [ ] 编译验证
- [ ] 测试验证

### 收尾阶段

- [ ] 更新文档
- [ ] 代码审查
- [ ] 提交代码
- [ ] 创建完成报告

---

**创建人**: AI Agent  
**创建日期**: 2026-04-09  
**状态**: 🟡 准备实施  
**预计完成**: 2026-04-09（当天）
