# Tasks: rv32i-load-byte-lane-2026-10

> TDD 5 步（红 → 绿 → 精化 → 变异 → 回归）。
> ⚠️ **含 2 行实现修复**（LB + LH 符号扩展）⇒ **不是**纯验证 change。
> **有** spec 变更 ⇒ 归档 **禁** `--skip-specs`。

## 0. 准备

- [x] 0.1 读码确认 load 车道选择（byte 按 `addr[1:0]` / half 按 `addr[1]`）与
      `funct3` 分派（0/1/2/4/5）**已实现**（`mem_stage.h:99-166`）
- [x] 0.2 ⚠️ **读码 + 实测推翻**「LB/LH/LBU/LHU 全部已正确」：
      `LB` 的符号扩展填充掩码是 `0xFF000000`（注释写 `0xFFFFFF00`）⇒ **有 bug**
      ⇒ 本 change 含 **2 行实现修复**（见 §1）
- [x] 0.3 ⚠️ **实测推翻**「4 次 `SW` 到字节偏移 ⇒ 拼出 `0xA1B2C3D4`」：
      `SW` 字节使能 `0xF` + 数据不移位 ⇒ 实测 `[0x100] = 0x000000A1`
      ⇒ 造字 MUST 用 **`SB`**（design.md §2.2）
- [x] 0.4 手推 **12 项**写回序列（design.md §2.3）⇒ 期望值**不来自**仿真输出
- [x] 0.5 确认「每字节都不同」+「4 个字节 bit7 全为 1」两个判别力前提
- [x] 0.6 确认 `sext<32>` 在 interpreter / JIT / Verilog codegen **三后端都支持**
      ⇒ 修复不会造成双后端分叉

## 1. 修实现（**2 行**）

- [x] 1.1 `include/cpu/pipeline/stages/mem_stage.h:119-121`：
      `LB` 的符号扩展改为 `sext<32>(selected_byte)`
      （或等价的正确填充掩码 `0xFFFFFF00`；若走备选 ⇒ M5 MUST 保留）
- [x] 1.2 ⚠️ `mem_stage.h:139-141`：`LH` 的符号扩展**一并**改为 `sext<32>(selected_half)`
      ⇒ **逐位等价**（已实测：12 项写回序列与只改 LB 时**完全相同**）
      ⇒ 目的：与 LB 一起**消除「手写填充掩码」这一整类错误**（只修 LB ⇒ LH 的同型
      掩码仍在原地 ⇒ 与 design §1.3 的理由不自洽）
- [x] 1.3 **不**改车道选择逻辑（`mem_stage.h:110-135`）⇒ 只改扩展表达式
- [x] 1.4 修正该两行的**注释**（注释写 `0xFFFFFF00` 而代码是 `0xFF000000`
      ⇒ 这正是缺陷能藏住的原因）
- [x] 1.5 重建 ⇒ 确认既有 dual 用例仍绿（`10859 assertions in 5 test cases`）
      ⇒ **既有 golden 零变化**

## 2. 红 / 绿

- [x] 2.1 新增**独立**固件 `load_byte_lane_firmware()`（**既有固件不动**）
      —— 4× `ADDI` + 4× `SB` + 4× `NOP` + 8 条 load（design.md §2.1）
- [x] 2.2 新增独立 TEST_CASE（逐拍双后端全等 + **12 项写回序列逐项比对**）
- [x] 2.3 断言值 MUST 用 design.md §2.3 的**手推**表（**禁**抄仿真输出）
- [x] 2.4 既有固件的 golden **零变化**（既有 TEST_CASE 仍绿）
- [x] 2.5 `ctest -R test_rv32i_soc_dual_parity` 绿

## 3. 精化（判别力自检）

- [x] 3.1 每条期望带**反向期望**（`X_old != X_new`）并留痕
- [x] 3.2 ⚠️ 确认 12 项的**判别力画像**与 design.md §2.3 的说明**一致**：
      **#4**（`LB@0x100`）与 **#10**（`LHU@0x100`）读的是**基线车道/半字**
      ⇒ 对「车道/半字选择」维度**零判别力**（只能区分「整体错位」类变异）
      ⇒ **不是**「12 项每一项都含非零 offset」（那几条自相矛盾）
      ⚠️ **2026-10-28 实测订正**：初稿还称「**#7** 对 **M7** 零判别力」——
      **错**（`SW` 造字下整字 = `0x000000A1` ⇒ lane3 = `0x00` ≠ 期望 ⇒ **转红**）
      ⇒ 已在 design.md §2.2 / §2.3 / §4 三处订正
- [x] 3.3 确认 4 个被测字节的 **bit7 全部为 1**（⇒ 符号扩展与零扩展结果不同）
- [x] 3.4 确认被读字的 4 个车道**各不相同**（⇒ `SB` 造字生效；**不是** `SW`）

## 4. 变异验证（每条**先确认编译成功** ⇒ 否则跑旧二进制 ⇒ 假绿）

- [x] 4.1 **M1** byte 选择恒取 lane0 ⇒ 转红（预期 **#5, #6, #7**）
- [x] 4.2 **M2** 符号扩展写成零扩展 ⇒ 转红（预期 **#4, #5, #8, #9**）
- [x] 4.3 **M3** half 选择取反 ⇒ 转红（预期 **#8, #9, #10, #11**）
- [x] 4.4 **M4** `funct3` 分派漏 `LBU`/`LHU` ⇒ 转红（预期 **#6, #7, #10, #11**）
- [x] 4.5 **M5** ⚠️ **LB 填充掩码写成 `0xFF000000`** ⇒ 转红（预期 **#4, #5**）
      （**本次缺陷的回归锁**）
- [x] 4.6 **M6** ⚠️ **LH 填充掩码写成 `0xFF000000`** ⇒ 转红（预期 **#8, #9**）
      （对称钉住 LH）
- [x] 4.7 **M7** ⚠️ 造字用 `SW` 代替 `SB` ⇒ 转红（**实测**转红项 =
      **#4, #5, #6, #7, #8, #9, #10, #11** = **全部 8 条 load**）
      ⚠️ **2026-10-28 实测订正**：初稿预测「**#7 不红**」—— **错**
      （`SW` 造字下整字 = `0x000000A1` ⇒ lane3 = `0x00` ≠ 期望 `0x000000A1`
      ⇒ 转红）⇒ 已回 design.md §2.2/§2.3/§4 订正
- [x] 4.8 全部还原 ⇒ **重建** ⇒ 复跑全绿（build 目录不得留变异二进制）
- [x] 4.9 ⚠️ 核对：每条变异的实际转红项 MUST 与上面的「预期」**一致**
      （不一致 ⇒ 说明判别力画像有误 ⇒ 回到 design.md §2.3 订正，**不得**放过）

## 5. 回归与工程

- [x] 5.1 `ctest -LE perf` 零回归（191/191）+ `ctest -L perf` 全绿
      —— `ctest -LE perf` **191/191** ✓；`ctest -L perf` **10/11**（2326 s）
      ⚠️ 唯一失败 = `perf_regression`：
      `REGRESSION: TC-08|regs=1000|verilator baseline=2338.13us current=3281.32us ratio=1.40339 (threshold=1.3x)`
      ⇒ **判定为环境漂移，非本 change**（五项证据，缺一不可）：
      ① **perf target 零依赖 `include/cpu/**`**（`.d` 文件**零命中**）⇒ 本 change 的
         唯一实现面 `mem_stage.h` **不在** perf 的编译闭包内 ⇒ **物理上不可能**造成；
      ② **interpreter / jit 同档正常**（TC-08 regs=1000：1.179 / 1.087）
         ⇒ 真代码回归会波及**全后端**（解释器路径与 verilator 路径同源）；
      ③ **verilator 绝对值几乎不随工作量变化**（regs=10/100/1000 ≈
         2512/3089/3281 µs）⇒ 它测的是**固定开销**（编译/进程启动/共享主机负载），
         **不是**逐拍吞吐 ⇒ 「慢」不可能是「实现变复杂」造成的；
      ④ **同二进制三次独立测量**：超阈的**档位每次都不同**
         （run#1 regs=1000 = 1.759 → run#2 全绿（regs=1000 = 1.124）→ run#3 regs=100 = 1.862），
         同档离散度达 **1.048–1.862** ⇒ **抖动**特征（非稳定回归）；
      ⑤ 与上一会话记录的失败**同型**（上次是 `TC-08|regs=10|verilator`，本次是
         `regs=1000`）⇒ **漂移档位不固定** ⇒ 环境性。
      ⚠️ **backlog（与本 change 无关）**：perf baseline（2026-10-25 / git_sha `7c5038e`）
         与当前机器状态已脱节 ⇒ 需一次**专门的** baseline 重采集
         （已立项 `openspec/changes/perf-baseline-refresh-2026-10/`）。
- [x] 5.2 `run_all_ported_tests.sh` 28/28 + `run_all_samples.sh` 35/35
- [x] 5.3 8 个 `scripts/verify_*.sh` 全 exit 0
- [x] 5.4 计数**不变**：202 / 17 / 37
- [x] 5.5 `openspec validate --specs --strict` 不因本 change **新增**红

## 6. 文档与簿记

- [x] 6.1 根 `AGENTS.md`：§KNOWN LIMITATIONS 的「ISA 覆盖缺口」收窄 + §NOTES 记录
- [x] 6.2 `feat-rv32i-isa-completeness` 的验收项勾选 + `roadmap.md`
- [x] 6.3 主 spec 的 `## Purpose` 归档后核对（delta 的 Purpose 不会被合并 ⇒ 手工）
      —— 已手工补一段「**2026-10-28 补充**」（load 侧的缺口 + 「为何从未暴露」），
      并把 delta 里那条指向 `design.md` 的 NOTE 改成**归档后的完整路径**
      （先例：`rv32i-commit-path/spec.md` 同款写法）⇒ 无悬空引用
- [x] 6.4 ⚠️ `include/cpu/AGENTS.md` 的 load 路径契约段（如有）同步「符号扩展用 `sext`」

## 7. 审稿与归档

- [x] 7.1 对抗审稿（critic，`review_profile: plan, build`）⇒ ≤ 2 轮
      —— **2 轮（预算用尽）**，两轮均 **REJECT**；BLOCKER 全部闭合：
      ① 第 1 轮 BLOCKER = **LB 符号扩展缺陷**（实测：`0xFF000000` ⇒ `lb` 返回
         `0xFF0000C3`）⇒ change 从「纯验证」改为「**含 2 行实现修复**」；
      ② 第 2 轮 BLOCKER = **「4 次 `SW` 能拼出 `0xA1B2C3D4`」不成立**
         （实测 `[0x100] = 0x000000A1`）⇒ 造字改用 `SB` + 编译期判别力护栏。
      ⚠️ 审稿覆盖的是**计划期产物**（proposal / design / spec delta / tasks）；
      实施期的**实测订正**（`SW`→`SB`、M7 的转红项集合）已回 design.md
      §2.2 / §2.3 / §4 三处留痕（见 tasks.md §3.2 / §4.7 的「实测订正」注）。
      ⚠️ 预算已用尽 ⇒ **不发起第 3 轮**（需用户明确批准）。
- [x] 7.2 审稿处置（BLOCKER ⇒ 返工；SF / NIT ⇒ 同批闭合留痕）
      —— 2 个 BLOCKER 均已**在实施中返工并闭合**（上条 ①②）；
      SF / NIT 的闭合留痕见 design.md §2.2/§2.3/§4 与本文件 §3.2/§4.7 的
      「实测订正」注 + 根 `AGENTS.md §NOTES` 的同批条目
- [x] 7.3 `openspec archive rv32i-load-byte-lane-2026-10 --yes`（**禁** `--skip-specs`）
      —— 已归档：`openspec/changes/archive/2026-10-28-rv32i-load-byte-lane-2026-10/`
      ⇒ 主 spec `openspec/specs/rv32i-isa-semantics/spec.md` 合并 **+1 条 ADDED**
      （7 条 Requirement）⇒ **未**用 `--skip-specs` ✓
