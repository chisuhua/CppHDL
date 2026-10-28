# Tasks: riscv-mini-cache-wrapper-live-logic-2026-10

> TDD 5 步（红 → 绿 → 精化 → 变异 → 回归）。
> ⚠️ **既有断言会转红** ⇒ 反转断言与修实现 MUST **同 commit**。
> **有** spec 变更 ⇒ 归档 **禁** `--skip-specs`。

## 0. 准备

- [x] 0.1 读码定位全部 **5 处**建图期 C++ `if`（`design.md` §1）
- [x] 0.2 确认 `d_cache_write_buffer.h` 的 `axi_addr` / `axi_wdata` 驱动是**注释掉的**、
      `axi_we` **从未被驱动** ⇒ 只改 `if` 不够（`design.md` §2.2）
- [x] 0.3 确认 `ICachePrefetch` 的 `ready` / `data` / `miss` **本来就**无条件驱动
      ⇒ 本 change **不需要**动 `ready`
- [x] 0.4 确认内层 cache 的 **5 个输入端口全部未驱动** ⇒ `miss` 恒为「冷 miss」
- [x] 0.5 范围裁决：**采纳 (b) 直通式**（`design.md` §2）⇒ (a) 会发出垃圾 AXI 请求、
      (c) 真 FIFO 是独立 feature

## 1. 修实现（两个 wrapper）

- [x] 1.1 `i_cache_prefetch.h`：驱动内层 cache 的 **5 个输入**
      （`addr` / `req` / `axi_ready` / `axi_data` / `axi_resp_valid`）
- [x] 1.2 `i_cache_prefetch.h`：**转发**内层的 AXI 主端输出
      （`io().axi_addr <<= icache.io().axi_addr;`
      `io().axi_valid <<= icache.io().axi_valid;`）
      ⚠️ **不**在 wrapper 层重推导（`design.md` §2.4「为什么 MUST 转发」三条理由：
      AXI 的 VALID 不得等 READY / 内层输出不留死逻辑 / 值同源）
- [x] 1.3 `d_cache_write_buffer.h`：`io().ready` ← `io().req & io().we & io().axi_ready`
      （语义 =「本次写**已被 AXI 接受**」）、
      `io().axi_valid` ← `select(io().req & io().we, true, false)`
      ⚠️ **`axi_valid` MUST NOT 含 `io().axi_ready`**（AXI：VALID 不得等 READY
      ⇒ READY 为低时写**根本不出现在总线上** ⇒ 本 wrapper 无状态/队列
      ⇒ **静默丢写**；见 `design.md` §2.4「`axi_valid` MUST NOT 依赖 `axi_ready`」）、
      `io().axi_addr` ← `io().addr`、`io().axi_wdata` ← `io().wdata`、
      `io().axi_we` ← `io().we`
- [x] 1.4 **5 处 C++ `if` 全部清除**（含两处空体注释块）⇒ grep 护栏输出为空
- [x] 1.5 ⚠️ 确认**每个** `ch_out` 端口都被**无条件**驱动（无「条件为真才 `<<=`」）
      —— 实测：`ICachePrefetch` 5 个 `ch_out`（data/ready/miss/axi_addr/axi_valid）
      + `DCacheWriteBuffer` 5 个（ready/axi_addr/axi_wdata/axi_we/axi_valid）
      ⇒ **10/10 无条件驱动** ✓（每条都有「跟随自己的输入」的行为断言钉住）
      ⇒ ⚠️ 另**删掉 2 处死代码**：`next_line_addr` / `get_block_addr`（design §8.4）

## 2. 红 / 绿

- [x] 2.1 反转 `test_cache_smoke.cpp` 的两条断言：
      ⚠️ **不是**把 `== false` 改成 `== true`（那只证明「有驱动」，证不出「随输入变化」）
      ⇒ 改为「**同一 TEST_CASE 内依次改变输入** ⇒ 逐个输出断言**跟随自己的输入**变化」：
      `req` 变 ⇒ `ready`/`axi_valid` 变；`addr` 变 ⇒ `axi_addr` 变；
      `wdata` 变 ⇒ `axi_wdata` 变；`we` 变 ⇒ `axi_we` 变
- [x] 2.2 ⚠️ **READY 依赖的判别力用例**（新增，堵住「静默丢写」）：
      把 `io().axi_ready` **拉低** ⇒ 断言 `axi_valid` **仍随** `req`/`we` 变化
      （⇒ 证明 `axi_valid` **不**依赖 `axi_ready`）
      ⇒ ⚠️ 现有双后端用例把 `axi_ready` 恒驱 1 ⇒ 该门控**完全不可观测**
      ⇒ 必须有这一条独立用例
- [x] 2.3 ⚠️ **更新该文件的头部注释**（它钉住了旧事实「未驱动 ⇒ 读 0」）
- [x] 2.4 ⚠️ **实测**确认 `ICachePrefetch` 的 `miss == true` 断言（接上 `addr`/`req` 后）**仍绿**
      —— 实测**仍绿** ✓（内层 valid 位从未被置 1 ⇒ 冷 miss 恒成立）
      —— ⚠️ 另**补了计划期未设计的判别力最强一条**：内层 cache 的 **fill → hit** 路径
      （`axi_resp_valid` + `axi_data` ⇒ 填充 ⇒ 再读同一块 ⇒ hit ⇒ `miss`→0 且
      `data` 返回**缓存值**）⇒ design §8.3
- [x] 2.5 **CMake 三属性**：`riscv_mini_cache_smoke` 加 `verilator_e2e` 标签 +
      `RESOURCE_LOCK verilator_backend` + verilator PATH 注入
      （照 `examples/riscv-mini/CMakeLists.txt:113-118` 的
      `test_rv32i_soc_dual_parity` 写法）
- [x] 2.6 双后端用例（interpreter + verilator 逐拍全等）进 `riscv_mini_cache_smoke`
      —— 用仓内**共享**的 `DUAL_BACKEND_TEST_PAIR` 宏（`tests/dual_backend_fixture.h`）
      ⇒ **不**复制粘贴 SoC 测试里的本地 helper
      ⇒ 实测：**8 个 TEST_CASE / 58 断言**全绿（4 对 = interpreter + verilator）
- [x] 2.7 `ctest -R riscv_mini_cache_smoke` 绿
      ⚠️ ⚠️ **命令名必须是 `riscv_mini_cache_smoke`** ——
      `ctest -R test_cache_smoke` **匹配 0 个测试且 exit 0**
      （实测 `ctest -N -R test_cache_smoke` ⇒ `Total Tests: 0`）
      ⇒ 那是**静默「验过」什么也没跑** ⇒ MUST 用真实 target 名

## 3. 精化（判别力自检）

- [x] 3.1 每条断言带**反向期望**（`X_old != X_new`）并留痕
- [x] 3.2 确认「输出随输入变化」类断言**真的**能转红（不是恒真断言）
      —— 实测：M1–M5 **5/5 全转红**（每条都**先确认编译成功**）⇒ 证明这些断言
      **不是恒真** ✓（详见 §4）

## 4. 变异验证（每条**先确认编译成功** ⇒ 否则跑旧二进制 ⇒ 假绿）

- [x] 4.1 **M1** 把无条件赋值退回建图期 C++ `if` ⇒ 转红（**核心判据**）
- [x] 4.2 **M2** ⚠️ 把 `axi_addr` 的驱动改成常量 `0` ⇒ 转红
      （初稿写「**删掉**驱动」**前提不成立** ⇒ 驱动是**新增**的）
- [x] 4.3 **M3** 把 `ready` 的条件取反 ⇒ 转红
- [x] 4.4 **M4** ⚠️ 把内层 cache 的 `addr`/`req` 驱动改成**常量 `0`** ⇒ 转红
      （⚠️ **不是**「断开驱动」—— 「断开」读到的是**未定义值** ⇒ 转红**不确定**
      ⇒ design §8.2）
- [x] 4.5 **M5** ⚠️ 把 `axi_valid` 的条件**加上** `io().axi_ready` ⇒ 转红
      —— ⚠️ **审稿第 1 轮补**：M5 需要**独立的 READY 依赖用例**才有判别力
      （`axi_ready` 恒驱 1 的用例**抓不到**）⇒ 已加（见 §2.2）
- [x] 4.6 **M6** ⚠️ 去掉内层的块对齐（`axi_addr` 改为直传 `addr`）⇒ 转红
      —— ⚠️ **审稿第 1 轮补**：初稿的地址断言**全用已对齐地址** ⇒ 「对齐」与
      「不对齐」两种**相反**语义**给出同一个值** ⇒ **零判别力** ⇒ 断言改用
      **不对齐**地址（`0x80000042` / `0x2003`）⇒ 本变异才转红 ✓
- [x] 4.7 **M7** ⚠️ 给 `DCacheWriteBuffer::axi_addr` **错加**块对齐 ⇒ 转红
      —— 与 M6 **相反**的语义 ⇒ 两条各自钉住「对齐」与「直通」
- [x] 4.8 全部还原 ⇒ **重建** ⇒ 复跑全绿（build 目录不得留变异二进制）
      —— 实测：还原后 **58 断言 / 8 用例**全绿 ✓
      —— ⚠️ 变异 M6 动的是**内层 cache**（`i_cache_sync.h`）⇒ 脚本**额外**备份/
      还原该文件 ⇒ 事后 MUST 确认它未被污染
- [x] 4.9 ⚠️ 核对：每条变异的实际转红项 MUST 与上面的「预期」**一致**
      —— 实测：**7/7 全转红**（每条都**先确认编译成功**）✓
      —— ⚠️ 变异 M6/M7 是**审稿第 1 轮**新增的（初稿的 5 条**没有**它们）⇒
      初稿的地址断言**零判别力**（见 4.6/4.7 的注）⇒ 补齐后才谈得上「地址正确」

## 5. 回归与工程

- [x] 5.1 `ctest -LE perf` 零回归 + `run_all_ported_tests.sh` 28/28 +
      `run_all_samples.sh` 35/35
- [x] 5.2 8 个 `scripts/verify_*.sh` 全 exit 0
- [x] 5.3 计数核对（`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` / `ctest -L verilator_e2e`）
      —— **实测**：`CTEST_TOTAL` **202 不变** / non-perf **191 不变** /
      `EXAMPLES_TEST_SOURCES` **17 不变**（无新测试文件 ⇒ 本 target 已存在）/
      `ctest -L verilator_e2e` **37 → 38**（加 `verilator_e2e` 标签 ⇒ +1）
      ⇒ ⚠️ 根 `AGENTS.md` 此前**没有** `verilator_e2e` 的**活**锚点 ⇒ **新建**
      `- VERILATOR_E2E_TOTAL: 38` + 护栏第 2 条检查（design §8.6）
      ⇒ ⚠️ 加 `verilator_e2e` 标签 ⇒ `verilator_e2e` **37 → 38**
      （`CTEST_TOTAL` **不变** —— 该 target **已**注册进 CTest，只是加标签）
      ⇒ 以**实测**为准并同步锚点
- [x] 5.4 `openspec validate --specs --strict` 不因本 change **新增**红

## 6. 文档与簿记

- [x] 6.1 根 `AGENTS.md §KNOWN LIMITATIONS` 的「cache wrapper 分支驱动输出从不发射」条目
      **移除**（该缺陷已修）
      —— 已改为 `~~...~~ —— ✅ **已修复**`（保留**历史**形态 + 归因 + 遗留债务）
      ⇒ 另加 §NOTES 条目（根因 / 修法 / 判别力 / 3 处前提订正 / 计数）
- [x] 6.2 ⚠️ **两个 wrapper 的头注释 MUST 改为如实描述**
      （`i_cache_prefetch.h:1-18` 的「预取下一个 line / 预取缓冲 2 条目」、
      `d_cache_write_buffer.h:1-24` 的「写缓冲队列 4 条目 / 写合并 / AXI 仲裁」
      ⇒ 直通式下**均未实现** ⇒ ZERO-DEBT「无未文档化变更」）
- [x] 6.3 ⚠️ 如实登记**遗留债务**（`design.md` §7）：`DCacheWriteBuffer` **并不真缓冲**
      （直通式 ⇒ 名字 over-promise，`ready` 无背压语义）；
      内层 cache 的 `miss`(1 拍) 与 `axi_addr`(当拍) 存在 1 拍错位（属 cache 本体）
- [x] 6.4 `feat-riscv-mini-rtl-debt` 的验收项勾选 + `roadmap.md`
- [x] 6.5 ⚠️ **按实测更新根 `AGENTS.md` 的计数锚点**（`verilator_e2e 37 → 38`）
      ⇒ `scripts/verify_test_counts.sh` exit 0
      —— ⚠️ **前提不成立**：根 `AGENTS.md` **没有** `verilator_e2e` 的**活**锚点
      （`37` 只出现在**历史** NOTES 条目里 ⇒ 记录当时的真值 ⇒ **不可**改）
      ⇒ **新建** `- VERILATOR_E2E_TOTAL: 38` 锚点 + 护栏**第 2 条**检查
      ⇒ 护栏自检：改成 37 ⇒ exit **1**；改回 38 ⇒ exit **0** ✓（design §8.6）

## 7. 审稿与归档

- [x] 7.1 对抗审稿（critic，`review_profile: plan, build`）⇒ ≤ 2 轮
      —— **第 1 轮 = REJECT**（2 BLOCKER + 7 SHOULD_FIX + 6 NIT；`mergeable: no`）
      ⇒ 逐条处置留痕见 `design.md` §8.8
- [x] 7.2 审稿处置（BLOCKER ⇒ 返工；SF / NIT ⇒ 同批闭合留痕）
      —— **2 个 BLOCKER 均已闭合**：
      ① delta 引用了**自己的 change 名** ⇒ `verify_openspec_delta.sh` 的 **R1**
      命中 ⇒ **CI 硬门必红** ⇒ 已删（理由改记 design.md）⇒ 护栏复跑 exit 0 ✓
      ② spec 的 `ready` 语义子句是**写**语义 ⇒ 字面命中**两个** wrapper ⇒
      `ICachePrefetch`（读主端、**无** `we`/`wdata`）**无法满足** ⇒ 会**冻错主 spec**
      ⇒ 已拆成**按组件分别适用**的两条子句（读侧补「保持 `addr`/`req`」）
      —— 7 SF + 6 NIT **同批闭合**（含 2 条**新增**变异 M6/M7 与**不对齐**地址断言）
      —— ⚠️ **范围外观察（backlog）**：主 spec 的「hit-path uses **aread**」与
      ADR-041（Sync 用 1-cycle `sread`）**直接矛盾** ⇒ **既有**漂移 ⇒ 独立立项
- [x] 7.3 `openspec archive riscv-mini-cache-wrapper-live-logic-2026-10 --yes`
      （**禁** `--skip-specs`）
      —— 已归档：`openspec/changes/archive/2026-10-28-riscv-mini-cache-wrapper-live-logic-2026-10/`
      ⇒ 主 spec `openspec/specs/cpu-cache-subsystem/spec.md` 合并
      **+1 added / ~2 modified**（13 条 Requirement）⇒ **未**用 `--skip-specs` ✓
      ⇒ 主 spec 的 `## Purpose` **手工**补充 wrapper 层（delta 的 Purpose 不合并）
