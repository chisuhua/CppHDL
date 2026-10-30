# Design: `multi_if::elif` 的条件端口化

## 1. 缺陷形态（实测）

```cpp
io().out = if_then<ch_uint<8>>(io().is_zero, ch_uint<8>(0_d))
               .elif(io().is_max, ch_uint<8>(255_d))   // ← 编译不过
               .else_(ch_uint<8>(1_d));
```

```
error: no matching function for call to
  'chlib::multi_if<ch::core::ch_uint<8> >::elif(
     ch::core::ch_in<ch::core::ch_bool>&, ch::core::ch_uint<8>)'
```

⇒ ⇒ `multi_if::elif` 的形参是 `const ch_bool &` ⇒ ⇒ 端口**绑不上**。

## 2. 为什么 A3 漏了它

A3（E3）修的是 **`if_then` 的首分支条件** ⇒ ⇒ `if_then` 内部先归一化
（`ch_bool cond(to_operand(condition).impl());`）⇒ ⇒ 再传给 `multi_if` 的 ctor
⇒ ⇒ **首分支**的条件是端口**没问题**。

⇒ ⇒ 但 **`elif`** 是**另一个**公开入口 ⇒ ⇒ 它**自己**的形参没人改
⇒ ⇒ ⇒ 「`if_then` 的条件可以是端口、`.elif` 的不行」。

⇒ ⇒ ⚠️ **更一般的教训**：A3 的护栏 R3 用 **grep 找 `if_then` 的形参**
⇒ ⇒ 它**只能**看见它**知道**要看的那个符号 ⇒ ⇒ **漏**了 `elif` 与 ctor
⇒ ⇒ ⇒ **又一次「护栏自身有盲区」实例**（本仓前例：只查标签数看不见文件数
缩面 · 只查 include 到的头文件看不见没 include 的 · fixture 缺失被当成
「编译失败」· R1 与 R3 不可互替）。

⇒ ⇒ **对策**：R3 改为**穷举**该头里**所有**接受条件的公开入口
（`if_then` / `multi_if` ctor / `multi_if::elif`）⇒ ⇒ 且每条都有**负向**检查
（不得退回 `const ch_bool &`）⇒ ⇒ 加**原地投毒**的 self-test 钉住。

## 3. 修法

```cpp
template <typename C, typename U>
multi_if(const C &condition, const U &value, ...) {
    ch_bool cond(to_operand(condition).impl());
    branches_.push_back({cond, checked_value(value, "first branch")});
}

template <typename C, typename U>
multi_if &elif(const C &condition, const U &value) {
    ...
    ch_bool cond(to_operand(condition).impl());
    branches_.push_back({cond, checked_value(value, "elif")});
}
```

⚠️ **幂等性**：`if_then` 已归一化一次 ⇒ ⇒ `multi_if` 再归一化 ⇒ ⇒ 第二次
`to_operand(ch_bool)` ⇒ `get_lnode` 取**现有节点** ⇒ ⇒ **不新建节点**
⇒ ⇒ 零额外节点 ⇒ ⇒ 既有行为**逐位不变** ✔。

⚠️ `branch_info::condition` 的类型是 `ch_bool` ⇒ ⇒ 归一化后**类型不变**
⇒ ⇒ `branches_` 的存储**零改动** ✔。

## 4. self-test 的坑（⚠️ 实测）

⚠️⚠️ 投毒**副本**是**无效**的 —— fixture 通过 `-I include` include **仓内**的
`include/chlib/if.h` ⇒ ⇒ 投毒到 `$TMPDIR/if_h_bak.h` 的副本**根本不会被
include** ⇒ ⇒ 编译**仍成功** ⇒ ⇒ 判据**恒绿**（假绿！）

⇒ ⇒ MUST **原地**投毒（备份 → 改 → 编译 → **还原**）⇒ ⇒ 且用 `trap` 保证
异常路径也还原 ⇒ ⇒ 且**还原后 MUST 再确认一次绿**（⇒ 证明还原没写坏文件）。

## 5. 变异

| # | 注入 | 结果 |
|---|---|---|
| **M1** | `multi_if::elif` 的条件退回 `const ch_bool &` | ✅ self-test **投毒成功** ⇒ fixture 编译失败 ⇒ ⇒ R1 转红 ⇒ ⇒ 判据有判别力 |
