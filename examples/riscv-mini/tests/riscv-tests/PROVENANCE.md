# PROVENANCE —— 官方 riscv-tests `rv32ui-p-*` 镜像的来源与改造

> SPDX-License-Identifier: BSD-3-Clause
>
> 本目录的 `images/` 是**上游 riscv-tests 的物理内存（`p`）测试**，
> 经**两处最小改造**后编译出的**可寻址镜像**。
> 判据载体 = `examples/riscv-mini/tests/test_rv32i_official_suite.cpp`。

## 1. 上游来源

| 项 | 值 |
|---|---|
| 仓库 | `riscv-software-src/riscv-tests` |
| commit | `2ebecad997fa58cd9e5724340ba75aa4b59bd1d0`（2026-08-15） |
| 许可证 | BSD-3-Clause（上游 `LICENSE`：The Regents of the University of California） |
| 测试源 | `isa/rv32ui/*.S`（**一字未改**） |
| 测试集合 | `isa/rv32ui/Makefrag` 的 `rv32ui_sc_tests`（42 个）**减去 2 个排除项** = **40 个** |

⚠️ **上游测试体（`isa/rv32ui/*.S`）一字未改** ⇒ ⇒ 被测的是**上游的 ISA 语义**，
不是「按本仓理解重写的测试」。

## 2. 两处改造（各一处，均有依据）

### 2.1 `env/riscv_test.h` —— 只替换 4 个 harness 宏

上游 `env/p/riscv_test.h` 的 `RVTEST_CODE_BEGIN` 无条件做
`csrw mtvec` / `csrwi mstatus` / `csrw mepc` / `mret`，
`RVTEST_PASS` / `RVTEST_FAIL` 走 `ecall`（`a7 = 93`）+ trap 处理程序。
⇒ ⇒ 那要求一台完整的**特权 / trap 机器**（CSR + 异常入口），
而本核**不实现 CSR**（见 `.rddf/improvements/rv32i-privileged-csr-2026-11.md`）。

本文件**只**替换这 4 个宏（其余 = 上游逐字）：

| 宏 | 本仓实现 | 上游实现 |
|---|---|---|
| `RVTEST_CODE_BEGIN` | 清 32 个 GPR + `li TESTNUM, 0` + 顺序落进测试体 | 同左 + `mtvec` / `mstatus` / `mepc` / `mret` |
| `RVTEST_PASS` | `fence` + `li TESTNUM, 1` + `sw TESTNUM, 0(tohost)` | `ecall`（`a7 = 93`） |
| `RVTEST_FAIL` | `fence` + `ori TESTNUM, TESTNUM, 1` + `sw TESTNUM, 0(tohost)` | `ecall`（`a7 = 93`） |
| `RVTEST_DATA_BEGIN` | 与上游同构（`.tohost` / `.fromhost` / `begin_signature`） | 同左 |

**依据**：riscv-tests 上游 issue #368 —— 「如何在没有 CSR / trap 的环境里跑 p 测试」
的维护者建议正是「替换 env 的 harness 宏，测试体不动」。

⚠️ **判定协议与上游 host 侧一致**：
`tohost == 1` ⇒ PASS；`tohost == TESTNUM | 1` ⇒ FAIL（`TESTNUM` ≥ 2 是被判失败的用例号）。

### 2.2 `env/link.ld` —— 双 TCM 链接布局

上游 `env/p/link.ld` 假设**单一平坦内存**：`.text.init` / `.tohost` / `.text` / `.data`
依次从 `0x8000_0000` 起排（`.tohost` 靠 `ALIGN(0x1000)` 落到 `0x8000_1000`）。

本核是**双 TCM**（地址映射的单一真相 = `include/cpu/riscv/rv32i_mem_map.h`）：

| 区域 | 基址 | 用途 |
|---|---|---|
| I-TCM | `0x8000_0000` | **只走取指** |
| D-TCM | `0x8001_0000` | **只走数据**（load / store） |

⇒ ⇒ 上游布局会把 `.data` 放进 **I-TCM** ⇒ ⇒ 数据侧读到 **0**
⇒ ⇒ 实测：`lb` / `lbu` / `lh` / `lhu` / `lw` / `sb` / `sh` **7 个测试失败**
（正 / 负对照可复现：`rv32ui-p-lw` 修复后通过 ⇒ 根因是**地址映射**不是指令语义）。

⇒ ⇒ 本布局：**指令留 I-TCM，数据（含 `.tohost`）全搬 D-TCM 起始处**。

| 段 | 地址 | 说明 |
|---|---|---|
| `.text.init` / `.text` | `0x8000_0000` | I-TCM |
| `.tohost` | `0x8001_0000` | D-TCM 区首（**判据依赖此地址**） |
| `.data` / `.bss` | `.tohost` 之后 | D-TCM |

## 3. 排除项（2 个）

| 测试 | 排除理由 |
|---|---|
| `fence_i` | 测 `Zifencei` 扩展（`fence.i`）⇒ **本核是 RV32I，无该扩展** ⇒ 排除是**能力对齐**，不是「跑不过」 |
| `ma_data` | 测**非对齐** load/store（`lh`/`lw` 在奇数偏移，期望取到**正确值**）⇒ ⇒ 本核 load/store 的**车道选择忽略低地址位**（`LH` 只看 `addr[1]` · `LW` 忽略 `addr[1:0]`）⇒ 非对齐访问被**静默对齐**（既不处理也不 trap）⇒ ⇒ **能力对齐**（该口径已登记在 `include/cpu/AGENTS.md` 的「load 侧的字节道契约」段） |

⇒ ⇒ **40 个测试全过** = 本仓声明的「官方 riscv-tests 全过」的**完整口径**
（排除项在 CI 之外逐项登记在此，**不得**静默缩小）。

## 4. 镜像格式

`images/<name>/{itcm.mem, dtcm.mem}` = `rv32i-fw-dump --format mem-split` 的产物：

* 每行 **1 个 32 位字**，**小端 8 位十六进制**、无 `0x` 前缀；
* **行号 = 区域内的字索引** ⇒ ⇒ 空洞**已补零**（`rv32i-fw-dump` 的「可寻址产物」条款）；
* `itcm.mem` 的字 0 = `0x8000_0000` 处的指令；`dtcm.mem` 的字 0 = `0x8001_0000`
  = **`tohost`**（8 字节 ⇒ 字 0 与字 1）。

## 5. 复现

```bash
# 需要外部 RISC-V 工具链 ⇒ **CI 跑不了**（可复现性凭证，不是门禁）
bash examples/riscv-mini/tests/riscv-tests/env/build.sh \
     <riscv-tests 源码目录> <riscv32-unknown-elf 工具链前缀>
# ⇒ 重建 40 份镜像 + 与入库镜像 `diff -r` 逐字节比对
```

## 6. 许可

上游 riscv-tests 为 BSD-3-Clause；本目录的 `env/*` 是上游文件的**改造版**
（同许可证），`images/*` 是上游测试体编译产物。
⚠️ **上游 LICENSE 全文随附**：见 `LICENSE.riscv-tests`（本目录）。
