# Change: 官方 riscv-tests 套件门禁（`rv32ui-p-*` 40 个全过 = 「riscv-mini 完成」）

> **Change 名**：`riscv-tests-gate-2026-11`
> **母 improvement**：**无独立草案** —— 本项由**用户直接发起**（2026-11-25 · 用户口径：
> 「让 `examples/riscv-mini` 跑通官方 riscv-tests，并把『riscv-mini 完成』定义为该套件全过」）
> ⇒ ⇒ `.rddf/improvements/` **不新增**文件（builder 不越权起草 improvement 草案）。
> **日期**：2026-11-25 · builder
> **性质**：
> - 🔴 **含一处核心缺陷修复**（反压期旁路档位未冻结 ⇒ 操作数被改写）
> - 🟢 **含测试基础设施**（官方镜像重建 + 入库 + 核级门禁）
> ⇒ ⇒ **零端口面变更**（53 端口契约表与 C6 快照**不动**）
> ⇒ ⇒ **零公开 API 变更**（`fwd_a_d1` / `fwd_b_d1` 是**内部**派生状态）

---

## Why

### 1. 官方套件在本仓的**有效覆盖 = 0**

`examples/riscv-mini/tests/test_riscv_tests.cpp` 里有 **40 个** `rv32ui-p-*` 用例
⇒ ⇒ 但它们依赖**仓外**的预编译二进制（`/workspace/project/riscv-tests/build/...`）
⇒ ⇒ CI 里**全部 SKIP** ⇒ ⇒ **有效覆盖 = 0**
（根 `AGENTS.md` §KNOWN LIMITATIONS 已登记这条债：「LB/LBU/LH/LHU 在仓内的唯一用例
依赖**仓外** riscv-tests 二进制 ⇒ CI 中全部 SKIP ⇒ 有效覆盖为 0」）。

⇒ ⇒ **RV32I ISA 语义没有权威判据** ⇒ ⇒ 本仓对「本核是否符合 RV32I」**无法回答**。

### 2. 把官方镜像真正跑起来 ⇒ 挖出 **2 个独立缺陷**

实测（修复前，40 个官方测试跑在核上）⇒ **33 PASS / 7 FAIL**
（`lb` / `lbu` / `lh` / `lhu` / `lw` / `sb` / `sh`）。⇒ ⇒ 两个根因：

| # | 根因 | 性质 | 证据 |
|---|---|---|---|
| **W1** | **链接布局**：上游 `env/p/link.ld` 假设**单一平坦内存** ⇒ `.data` 落在 `0x8000_0000`（I-TCM）⇒ ⇒ 本核（**双 TCM**）从 D-TCM 侧读它 ⇒ ⇒ 读到 **0** | **测试环境**问题（不是核的缺陷） | `include/cpu/riscv/rv32i_mem_map.h`（地址映射单一真相）· 正/负对照：`rv32ui-p-lw` 修复后通过 ⇒ 根因是**地址映射**不是指令语义 |
| **W2** | **反压期操作数完整性**：MEM 级反压（D-L1 冷未命中 ≈ 9 拍）期间，旁路**档位**寄存器 `fwd_a_d1` / `fwd_b_d1` **没有跟着冻结** ⇒ ⇒ 每拍按「**ID 级输入那条指令**」的 rs 地址刷新 ⇒ ⇒ 该档位被施加到 **EX 级那条被冻结的**指令上 ⇒ ⇒ 它的操作数被**别的指令的旁路值**改写 | 🔴 **核的缺陷** | `include/cpu/pipeline/rv32i_pipeline.h`（`fwd_a_d1` / `fwd_b_d1` 的 `->next`）· 三处数值互证（`rv32ui-p-lb` 轨迹 c=40..49） |

⇒ ⇒ **W2 是本 change 的核心**：它**同时**解释了官方套件里 7 个失败中的**一部分**，
且它是一个**独立于 ISA 语义**的微架构缺陷 ⇒ ⇒ 官方套件是发现它的**有效手段**。

### 3. 「riscv-mini 完成」需要一个**可判定的**定义

用户口径：**官方 riscv-tests 全过 = riscv-mini 完成** ⇒ ⇒ 门禁必须
① 在**仓内**（不依赖仓外 checkout）② **可重复** ③ **进 CI**。

---

## What Changes

### 1. 核心修复（W2）：反压期间**冻结**旁路档位

`include/cpu/pipeline/rv32i_pipeline.h`（`build_forwarding_paths()`）：

```
fwd_a_d1->next = select(reset_active, 0_d,
                        select(mem_stall_fwd, fwd_a_d1, hazard.io().forward_a));
fwd_b_d1->next = select(reset_active, 0_d,
                        select(mem_stall_fwd, fwd_b_d1, hazard.io().forward_b));
```

⇒ **反压期间档位保持不变** ⇒ ⇒ 档位与它服务的指令**同冻结**。

⚠️ **两个被排除的「看起来更保守」的错法**（**实测二者都必红** ⇒ 已写进注释）：
- ❌ **反压期间清零档位** ⇒ ⇒ 被冻结指令若自身在冻结入口档位就非 0（距离-2 生产者
  此刻在 M2）⇒ ⇒ 清零把「错前推」换成「读 `idex_rs1_data` 的**陈旧**值」⇒ ⇒ 同样是错；
- ❌ **改用「ID 读拍的 M2/WB 旁路冻结」** ⇒ ⇒ 那条通路服务的是**解冻沿锁存的后继指令**
  ⇒ ⇒ 冻结它反而请回 ADR-069 §7.1.2（D11）处理的同沿竞态。

⇒ ⇒ **不动** ADR-069 D11 的「两级 + d1」结构 ⇒ ⇒ **零 spec 语义变更**（只加约束）。

### 2. R1：官方镜像重建 + **入库**

- `examples/riscv-mini/tests/riscv-tests/env/riscv_test.h` —— 只替换 **4 个** harness 宏
  （`RVTEST_CODE_BEGIN` / `RVTEST_PASS` / `RVTEST_FAIL` / `RVTEST_DATA_BEGIN`）
  ⇒ ⇒ **测试体（`isa/rv32ui/*.S`）一字未改**
  ⇒ ⇒ 依据 = riscv-tests 上游 **issue #368**（维护者建议：换 env 的 harness 宏，测试体不动）
  ⇒ ⇒ 原因 = 上游 env 要求**完整特权 / trap 机器**（CSR + 异常入口）⇒ ⇒ 本核**不实现 CSR**；
- `examples/riscv-mini/tests/riscv-tests/env/link.ld` —— **双 TCM 链接布局**
  （`.text` @ `0x8000_0000` · `.tohost` @ `0x8001_0000` · `.data` / `.bss` 随后）；
- `examples/riscv-mini/tests/riscv-tests/images/rv32ui-p-*/{itcm.mem,dtcm.mem}` —— **40 份**；
- `examples/riscv-mini/tests/riscv-tests/env/build.sh` —— **可复现性凭证**（重建 + 逐字节比对）；
- `examples/riscv-mini/tests/riscv-tests/PROVENANCE.md` + `LICENSE.riscv-tests` —— 来源与许可。

⇒ ⇒ **排除 2 个**：`fence_i`（无 Zifencei 扩展）· `ma_data`（主动式多 hart）
⇒ ⇒ 排除项**逐条登记**在 `PROVENANCE.md` §3 ⇒ ⇒ **不是静默缩小**。

### 3. R2：核级门禁（4 个 CTest 条目 × 10 个官方测试）

`examples/riscv-mini/tests/test_rv32i_official_suite.cpp` ⇒ DUT =
`ch_device<Rv32iCoreTop, itcm, dtcm>`（核 IP 顶层）
⇒ ⇒ 判定口径 = **上游 env 的协议**（`tohost == 1` ⇒ PASS；`tohost == TESTNUM|1` ⇒ FAIL）
⇒ ⇒ **不新增端口** ⇒ ⇒ 用核的**既有** store 提交事件
（`obs_data_write_en` / `obs_data_addr` / `obs_data_write_data`）观察 `tohost` 的写
⇒ ⇒ 顺带覆盖 **store 路径**（D-L1 写穿 → D-TCM）。

⚠️ **为什么是 4 个独立二进制**（不是 1 个二进制 + 4 个过滤参数）：
40 个测试跑完 ≈ **7 分钟**（解释器后端 ≈ 36 拍/秒）⇒ ⇒ 超过仓内**单测 300 s 上限**
（ctest `TIMEOUT 300` + 护栏 `verify_example_test_wiring.sh` C3 的硬编码 300 s）；
且护栏 C3 按「**ctest 条目名 = 可执行文件名**」找二进制并**不带参数**运行
⇒ ⇒ 「同一二进制 + 过滤参数」⇒ 护栏跑的是**默认集合** ⇒ 与 ctest 条目**不等价**
⇒ ⇒ MUST 是**独立二进制**（每个只含自己那 10 个用例）。
⇒ 实测单组耗时 **95 / 97 / 106 / 122 s** ⇒ ⇒ 全部 < 300 s（2.5× 余量）。

### 4. 缺陷判据（核级 · 3 个形态）

`examples/riscv-mini/tests/test_rv32i_stall_integrity.cpp`（新 CTest 条目）：

| 用例 | 形态 | 判别点 |
|---|---|---|
| **A1** | **M1 · 后继别名型** | 后继指令的 rs 与 EX / EX-MEM 的 rd 相等 ⇒ 操作数被**前推值**改写 |
| **A2** | **M2 · 决策回退型** | 被冻结指令**自身**在冻结入口档位就非 0 ⇒ 操作数**回退**到 `idex_rs1_data` 的**陈旧**值 |
| **A3** | **控制面** | 冻结中的**条件分支**方向被翻转（静默重定向 ⇒ 静默 PC 劫持） |

⇒ ⇒ A2 同时是「**反压期清零档位**」这种错法的杀手。
⇒ ⇒ **判别力自证**：撤掉冻结（变异 M1）⇒ ⇒ **A1 / A2 / A3 三个全红**（实测）。

---

## 验收标准

| # | 判据 | 验证命令 | 期望 |
|---|---|---|---|
| **AC1** | 官方 40 个测试**全过** | `ctest --test-dir build -R test_rv32i_official_suite --output-on-failure` | 4 个条目全绿（每个 10 个用例） |
| **AC2** | 缺陷判据 3 个形态全绿 | `ctest --test-dir build -R test_rv32i_stall_integrity --output-on-failure` | 绿（21 断言 / 3 用例） |
| **AC3** | **判别力自证**：变异（只回退 A 口 / 只回退 B 口）⇒ 各自必红 | ① 只把 `fwd_a_d1->next` 恢复为无条件刷新 ⇒ A1 / A3 红 · A2 绿；② 只把 `fwd_b_d1->next` 恢复 ⇒ **A2 红**（`0 == 42`）· A1/A3 绿；③ 两个都恢复 ⇒ **三个全红**（实测） | 还原后复绿 |
| **AC4** | 镜像**入库且可复现** | `bash examples/riscv-mini/tests/riscv-tests/env/build.sh <上游> <工具链>` | 重建 40 份 + 与入库镜像 `diff -r` **逐字节一致** |
| **AC5** | **零端口面变更** | `git diff --stat` + `scripts/verify_no_duplicate_rv32i_headers.sh` | 53 端口契约表 + C6 快照**不动** |
| **AC6** | 锚点同步 | `scripts/update_test_count_anchors.sh --check` | 绿（`CTEST_TOTAL` +5 · `EXAMPLES_TEST_SOURCES` +2） |
| **AC7** | 全量回归 | `ctest --test-dir build --output-on-failure -j$(nproc)` + 51 护栏 | 全绿 |

---

## Out of Scope

- ❌ **CSR / 特权级**（`rv32i-privileged-csr-2026-11`）⇒ ⇒ 官方 env 的 `mtvec` / `mret` /
  `ecall` 路径**不做** ⇒ ⇒ 官方套件**不含**特权级测试（上游 `rv32ui-p-*` 本来也不含）；
- ❌ **`fence_i` / `ma_data`** ⇒ ⇒ 排除理由 = **能力对齐**（无 Zifencei / 单 hart），
  **登记**在 `PROVENANCE.md` §3；
- ❌ **JIT 加速** ⇒ ⇒ 当前该核的设计含 **128 bit** 的 mem 节点 ⇒ ⇒ JIT 整体跳过
  ⇒ ⇒ 官方套件只能跑解释器后端（≈ 36 拍/秒）⇒ ⇒ 这是**性能债**，不是本 change 的范围；
- ❌ **官方套件的 Verilator 后端** ⇒ ⇒ 门禁是**核级**（解释器）⇒ ⇒ 双后端 parity 覆盖
  由既有 `test_rv32i_soc_dual_parity` 承担；
- ❌ **改上游 riscv-tests**（测试体 / Makefrag / 上游 `env/`）⇒ ⇒ 只改**本仓的副本**。
