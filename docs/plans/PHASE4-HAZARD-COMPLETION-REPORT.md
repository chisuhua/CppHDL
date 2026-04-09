# Phase 4: Hazard Unit 完整实现报告

**日期**: 2026-04-10  
**任务**: Hazard Unit 功能完善  
**状态**: ✅ 完成  

---

## 📊 执行摘要

成功实现完整的冒险检测与前推单元，包括 3 路数据前推、Load-Use 冒险检测和 Branch Flush 逻辑。

---

## 📁 交付清单

| 模块 | 文件 | 行数 | 功能 |
|------|------|------|------|
| Hazard Unit | `rv32i_hazard_complete.h` | ~260 | 完整冒险检测 |
| 测试 | `test_hazard_complete.cpp` | ~90 | 7 个测试用例 |
| 计划 | `hazard-completion-plan.md` | ~40 | 完善计划 |

---

## ✅ 功能实现

### Forwarding 单元
| 特性 | 实现 |
|------|------|
| EX→EX 前推 | ✅ |
| MEM→EX 前推 | ✅ |
| WB→EX 前推 | ✅ |
| 优先级逻辑 | ✅ EX > MEM > WB > REG |
| x0 保护 | ✅ (永不匹配) |
| MUX 选择 | ✅ (2-bit src 选择) |

### Load-Use 检测
| 特性 | 实现 |
|------|------|
| Load 指令识别 | ✅ (MEM 级) |
| 寄存器相关检查 | ✅ (RS1/RS2) |
| Stall 信号生成 | ✅ |
| 流水线停顿 | ✅ |

### Branch Flush
| 特性 | 实现 |
|------|------|
| 分支指令识别 | ✅ (EX 级) |
| Flush IF/ID | ✅ |
| PC 源选择 | ✅ |

---

## 🔧 技术细节

### Forwarding 优先级
```cpp
// 源选择：EX(1) > MEM(2) > WB(3) > REG(0)
auto rs1_src = 
    select(rs1_match_ex, 1_d,
        select(rs1_match_mem, 2_d,
            select(rs1_match_wb, 3_d, 0_d)));
```

### Load-Use 检测
```cpp
ch_bool load_use_hazard = 
    io().mem_is_load && 
    (mem_rd_addr == id_rs1_addr || mem_rd_addr == id_rs2_addr);
```

### Branch Flush
```cpp
io().flush <<= io().ex_branch;
io().pc_src <<= io().ex_branch;
```

---

## 🎯 验收标准

| 验收项 | 状态 |
|--------|------|
| Forwarding 3 路前推 | ✅ |
| 优先级逻辑正确 | ✅ |
| Load-Use 检测 | ✅ |
| Branch Flush | ✅ |
| x0 保护 | ✅ |
| 测试用例 | ✅ |

---

## ⏭️ 待完善功能

| 功能 | 说明 |
|------|------|
| 精确 Branch 预测 | 当前简化为 flush |
| 动态 Branch 预测 | 需结合 BHT/BTB |
| Hazard 计数器 | 性能统计 |

---

**提交**: 已推送到 main 分支  
**下一步**: Phase 4 总结 / 功能完善
