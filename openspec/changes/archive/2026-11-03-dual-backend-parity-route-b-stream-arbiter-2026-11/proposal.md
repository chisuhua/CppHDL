# 提案：路线 B（I 批）—— `stream_arbiter_priority` / `stream_arbiter_round_robin` 进双后端 parity

<!-- touched-components: stream_arbiter_priority, stream_arbiter_round_robin -->

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 批次表 + §4 的「可转性 4 问」
- **规格影响**：**无 spec 变更** ⇒ 归档可用 `--skip-specs`
- **规模**：1 个新测试文件（2 个 `DUAL_BACKEND_TEST_PAIR` / 96 断言每后端）+ 1 条 CMake 注册
  + 计数锚点 + 命名诚实性登记表 **+1 条**（并升级 `stream_arbiter_round_robin` 的判别性用例）
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage` ·
  capability `chlib-stream-arbitration` · ADR-055（stream 链内闭包不变式 · 轮转反馈修复）

## Why

### ① `stream_arbiter_round_robin` 含 `ch_reg` ⇒ 走 `always_ff` codegen 路径

它的**轮转反馈**（`last_grant_reg`）是 **ADR-055（2026-11-01）刚修好的**：
修前恒传 `last_grant = 0_d` ⇒ ⇒ 扫描起点恒为 1 ⇒ ⇒ **低位通道饿死**
⇒ ⇒ **修复后的语义从未被双后端验证过** ⇒ ⇒ 而它恰好是 `ch_reg` 驱动的
（⇒ ⇒ `always_ff` ⇒ ⇒ 两后端最易分叉处）。

### ② 两条「反直觉」语义从未被任何测试钉住

| # | 语义 | 实测证据 |
|---|---|---|
| ① | **优先级方向 = 高位优先** | `priority_encoder(0b1010) == 3` ⇒ `priority_encoder` 的循环 `i = 0 → N-1` 且每次 `select` **覆盖** ⇒ **最高**位胜 ⇒ ⚠️ **它的注释写「优先处理低位」⇒ 与实现相反** ⇒ 且与同仓 `priority_selector`（**低位**优先）**方向相反** ⇒ 「优先级」在本仓含义不唯一 |
| ② | 两个仲裁器的 `ready` 门控**不同** | priority：`ins[i].ready = (selected==i) & out.ready` ⇒ ⚠️ **不被 valid 门控** ⇒ **无请求时通道 0 仍拿到 ready**；round_robin：`... & valid` ⇒ ✅ **被门控** ⇒ **无请求时全部 ready=0** |

⇒ ⇒ 既有测试**零判别力**：`tests/test_stream_arbiter.cpp` 的 priority 用例只构造
「**唯一**有效输入」⇒ ⇒ 单路有效时两种方向**逐位相同** ⇒ ⇒ 方向不可观测。

## 可转性 4 问

| # | 问题 | 回答 |
|---|---|---|
| ① | 语义是否已被别的 dual 文件覆盖？ | **否** —— 两个符号在全部 54 个 dual 文件里**各出现 0 次** |
| ② | 断言形态有判别力吗？ | **有** —— 变异 **7/7 全抓** |
| ③ | 与后端无关吗？ | **否** —— 真跑仿真 + 读顶层端口，正是要对比两后端 |
| ④ | 形态值得新写吗？ | **是** —— 既有测试**不可转换**（见下）⇒ 只能新写 |

### ⚠️ 既有测试**不可转换**

`tests/test_stream_arbiter.cpp`（4 用例）与 `tests/chlib/test_stream.cpp` 都是
**裸 context**（`ch_device=0`、无顶层端口）⇒ ⇒ 命中 **R6 排除项**
⇒ ⇒ **结构上不可转换** ⇒ ⇒ 必须新写（wrapper DUT + 手推期望）。
⇒ ⚠️ 两个原文件 **MUST NOT 修改**。

## 期望值（**手推**，禁抄仿真输出）

模型（`/tmp/opencode/gen_arbiter_table.py`，独立 Python 实现）：

```
priority:  selected = priority_encoder(valid_vector)   # **最高**位胜
           out.payload = payloads[selected] ; out.valid = (valid_vector != 0)
           ins[i].ready = (selected==i) & out.ready     # ⚠️ 无 valid 门控

round_robin: grant = round_robin_selector(valid_vector, last_grant_reg).grant
              # 起点 = (last_idx + 1) % N（last_idx = onehot_to_binary(last_grant)）
              selected = priority_encoder(grant)        # grant=0 ⇒ selected=0
              ins[i].ready = (selected==i) & out.ready & valid   # ✅ 有 valid 门控
              last_grant_reg->next = select(valid, grant, last_grant_reg)
```

⚠️ **读模型**（`Simulator::tick`：边沿前组合求值 → 寄存器提交 → 边沿后组合求值）：
**tick 之后读到的端口 = f(边沿后的寄存器, 本拍输入)** ⇒ ⇒ 对 round_robin 而言，
读到的选择用的是**边沿后**的 `last_grant_reg` ⇒ ⇒ 期望表按此生成
（**独立探针实测逐位吻合**：读到的 `selected` 序列 = 2,0,1,2,0,1…）。

⚠️ **`last_grant_reg` 的初值 MUST 是字面量零**（`stream.h:786` 的注释记录了
「`ch_reg` 的【表达式】初值在 Verilog codegen 侧失效 ⇒ ⇒ 静默回退 0」）
⇒ ⇒ 本文件的 round_robin 期望值**依赖**这个字面量零 ⇒ ⇒ 它同时是一条
**双后端一致性**判据。

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 转红 |
|---|---|---|
| M1 | `priority_encoder` 的方向反转（最高位胜 ⇒ 最低位胜） | 两半边 |
| M2 | priority 的上游 `ready` 恒 0（反压丢失） | 两半边 |
| M3 | priority 的输出 `valid` 恒 1 | 两半边 |
| M4 | round_robin 的轮转反馈恒 0 ⇒ 起点恒 1 ⇒ **饿死复现** | 两半边 |
| M5 | round_robin 的 `ready` 去掉 valid 门控 | 两半边 |
| M6 | round_robin 的反馈寄存器初值改 bit1（⇒ 相位变化） | 两半边 |
| M7 | `round_robin_selector` 的扫描起点少 1 | 两半边 |

⇒ **7/7 全抓**。

### ⚠️ 如实登记一个**无效变异**

M6 的初版把初值改成 `1_d` ⇒ ⇒ **恒绿** ⇒ ⇒ 原因：**零初值与 `1_d` 初值的
`last_idx` 都是 0** ⇒ ⇒ `start_pos` 相同 ⇒ ⇒ **相位不可观测**
⇒ ⇒ 改用 `2_d`（bit1 ⇒ `last_idx=1`）才可观测
⇒ ⇒ 这条**说明「初值 ≠ 0」本身不是判别点**，**相位偏移量**才是。

## 🔴 顺带挖出一个框架层问题（跨 DUT 状态污染）

本文件**完整运行**时 stderr 出现 **6 条** `[instr_op_bit_sel] Error: Bit index
out of range!` ⇒ ⇒ 插桩显示 **`operand_width=1` + `src1_width=32`**
⇒ ⇒ 某个 `bit_select` 的**操作数被污染成 1 位** ⇒ ⇒ 复现矩阵：
**只有 interpreter → interpreter（同进程建两个 DUT）才复现**（单独跑任一半边 = 0 条；
两个 DUT 的 verilator 半边一起跑 = 0 条）⇒ ⇒ **不是 verilator 的问题**
⇒ ⇒ 与 `tests/AGENTS.md` 记录的 **K1「ORC JIT 跨 DUT 状态污染」同族**
（但**不涉及 JIT** ⇒ 污染源不限于 JIT 状态）⇒ ⇒ 仓内其它 8 个 dual 文件**均不复现**
⇒ ⇒ **本批断言值未受影响**（192 全绿 ⇒ 越界落在**未被断言消费**的死路径上）
⇒ ⇒ 但「换个激励就可能落到被断言的路径上」⇒ ⇒ **独立立项**
`.rddf/improvements/cross-dut-bit-select-out-of-range-2026-11.md`（**根因未定位**）。

## 🔴 连带：命名诚实性（B8 的 R3）

本批**改动过** chlib 组件 ⇒ ⇒ 护栏 `scripts/verify_chlib_naming_honesty.sh` 的 **R3**
要求「最近的 chlib change MUST 在 proposal 里用 `<!-- touched-components: ... -->`
声明它触及的组件，**且那些组件 MUST 已在登记表登记**」⇒ ⇒
① 本 change 的 proposal 已带标记 · ② 登记表 **+1 条**（`stream_arbiter_priority`）
+ **升级** `stream_arbiter_round_robin` 的判别性用例为**双后端版**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_stream_arbiter_dual.cpp`（**新**） | 2 个 wrapper DUT + 2 pair（8 组 + 8 组向量 × 6 输出）= **96 断言/后端** |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| `AGENTS.md` | 4 个计数锚点 + 3 处 NOTES 计数 + 本批 NOTES 条目（含缺陷登记） |
| `.rddf/checks/chlib-naming-honesty-registry.md` | **+1 条** + 升级 1 条的判别性用例 |
| `.rddf/improvements/*.md`（**3 个新**） | `cross-dut-bit-select-out-of-range-2026-11`（框架层）· `priority-encoder-doc-contradicts-implementation-2026-11` · `stream-width-adapters-crash-on-simulate-2026-11` |
| 台账 | plan §5 墙钟 · objective 现状 58 → 59 + **「阈值 60 不可达」的实测裁决** · handoff §1/§4.6/§5 |

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**192 == 2 × 96**
- [x] 变异 **7/7 全抓**（每条先确认仍可编译；还原用 `cp` 后重建）
- [x] 期望值**手推**（独立 Python 模型 ⇒ 探针实测逐位吻合 ⇒ 一次通过）
- [x] **原文件 `tests/test_stream_arbiter.cpp` / `tests/chlib/test_stream.cpp` MUST NOT 修改**
- [x] 全量重建后 `ctest -LE perf` **225/225** 零回归
- [x] 命名诚实性护栏 + 自检全绿
- [x] 计数锚点同步且两个计数护栏双绿
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
