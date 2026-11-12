---
id: decision-request-uart-rx-defects-2026-11
日期: 2026-11-12
提出者: builder（B2 / E3 · `uart_rx_example` 的独立 RTL TB）
状态: **已裁决并落地（2026-11-12 · 用户「按建议推进」⇒ 选项 A）**
关联: .rddf/checks/examples-rtl-expectations.md#uart-rx-example · .rddf/checks/examples-rtl-sim-targets.tsv
---

# 决策请求单：UART RX 的两个缺陷（采样点不缩放 + 解出的字节位反转）

> ⚠️ **本单的两个缺陷都不是「待裁决」** ⇒ ⇒ 它们是**已实测**的实现缺陷，
> 且用户 2026-11-12 裁决「按建议推进直到完成」⇒ ⇒ **按选项 A 落地**（已修 + 已登记）。

---

## 1. D6 —— 采样点**硬编码** `div == 4`（不随 `prescale` 缩放）

**源码原状**（`examples/spinalhdl-ported/uart_rx/uart_rx.cpp`）：

```cpp
// 采样点：每位中间 (div == prescale/2)
// prescale=9, prescale/2=4, 所以采样点在 div=4
auto sample_point = (div == ch_uint<16>(4_d));      // ⚠️ 注释声明了意图，实现没兑现
```

⇒ ⇒ `prescale == 9` 时**恰好**在位中点 ⇒ ⇒ 任何别的 `prescale` 都会让接收器
在**位的前沿**采样（`prescale = 99` 时采样点落在 ≈1/20 处）⇒ ⇒ 真实串口的
时序抖动会**直接采错**。

**修法**：`div == (prescale >> 1)` ⇒ ⇒ `prescale == 9` 时**行为完全不变**（9/2 = 4）
⇒ ⇒ 零回归。

**判别力**：TB 的采样相位 3 探针（`prescale = 99`）⇒ ⇒ 硬编码写法下探针 A 读到 1 ⇒ **红**。

## 2. D7 —— 解出的字节被**位反转**

**源码原状**：

```cpp
auto shifted = shift << ch_uint<8>(1_d);      // ⚠️ 左移 ⇒ 首个采样位被推到 bit7
auto new_shift = shifted | rx_bit;
```

⇒ ⇒ 线上先发 **LSB** ⇒ ⇒ 收下来的**第一个**位 MUST 落在 **bit0** ⇒ ⇒
左移把它推到了 bit7 ⇒ ⇒ **解出的字节是位反转的**。

**实测**：线上 `0x10` ⇒ 解出 `0x08`；`0x11` ⇒ `0x88`；`0x12` ⇒ `0x48`；`0x13` ⇒ `0xC8`
⇒ ⇒ 逐条都是**位反转**（`0x10` 的 bit4 ⇒ 解出 bit3）。

**修法**：累加改为**右移 + 新位进 MSB**（`shift >> 1 | (rx_bit << 7)`）⇒ ⇒
首个采样位落在 bit0 ⇒ ⇒ 正确。

### ⚠️ **D7 为什么藏了这么久**（本单最重要的一条）

example 自带的 C++ demo 用 **0x5A** 做测试值 ⇒ ⇒ **0x5A 是位回文**
（`0x5A` 反转后还是 `0x5A`）⇒ ⇒ **恰好掩盖**了缺陷 ⇒ ⇒ demo 一直报 PASS。

⇒ ⇒ 已把 demo 的测试值改为 **0x4B**（非回文）⇒ ⇒ 实测：
临时重新注入缺陷后 demo **FAILURE（Expected 0x4B, got 0xd2）** ⇒ ⇒
**C++ 侧这条断言现在真的有判别力**。

⇒ ⇒ **教训（值得写进仓内纪律）**：**测试值必须避开「对称值」**（位回文 / 字节回文），
否则实现级的**位序 / 端序**缺陷会被**恰好**掩盖 ⇒ ⇒ 这与仓内「期望值 MUST 手推」
是同族纪律：**手推的期望值本身也要能判别**。

## 3. 顺带修的既有告警

`uart_rx.cpp` 的 demo `main()` 里 10 处 C++ 条件把 `&&` 写成 `&`（编译器告警
「suggest parentheses around comparison in operand of '&'」）⇒ ⇒ 本文件本次被改动
⇒ ⇒ 按 Zero-Debt「改动文件 MUST 零告警」⇒ ⇒ 一并改成 `&&`（语义等价）。

## 4. 登记

- 行为变更：`.rddf/checks/public-api-surface.md`「行为变更（非符号移除）」节 +1 行
- `docs/CHANGELOG.md` 一条
- 期望登记：`.rddf/checks/examples-rtl-expectations.md#uart-rx-example`
- 清单：`.rddf/checks/examples-rtl-sim-targets.tsv`（`uart_rx_example` → verified）
