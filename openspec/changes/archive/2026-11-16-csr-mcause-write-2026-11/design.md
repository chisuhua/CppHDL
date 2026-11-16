# Design — csr-mcause-write-2026-11

## 1. 状态机（`ExceptionUnit` 内部）

```
trap_phase : ch_reg<ch_uint<2>>   0 = 空闲 · 1 = 已写 mepc（待写 mcause）
cause_latched : ch_reg<ch_uint<32>>  拍 1 锁存的 exception_cause
```

| 拍 | `trap_phase`（沿前） | `start_trap` | `finish_trap` | 写口 | 沿后 `trap_phase` |
|---|---|---|---|---|---|
| 1 | 0 | `exception_valid` | 0 | **`mepc`** = `current_pc` | 1 |
| 2 | 1 | 0 | 1 | **`mcause`** = `cause_latched` | 0（若 `exception_valid` 仍高 ⇒ ⇒ 拍 3 起重复） |

⚠️ **配对正确性**：`mcause` 用的是**拍 1 锁存**的原因码 ⇒ ⇒ 即便 `exception_valid` 是
**电平**（持续拉高 ⇒ ⇒ 每 2 拍重复一次序列）⇒ ⇒ `mepc` 与 `mcause` 始终是**同一次异常**的一对
⇒ ⇒ **无数据丢失**、**不串**。

⚠️ **`exception_valid` 的契约**：它是**电平** ⇒ ⇒ 真实使用方 MUST 在取走陷阱后**撤销**它
（否则重复写）⇒ ⇒ 两个用例分别钉住「撤销后停止写」与「电平下值仍正确」。

## 2. 「陷阱当拍写 mepc」这条契约**为什么不能用写口观察**

写口是**组合输出** ⇒ ⇒ 沿后读到的是**下一拍**的写 ⇒ ⇒ 「陷阱当拍（cycle 1）写的是 mepc」
在 `ExceptionUnit` 单独测试里**不可见** ⇒ ⇒ MUST 落到**复合 DUT** 的 `CsrBank.mepc` 值上
（写口写进去的值在沿后**可读**）⇒ ⇒ 这就是复合 DUT 的必要性。

⇒ ⇒ ⚠️ **既有 case 6 的断言口径必须改**（不是放松）：旧断言「写 addr == MEPC」**恰好是缺陷的
产物**（MEPC 恒命中 ⇒ ⇒ 它对「是否也写了 mcause」**零判别力**）⇒ ⇒ 改为断言「下一拍指向
`MCAUSE`」，而「陷阱当拍写 mepc」由复合 DUT 钉住。

## 3. 判别力（**变异自证**）

| 变异 | 期望 | 实测 |
|---|---|---|
| `write_mcause = finish_trap` ⇒ ⇒ 还原为 `do_trap_entry`（旧形态） | 复合 DUT 用例**转红** | ✅ **1 个用例转红**（`mcause` 读回恒 0） |
| 还原 + **重建**（二进制也是状态） | 全绿 | ✅ **14 用例 / 57 断言全绿** |

⚠️ 变异**必须**先确认仍能编译（编译失败 ⇒ 跑的是**旧二进制** ⇒ 假绿）。

## 4. 为什么**不需要 ADR**

本 change 是**缺陷修复**：契约早已写明「`mcause` MUST 能被写到」⇒ ⇒ 旧实现**没做到** ⇒ ⇒
修它 = **收口**，不是**新裁决** ⇒ ⇒ 契约载体 = capability（`rv32i-privilege` 的种子 Requirement）
⇒ ⇒ **无新 ADR**。⚠️ 但「跨两拍」这个**时序形态**是契约面事实 ⇒ ⇒ 已写入 capability 的
Requirement 正文（不是只写在 change 里）。

## 5. 影响面

- `riscv::ExceptionUnit` **零生产消费者**（`include/cpu/pipeline/**` 对它的符号命中 = 0）⇒ ⇒
  **零交付风险**（metis finding 8）
- `CsrBank` **零改动** ⇒ ⇒ 不触发「新增输入端口 ⇒ 全部实例化点 MUST 显式驱动」
- `mtval`：**恒 0**（无数据源）⇒ ⇒ 登记为特权级立项项
