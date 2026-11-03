---
name: stream-builder-chain-silent-word-loss-2026-11
priority: P1
source: P1-a 实施期实测（探针 `/tmp/opencode/p1/p1a_probe11.cpp` + `dbg_builder.cpp`，2026-11-01）
phase: phase-5
category: chlib
type: bugfix
created_at: 2026-11-01
evidence: unmeasured
---

# Improvement: `StreamBuilder` 的链式 `queue<>()` **静默丢字** + **上游反压丢失**

- **Date drafted**: 2026-11-01
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已修复**（2026-11-01 · ADR-055 + change
  `stream-builder-and-pipe-ready-ownership-2026-11` ⇒ 已归档
  `openspec/changes/archive/2026-11-01-stream-builder-and-pipe-ready-ownership-2026-11/`）
  ⇒ ⇒ **上游反压**已落地（`include/chlib/stream_builder.h` 的
  `drive_pending_ready(result.push_stream.ready)` + `adopt_stage_output(...)`
  ⇒ ⇒ 链内闭包不变式成立）⇒ ⇒ 判据 =
  `tests/test_stream_builder.cpp` 的「queue() 的反压 MUST 能传过 builder 链」
  （含 `source.ready.impl() != nullptr` 断言）+ `tests/test_stream_builder_backpressure_dual.cpp`
  （**双后端** parity）⇒ ⇒ ⚠️ 本文件原先的「静默丢字」标题本身**已被实测推翻**
  （真缺陷只是「上游反压从未建立」）⇒ ⇒ 标题保留为历史线索。
- **来源**：change `stream-forward-ready-closure-p1a-2026-11` 的实施期探针
  ⇒ ⇒ 该 change 的 builder 子项**已移出**（本项独立立项）
- ⚠️ **关联**：capability `chlib-stream-ready-ownership` · ADR-054 §3.2 第 1 条
  · `.rddf/improvements/stream-forward-ready-closure-2026-10.md` §2.2-2

## 1. Why (背景与动机)

🔴🔴🔴 **2026-11-01 实测订正：本项的「静默丢字」前提【被推翻】** ⇒ ⇒
**真缺陷是「上游反压从未建立」** ⇒ ⇒ ⇒ **⇒ 「丢字」是观测解读错误造成的
假症状**（⇒ ⇒ 这是本仓登记的「**探针输出的解读必须被独立复核**」**第四次**
生效 ⇒ ⇒ ⇒ **⇒ 前三次**分别踩在 `ch_reg` 相位、探针节点创建时机、
`ch_reg` 初值机制上）。

### 🔴 实测证据（探针 `/tmp/opencode/a3/probe_chain.cpp` + `probe_bp.cpp`，2026-11-01）

**① 正确接线**（`as_master()` 建立字段节点 + `<<=` 数据流边）+ 消费端恒 ready
+ 深度 4 + **推入 10 个**可区分 word：

```
t   source.ready   out.valid   out.payload
 0   0              1          10
 1   0              1          11
 ...
 9   0              1          19      ← **10 个 word 全部无损通过**
10   0              0          16      ← 弹光后的**残留**（不是丢字）
⇒ ⇒ 观测到 **10 个不同 word**（容量 4，但消费端恒 ready ⇒ 每拍弹一个
   ⇒ ⇒ FIFO 从不积压 ⇒ ⇒ 容量限制**从未触发**）⇒ ⇒ **零丢字**
```

⇒ ⇒⇒ **「静默丢字」不成立** ✔

### ⚠️⚠️ 台账原描述的两个观测点，**逐条是误读**

| 原描述 | 实测真相 |
|---|---|
| 「第 10 拍 `out.payload=0x00 out.valid=0`（期望 5A 1）❌」 | **消费端恒 ready ⇒ 每拍弹一个 ⇒ 弹光后自然为空** ⇒ ⇒ **不是丢字**（是**消费完成**） |
| 「链长 2：第 1 拍后 `out.payload=0x00 out.valid=0`」 | **每级 `sread` 1 拍 ⇒ 链长 2 ⇒ 延迟更深** ⇒ ⇒ 第 1 拍为空是**正常**的 ⇒ ⇒ **不是丢字** |

### 🔴⚠️ 探针自身的接线缺陷（**第 4 次元教训的载体**）

原探针用 `operator=` 接线（`source.payload = payload`）且**未** `as_master()`
⇒ ⇒ ⇒ `source.ready` 是**空节点** ⇒ ⇒ ⇒ 读它 ⇒ 💥 **ABORT**
（`ch_bool implementation is null`）⇒ ⇒ ⇒ **必须**先 `as_master()`
（建立字段节点 + 方向）⇒ ⇒ 再用 `<<=` 接数据流边。

⇒ ⇒ ⚠️ **既有测试也踩同一个坑**：`tests/test_stream_builder.cpp:116-117`
用 `source.payload = payload; source.valid = valid;`（`operator=`）且**无**
`as_master()` ⇒ ⇒ ⇒ 它**不读** `source.ready` ⇒ ⇒ ⇒ **不崩** ⇒ ⇒ ⇒
⇒ 但它也**永远无法**观测上游反压 ⇒ ⇒ ⇒ **覆盖面缺口**（非假绿）。

### 🔴 真缺陷（**唯一成立的**）：上游反压**从未建立**

```
E10 builder 链（正确接线）：`source.ready` **恒 0**（16 拍全 0）
E11 手写 2 级 + **显式** `source.ready <<= f1.push_stream.ready`：
    `source.ready` **恒 1**（机制**正常**，本场景因消费太快未触发）
```

⇒ ⇒ ⇒ `stream_fifo` **不驱动**入参 `ready`（写使能只用 `input_stream.valid`
⇒ ⇒ 内部 `sync_fifo` 自保护 ⇒ ⇒ **见 `include/chlib/stream.h` 的
`stream_fifo` 实现**）⇒ ⇒ ⇒ ⇒ **⇒ 没人驱动 `source.ready` ⇒ ⇒ 读到 0**
⇒ ⇒ ⇒ ⇒ 「FIFO 满 ⇒ 上游暂停」在 builder 链里**不可演示**
⇒ ⇒ ⇒ ⇒ 且上游若**遵守** ready 协议 ⇒ ⇒ ⇒ **永远推不动 = 死路**。

⚠️ 「中间级无条件弹」这个**机制**也成立（`current_stream_ = result.pop_stream`
⇒ ⇒ 中间级 `pop_stream.ready` 的驱动是**默认源** ⇒ ⇒ 恒就绪）⇒ ⇒
但由于**上游根本推不动** ⇒ ⇒ ⇒ **它不会造成可观测的丢字** ⇒ ⇒
⇒ ⇒ 「两者叠加 = 静默丢数据」这个**后果陈述不成立**。

### ⚠️ 为什么长期无人发现（**仍然成立**）

⚠️ `tests/test_stream_builder.cpp` 的既有用例**只做结构断言**（链能建起来）
⇒ ⇒ ⇒ **零行为覆盖** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **「假绿」**。
⚠️ 且 `samples/` 与 `examples/` 里的 builder 用例**不检查 payload 序列**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 数据丢了也没人看见**。
⚠️ **补充（2026-11-01）**：既有测试的**接线**本身也是错的（`operator=`
+ 无 `as_master()`）⇒ ⇒ ⇒ 它**结构上无法**观测上游反压。
## 2. What Changes (变更范围)

### 2.1 🔴 **候选修法**（**2026-11-01 实测后重评** ⇒ ⇒ 裁决范围已缩小）

⚠️⚠️ **实测订正了原修法表的两处前提** ⇒ ⇒ ⇒ **⇒ 修法 B 不再需要**
⇒ ⇒ ⇒ **⇒ 修法 C 已被 A 吸收** ⇒ ⇒ ⇒ **⇒ 只剩一个语义裁决点**。

- **A｜`build()` 延迟连接**（**现在唯一可行的方向**）：builder 保存链上
  **所有** FIFO 的结果对象（⇒ ⇒ **加私有成员**）⇒ ⇒ 在 `build()` 里把
  **中间级**的 `pop_stream.ready` 接到**下一级**的 `push_stream.ready`
  （= `!full`）⇒ ⇒ ⇒ ⚠️ **代价 1**：builder 的链是**逐步**建立的 ⇒ ⇒
  第 1 次 `queue()` 时**不知道**后面还有几级 ⇒ ⇒ ⇒ **连接 MUST 延迟**
  ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这只是实现细节，不是障碍**；
  ⇒ ⇒ ⇒ ⚠️ **代价 2（唯一剩下的障碍）**：中间级的 `pop_stream.ready` 被接到
  `!full` ⇒ ⇒ ⇒ **消费端失去覆盖权** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 与 ADR-054 §3.2
  第 1 条（读使能 = 消费端的 ready，可覆盖）存在张力** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
  **⇒ 需 architect 裁决**（见 §2.2）；
  ⇒ ⇒ ⇒ ✅ **实测支撑（2026-11-01）**：手写 2 级 + 显式回接
  （`f1.pop_stream.ready <<= f2.push_stream.ready`）⇒ ⇒ **零丢字**
  （10 个 word 全齐）⇒ ⇒ 且 **多驱动守卫不误炸**（对下游 `connect_ready`
  **不抛**；只有「已覆盖后再调」才抛 = **正确**的多驱动检测）
  ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 技术可行性已实测确认**；
- ~~**B｜给 `sync_fifo` 加「写口 ready」输出**~~ ⇒ ⇒ ❌ **不需要**（**原前提
  已被推翻**）：原表称「FIFO 没有写口 ready 输出」⇒ ⇒ ⇒ **实测不成立** ——
  `sync_fifo` 的 result 确实**没有** ready 字段（只有 `empty`/`full`/`q`/
  `count`/`q_async`）⇒ ⇒ **但组合子 `stream_fifo` 的 result【有】**：
  `include/chlib/stream.h:469` `result.push_stream.ready = !fifo_result.full;`
  ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「写口 ready」在【组合子层】已经存在** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
  **⇒ 无需改 `chlib/fifo.h` 的公开 API** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
  **⇒ ⇒ 本项【不需要】独立 ADR**（公开 API 零变更）**；
- ~~**C｜只修缺陷 1（上游反压）**~~ ⇒ ⇒ ❌ **已被 A 吸收**：原表的 C 是
  「只修上游反压、留缺陷 2（丢字）」⇒ ⇒ ⇒ 但**「丢字」不是缺陷**
  （§1 已实测推翻）⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 修好上游反压 = 修完** ⇒ ⇒ ⇒ ⇒ ⇒
  **⇒ 「半修 = 假绿」的顾虑随之消失**。
### 2.2 ⚠️ **本项 MUST 与 ADR-054 的关系**

⚠️ ADR-054 §3.2 第 1 条规定 fifo 类的读使能 = **消费端的 ready**（**可覆盖**）
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 修法 A 会让「中间级」的读使能**不再可覆盖**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 必须由 architect 明确**：
「中间级」是否属于 ADR-054 所说的「fifo 类」⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 若属于 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 修法 A 与之冲突**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 若不属于（视为「链内实现细节」）⇒ ⇒ ⇒
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 修法 A 可行** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 这是必须先裁决的点**。

## 3. Acceptance (验收判据 —— MUST 可勾选)

> ⚠️ **2026-11-01 订正**：原判据以「**零丢字**」为核心 ⇒ ⇒ 但 §1 已实测
> 推翻「丢字」这个症状 ⇒ ⇒ ⇒ **判据 MUST 改为「上游反压」**（唯一真缺陷）
> ⇒ ⇒ ⇒ **⇒ 「零丢字」降级为「回归锁」**（防未来引入真丢字），**不是**缺陷判据。

- [ ] **效果判据（唯一的缺陷判据）**：调用方的 `source.ready`
      （`as_master()` 后）⇒ ⇒ **MUST 有驱动**（`num_srcs() > 0`）
      ⇒ ⇒ 且其**值 MUST 随 FIFO 占用变化**（FIFO 满 ⇒ ⇒ 落 0
      ⇒ ⇒ ⇒ **⇒ 当前实测恒 0 ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 修复后 MUST ≠ 恒 0**）
      ⇒ ⇒ ⚠️ **判据 MUST 是「随容量变化」而非「某个字面量」**
      （消费速率影响占用 ⇒ ⇒ 锁死字面量会冻错主 spec）
- [ ] **效果判据**：**回接修法**（中间级 `pop_stream.ready` ← 下一级
      `push_stream.ready`）⇒ ⇒ **零丢字** ⇒ ⇒ 且 `source.ready` **落 0**
      ⇒ ⇒ （**手写 2 级已实测**：零丢字 + 守卫不误炸 ⇒ ⇒ ⇒ **修法 A 的技术
      可行性已确认** ⇒ ⇒ ⇒ 剩下的**只有契约语义裁决**）
- [ ] **回归锁（非缺陷判据）**：`queue<4>()` 推入 4 个可区分 word ⇒ ⇒
      **10 拍内** `out.payload` 序列 MUST 含全部 4 个（实测 `0x10..0x13`
      各**恰好一次**）⇒ ⇒ ⇒ 防未来引入**真**丢字
- [ ] **接线纪律**：既有测试 `tests/test_stream_builder.cpp:116-117` 用
      `operator=` 接线且**无** `as_master()` ⇒ ⇒ ⇒ **MUST 改成**
      `as_master()` + `<<=` ⇒ ⇒ ⇒ 否则它**结构上无法**观测上游反压
      （⇒ ⇒ 读 `source.ready` 会 💥 **ABORT**：空节点）
- [ ] **变异**：把 `build()` 的延迟连接去掉 ⇒ ⇒ 上游反压判据 MUST **转红**
      （⇒ ⇒ ⚠️ **先确认编译成功**）
- [ ] **变异**：把中间级连接从 `<<=` 改 `operator=` ⇒ ⇒ MUST **转红**
- [ ] ⚠️ **穷举**：builder 的**全部**链式方法（`queue` / `haltWhen` /
      `continueWhen` / `map` / `throwWhen` / `takeWhen`）⇒ ⇒ 每个都 MUST
      参与「零丢字」判据（⚠️ 「只查已知符号 ⇒ 漏掉同族入口」是本仓登记的
      护栏盲区）
- [x] ✅ **已落地（A3 · 2026-11-01）**：`connect_ready` 的**多驱动守卫**在
      **串联场景 MUST NOT 误抛** ⇒ ⇒ 已加行为用例
      （`tests/chlib/test_stream.cpp` 的 `[stream][ready][series]` · 3 断言）
      ⇒ ⇒ + 新静态判据 **R9**（守卫的比较式 + `CHFATAL_EXCEPTION`）
      ⇒ ⇒ + 新投毒 `connect-ready-guard-removed`（**双判据**红）
      ⇒ ⇒ ⇒ **护栏自检 10 种投毒全抓**
- [ ] `ctest -LE perf` 零回归 · **计数锚点不变**
## 4. Capabilities (契约影响)

- **可能** MODIFIED `chlib-stream-ready-ownership`（若「中间级」属该能力
  的「fifo 类」⇒ ⇒ 需加条款）
- **可能** MODIFIED `chlib-stream-semantics`（fifo 语义）
- ⚠️ 若选修法 B ⇒ ⇒ **必须**新建 ADR（`chlib/fifo.h` 的公开 API 变更）

## 5. Impact (影响面)

- **改动文件**：`include/chlib/stream_builder.h`（私有成员 + `build()` 语义）
  · 可能 `include/chlib/fifo.h`（修法 B）· `tests/test_stream_builder.cpp`
  · 可能 `samples/`（builder 用例）· 护栏脚本
- **爆炸半径**：**中** —— builder 的**公开行为**变化（从「丢字」变「不丢字」
  ⇒ ⇒ ⇒ **既有断言可能依赖旧行为** ⇒ ⇒ ⇒ MUST 逐项复核）
- **风险**：**中** —— ① 与 ADR-054 的张力（**需裁决**）② `build()` 语义变化
  ③ 「零丢字」会**暴露**既有的其它缺陷（⇒ ⇒ 好事，但会增加本项的修复面）
- **回滚路径**：`revert` 实施 commit ⇒ ⇒ 无持久化面
