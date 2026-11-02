# 提案：路线 B（G 批）—— `chlib` 三个 stream pipe 进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 批次表 + §4 的「可转性 4 问」
- **规格影响**：**无 spec 变更** ⇒ 归档可用 `--skip-specs`
- **规模**：1 个新测试文件（3 个 `DUAL_BACKEND_TEST_PAIR` / 72 断言每后端）+ 1 条 CMake 注册 + 计数锚点
  + **2 个新缺陷的独立立项**（只读查重的副产品）
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage` ·
<!-- touched-components: stream_m2s_pipe, stream_s2m_pipe, stream_half_pipe -->
  ADR-055（stream 链内闭包不变式 · 三个 pipe 的重写）

## Why

### 一、🔴 这是路线 B 里**回归价值最高**的一批

`include/chlib/stream_pipeline.h` 的三个 pipe 在 **ADR-055（2026-11-01）里刚被重写**，
且重写**修掉了三个真缺陷**：

| pipe | ADR-055 修掉的缺陷 |
|---|---|
| `stream_m2s_pipe` | `valid_reg->next = input_fire` ⇒ **下游一停顿就丢数据** |
| `stream_s2m_pipe` | 寄存的是 `input_stream.ready`（上游自己的 ready）⇒ **方向反了的自环** |
| `stream_half_pipe` | `toggle` 由传输驱动 ⇒ **无数据时卡住 ⇒ 死锁** |

⇒ ⇒ **重写后的语义从未被双后端验证过** ⇒ ⇒ 生成的 Verilog（`always_ff` + `select` 链）
与 interpreter 是否逐周期一致 = **完全未知**。

⇒ 且三个 pipe **含 `ch_reg`** ⇒ ⇒ 走 `always_ff` codegen 路径 ⇒ ⇒ 正是两后端最容易分叉的地方
（对比：纯组合的组合子没有这条风险）。

⇒ 且**有生产消费者**：`include/bundle/stream_bundle_member_inlines.h` 的
`m2sPipe()` / `s2mPipe()` / `halfPipe()` 三个成员函数都建在它们之上。

## 可转性 4 问

| # | 问题 | 回答 |
|---|---|---|
| ① | 语义是否已被别的 dual 文件覆盖？ | **否** —— 三个符号在全部 54 个 dual 文件里**各出现 0 次** |
| ② | 断言形态有判别力吗？ | **有** —— 变异 **7/7 全抓** |
| ③ | 与后端无关吗？ | **否** —— 真跑仿真 + 读顶层端口，正是要对比两后端 |
| ④ | 形态值得新写吗？ | **是** —— 既有测试**不可转换**（见下）⇒ 只能新写 |

### ⚠️ 既有测试**不可转换**（4 问① 的另一半）

`tests/test_stream_pipeline.cpp`（12 用例 / 68 断言）**全是裸 context**
（`ch_device=0`、无顶层端口）⇒ ⇒ 命中 **R6 排除项** ⇒ ⇒ **结构上不可转换**
⇒ ⇒ 必须新写（自带 wrapper DUT + 手推期望）。
⇒ ⚠️ **原文件 MUST NOT 修改**。

### ⚠️ DUT 是**端口 wrapper**，不是「DUT 复制」

三个 pipe 是**函数式组合子**（接收 `ch_stream<T>`，返回 result struct），**不是** `Component`
⇒ ⇒ 需要一层「端口 ↔ stream 字段」的适配 wrapper（本文件的 3 个 DUT 类）
⇒ ⇒ **被测的语义全部在库组件里** ⇒ ⇒ **测的就是生产代码本身**
⇒ ⇒ 先例：`tests/chlib/test_stream_builder_backpressure_dual.cpp`（同型 wrapper）。

## 读模型（期望值的承重前提）

`Simulator::tick()`（`src/simulator.cpp:308-328`）的精确顺序：

```
1) eval_combinational()   ← 边沿前
2) 寄存器提交（用边沿前的组合值）
3) eval_combinational()   ← 边沿后
```

⇒ ⇒ **tick 之后读到的端口 = 边沿后的状态**（与 `ch_reg` 同约定）
⇒ ⇒ 与既有单后端测试（`tests/test_stream_pipeline.cpp:30-33`「tick 一次即见 `0x42`」）一致。

## 期望值（**手推**，禁抄仿真输出）

模型由**独立 Python 实现**（`/tmp/opencode/gen_pipe_table.py`）逐拍生成：

```
m2s:  out_free_pre = (!pre_valid) or R ; accept = out_free_pre and in_valid
      post_valid   = accept or (pre_valid and not R)
      post_payload = in_payload if accept else pre_payload
      观测: out_valid=post_valid  out_payload=post_payload
            in_ready = (!post_valid) or R          ← 用边沿后的寄存器重算
s2m:  post_ready = R  (ready_reg <- output.ready)
      观测: out_valid=in_valid  out_payload=in_payload  in_ready=post_ready
half: out_free_pre = (!pre_valid) or R ; accept = pre_toggle and out_free_pre
      take = accept and in_valid ; post_toggle = not pre_toggle
      post_valid = take or (pre_valid and not R)
      post_payload = in_payload if take else pre_payload
      观测: in_ready = post_toggle and ((!post_valid) or R)
```

⇒ ⇒ **一次通过**（两后端都吻合 ⇒ 无推导错；这是「两后端一致 ⇒ 同一处推导错」判据的**反向**用法）。

### 激励（8 拍，三个 pipe 共用）

```
c1: (in_valid=0, payload=0x00, consumer_ready=0)   ← 空缓冲 + 消费端不就绪
c2: (1, 0xA1, 1)   c3: (0, 0x00, 1)
c4: (1, 0xB2, 0)   c5: (1, 0xC3, 0)   c6: (1, 0xC3, 1)
c7: (0, 0x00, 1)   c8: (1, 0xD4, 1)
```

⚠️ **c1 是 m2s 与 s2m 的判别点**：空缓冲 + 消费端不就绪 ⇒
**m2s** 的 `in_ready` = `(!post_valid) or R` = **1**（缓冲还有空位 ⇒ 上游能推）
而 **s2m** 的 `in_ready` = **R** = **0**（ready 是寄存的 ⇒ 上游被挡住）
⇒ ⇒ 若把 s2m 的 `in_ready` 误改成组合直通 `output.ready`，c1 会读到 1 ⇒ 转红。

## 三个 pipe 各钉住的语义

| 维度 | m2s | s2m | half |
|---|---|---|---|
| **1 拍延迟** | ✅ | ❌（payload/valid 组合直通） | ✅ |
| **下游停顿 ⇒ 数据保持**（`out_payload` 不变 + `out_valid` 保持 1） | ✅ c4→c5 | ❌ | ✅ c5→c6 |
| **缓冲满 ⇒ 上游被挡住**（`in_ready`=0） | ✅ c4/c5 | ❌ | ✅ c5/c6 |
| **半带宽**（隔拍才捕获） | ❌ | ❌ | ✅ |
| **ready 的来源** | `!valid_reg or output.ready`（组合） | `output.ready`（寄存） | `toggle & out_free`（组合） |

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 转红 |
|---|---|---|
| M1 | m2s `valid_reg->next = accept`（**ADR-055 修复前的形态** ⇒ 停顿丢数据） | 两半边 |
| M2 | m2s `out_free` 忽略 `output.ready`（缓冲满也不挡上游） | 两半边 |
| M3 | s2m `output.valid` 恒 0（丢掉组合直通） | 两半边 |
| M4 | s2m `ready_reg` 寄存 `input_stream.ready`（**方向反了的自环** ⇒ ADR-055 修复前的形态） | 两半边 |
| M5 | half `accept` 不看 `toggle`（半带宽被破坏） | 两半边 |
| M6 | half `toggle` 永不前进（捕获全灭） | 两半边 |
| M7 | m2s `input.ready` 忽略缓冲占用（只看消费端 ready） | 两半边 |

⇒ **7/7 全抓**。

### ⚠️ 两条判别力边界（如实登记 —— 是读模型的固有限制，**不是**缺陷）

① **s2m 的「ready 被寄存」与「组合直通」在读模型下不可区分**（两者都观测到**本拍**的 R）
   ⇒ ⇒ 本文件钉的是「`in_ready` **跟随**消费端 ready 且非零」，**不是**「它被寄存了」
   ⇒ ⇒ 要钉「寄存」需要「两次 tick 之间改 R 再读」这个观察点，但 `get_port_value` 只读
   `data_map_`、组合值不会自动重算 ⇒ ⇒ 需要框架级 eval 触发点 ⇒ **独立议题**。
② **「1 拍延迟」在端口读值上与 `ch_reg` 同约定**（tick 后即可见）⇒ ⇒ 本文件钉的是
   「c1 时 `out_valid` 仍为 0、c2 才为 1」这个**跨拍相对关系**，而非绝对的「晚一拍」。

## 🔴🔴 本批的第二个产出：只读查重挖出**两个真缺陷**（已独立立项）

建议 3（路线 B 4 个候选的**只读** 4 问① 查重）⇒ 除本批外，还挖出两个**既有**缺陷
（**与本批的 parity 覆盖无关**，但必须登记）：

| # | 组件 | 缺陷 | 实测证据 | 立项 |
|---|---|---|---|---|
| 1 | `chlib::stream_arbiter_lock`（`stream_arbiter.h:62-68`） | C++ `if/else if/else` 做硬件选择 ⇒ 建图期求值 ⇒ 恒走 `else` ⇒ **恒选通道 0** | 唯一有效输入 = 通道 1 ⇒ `selected=0` 且 **`out.payload=0x11`（通道 0 的数据！）** ⇒ **静默数据损坏** | `.rddf/improvements/stream-arbiter-lock-selects-channel-zero-2026-11.md` |
| 2 | `chlib::round_robin_arbiter`（`selector_arbiter.h:94-115`） | `UpdatePriority` 的 `if (result.valid)` 建图期求值 ⇒ 恒假 ⇒ `next_priority` 永不更新 ⇒ **不轮转** | `next_ptr` 连续 3 拍恒 0（期望每拍 +1） | `.rddf/improvements/selector-round-robin-not-rotating-2026-11.md` |

⇒ ⚠️ **对执行顺序的影响**：带**已知缺陷**的组件**不做** dual parity
⇒ ⇒ 否则 parity 会给缺陷**盖章**（两后端一致地错 ⇒ parity **看不见**）
⇒ ⇒ **先修缺陷、再做覆盖** ⇒ ⇒ 本轮**避开**它们，做干净的 `stream_pipeline`。

⇒ ⚠️ **全 `include/chlib/*.h` 已扫**（剥注释后逐行找 C++ `if`）⇒ 同家族其余命中点
均为 **C++ 侧判断**（容器 `size()` / 模板常量 / 节点守卫）⇒ **无第二处同型缺陷**；
唯一边缘项 = `arithmetic.h:190` 的 `if (sign_bit)`（**仅**当移位量 ≥ 位宽时命中）。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_stream_pipeline_dual.cpp`（**新**） | 3 个 wrapper DUT + 3 pair × 8 拍 × 3 输出 = **72 断言/后端** |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| `AGENTS.md` | 4 个计数锚点 + 3 处 NOTES 计数 + 本批 NOTES 条目 + 只读查重结论条目 |
| `.rddf/improvements/stream-arbiter-lock-selects-channel-zero-2026-11.md`（**新**） | 缺陷 1 的独立立项（5 段） |
| `.rddf/improvements/selector-round-robin-not-rotating-2026-11.md`（**新**） | 缺陷 2 的独立立项（5 段） |
| 台账 | plan §5 墙钟 · objective 现状 56 → 57 与可达上限 · handoff §1/§4.6/§5 |

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**144 == 2 × 72**
- [x] 变异 **7/7 全抓**（每条先确认仍可编译；还原用 `cp` 后重建）
- [x] 期望值由**独立 Python 模型**逐拍生成 ⇒ **一次通过**（无推导错）
- [x] **原文件 `tests/test_stream_pipeline.cpp` MUST NOT 修改**（已遵守）
- [x] verilator 半边**真的跑了**（N2′ 的 2× 断言差是正向证据）
- [x] 全量重建后 `ctest -LE perf` **223/223** 零回归
- [x] 计数锚点同步且两个计数护栏双绿
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
