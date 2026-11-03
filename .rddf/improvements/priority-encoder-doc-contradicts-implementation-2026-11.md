---
name: priority-encoder-doc-contradicts-implementation-2026-11
priority: P2
source: 路线 B 只读查重（dual-backend-parity route B · 4 问①）实测挖出
phase: phase-5
category: chlib
type: docfix
created_at: 2026-11-03
evidence: test:tests/chlib/test_arithmetic_encoding_dual.cpp
---

# Improvement: `priority_encoder` 的注释「优先处理低位」与实现**相反**（实测）

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已修复（文档订正部分）**（2026-11-03 · change
  `header-self-containment-fix-2026-11` 的 A2 类）⇒ ⇒ `combinational.h` 的头注释
  已订正为「**编号最大者胜（高位优先）**」，并加了「本仓『优先级』有两个方向」的
  交叉说明（`priority_selector` 是**低位**优先 ⇒ 与本组件**相反**）。
  ⇒ ⚠️ **行为未改**（只改注释）—— 是否把实现改成低位优先 = **另一决策**
  （会改变所有消费者的语义）⇒ ⇒ 仍待裁决；本条只关闭「名字/注释说谎」这一半。
- **来源**: 路线 B 的 4 问① **只读查重** ⇒ 读 `include/chlib/combinational.h` 时**发现**，
  并用**独立探针实测确认**。

## 1. Why (背景与动机)

`include/chlib/combinational.h:14-17` 的文档注释：

```
/**
 * 优先级编码器 - 函数式接口
 *
 * 将输入的one-hot编码转换为二进制索引，优先处理低位
 */
template <unsigned N>
ch_uint<compute_idx_width(N)> priority_encoder(ch_uint<N> input) {
    ...
    for (unsigned i = 0; i < N; ++i) {
        ch_bool bit_at_i = bit_select(input, i);
        ch_uint<OUTPUT_WIDTH> current_value = make_uint<OUTPUT_WIDTH>(i);
        result = select(bit_at_i, current_value, result);   // ← 后写覆盖 ⇒ **最高**位胜
    }
```

🔴 循环是 `i = 0 → N-1`，且每次 `select(bit_at_i, current_value, result)` 都会
**覆盖**先前结果 ⇒ ⇒ **最后一个（= 最高的）置位胜出** ⇒ ⇒ 实现是「**优先处理高位**」
⇒ ⇒ 与注释「**优先处理低位**」**相反**。

### 实测证据（独立探针）

```
[probe1] priority_encoder(0b1010) = 3   (注释「优先处理低位」=> 1；实现 select 覆盖 => 3)
```

⇒ ⇒ **实测 = 3**（最高位 bit3）⇒ ⇒ 注释是错的。

### 下游影响（实测）

`include/chlib/stream.h` 的 `stream_arbiter_priority` 用 `priority_encoder` 选通道 ⇒ ⇒

```
[probe2] stream_arbiter_priority (通道0/2 有效) => selected=2 payload=0xA2
         in[0].ready = 0
         in[1].ready = 0
         in[2].ready = 1
```

⇒ ⇒ **高位优先**（通道 2 胜）⇒ ⇒ ⚠️ 与同仓 `chlib::priority_selector`
（`include/chlib/selector_arbiter.h:35`，注释「从低位到高位检查请求，低位优先级更高」）
**方向相反** ⇒ ⇒ **两个都叫「优先级」的公开组合子，方向却相反** ⇒ ⇒
「优先级」这个词在本仓**含义不唯一** ⇒ ⇒ 使用者极易误用。

⚠️ **既有的单后端测试零判别力**：`tests/test_stream_arbiter.cpp` 的 priority 用例
只构造「**唯一**有效输入」⇒ ⇒ 单路有效时两种方向**逐位相同** ⇒ ⇒ 方向**不可观测**
⇒ ⇒ 文档与实现的矛盾从未被暴露。

## 2. What Changes (变更范围)

⚠️ **先裁决方向，再动代码**（**不要**顺手改实现）：

- **选项 A（推荐）**：**改注释**，把 `priority_encoder` 的文档改成「**优先处理高位**」
  ⇒ ⇒ 零行为变更 ⇒ ⇒ 同时在 `stream_arbiter_priority` 的注释里**显式写明**它是
  **高位优先**（并与 `priority_selector` 的低位优先**对照**）⇒ ⇒ 消除歧义。
  ⇒ 代价：调用方若依赖「低位优先」的**预期**，会得到相反结果 ⇒ ⇒ 但实现**从未**是
  低位优先 ⇒ ⇒ 依赖它的人本来就错 ⇒ ⇒ 订正注释是**如实化**而非破坏。
- **选项 B**：把实现改成「低位优先」⇒ ⇒ **行为变更** ⇒ ⇒ 与 ADR-047 接入真实流水线的
  `chlib::ForwardingMux`/仲裁路径**可能冲突** ⇒ ⇒ 需独立 ADR + 全量回归
  ⇒ ⇒ **本 change 不做**。
- ⚠️ **另外**：无论选 A 还是 B，**MUST** 在 `chlib/AGENTS.md` 里写明
  「`priority_encoder` = 高位优先」与「`priority_selector` = 低位优先」的**对照**
  ⇒ ⇒ 否则下一个使用者还会踩。

## 3. Acceptance (验收标准)

- [ ] `priority_encoder` 的注释与实现**一致**（并注明是高位还是低位）
- [ ] `stream_arbiter_priority` 的注释**显式**写明选择方向
- [ ] `chlib/AGENTS.md` 给出 `priority_encoder` vs `priority_selector` 的**方向对照**
- [ ] **判别性用例**：`priority_encoder` 与 `stream_arbiter_priority` MUST 有
      **多位置位**的向量（⇒ 单路有效时方向不可观测 ⇒ ⇒ 旧测试抓不到）
      ⇒ ⇒ 期望值按**当前实现**（高位优先）写 ⇒ ⇒ 若将来改成低位优先 ⇒ 用例转红
      ⇒ ⇒ 这是一次**有意的行为变更信号**
- [ ] **变异证据**：把 `priority_encoder` 的循环方向反转 ⇒ ⇒ 用例 MUST 转红

## 4. Capabilities (契约归属)

- capability `chlib-stream-arbitration`（若已有）**MODIFIED ×1**：写明
  「`stream_arbiter_priority` 的选择方向为**高位优先**（`priority_encoder` 的语义）」
  与「`priority_selector` 的方向为**低位优先**」⇒ ⇒ **两者的差异 MUST 显式**。
- ⚠️ **命名诚实性**：若登记表收录了 `stream_arbiter_priority`，其「声称的语义」列
  MUST 写明方向 ⇒ ⇒ 否则登记表会把**错误的**方向固化成「声称」。

## 5. Impact (影响面)

- **生产消费者**：`priority_encoder` 被 `include/chlib/stream.h` 的
  `stream_arbiter_priority` 使用；`stream_arbiter_priority` 本身**零生产消费者**
  （只有 `include/chlib/stream.h` + `tests/chlib/test_stream.cpp` +
  `tests/test_stream_arbiter.cpp`）⇒ ⇒ **爆炸半径小**。
- ⚠️ **本批（路线 B 的 `stream_arbiter` dual 覆盖）按「当前实现」断言**（高位优先）
  ⇒ ⇒ 与本 draft 的选项 A 一致 ⇒ ⇒ **两者不冲突** ⇒ ⇒ 修注释不需要改测试期望。

## 6. ✅ 修复记录（2026-11-03）

- **改动**：`include/chlib/combinational.h` 的 `priority_encoder` 头注释 ——
  「优先处理低位」⇒ 「**编号最大者胜（高位优先）**」，并写明与 `priority_selector`
  的方向**相反**（后者是低位优先）。
- **行为未动**：这是 `type: docfix` ⇒ ⇒ 只消除「注释与实现相反」这一点
  （它曾让 `stream_arbiter_sequence` 的注释也照抄同一句错误表述）。
- **交叉登记**：`.rddf/checks/chlib-naming-honesty-registry.md` 新增
  `priority_encoder` 一行（判别性用例 = `tests/chlib/test_stream_arbiter_dual.cpp`
  的 `[chlib][stream][arbiter][priority][grant][ready][dual]`）⇒ ⇒ 登记表 **16 → 17**。
- **验证**：`bash scripts/verify_chlib_naming_honesty.sh` 绿（R1/R2/R3）。
