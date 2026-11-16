# Proposal: 陷阱入口 MUST 把 `mepc` **与** `mcause` 都写进 CSR（并可读回）

## Why

⚠️ **实测缺陷（2026-11-10 登记 · 2026-11-16 修）**：`riscv::ExceptionUnit` 的 CSR 写路径是
`write_mepc = write_mcause = do_trap_entry` ⇒ ⇒ 两者**同时**为真 ⇒ ⇒
`csr_addr = select(write_mepc, CSR_MEPC, …)` ⇒ ⇒ **MEPC 分支恒命中** ⇒ ⇒
**`mcause` 分支永远不可达** ⇒ ⇒ **`mcause` 永远写不到**（实测读回恒为 `0`）。

⇒ ⇒ 而既有测试（`test_csr_unit.cpp` 的 case 6）**只查写口的值** ⇒ ⇒ **从不查
`CsrBank` 的读回** ⇒ ⇒ ⇒ 该缺陷**零信号**（它能活到今天的原因）。

⚠️ **架构含义**：特权级一旦接线（`mcause` 是异常原因的**唯一**载体）⇒ ⇒ 异常原因**恒为 0**
⇒ ⇒ 「为什么 trap 进来的是 ECALL」这类问题**无法回答** ⇒ ⇒ 故**必须先修**（先让缺陷可见，
再谈特权级落地）。

## What Changes

| # | 内容 |
|---|---|
| 1 | `ExceptionUnit` 的 CSR 写改为**跨两拍**：**拍 1** 写 `mepc`（= `current_pc`）并**锁存** `exception_cause`；**拍 2** 写 `mcause`（= **锁存的**原因码）⇒ ⇒ 两拍**同配对** |
| 2 | 新增**复合 DUT**（`ExceptionUnit` → `CsrBank`）用例 ⇒ ⇒ 判据落在 **`CsrBank` 的读回值**上（写口观察不到「陷阱当拍」）⇒ ⇒ **补上原缺陷缺失的那一维** |
| 3 | 既有 case 6 的写口断言**改观察点**（`MEPC` → `MCAUSE`）并**写明理由**（旧断言恰好是缺陷的产物：MEPC 恒命中 ⇒ ⇒ 钉不住正确行为） |
| 4 | 新 capability `rv32i-privilege`（**种子**）⇒ ⇒ 承载「陷阱入口的 CSR 写入契约」 |

## 设计取舍（**为什么是「跨两拍」而不是别的**）

| 备选 | 处置 | 理由 |
|---|---|---|
| **A**：`CsrBank` 增第二条写口（**同拍双写**） | ❌ 否决 | 公共 API 变化 ⇒ ⇒ ~20 处实例化的「新增输入端口 MUST 全部显式驱动」义务（未驱动读到**未定义值**）⇒ ⇒ 代价被低估（metis finding 9） |
| **B**：`ExceptionUnit` **内部跨两拍** | ✅ **采纳** | `CsrBank` **零改动** ⇒ ⇒ 不触发上面的义务；该组件**零流水线消费者**⇒ ⇒ 时序变更**零交付风险**；且「写回读」闭环**不依赖**任何新端口 |
| **C**：`CsrBank` 增 `mcause`/`mtval` 专用写口 | ❌ 否决 | 同 A（公共 API 变化）⇒ ⇒ 且专用口对「未来还要写别的 CSR」不通用 |

⚠️ **不做**「先写 mcause」：既有 case 6 的断言钉住的是「陷阱当拍写 `mepc`」⇒ ⇒ **顺序不可反**
（metis finding 7）。

## Out of Scope（MUST 独立立项）

- **`mtval`**：本组件**没有数据源**（只有 `exception_cause`）⇒ ⇒ 本版 **`mtval` 恒 0**，
  写入属**特权级立项**（metis finding 6）
- **CSR 指令 / `ECALL` / `EBREAK` / `MRET` / 中断 / trap 入口接线** ⇒ ⇒ 特权级独立立项
  （本 change 只把**已有的**陷阱入口单元修对）
- **并发异常的排队**：本版 `exception_valid` 是**电平** ⇒ ⇒ 持续拉高 ⇒ ⇒ 每 **2** 拍重复一次
  「写 mepc / 写 mcause」序列 ⇒ ⇒ 两个 CSR 的**值**始终等于**最近一次异常** ⇒ ⇒ **无数据丢失**
  ⇒ ⇒ 真正的「排队」语义（多异常在途）属特权级立项

## 验收标准（可勾选）

- [x] **G5-1**：`mcause` 在陷阱后的**下一拍**被写入 ⇒ ⇒ **读回** == `exception_cause`
- [x] **G5-2**：`mepc` 在**陷阱当拍**被写入 ⇒ ⇒ **读回** == `current_pc`
- [x] **G5-3**：两者的**配对**正确（`mcause` 用**拍 1 锁存**的原因码 ⇒ ⇒ 持续电平下也不串）
- [x] **G5-4**：既有 case 6 的「陷阱当拍写 mepc」契约**仍成立**（由复合 DUT 观察）
- [x] **G5-5**：**变异自证** —— 还原旧形态（`write_mcause = do_trap_entry`）⇒ ⇒ 复合 DUT 用例
      **转红**；还原 + **重建** ⇒ ⇒ 复绿
- [x] **G5-6**：`ctest -LE perf` 全绿 · 全部 `scripts/verify_*.sh` PASS
- [x] **G5-7**：spec delta 进主 spec（新 capability `rv32i-privilege`）

## 用户可见变更

| 面 | 变更 | 迁移动作 |
|---|---|---|
| **`riscv::ExceptionUnit` 的 CSR 写时序** | 陷阱入口改为**跨两拍**写：**拍 1** 写 `mepc`、**拍 2** 写 `mcause` ⇒ ⇒ 旧行为（同拍写 ⇒ `mcause` **永不可达**）已消失 | ⚠️ **依赖「陷阱当拍 `csr_write_addr == CSR_MEPC`」的下游 MUST 改**：写口是组合输出 ⇒ ⇒ 沿后读到的是**下一拍**的写 ⇒ ⇒ 该断言在**沿后**读到 `CSR_MCAUSE` ⇒ ⇒ 「陷阱当拍写 mepc」MUST 改用 **`CsrBank.mepc` 的读回值**观察（`CsrBank` 本身**零改动**） |
| **`riscv::ExceptionUnit`** | **端口面零变更**（`__io` 未动）⇒ ⇒ 无新增端口驱动义务 | — |
| **`riscv::CsrBank`** | **零改动** | — |
| **capability** | 新增 `rv32i-privilege`（**种子**：陷阱入口的 CSR 写入契约）⇒ ⇒ `mtval` 仍**恒 0**（无数据源 ⇒ 属特权级立项） | — |
