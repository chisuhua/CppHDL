---
name: stream-arbiter-rr-rotation-2026-10
priority: P2
source: P0 探针（`/tmp/opencode/p0/p0v2.cpp`，2026-10-31 实测）+ oracle 架构评审
phase: phase-5
category: chlib
type: bugfix
created_at: 2026-10-31
---

# Improvement: `stream_arbiter_round_robin` 的 **round-robin 从不轮转**（名字 over-promise）

- **Date drafted**: 2026-10-31
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已实施**（2026-11-01 · 方案 **A** —— 真轮转）
  ⇒ ⇒ change `stream-arbiter-rr-rotation-2026-11`
  ⇒ ⇒ **实施要点**：`last_grant_reg`（one-hot，**零初值**）反馈「上一次实际授予」
  ⇒ ⇒ `valid` 条件更新（无授予则保持 ⇒ 不丢相位）⇒ ⇒ **相位 = 首拍从通道 1 起**
  （选择器的既有语义，本 change 未改它 —— 实测金标已按「posedge 后稳态」递推）
  ⇒ ⇒ `stream_arbiter_sequence` 按用户裁决 **只订正注释**（零行为变更）
  ⇒ ⇒ ⚠️ **新发现**：`sequence` 与 `priority` 在**仲裁逻辑上逐位相同**
  ⇒ ⇒ ⇒ **组件去留登记 backlog**（架构裁决，不在本 change 顺手删）
  ⇒ ⇒ 判别力：`[rotation]` 3 个 SECTION / **38 断言**（逐拍金标）
  ⇒ ⇒ 护栏：复用既有自检框架 + 投毒 `rr-feedback-const`（**9 种投毒全被抓**）
- **来源**：P0 探针扫 `include/chlib/stream.h` 时的**静态阅读** ⇒ ⇒ 经**代码定位**
  确认（oracle 评审提出 ⇒ builder 抽验）

## 1. Why (背景与动机)

🔴 **`stream_arbiter_round_robin` 名为 round-robin，行为却是固定优先级。**

⇒ ⇒ `include/chlib/stream.h:564` 建了轮询位置寄存器：

```cpp
auto current_channel = ch_reg<ch_uint<compute_idx_width(N_INPUTS)>>(0_d);
```

⇒ ⇒ ⚠️ **该寄存器从未接到任何地方** ⇒ ⇒ **死节点** ⇒ ⇒ 且

```cpp
ch_uint<N_INPUTS> last_grant = 0_d;                       // :581  恒为 0
auto arb_result = round_robin_selector<N_INPUTS>(valid_vector, last_grant);  // :582
```

⇒ ⇒ ⇒ **每拍拿到的 `last_grant` 都相同** ⇒ ⇒ ⇒ 轮转**从不发生**
⇒ ⇒ ⇒ **低位输入饿死**（若高位输入持续有效，低位**永远**得不到服务）。

⚠️ 这与本仓已登记的「**名字 over-promise**」同族：
`DCacheWriteBuffer`（名字说缓冲、实为直通）· `fwft_fifo`（注释已声明「MUST NOT
当作 FWFT 参照」）。

⚠️ **为什么长期无人发现**：`stream_arbiter_round_robin` 的既有测试
（`tests/chlib/test_stream.cpp` 的 arbiter 段）**只断言「选中唯一有效的输入」**
⇒ ⇒ 单输入场景下 round-robin 与 priority **逐位相同** ⇒ ⇒ ⇒ **零判别力**。

## 1.1 🔴 **实测依据**（2026-10-31，P0 探针发现 ⑤）

⚠️ **代码定位**（`include/chlib/stream.h`）：
- `:564` —— `auto current_channel = ch_reg<ch_uint<compute_idx_width(N_INPUTS)>>(0_d);`
  ⇒ ⇒ **从未接到任何地方** ⇒ ⇒ **死节点**；
- `:581` —— `ch_uint<N_INPUTS> last_grant = 0_d;` ⇒ ⇒ **恒为 0**；
- `:582` —— `auto arb_result = round_robin_selector<N_INPUTS>(valid_vector, last_grant);`
  ⇒ ⇒ ⇒ **每拍拿到的 `last_grant` 都相同** ⇒ ⇒ ⇒ **轮转从不发生**
  ⇒ ⇒ ⇒ **低位输入饿死**。

⇒ ⇒ ⇒ **「名为 round-robin、实为固定优先级」已被实测确认** ⇒ ⇒ ⇒
本项 MUST 与 P1（ready 链收口）**串行**（两者是**正交**关注点）。

## 2. What Changes (变更范围)

### 2.1 候选方案（**待 architect 裁决**）

- **A｜真轮转**：把 `current_channel` 接进选择逻辑 ⇒ ⇒
  `last_grant` MUST 反馈**上一次实际授予**的通道 ⇒ ⇒ 每次授予后指针前进
  ⇒ ⇒ ⇒ **行为变更**（既有调用点的选中顺序会变）⇒ ⇒ 需自己的金标 + 既有调用点复核
  （`examples/stream/stream_arbiter_demo.cpp` · `tests/chlib/test_stream.cpp` 的
  arbiter 段）；
- **B｜改名 + 文档**：把它**如实**命名为「固定优先级」⇒ ⇒ 零行为变更、零风险
  ⇒ ⇒ 消除 over-promise ⇒ ⇒ 但**没有**轮转能力（SpinalHDL 的 `Arbiter` 是
  round-robin ⇒ ⇒ 语义**不符**）；
- **C｜A + B**：先改名（诚实）+ 再实现真轮转（能力）⇒ ⇒ ⇒ **两步** ⇒ ⇒
  建议：**A 的金标 + B 的命名**同批做，但**分 2 个 commit** ⇒ ⇒ 便于回滚。

⚠️ **范围界定**：本草案**只**处理 **RR 轮转** ⇒ ⇒ **ready 所有权**（`out_ready`
是**局部**节点 ⇒ ⇒ 消费端覆盖**不传播** ⇒ ⇒ 与 mux 已修的那处**同型**）
⇒ ⇒ 归 **`.rddf/improvements/stream-forward-ready-closure-2026-10.md`** ⇒ ⇒
**两个 change 串行**（ready 先 ⇒ RR 后）⇒ ⇒ 理由：改**仲裁语义**与改
**ready 链连接手段**是**正交**关注点 ⇒ ⇒ 混在一起会让对抗审稿的爆炸半径翻倍。

## 3. Acceptance (验收判据 —— MUST 可勾选)

- [ ] **效果判据**（**MUST NOT** 词法）：**2 个输入持续有效** ⇒ ⇒ 授予顺序
      MUST **交替** ⇒ ⇒ 手推金标序列（如 `0,1,0,1,…`）⇒ ⇒
      ⚠️ MUST **逐拍**断言（不是「最终态」）
- [ ] **反向判据**：把 `current_channel` 断回 ⇒ ⇒ 断言**转红**（钉住「轮转真的
      发生了」）
- [ ] 既有调用点复核：`examples/stream/stream_arbiter_demo.cpp` ·
      `tests/chlib/test_stream.cpp` 的 arbiter 段 ⇒ ⇒ 若行为变化 ⇒ ⇒ **逐项记录**
- [ ] ⚠️ **穷举**：`stream_arbiter.h` 内另有 `stream_arbiter_lock` /
      `stream_arbiter_sequence` ⇒ ⇒ **不得**只修 1 个 ⇒ ⇒
      （⚠️ 「修复不完整」病症的高发区）
- [ ] `ctest -LE perf` 零回归 · **计数锚点不变**

## 4. Capabilities (契约影响)

- **可能**新建 capability `chlib-stream-semantics`（或 **MODIFIED** 既有
  `chlib-stream-semantics` 的「FIFO 类组件的输出时序」段）
  ⇒ ⇒ 具体落点**待 architect 裁决** ⇒ ⇒ ⚠️ 既有 `chlib-stream-semantics`
  主 spec 的 Requirement 标题**MUST NOT 改**（OpenSpec CLI 按标题匹配 ⇒ ⇒
  改标题 = 丢弃旧 + 新增 ⇒ ⇒ 拒绝归档）⇒ ⇒ 只能**原地订正**或**新增**；
- ⚠️ 若涉及**行为变更** ⇒ ⇒ MUST 在 ADR 中**记录**（「逐位不变」承诺不成立）。

## 5. Impact (影响面)

- **改动文件**：`include/chlib/stream.h`（`stream_arbiter_round_robin`）·
  可能 `include/chlib/stream_arbiter.h` · `tests/chlib/test_stream.cpp` ·
  `examples/stream/stream_arbiter_demo.cpp`
- **爆炸半径**：**小-中** —— 单个函数 + 其调用点
- **风险**：**中** —— **行为变更**（授予顺序变）⇒ ⇒ 既有消费者 MUST 复核
- **回滚路径**：`revert` 实施 commit ⇒ ⇒ 无持久化面
