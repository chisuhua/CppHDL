# 集成测试计划：Forwarding + Hazard

**日期**: 2026-04-10  
**优先级**: P0  
**目标**: 验证 Forwarding 和 Hazard 单元协同工作

---

## 📋 测试场景

### 场景 1: ADD→ADD 数据前推
```
ADD x1, x0, 1    # x1 = 1
ADD x2, x1, 1    # x2 = x1 + 1 (需要 EX→EX 前推)
```
**验证点**: Forwarding 单元正确选择 EX 级结果

### 场景 2: LOAD→ADD Load-Use 冒险
```
LW x1, 0(x0)     # 加载内存
ADD x2, x1, 1    # 需要使用加载的数据 (需要 Stall)
```
**验证点**: Hazard 单元检测并插入 Stall

### 场景 3: 连续指令流
```
ADD x1, x0, 1
ADD x2, x1, 2
ADD x3, x2, 3
ADD x4, x3, 4
```
**验证点**: 多级前推正确工作

---

## ✅ 验收标准

- [ ] EX→EX 前推功能正确
- [ ] MEM→EX 前推功能正确
- [ ] Load-Use 冒险检测正确
- [ ] Stall 插入逻辑正确
- [ ] 测试 100% 通过

---

**状态**: 🟡 待执行
