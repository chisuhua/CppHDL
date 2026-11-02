# 提案：路线 A（E 批）—— `ICacheAsync` / `DCacheAsync` 的 0-cycle 语义进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **E 批** + §4 的「可转性 4 问」
- **规格影响**：⚠️ **本 change 有 spec 变更** ⇒ capability `dual-backend-test-coverage` **MODIFIED ×1** ⇒ 归档**不得**用 `--skip-specs`
- **规模**：1 个新测试文件（3 个 `DUAL_BACKEND_TEST_PAIR` / 73 断言/后端）+ 1 条 CMake 注册 + 1 个 spec delta + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage` · ADR-041（cache Sync/Async 变体）

## Why（两件事，第二件是本 change 的真正产出）

### 一、Async cache 的双后端覆盖 = 0

`ICacheAsync` / `DCacheAsync` 的 0-cycle `aread` 语义此前**只在 interpreter 上跑过**
（`tests/chlib/test_cache_complete.cpp` 是 `add_catch_test`）⇒ 生成的 Verilog 里
`assign proxy = mem[addr];` 这条路径**从未与 Simulator 逐周期对过**。

⚠️ **只取 Async 3 个用例**：Sync 6 个已被 `tests/chlib/test_cache_sync.cpp`（**10 pair**）覆盖
⇒ 转 Sync = **重复计数**（4 问 ①）。

### 二、🔴 用**事实**裁决 capability 里一条**自相矛盾**的条款

`openspec/specs/dual-backend-test-coverage/spec.md` 里两条条款互斥：

| 位置 | 内容 |
|---|---|
| Requirement「cache 双后端 MUST 按 Sync/Async 分类」 | Async dual-run **可以**走 parity，「『Async 强制 parity 会永远红』这一前提**已不成立**，禁令解除」 |
| Requirement「cache Sync runtime test SHALL 走 macro」的 Scenario「spec R6 排除清单遵守」 | runtime test 涉及 `ICacheAsync`/`DCacheAsync` ⇒ **MUST NOT** 用 `DUAL_BACKEND_TEST_PAIR`（理由**还是**那条已消除的 skew） |

⇒ ⇒ **本 change 用实测裁决**：本文件 3 个 pair 在 verilator 半边**全绿**
⇒ ⇒ 后者的前提**确已消失** ⇒ ⇒ 该 Scenario 的 `MUST NOT` **被证伪** ⇒ ⇒ 原地订正为 `MAY`
+ 三条**实测得来的**约束（见 delta）。

⚠️ **订正方式的两条硬约束**：
1. **Scenario 标题 MUST NOT 改** —— OpenSpec CLI 按**标题**匹配 Scenario，改标题 =
   「丢弃旧 + 新增」⇒ **拒绝归档**（ADR-052 / ADR-053 已两次踩过 ⇒ 只改正文）；
2. **不是无条件放行** —— delta 明写「若某个 Async dual-run 实测不绿，该用例 MUST 回退
   golden-value 并记录分歧」⇒ ⇒ **禁令只对已被证伪的前提失效**，不是「以后都随便用」。

## ⭐ 相对原用例的 4 个增量（原用例**从未**断言的维度）

| # | 增量 | 为什么重要（判别力证据） |
|---|---|---|
| ① | **`miss == 1`** | 原用例**从未**在 miss 拍断言（原因见 T6）⇒ 变异 M3/M7（填充失效）**只**能被它抓到 |
| ② | **`ready` 在 miss 时跟随 `axi_ready`** | 原用例的 miss 拍 `axi_ready` 恒 1 ⇒ `select(hit,1,axi_ready)` 接反也看不出来（M2） |
| ③ | **写直达真的写进了 cache**（`rdata == 0xCAFEBABE`） | 原用例只断「hit 了 / axi_valid==0」⇒ 若 `do_write & hit_way0` 被写成 `& hit_way1`（way0 永不写），**原用例仍然全绿**（M4） |
| ④ | **`axi_wdata` / `axi_we` / `axi_addr`** | 三个 AXI 主端输出此前零断言（M9/M10/M8） |

## 🔴🔴 陷阱 T6（本批实测挖出，比本批本身重要）

**填充在同一个 `tick()` 内就可见 ⇒ 「填充拍之后」`miss` / `axi_valid` 不可观测。**

机制：`aread` 是 **0-cycle 组合**读，而填充是 `ch_mem::write`（**沿上**提交）；
`Simulator::tick()` 的**最后一步是 `eval_combinational()`** ⇒ ⇒ 沿之后**再求值一次**
⇒ ⇒ 刚写进去的 valid/tag 在**同一 tick 的收尾求值**里就被 `aread` 读到 ⇒ `hit=1`
⇒ ⇒ `miss` 已经是 0、`axi_valid`（= miss & req）已经是 0。

⇒ ⇒ **要观测 `miss=1`，MUST 用「冷地址 + `axi_resp_valid=0`」的拍**（不填充 ⇒ 下一拍仍 miss）。
⇒ 本文件每一组的**第一拍**就是这一形态。

⚠️ 我首版把「填充拍」的 `miss` 期望写成 1 ⇒ ⇒ **两后端一致地给 0** ⇒ ⇒ 按
「两后端一致 ⇒ 同一处推导错」这条判据，改的是**期望值**，不是实现（第 3 次命中该判据）。

## ⚠️ 我自己发现并补掉的两处判别力缺口（不是审稿要求）

| 缺口 | 怎么发现的 | 补法 |
|---|---|---|
| **块对齐掩码**接不接都看不出来 | 首版三条向量**全部 16 字节对齐**（`0x2000` 掩码后还是 `0x2000`） | 补非对齐地址 `0x2004`：`DCacheConfig` 是 `OFFSET_BITS=4 / INDEX_BITS=7` ⇒ 同 line 内偏移**不参与 hit 判定** ⇒ 仍应 hit，而 `axi_addr` MUST 被掩码成 `0x2000` + 变异 **M8** |
| **`axi_valid & req` 门控**不可见 | `req` 恒 1 ⇒ 去掉 `& req` 无差别 | 补一拍「**冷地址 + `req=0`**」（`miss=1` 且 `axi_valid=0`）+ 变异 **M11** |

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 转红 |
|---|---|---|
| M1 | ICache `data` 的 hit/直通**接反** | `:176` |
| M2 | ICache `ready` 的 hit/`axi_ready` **接反** | `:177` |
| M3 | ICache 填充门控 `& miss` → `& !miss`（永不填充） | `:175` |
| M4 | DCache 写直达 way0 门控 → `& hit_way1`（way0 永不写） | `:235` |
| M5 | DCache `axi_valid` 的 miss_read/we **互换** | `:207` `:237` |
| M6 | DCache `rdata` 的 hit/直通**接反** | `:204` `:235` |
| M7 | DCache `fill_way0_en` **取反**（永不填充） | `:203` `:233` |
| M8 | DCache **块对齐掩码去掉** | `:216` |
| M9 | DCache `axi_wdata` 接成 `axi_rdata` | `:246` |
| M10 | DCache `axi_we` 接成 `req` | `:214` `:244` |
| M11 | ICache `axi_valid` 的 **`& req` 门控去掉** | `:178` |

⇒ **11/11 全抓**，且每个断言组都有可追溯的变异（含我在补缺口时新增的 M8/M11）。

## 另登记一个既有缺陷（本批**绕过**，独立立项）

`include/cpu/cache/i_cache_async.h` / `d_cache_async.h` **不自包含**：它们使用
`ICacheConfig` / `DCacheConfig`（定义在 `*_sync.h`）却**不** include 它 ⇒ ⇒
单独 include async 头**编译不过**（实测 `'DCacheConfig' was not declared in this scope`）
⇒ ⇒ 违反根 `AGENTS.md` 的 CONVENTIONS「Headers self-contained」。
本文件通过**先 include `*_sync.h`** 绕过；修它只需各加一行 include，但会改变公开头的
依赖图 ⇒ 不在本批做。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_cache_async_dual.cpp`（**新**） | 3 pair：`ICacheAsync`（冷不填充 / 冷填充 / hit / `req=0` 门控，4 拍 × 5 输出）· `DCacheAsync` 读（3 拍 × 6 输出，含非对齐地址）· `DCacheAsync` 写直达（5 拍 × 7 输出）⇒ **73 断言/后端** |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test`（含 T6 与 4 个增量的注释） |
| `specs/dual-backend-test-coverage/spec.md`（delta） | **MODIFIED ×1**：订正「spec R6 排除清单遵守」Scenario 的正文（**标题不动**） |
| `AGENTS.md` | 4 个计数锚点 + NOTES 计数 + 本批 NOTES 条目 |
| 计数锚点 | `CTEST_TOTAL` 231 → **232** · `VERILATOR_E2E_TOTAL` 63 → **64** · `DUAL_BACKEND_PARITY_TOTAL` 54 → **55** · `PAIRS` 254 → **257** · non-perf 220 → **221** · `tests/chlib/test_*.cpp` 45 → **46** |

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**146 == 2 × 73**
- [x] 变异 **11/11 全抓**（每条先确认仍可编译；还原用 `cp` 后重建）
- [x] 全部输入端口**每拍逐个显式驱动**（ICache 5 个 / DCache 7 个）
- [x] 期望值**手推**（公式逐条对照实现；一次推导错已被双后端一致证伪并改正）
- [x] **原文件 `tests/chlib/test_cache_complete.cpp` MUST NOT 修改**（已遵守）
- [x] **Async 在 verilator 半边真的跑了** ⇒ `missing` 前提被事实证伪 ⇒ 据此订正 spec
- [x] Scenario **标题未改**（只改正文）⇒ CLI 能匹配
- [x] 全量重建后 `ctest -LE perf` 零回归
- [x] 计数锚点同步且两个计数护栏双绿
- [x] 归档**不用** `--skip-specs`（本 change 有 spec delta）
