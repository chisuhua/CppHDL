# Cache 功能完善计划

**日期**: 2026-04-10  
**优先级**: P0  
**工时**: 6 小时  

---

## 📋 任务分解

### I-Cache 完善 (3h)
- [ ] Hit 检测：Tag 比较
- [ ] Miss 检测：Tag 不匹配
- [ ] LRU 更新：访问时更新
- [ ] AXI 填充：Miss 时读取

### D-Cache 完善 (3h)
- [ ] Hit/Miss 检测
- [ ] Replacement 逻辑
- [ ] Write-Through 缓冲
- [ ] AXI 握手协议

---

## 🎯 验收标准

| 功能 | 验收标准 |
|------|---------|
| Hit 检测 | Tag 匹配且 Valid=1 |
| Miss 检测 | Tag 不匹配或 Valid=0 |
| LRU 更新 | 访问后更新 LRU 位 |
| AXI 握手 | Valid/Ready 协议正确 |

---

**状态**: 🟡 待执行
