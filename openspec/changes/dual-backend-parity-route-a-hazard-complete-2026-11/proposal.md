# 提案：路线 A（F 批）—— `riscv::HazardUnitComplete` 进双后端 parity（零 DUT 复制）

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 批次表 + §4 的「可转性 4 问」
- **规格影响**：**无 spec 变更** ⇒ 归档可用 `--skip-specs`
- **规模**：1 个新测试文件（1 个 `DUAL_BACKEND_TEST_PAIR` / 112 断言每后端）+ 1 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage` · ADR-047（数据前推档位语义）

## Why

### 一、成本最低的一批：**零 DUT 复制**

`riscv::HazardUnitComplete`（`include/cpu/pipeline/rv32i_hazard_complete.h`）**本身就是**
带顶层端口的 `Component`：**18 个 `ch_in` + 7 个 `ch_out`**，全部 ≤ 32 bit
（不触 ADR-035 R3 的 64-bit 上限）⇒ ⇒ 可直接用**库组件**当 macro 的 DUT
（先例 `tests/chlib/test_forwarding_dual.cpp` · `tests/test_blt_debug_behavior.cpp`）
⇒ ⇒ **测的就是生产代码本身**，不复制、不复刻、无需自建 fixture。

### 二、现有单后端覆盖极薄

`tests/chlib/test_hazard_complete.cpp` 是 **interpreter-only**，且**每个用例只断 1–2 个输出**
（`:19` 还是 `REQUIRE(true)` 空断言）⇒ 该组件的**组合语义契约**（前推档位 / 优先级 /
load-use 停顿 / 分支冲刷）在 verilator 侧**从未被验证过**。

⚠️ **如实登记价值定位**：该组件的**生产消费者 = 0**（真实流水线用的是 `riscv::HazardUnit`，
见 ADR-047 D7）⇒ ⇒ 本文件锁的是**语义契约**，**不是**关键路径回归 ⇒ ⇒
**不包装成「性能/集成保障」**。

## 可转性 4 问

| # | 问题 | 回答 |
|---|---|---|
| ① | 语义是否已被别的 dual 文件覆盖？ | **否** —— 全仓 dual 文件里无 `HazardUnitComplete` |
| ② | 断言形态有判别力吗？ | **有** —— 变异 7/7 全抓（见下） |
| ③ | 与后端无关吗？ | **否** —— 真跑仿真 + 读顶层端口，正是要对比两后端 |
| ④ | 形态值得新写吗？ | **是** —— 无需新写 DUT，只需 16 个向量表 |

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_hazard_complete_dual.cpp`（**新**） | **16 个向量 × 7 个输出** = **112 断言/后端**；用库组件当 DUT |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test`（含「零 DUT 复制」与「死端口」的注释） |
| `AGENTS.md` | 4 个计数锚点 + 3 处 NOTES 计数 + 本批 NOTES 条目 |
| 台账 | plan §5 墙钟 · objective 现状 55 → 56 与可达上限 · handoff |

## 期望值（**手推**，禁抄仿真输出）

语义逐条对照 `describe()`：

```
match<N>(a, rd, we)  = (a == rd) ? (we ? (a != 0) : 0) : 0
src<N>               = match_EX ? 1 : match_MEM ? 2 : match_WB ? 3 : 0
data<N>              = src==1 ? ex_alu_result
                     : src==2 ? mem_alu_result
                     : src==3 ? wb_write_data : rs<N>_data_reg
match_load<N>        = (mem_rd_addr == id_rs<N>_addr) ? (id_rs<N>_addr != 0) : 0
stall                = mem_is_load ? (match_load<1> ? 1 : match_load<2>) : 0
flush = pc_src       = ex_branch ? ex_branch_taken : 0
```

钉住的四条契约：

1. **优先级 `EX(1) > MEM(2) > WB(3) > REG(0)`** —— 「三处全命中 ⇒ EX 胜」+「EX 的 `we=0` ⇒ MEM 胜 WB」
2. **`x0` 不前推** —— 前推侧与 load-use 侧**各一条**
3. **`we` 门控** —— EX / MEM / WB 三级**各一条**
4. **load-use 只看 `mem_is_load` 且 `rs1`/`rs2` 独立** + **`flush`/`pc_src` 的 `ex_branch` 门控两面**
   （branch+taken / branch+not-taken / **non-branch+taken**）

⚠️ **5 个数据源取互不相同**的值（`ex_alu=0x11111111` · `mem_alu=0x22222222` · `wb=0x33333333` ·
`rs1_reg=0xAAAA0001` · `rs2_reg=0xBBBB0002`）⇒ ⇒ 任何**串源/漏接**立刻可见。

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 转红 |
|---|---|---|
| M1 | 优先级链**跳过 EX**（MEM 胜 EX） | 两半边 |
| M2 | 前推的 **`x0` 保护去掉** | 两半边 |
| M3 | **`we` 门控去掉**（rs1 EX 匹配） | 两半边 |
| M4 | **load-use 只看 rs1** | 两半边 |
| M5 | **`flush` 丢掉 `ex_branch` 门控** | 两半边 |
| M6 | **load-use 的 `x0` 保护去掉** | 两半边 |
| M7 | rs2 数据 mux 默认支路接成 `rs1_data_reg` | 两半边 |

⇒ **7/7 全抓**，且**每条都是「两半边同时红」** ⇒ 变异在两后端**同构**生效（不是单侧假象）。

## 🔴 读实现时发现的两个死端口（本批**不修**，独立立项）

`rv32i_hazard_complete.h:35-36` 声明了 `id_is_load` / `id_is_branch` **两个输入端口**，
但 `describe()`（`:80-192`）**从未读取它们** —— 实测：两个名字在全文各只出现 **1** 次
（即各自的声明处）。

⇒ 与 ADR-047 D7 从 `HazardUnit` 删掉的那 5 个「预留输入」**同类** ⇒ ⇒ 删它们 =
**改公开 API** ⇒ 不在本批范围。

⚠️ 本文件**仍然显式驱动**它们：未驱动端口读到的是**未定义值**，双后端下两侧未必相同
⇒ 即使当前无消费者，依赖「未驱动也无所谓」也是**碰巧通过**。

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**224 == 2 × 112**
- [x] 变异 **7/7 全抓**（每条先确认仍可编译；还原用 `cp` 后重建）
- [x] **18 个输入端口逐个显式驱动**（含两个死端口）
- [x] 期望值**手推**（公式逐条对照实现；一次通过，无推导错）
- [x] **原文件 `tests/chlib/test_hazard_complete.cpp` MUST NOT 修改**（已遵守）
- [x] verilator 半边**真的跑了**（N2′ 的 2× 断言差是正向证据）
- [x] 全量重建后 `ctest -LE perf` **222/222** 零回归
- [x] 计数锚点同步且 `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 双绿
- [x] 归档可用 `--skip-specs`（本 change 显式声明无 spec 变更）
