# 提案：路线 C 第 3 批 —— RV32I 的三个「含时序状态」组件进双后端 parity

- **路线**：objective `test-coverage-100pct` 的**路线 C**（第 6 类候选 = 全仓
  `include/**` 只读盘点发现的 RV32I 微架构组件）
- **关联 capability**（**只读引用，本批不改**）：`rv32i-pipeline-commit-path` ·
  `cpu-clint-timer` · `rv32i-isa-semantics`
- **行为变更**：**无**（纯测试覆盖增量）⇒ 归档用 `--skip-specs`

## Why

| 组件 | 头 | `ch_reg` 数 | dual 覆盖 | 既有单后端覆盖 |
|---|---|---|---|---|
| `riscv::Rv32iPc` | `include/cpu/pipeline/rv32i_pc.h` | 1 | **0** | **0** |
| `ch::chlib::Clint<1>` | `include/cpu/riscv/clint.h` | 4 | **0** | 仅 `test_interrupt_flow`（SoC 级） |
| `riscv::CsrBank` | `include/cpu/riscv/rv32i_csr.h` | 9 | **0** | 仅 `test_csr_unit`（SoC 级） |

⇒ ⇒ 三者都**含寄存器状态** ⇒ ⇒ 走 `always_ff` codegen 路径 ⇒ ⇒ **正是两后端最易
分叉处**（组合逻辑的错在两后端**同构** ⇒ parity 看不见；**时序**的错会分叉）。
⇒ ⇒ 且与路线 C 第 1/2 批（流水线核心：`MemStage`/`ExStage`/`Rv32iDecoder` ·
`IdStage`/`Rv32iRegFile`/`DataTCM`）**互补** —— 本批覆盖**体系结构层**
（PC / 定时器 / CSR）。

## What Changes

| 文件 | 改动 |
|---|---|
| `tests/test_rv32i_arch_dual.cpp`（**新**） | **4** 个 `DUAL_BACKEND_TEST_PAIR` / **366 == 2 × 183** 断言 |
| `tests/CMakeLists.txt` | 注册新 dual target |

## 覆盖的契约

- **`Rv32iPc`**：顺序 +4 / 跳转 / 复位（**高有效**！与 AXI 的 `rst_n` 约定不同）/
  **32 位回绕**（`0xFFFFFFFC + 4 = 0`，靠 33 位加法截断到 32 位实现）
- **`Clint`**：`mtime` 64 位**自由运行**计数器（ADR-049）· **写优先**校时（写的那一拍
  **不递增**）· 高低半读写（`bits_update<32>` 路径）· `mtimecmp` 写入 ·
  `mtip = (hi > cmp_hi) | (hi == cmp_hi & lo >= cmp_lo)`
- **`CsrBank`**：复位值（`mstatus=0x80` / `misa=0x40101101` / `mtvec=0x20000100`）·
  读写 · **只读寄存器**（`misa` / `mip` 写被忽略）· `mip` **粘住** ·
  `mstatus` 字段抽取（MIE / MPIE / MPP）· 中断请求链
  （`mie.MTIE|MEIE` **且** `mstatus.MIE` **且** 外部 pending）

## 期望值（**手推**，禁抄仿真输出）

由**独立 Python 模型**（`/tmp/opencode/gen_route_c_batch3.py`）逐拍生成三张表 ——
模型是**第二个实现**，不是抄仿真输出。读模型：`tick()` = 沿前组合 → 寄存器提交
→ **沿后**组合 ⇒ ⇒ **tick 后读到的端口 = 沿后状态**。

## 变异证据（每条**先确认编译成功** ⇒ 否则跑的是旧二进制 ⇒ 假绿）

| 变异 | 结果 |
|---|---|
| M1 `Rv32iPc` 步长 +4 ⇒ +2 | 🔴 2 failed |
| M2 `Rv32iPc` 去掉 `rst` 分支 | 🔴 2 failed |
| M3 `Rv32iPc` `jump_enable` 取反 | 🔴 2 failed |
| M4 `Clint` 递增条件恒 0 | 🔴 2 failed |
| M5 `Clint` 去掉写优先 | 🔴 2 failed |
| M6 `Clint` `mtip` 恒 0 | 🔴 2 failed |
| M7 `CsrBank` `misa` 变成可写 | 🔴 2 failed |
| M8 `CsrBank` 中断链去掉 `mstatus.MIE` 全局门控 | 🔴 2 failed（⚠️ **首轮恒绿** ⇒ 见下） |
| M9 `CsrBank` `mip` 不粘住 | 🔴 2 failed |

### ⚠️ 如实登记：**激励的判别力缺口**（M8 首轮恒绿）

首版激励把 `mstatus.MIE` 在**写 `mie` 之前**就置 1 ⇒ ⇒ **全局门控从未起作用**
⇒ ⇒ 「去掉 `mstatus.MIE` 门控」这条变异**不可判别**（恒绿）。
⇒ ⇒ 修法：把激励**重排**，补一拍「`mie.MTIE=1` 且 `mtip=1` 但 **`mstatus.MIE=0`**
⇒ 中断请求 MUST 为 0」⇒ M8 转红。

⇒ 这是本仓**第 4 次**「变异抓到了测试自身的缺陷」（前三次：ADR-063 的 M-d 吞分号 ·
ADR-064 的按位或掩盖污染 · ADR-065 的链序等价变异）。

## Verification

- [x] `ctest -LE perf` 零回归
- [x] N2′：断言数(with verilator) == 2 × 断言数(interpreter-only)（183 × 2 = 366）
- [x] 变异 **9/9 全抓**
- [x] 计数锚点同步（parity 源文件数 +1）
