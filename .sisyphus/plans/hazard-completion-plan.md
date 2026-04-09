# Hazard Unit 完善计划

**日期**: 2026-04-10  
**优先级**: P0  
**工时**: 4 小时  

---

## 📋 任务分解

### Forwarding 完善 (2h)
- [ ] EX→EX 前推（ALU 结果）
- [ ] MEM→EX 前推（MEM 级 ALU 结果）
- [ ] WB→EX 前推（WB 级数据）
- [ ] 优先级逻辑：EX > MEM > WB > REG
- [ ] x0 寄存器保护（永不匹配）

### Load-Use 检测 (1h)
- [ ] Load 指令识别（MEM 级）
- [ ] 寄存器相关检查
- [ ] Stall 信号生成

### Branch Flush (1h)
- [ ] 分支指令识别（EX 级）
- [ ] 分支结果预测
- [ ] Flush 信号生成

---

## 🎯 验收标准

| 功能 | 验收标准 |
|------|---------|
| Forwarding | 3 路前推优先级正确 |
| Load-Use | 检测到冒险时 stall=1 |
| Branch | 分支预测错误时 flush=1 |
| 测试 | 100% 通过 |

---

**状态**: 🟡 待执行
