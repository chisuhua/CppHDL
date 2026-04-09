# Week 1 执行计划 - Phase 3 提交与关键修复

**创建日期**: 2026-04-09  
**执行人**: DevMate  
**预计工期**: 1 天  
**目标**: 完成 Phase 3 提交 + 关键 Bug 修复 + 技术债务清理

---

## 📋 Task 清单

### Task 1: 提交 Phase 3 成果 (优先级 🔴 高)

**目标**: 将 20 个未提交 commits 推送到远程

**步骤**:
1. 检查当前 git 状态和待提交文件
2. 整理提交历史，确保提交信息清晰
3. 分批提交（按模块分组）
4. 推送到远程

**提交分组**:
- **提交 A**: AXI4 组件 (axi4_full.h, axi_interconnect_4x4.h, axi_to_axilite.h)
- **提交 B**: RISC-V 组件 (rv32i_core.h, rv32i_tcm.h, rv32i_axi_interface.h)
- **提交 C**: SoC 集成 (cpphdl_soc.cpp, hello_world.cpp)
- **提交 D**: 技术债务修复 (位移 UB 修复 + converter.h)
- **提交 E**: 文档整理 (技能文档 + 规划文档)

**验收标准**:
- [ ] 所有新增文件已提交
- [ ] git push 成功
- [ ] 提交历史清晰可读

---

### Task 2: 修复 test_stream_arbiter SEGFAULT (优先级 🔴 高)

**目标**: 定位并修复导致 SEGFAULT 的根本原因

**步骤**:
1. 运行失败测试，获取详细错误信息
2. 使用 ASan/Valgrind 定位内存问题
3. 阅读相关源代码 (stream_arbiter.h)
4. 定位问题根源
5. 最小化修复
6. 验证修复后测试通过

**相关文件**:
- `tests/test_stream_arbiter.cpp` - 失败测试
- `include/chlib/stream_arbiter.h` - 被测模块 (待确认位置)

**验收标准**:
- [ ] 测试通过或明确失败原因
- [ ] 无 SEGFAULT
- [ ] 无内存泄漏

---

### Task 3: 位移 UB 全面审计 (优先级 🟡 中)

**目标**: 清理所有 `1 << N` 未定义行为风险

**步骤**:
1. 搜索所有 `1 <<` 位移操作
2. 评估风险等级 (N >= 32 为高危)
3. 分批修复
4. 添加 `-Wshift-overflow=2` 警告

**目标文件**:
- `include/chlib/fifo.h` - 6 处风险点
- `include/chlib/axi4lite.h` - 1 处风险点 (行 116)
- 其他潜在风险文件

**修复模式**:
```cpp
// ❌ 错误
1 << N

// ✅ 正确
static_cast<uint64_t>(1) << N
// 或
1ULL << N
```

**验收标准**:
- [ ] 所有 `1 << N` 已修复
- [ ] 编译无 shift-overflow 警告
- [ ] 测试通过

---

### Task 4: 创建技能文档 (优先级 🟡 中)

**目标**: 将位移修复经验固化为技能

**文件**: `skills/cpphdl-shift-fix/SKILL.md`

**内容**:
- 问题根因分析
- 修复方案
- 执行流程
- 检查清单

**验收标准**:
- [ ] 技能文档创建完成
- [ ] 包含完整检查清单

---

## 📅 执行顺序

```
1. Task 1: Git 提交 (30min)
   ↓
2. Task 2: 修复 SEGFAULT (2h)
   ↓
3. Task 3: 位移 UB 审计 (1h)
   ↓
4. Task 4: 技能文档 (30min)
   ↓
5. 验证：运行全量测试 (30min)
   ↓
6. 审查：代码审查 + 提交
```

---

## ✅ 完成标准

- [ ] 所有 Task 完成
- [ ] 全量测试通过 (63/63)
- [ ] 代码已提交到远程
- [ ] 技术债务已清理
- [ ] 文档已更新

---

**执行原则**:
1. **每个 Task 完成后立即验证**
2. **保持测试基线正确**
3. **不积累技术债务**
4. **及时审查代码质量**
