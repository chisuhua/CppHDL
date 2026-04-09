# Cache 功能完善实施计划

**日期**: 2026-04-10  
**优先级**: P0  
**工时**: 6 小时  

---

## 📋 任务分解

### I-Cache (3h)
- [ ] SRAM 阵列：128 sets × 2 ways × 128-bit
- [ ] Tag 比较逻辑：21-bit tag 匹配
- [ ] LRU 替换：1-bit per set
- [ ] Hit/Miss 检测

### D-Cache (3h)
- [ ] Data SRAM：128 sets × 2 ways × 128-bit
- [ ] Tag/Valid/Dirty 位
- [ ] Write-Through 缓冲
- [ ] AXI 握手逻辑

---

## 🎯 验收标准

| 指标 | 目标 |
|------|------|
| Cache 容量 | 4KB |
| 组相联 | 2-way |
| 行大小 | 16 字节 |
| 替换策略 | LRU |
| 写策略 | Write-Through (D-Cache) |

---

**状态**: 🟡 待执行
