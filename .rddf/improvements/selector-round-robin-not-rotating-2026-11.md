---
name: selector-round-robin-not-rotating-2026-11
priority: P2
source: 路线 B 只读查重（dual-backend-parity route B · 4 问①）实测挖出
phase: phase-5
category: chlib
type: bugfix
created_at: 2026-11-03
evidence: test:tests/chlib/test_selector_arbiter_dual.cpp
---

# Improvement: `chlib::round_robin_arbiter` **不轮转**（`next_ptr` 恒为 0）

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已修复**（2026-11-03，ADR-065 + change `chlib-arbiter-buildtime-eval-fix-2026-11` ⇒ 已归档 `openspec/changes/archive/2026-11-03-chlib-arbiter-buildtime-eval-fix-2026-11/`）
- **来源**: 路线 B 的 4 问① **只读查重** ⇒ 读 `include/chlib/selector_arbiter.h` 时发现，
  并用**独立探针实测确认**。

## 1. Why (背景与动机)

`include/chlib/selector_arbiter.h:98-117` 的 `UpdatePriority`（`round_robin_arbiter`
用来推进优先级指针的辅助结构体）用**普通 C++ `if`** 判断「本轮是否授予了某个请求」：

```cpp
template <unsigned N, unsigned Iter = 0> struct UpdatePriority {
    static void process(const RoundRobinArbiterResult<N> &result,
                        ch_uint<N> &next_priority, ch_uint<N> request,
                        ch_uint<N> priority_ptr) {
        if (result.valid) {                 // ← 建图期求值 ⇒ 恒假
            ...
            next_priority = select(req_at_pos & (result.grant == (ch_uint<N>(1_d) << pos)),
                                   next_pos, next_priority);
            if constexpr (Iter + 1 < N) { UpdatePriority<N, Iter + 1>::process(...); }
        }
    }
};
```

🔴 `result.valid` 是 `select(...)` 产出的**非 const 组合节点** ⇒ ⇒
`ch_bool::operator uint64_t()`（`src/lnode/bool.cpp:43-49`）**只对 const 节点返回真值**
⇒ ⇒ **C++ `if` 在建图期恒假** ⇒ ⇒ **整个 `if` 体从不执行** ⇒ ⇒
`next_priority` **永不更新** ⇒ ⇒ `ptr_reg->next = next_priority;` 恒写回原值
⇒ ⇒ **优先级指针永不前进** ⇒ ⇒ **`round_robin_arbiter` 恒从位置 0 开始扫描**
⇒ ⇒ **不是轮转，而是「固定从最低位开始」的优先级仲裁**。

### 实测证据（独立探针，`/tmp/opencode/probe_stream_arbiter.cpp`，不入仓）

请求向量恒为 `0b0001`（只有通道 0 有请求），连续 3 拍：

```
[probe2] rr tick0: grant=0x1 valid=1 next_ptr=0 (期望前进到 1)
[probe2] rr tick1: grant=0x1 valid=1 next_ptr=0 (期望前进到 1)
[probe2] rr tick2: grant=0x1 valid=1 next_ptr=0 (期望前进到 1)
```

⇒ ⇒ `next_ptr` **恒为 0**（期望每拍前进 1）。

⚠️ **与 `stream.h` 版的 `stream_arbiter_round_robin` 的区别**（实测澄清）：
后者用 `round_robin_selector`（**函数式**，无 C++ `if`）⇒ ⇒ 实测**选择正确**
（唯一有效输入 = 通道 1 ⇒ `selected=1`）⇒ ⇒ **缺陷只在 `selector_arbiter.h` 的
`round_robin_arbiter`** ⇒ ⇒ 两者**同名不同物**，容易被误判为「同一个东西」。

### 为什么既有测试没抓到（零判别力的证据）

`tests/chlib/test_selector_arbiter.cpp` 有 **5 个 TEST_CASE / 几十个 SECTION**，
看着很密 ⇒ 但它们**只断言 `grant` 与 `valid`** ⇒ ⇒ **`next_ptr` 从未被断言**
⇒ ⇒ 「指针不前进 ⇒ 不轮转」这条**正是轮转仲裁器的核心语义**，对既有测试**完全不可见**
⇒ ⇒ 长期「假绿」。

⚠️ 另注：既有测试也无法区分「轮转」与「固定低位优先」——只要请求向量不变，
两者的 `grant` 输出**逐位相同** ⇒ ⇒ 判别力必须来自「**请求向量随时间变化**」的序列。

## 2. What Changes (变更范围)

- **`UpdatePriority` MUST NOT 用 C++ `if` 判断 `result.valid`** ⇒ ⇒ 改成
  **无条件的链式 `select`**（对每个 `Iter` 都求值，`next_priority` 用 `select` 合成），
  或把 `valid` 判定折进 `select` 的条件里。
- ⚠️ **注意递归形态**：`UpdatePriority` 的递归目前在 `if` **体内** ⇒ ⇒
  改成无条件后，`ProcessRequests` / `UpdatePriority` 两个递归辅助结构体的
  **求值顺序**（串行依赖）必须重新核对 ⇒ ⇒ 改法需要小心，不是删一个 `if` 就行。
- ⚠️ **ADR 需求**：这是**行为变更**（轮转后 `grant` 的选择会变）⇒ ⇒ 需独立 ADR
  ⇒ ⇒ **不在 parity 批里做**。

## 3. Acceptance (验收标准)

- [ ] `round_robin_arbiter` 的 `next_ptr` 在**授予成功后**前进到被授予位置的下一位
- [ ] 请求向量**随时间变化**时，授予位置**真的轮转**（例如 `0b0001` → `0b0010`
      ⇒ 第二拍 MUST 授予通道 1，而不是继续授予通道 0）
- [ ] `grant` 仍是 **one-hot**，且恰好一位为 1
- [ ] **判别性用例**：既有 `tests/chlib/test_selector_arbiter.cpp` MUST 补
      **「请求向量变化」**的序列用例 + **`next_ptr` 的断言**
      ⇒ ⇒ 只断 `grant`/`valid` 的用例对「轮转 vs 固定低位优先」**零判别力**
- [ ] **变异证据**：把 `if (result.valid)` 改回原样 ⇒ ⇒ 用例 MUST 转红
- [ ] 双后端 parity：修好后 MUST 进 `DUAL_BACKEND_TEST_PAIR`（本组件**当前刻意不做**）

## 4. Capabilities (契约归属)

- 建议新建 capability `chlib-selector-arbitration`（或并入
  `chlib-stream-arbitration`）⇒ **ADDED ×1**，含
  「轮转仲裁器的优先级指针 MUST 在授予后前进」与
  「轮转语义 MUST 可被「请求向量随时间变化」的序列观测」两条 Requirement。
- ⚠️ **命名诚实性**：`round_robin_arbiter` 的名字承诺「round robin」⇒ ⇒
  修好前是**说谎**的 ⇒ ⇒ 修好后 MUST 登记进
  `.rddf/checks/chlib-naming-honesty-registry.md`（或与既有登记合并）。

## 5. Impact (影响面)

- **生产消费者**：`round_robin_arbiter` 本体**零生产消费者**
  （全仓只有 `include/chlib/selector_arbiter.h` + `tests/chlib/test_selector_arbiter.cpp`）
  ⇒ ⇒ **爆炸半径为零** ⇒ ⇒ 独立修不牵连其它组件。
- ⚠️ **不要与 `round_robin_selector` 混淆**（`include/chlib/selector_arbiter.h:164`）：
  后者**无 C++ `if`** ⇒ ⇒ 实测**行为正确** ⇒ ⇒ **不需要修**
  ⇒ ⇒ 且它**有生产消费者**（`include/chlib/stream.h` 的
  `stream_arbiter_round_robin` 调它）⇒ ⇒ 修 `round_robin_arbiter` **不影响**它。
- ⚠️ **本 parity 批刻意避开本组件**：dual parity 会把「不轮转」**盖章**
  ⇒ ⇒ 先修缺陷，再做覆盖。
