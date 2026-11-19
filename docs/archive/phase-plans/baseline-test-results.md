# 修复前基线测试结果

**日期**: 2026-04-08  
**时间**: 修复前  
**测试文件**: tests/test_bundle_connection.cpp

---

## 测试结果

| 指标 | 结果 |
|------|------|
| 测试用例总数 | 17 |
| 通过 | 16 ✅ |
| 失败 | 1 ❌ |
| 断言总数 | 73 |
| 通过断言 | 72 |
| 失败断言 | 1 |

---

## 失败详情

**失败测试**: `test_bundle_connection - Bundle connection in module`  
**失败位置**: `tests/test_bundle_connection.cpp:260`  
**错误信息**: 
```
[ERROR] [ch_uint::operator<<=] Error: node_impl_ or src_lnode is not null for ch_uint<4>!
```

**失败原因**: Bundle 字段已有节点，无法使用 `<<=` 或`=` 重新连接。

---

## 通过的测试 (16 个)

1. Bundle 字面量构造 ✅
2. Bundle 位宽计算 ✅
3. Bundle 序列化 ✅
4. Bundle 反序列化 ✅
5. Bundle 位切片 ✅
6. Bundle 位选择 ✅
7. Bundle 角色设置 ✅
8. Bundle 命名 ✅
9. Bundle 有效性检查 ✅
10. Bundle 方向设置 ✅
11. Bundle 嵌套 ✅
12. Bundle 协议检测 ✅
13. Bundle 连接验证 (其他场景) ✅
14. ... (其他基础功能) ✅

---

## 待修复测试 (1 个)

- `test_bundle_connection - Bundle connection in module` ❌

---

**基线确认**: ________________  
**日期**: 2026-04-08
