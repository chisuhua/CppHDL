# Bundle 连接修复实施计划

**版本**: v1.0  
**创建日期**: 2026-04-08  
**修复类型**: 短期方案 (推荐)  
**预计总工时**: 8-10 小时  

---

## 1. 修复概述

### 1.1 修复范围

| 修改文件 | 修改内容 | 影响评估 |
|----------|---------|----------|
| `include/core/bundle/bundle_base.h` | 修改 connect 函数使用 set_src | 所有 Bundle 连接 |
| `tests/test_bundle_connection.cpp` | 更新测试用例 | 测试验证 |

### 1.2 技术要点

1. **使用 set_src()**: 符合 `ch_logic_out` 的正确连接模式
2. **C++20 fold expression**: 处理可变参数模板展开
3. **if constexpr**: 编译期判断字段类型，跳过嵌套 Bundle

---

## 2. 详细实施步骤

### 步骤 1: 修改 bundle_base::connect 函数

**目标文件**: `include/core/bundle/bundle_base.h:323-331`

**原始代码**:
```cpp
template <typename BundleT> void connect(BundleT &src, BundleT &dst) {
    static_assert(std::is_base_of_v<bundle_base<BundleT>, BundleT>,
                  "connect() only works with bundle_base-derived types");

    const auto &fields = src.__bundle_fields();
    std::apply([&](auto &&...f) { ((dst.*(f.ptr) = src.*(f.ptr)), ...); },
               fields);
}
```

**修改为**:
```cpp
template <typename BundleT> void connect(BundleT &src, BundleT &dst) {
    static_assert(std::is_base_of_v<bundle_base<BundleT>, BundleT>,
                  "connect() only works with bundle_base-derived types");

    const auto &fields = src.__bundle_fields();
    
    // 使用 fold expression + set_src 建立硬件连接
    std::apply([&](auto &&...f) {
        ([&, f_ptr = f.ptr]() {
            using FieldT = std::decay_t<decltype(src.(*f_ptr))>;
            if constexpr (!is_bundle_v<FieldT>) {
                auto* dst_impl = (dst.*f_ptr).impl();
                auto* src_impl = (src.*f_ptr).impl();
                if (dst_impl && src_impl) {
                    dst_impl->set_src(0, src_impl);
                }
            }
        }(), ...);
    }, fields);
}
```

**技术说明**:
- `f_ptr = f.ptr`: 捕获字段指针，避免 fold expression 语法问题
- `if constexpr (!is_bundle_v<FieldT>)`: 跳过嵌套 Bundle
- `dst_impl->set_src(0, src_impl)`: 正确建立硬件连接

---

### 步骤 2: 编译验证

```bash
cd /workspace/CppHDL/build
cmake --build . --target cpphdl -j4
```

**预期输出**:
```
[100%] Built target cpphdl
```

**失败处理**:
- C++20 fold expression 语法错误 → 使用捕获指针方式
- template argument 错误 → 添加 is_bundle_v 约束

---

### 步骤 3: 运行单元测试

```bash
cd /workspace/CppHDL/build
ctest -R test_bundle_connection --output-on-failure
```

**预期输出**:
```
test cases: 17 | 17 passed
assertions: 73 | 73 passed
```

**当前状态**: ❌ 失败 1 个测试
**目标状态**: ✅ 全部通过

---

### 步骤 4: 回归测试

```bash
cd /workspace/CppHDL/build
ctest --output-on-failure 2>&1 | tail -10
```

**通过标准**:
- 所有测试通过
- 无新增失败测试
- 性能无显著下降

---

## 3. 风险管理

### 3.1 已识别风险

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| C++20 fold expression 语法错误 | 高 | 高 | 使用捕获指针方式 |
| 嵌套 Bundle 处理错误 | 中 | 中 | if constexpr 编译期判断 |
| 编译时间增加 | 低 | 低 | 优化模板展开 |

### 3.2 回退方案

如果修复失败，回退到原始代码：
```bash
git checkout include/core/bundle/bundle_base.h
```

---

## 4. 验收标准

### 4.1 功能验收

- [ ] `test_bundle_connection` 所有测试通过
- [ ] 无新增编译警告
- [ ] 无运行时错误

### 4.2 代码质量验收

- [ ] 代码符合现有项目风格
- [ ] 无 AI slop（过度注释）
- [ ] 模板语法正确

### 4.3 文档验收

- [ ] 问题报告文档更新
- [ ] 实施记录文档完成

---

## 5. 时间规划

| 日期 | 时间段 | 任务 | 完成状态 |
|------|--------|------|----------|
| 2026-04-08 | AM | 文档分析 | ✅ 完成 |
| 2026-04-08 | PM | 代码修改 + 编译 | ⏳ 进行中 |
| 2026-04-09 | AM | 测试验证 | ⏳ 待处理 |
| 2026-04-09 | PM | 回归测试 | ⏳ 待处理 |

---

## 6. 相关变更

### 6.1 关联 Commit

- `102b7c7`: 启用 thread_local 修饰符
- `352ccc4`: 测试文件修复
- `6dcbc6b`: 测试格式修复
- `35c1a27`: 多线程测试修正

### 6.2 依赖关系

本修复依赖以下现有功能：
- `set_src()` 方法 (已实现)
- `is_bundle_v` trait (已实现)
- C++20 fold expression (编译器支持)

---

## 7. 审批记录

| 角色 | 姓名 | 日期 | 意见 | 签名 |
|------|------|------|------|------|
| 技术负责人 | | | | |
| 项目经理 | | | | |
| 质量保证 | | | | |

---

## 附录 A: 术语表

| 术语 | 说明 |
|------|------|
| Bundle | CppHDL 的数据打包结构，类似 SpinalHDL 的 Bundle |
| set_src() | lnodeimpl 方法，设置节点的驱动源 |
| fold expression | C++17 引入的可变参数模板展开语法 |
| if constexpr | C++17 编译期条件判断 |

---

**文档维护人**: AI Agent  
**下次更新**: 修复完成或长期重构启动时
