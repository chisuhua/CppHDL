# Design: stream-forward-ready-closure-p1a-2026-11

## 1. 定位与范围

⚠️ 本 change = **P1-a**（`.rddf/improvements/stream-forward-ready-closure-2026-10.md`
§2.2-1 的三个子集中的第 1 个）⇒ ⇒ **只做**「纯修复 + 静默丢字」这一档：

| 子集 | 内容 | 本 change |
|---|---|---|
| **P1-a** ✅ | ⓐ 仅改连接手段：`mux` / `arbiter×4` / `join` / `filters×6` / `combine_with` ＋ **`StreamBuilder::queue<>()` 上游回接** | ✅ **本 change** |
| P1-b | `demux` 方向（行为变更 ⇒ 独立 ADR） | ❌ 不在范围 |
| P1-c | `fork` 同步语义（从未工作 ⇒ 等于新功能） | ❌ 不在范围 |

⇒ ⇒ **零行为变更承诺成立**（实测：转发类现状 = 「上游恒不 ready = 死路」
⇒ ⇒ 修复是「把死路变成活路」⇒ ⇒ **不是**行为变更）。

## 2. 🔴🔴 实施期的 10 个探针（**决定形态**）

⚠️ 探针位置 `/tmp/opencode/p1/p1a_probe{1..10}.cpp`（仓外 ⇒ 不可复算
⇒ ⇒ **输出如实登记在此** ⇒ ⇒ 任何后续读者 MUST 以此为准）。

### 2.1 转发类的 ready 链**全部断开**（探针 ①：5/5 CONFIRMED）

| 对象 | 实测 |
|---|---|
| `StreamBuilder::queue<4>().queue<4>()` | `in.ready` = **空节点** ⇒ ⇒ 上游永远收不到反压 |
| 单个 `queue<4>()` | 同上 |
| filters（`take_when` / `throw_when`） | `Invalid operands` ×2 ⇒ ⇒ 调用方与结果**两端都断** |
| `stream_join` | `a.ready` 空 + `output_stream.ready` **空** ⇒ ⇒ **两端都断** |
| `stream_arbiter_lock` | 构造期 `Invalid operands` ×2（**未**抛 ⇒ **静默**） |

### 2.2 🔴 **转发类现状 = 「上游恒不 ready = 死路」**（探针 ④）

```
in0.ready srcs=0   in1.ready srcs=0
in0.ready 值 = 0
⇒ ⇒ 结论：上游**恒不 ready** ⇒ ⇒ 现状是**死路** ⇒ ⇒ 修复不是破坏
```

⇒ ⇒ ⇒ **「修复 = 行为变更」不成立** ⇒ ⇒ ⇒ **零破坏承诺成立**。

### 2.3 🔴🔴 **`ch_stream` 的拷贝与原对象共享同一 ready 节点**（探针 ②：3/3）

```
a.ready impl=0x5639b4302040
b.ready impl=0x5639b4302040          ← 拷贝后**同一节点**
b.ready <<= sig ⇒ ⇒ a.ready srcs=1   ← 经副本驱动**到达原对象**
mux.input_streams[0].ready impl == in.ready.impl()
```

⇒ ⇒ ⇒ **组合子内部 `<<=` 能穿透副本驱动到调用方** ⇒ ⇒ ⇒
⇒ ⇒ **「调用方 MUST 先建立字段节点」是**唯一**的前置条件**
（节点存在 ⇒ ⇒ 可穿透；节点为空 ⇒ ⇒ `<<=` 另建节点 ⇒ ⇒ 副本专用 ⇒ ⇒ 失效）。

### 2.4 🔴 **形态 E（单阶段）在转发类上可行**（探针 ③ / ⑧：4/4 CONFIRMED）

```
探针 ③ A：消费端不就绪 ⇒ out.ready=0 in0.ready=0   ← 反压**穿透到调用方** ✔
探针 ③ B：不覆盖      ⇒ in0.ready=1                ← **恒就绪**（= 既有旧语义）✔
探针 ⑧：不调 connect_ready ⇒ a.ready=0            ← 单阶段可行 ✔
探针 ⑧ 反证：加一个消费者 ⇒ 值**更新** ✔
```

⇒ ⇒ ⇒ **单阶段成立**：组合子内部建「默认恒就绪」节点 ⇒ ⇒ 消费端**直接**
`<<=` 覆盖 ⇒ ⇒ **不需要** `connect_ready` ⇒ ⇒ ⇒ 心智模型与 fifo 一致
⇒ ⇒ ⇒ **无需调用方迁移**。

### 2.5 ⚠️⚠️ **三条被推翻的「推断」**（**元教训复刻**）

| # | 曾经的推断 | 实测结论 | 根因 |
|---|---|---|---|
| ① | 「`<<=` 覆盖在 mux 上不生效」 | ❌ **生效**（探针 ⑨：值 = 0 ✔） | ⚠️ **探针自身缺陷**：`static ch_bool` **跨 case 复用** ⇒ ⇒ 节点被污染 ⇒ ⇒ **探针输出的解读必须被独立复核** |
| ② | 「局部 `ch_bool` 作源 ⇒ ⇒ 不可被覆盖」 | ❌ **可被覆盖**（探针 ⑩ A ✔） | 同上（static 污染） |
| ③ | 「只写不读的节点不在 eval list ⇒ 值不更新」 | ❌ **不成立**（探针 ⑧ 反证组 ✔） | ⇒ ⇒ 该节点**有**消费者（`is_selected & output_stream.ready` 那个 and 节点）⇒ ⇒ ⇒ **组合子内部必须建立它** |

⇒ ⇒ ⇒ **教训**：「**探针输出的解读**也必须被独立复核」这条元教训在本轮**再次**
生效（连续 3 条推断被自己的探针推翻）⇒ ⇒ ⇒ ⇒ ⇒ **每条结论 MUST 有
**干净上下文**的对照**才能采信**。

### 2.6 🔴 **struct 成员默认构造后是空节点**（探针 ⑩ C）

```
r.default_ready_source impl=(nil)   ⇒ **空节点**（默认构造不建节点）
```

⇒ ⇒ ⇒ ⇒ **必须先 `member = ch_bool(true)` 再 `<<=`**（fifo 的既有做法）
⇒ ⇒ ⇒ ⇒ ⚠️ 否则 `<<=` 落在空成员上 ⇒ ⇒ `CHERROR`（**只打日志**）⇒ ⇒
**静默** ⇒ ⇒ ⇒ ⇒ ⇒ **这是「成员源」的唯一正确初始化顺序**。

## 3. 形态定稿（**10 个探针综合**）

### 3.1 三条实现纪律

① **组合子 MUST 立即建立「默认恒就绪」的 ready 节点**
   ⇒ ⇒ 源 MUST 由**结果对象持有**（`ch_bool default_ready_source;` 成员）
   ⇒ ⇒ MUST 先 `= ch_bool(true)` 再 `<<=`（探针 ⑩ C 的初始化顺序）
   ⇒ ⇒ 目标 MUST 是**已存在**的**非 literal** 节点（探针 ⑨/⑩ 证明覆盖有效）

② **组合子 MUST 用 `<<=` 驱动上游的 ready 字段**
   ⇒ ⇒ **MUST NOT** 用 `operator=`（⇒ ⇒ 只重绑定副本 ⇒ ⇒ 调用方被孤儿化）
   ⇒ ⇒ 可行性依据 = 探针 ②（拷贝**共享**节点 ⇒ ⇒ `<<=` 能**穿透副本**）

③ **组合子 MUST NOT 有 `out_ready = true` 这类局部常量**
   ⇒ ⇒ 那是「无条件忽略消费端 ready」⇒ ⇒ **MUST** 由 `output_stream.ready`
   驱动（⇒ ⇒ 该节点必须**有消费者**⇒ ⇒ 由 ② 的表达式提供）

### 3.2 消费端表达「不就绪」的两种方式（**都合法**）

- **方式 1（单阶段）**：直接 `<<=` 覆盖 `result.output_stream.ready`
- **方式 2（便利形式）**：`<组合子>_connect_ready(result, consumer_ready)`
  ⇒ ⇒ ⚠️ 与 fifo 类的 `connect_ready` **语义一致** ⇒ ⇒ 心智模型统一
  ⇒ ⇒ ⚠️ **MUST 幂等**（重复调用不报错）+ **MUST NOT 静默接受空源**（抛异常）

### 3.3 ⚠️ **`connect_ready` 的定位变化**（**architect 裁决 · 2026-11-01**）

⚠️ **裁决（用户）**：**A｜单阶段（形态 E）** ⇒ ⇒ **与 ADR-054 §3.2 第 2 条**
（「mux / demux / arbiter 类 MUST 保持决策 0-B 的**两阶段**形态」）**冲突**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **用户已批准由 builder 代改**（加注「角色越权执行」）
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **`stream_mux_connect_ready` 变为
「覆盖消费端 ready」的便利入口**（**保留** ⇒ ⇒ 仓内已有 **11** 个调用点
⇒ ⇒ ⇒ **不可删**）⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **其语义 MUST
被定义为幂等覆盖**。

### 3.4 ⚠️ **「默认恒就绪」承诺的适用范围**

⚠️ **仅 fifo 类 + 本 change 收口的转发类**（mux / arbiter / join / filters /
combine_with）⇒ ⇒ ⇒ **`demux` / `fork` 仍是「死路」**（P1-b / P1-c）
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **主 spec R1 的「转发类 MUST NOT 被读作『已合规』」Scenario
继续在约**（⚠️ **标题 MUST NOT 改** ⇒ ⇒ 只能**原地订正正文**）。

### 2.7 🔴🔴🔴 **第 7 条发现的表述需要订正**（**实施期实测**）

⚠️ **已归档**的主 spec R2 与 ADR-054 §3.2 第 1 条的第 4 个必要条件写的是：

> ④ **覆盖 MUST 在 `Simulator::initialize()` **之前**做**（⇒ ⇒ 指令的
>    `src0_buf` 指针是在 `initialize()` 时**绑定**的 ⇒ ⇒ `set_src` 改的是
>    **图**、**不重建指令** ⇒ ⇒ ⇒ **改边在 `initialize()` 之后不生效**）

⚠️ **实施期实测发现该表述过严**（探针：mux 形态 E 落地后的验证）：

```
不就绪 ⇒ out.ready=0 a.ready=0 b.ready=0        (覆盖在 Simulator 构造**之前**) ✔
运行期改值为1 ⇒ out.ready=1 a.ready=1 b.ready=0   (运行期改**值** ⇒ 生效) ✔
```

⚠️ **真相**（`src/simulator_init.cpp:17-40` + `include/ast/instr_op.h:36-60`）：
- `Simulator::initialize()` 绑定 `eval_list_` + 指令的 `src0_` / `src1_`
  **缓冲指针** ⇒ ⇒ ⇒ **改边（`set_src`）不更新这些指针**；
- ⇒ ⇒ ⇒ **旧 src 节点仍在 eval list 里** ⇒ ⇒ **仍被求值** ⇒ ⇒ ⇒
  **覆盖「看起来没生效」**（读到的是旧 src 的值）；
- ⚠️ **`reinitialize()` 不解决**（实测：仍不生效）⇒ ⇒ 因为它重跑
  `initialize()` 时**重新绑定了旧 src** ⇒ ⇒ ⇒ **改边的语义**在框架里是
  **「替换 src 指针」**，而指令缓冲指针**指向 data_map 里的缓冲** ⇒ ⇒
  ⇒ ⇒ **改边后必须让指令重新绑定** ⇒ ⇒ ⇒ **当前框架无此机制**。

⇒ ⇒ ⇒ ⇒ ⇒ **正确的规则**（**订正**）：
> **改边（`<<=` / `set_src`）MUST 在 `Simulator` 构造**之前**完成**；
> ⇒ ⇒ **运行期**只能改**值**（`sim.set_value(node, v)`）⇒ ⇒ ⇒
> ⇒ ⇒ ⇒ ⇒ **这正是测试台的正常用法** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **不构成限制**。

⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **「单阶段」形态**（消费端在 `Simulator` 构造前覆盖
`output_stream.ready` ⇒ ⇒ **不需要** `connect_ready` 这个函数）
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **成立** ✔ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **BUT**：
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⚠️ **它仍然受「改边须在 `Simulator` 构造前」约束**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ ⇒ ⇒ ⇒ 该约束 MUST 在契约里说清楚**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **否则调用方会踩坑**
（⇒ ⇒ **实测**：我自己的第一个验证探针就踩了 ⇒ ⇒ 踩坑率**不低**）。

### 3.5 ✅ **实施结果**（2026-11-01）

| 组合子 | 状态 |
|---|---|
| **mux** | ✅ 形态 E + 判别力验证 |
| **arbiter×4**（`priority` / `round_robin` / `lock` / `sequence`） | ✅（含 `stream_arbiter.h` 的 2 个） |
| **join** · **filters×6** · **combine_with** | ✅ |
| **`demux` / `fork`** | ❌ **未动**（P1-b / P1-c ⇒ ⇒ 护栏显式豁免 + `::notice`） |
| **`StreamBuilder::queue<>()`** | ❌ **已移出**（§2.2-2 ⇒ ⇒ 需 architect 裁决） |

⇒ ⇒ **零公开 API 变更**（局部源 ⇒ ⇒ 无需加结果 struct 成员）✔
⇒ ⇒ **错误噪声从 16 降到 6**（剩下来自 fork / demux）✔

### 3.6 🔴 **实施期踩到的 4 个护栏自身盲区**（**全部实测踩中** ⇒ ⇒ 已修）

| # | 盲区 | 症状 | 修法 |
|---|---|---|---|
| ① | **R8 的正则漏检下标形式** | `input_streams[i].ready` **全部漏检** ⇒ ⇒ 投毒后**静态判据仍绿** | 正则支持下标 |
| ② | **`restore()` 的「陈旧备份」** | `BACKUP` 是**固定路径** ⇒ ⇒ **跨次残留** ⇒ ⇒ **一次 `--self-test` 冲掉工作区全部改动** | 「无投毒快照 ⇒ ⇒ 拒绝还原」 |
| ③ | **自检只查行为判据** | 「结构判据恒绿」会被放过 | 对**双判据**投毒 ⇒ ⇒ 静态也 MUST 红（⚠️ 「多驱动守卫」类**除外** ⇒ ⇒ 强制会**误报**） |
| ④ | **判别力用例只覆盖 `take_when`** | 其余 filters 的 `operator=` 回归**不可见** ⇒ ⇒ 投毒后行为用例**仍绿** | 补 `translate_with` 覆盖 |

⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 「护栏自身有盲区」是本仓反复出现的病症**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 每一轮 MUST 把新踩到的盲区**当场修掉并登记**
⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 否则下一个 change 会再踩一次**。

## 4. 变更面

| 文件 | 变更 |
|---|---|
| `include/chlib/stream.h` | ⓐ `filters×6` + `combine_with` 改 `<<=` + 建默认源 · `join` 改 `<<=` + 建默认源 · `mux` 建默认源 + 内部建立 `input_streams[i].ready` · `priority_arbiter` / `round_robin_arbiter` 改硬编码 · `stream_mux_connect_ready` 幂等化 |
| `include/chlib/stream_arbiter.h` | `lock` / `sequence` 同上（**arbiter×4**） |
| `include/chlib/stream_builder.h` | `queue<>()` 的上游回接（**静默丢字**） |
| `tests/chlib/test_stream.cpp` | 判别力用例（进**既有** target ⇒ ⇒ 锚点不变） |
| `tests/test_stream_builder.cpp` | 链长 ≥2 的零丢字用例 |
| `scripts/lib/stream_ready_ownership.py` + `scripts/verify_stream_ready_ownership.sh` | R7（转发类**行为**判据）+ R8（转发类**调用点**判据）+ 自检投毒 |
| `docs/adr/ADR-054-stream-ready-ownership-form.md` | §3.2 第 2 条修订（architect 权限 · 用户已批准） |

## 5. 验收判据

- [ ] **效果判据**：转发类（mux / arbiter / join / filters / combine_with）
      消费端不就绪 ⇒ ⇒ **调用方持有的** ready 节点 `num_srcs() > 0`
      **且值 = 0**（⇒ ⇒ 变异 `<<=` → `operator=` MUST 转红）
- [ ] **效果判据**：未覆盖时上游 ready = 1（= 恒就绪 ⇒ ⇒ **零破坏**）
- [ ] **效果判据**：`connect_ready` 幂等（重复调用不抛）+ 空源抛异常
- [ ] **效果判据**：`queue<4>().queue<4>()` 填满 ⇒ ⇒ **链最上游** ready = 0
      且**零丢字**
- [ ] **穷举**：`stream.h` + `stream_arbiter.h` 内**所有**接受/产生 ready 的
      入口（⚠️ 「只查已知符号 ⇒ 漏掉同族入口」是本仓登记的护栏盲区）
- [ ] 变异：每条**先确认编译成功**
- [ ] `ctest -LE perf` 零回归 · **计数锚点不变**

## 6. 归档

⇒ ⇒ 归档**禁** `--skip-specs` ⇒ ⇒ ⇒ ⚠️ **archive 不合并 delta 的
`## Purpose`** ⇒ ⇒ ⇒ 归档后 MUST 另行补写主 spec 的 `## Purpose`
（先例 `core-lnode-entry-set`）。
