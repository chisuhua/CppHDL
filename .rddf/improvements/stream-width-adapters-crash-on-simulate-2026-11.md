---
name: stream-width-adapters-crash-on-simulate-2026-11
priority: P1
source: 路线 B 只读查重（dual-backend-parity route B · 4 问①）实测挖出
phase: phase-5
category: chlib
type: bugfix
created_at: 2026-11-03
evidence: test:tests/chlib/test_stream_width_adapter_dual.cpp
---

# Improvement: `chlib` 的三个 stream 宽度适配器**构建能过、一仿真就 SIGSEGV**

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已修复**（2026-11-03，ADR-064 + change `chlib-stream-width-adapter-rewrite-2026-11` ⇒ 已归档 `openspec/changes/archive/2026-11-03-chlib-stream-width-adapter-rewrite-2026-11/`，commit `30b1c95`）
- **来源**: 路线 B 的 4 问① **只读查重** ⇒ 逐个读 `include/chlib/stream_width_adapter.h`
  的实现时**发现**，并用**独立探针实测确认**。

## 1. Why (背景与动机)

`include/chlib/stream_width_adapter.h` 的 `stream_narrow_to_wide`（`:54`）里：

```cpp
    // Output valid when accumulator is full and input is valid
    result.valid = select(is_full, input.valid, ch_bool(false));
    result.payload = accumulator;
    // Input ready when not full or output is ready
    input.ready = select(is_full, result.ready, ch_bool(true));   // ← 🔴
```

🔴 `result.ready` 是**刚建出来、还没有任何节点**的字段（`result` 是函数内新建的
`ch_stream<TWide>`）⇒ ⇒ `select(is_full, result.ready, ...)` 拿到的是**空 proxy**
⇒ ⇒ mux 指令的**一个操作数是空指针** ⇒ ⇒ **构建期不报错**（节点建出来了），
但**一进 `Simulator::initialize()` 就 SIGSEGV**。

⚠️ **对比**：同仓的 `stream_arbiter_priority`（`include/chlib/stream.h:369`）**显式**做了
「**先建默认驱动再读**」并留了注释：

```
    // ⚠️ **为什么「先建默认驱动」再「读 `output_stream.ready`」是安全的**：
    ch_bool default_ready(true);
    result.output_stream.ready <<= default_ready;   // ② 默认恒就绪
    ...
    result.input_streams[i].ready <<= (is_selected & result.output_stream.ready);
```

⇒ ⇒ **宽度适配器缺了这一步** ⇒ ⇒ 与 ADR-054 形态 E 的既有纪律**不一致**。

### 实测证据（独立探针 `/tmp/opencode/probe_width_adapter2.cpp`，不入仓）

探针：建组合子 → **构造 `Simulator`**（⇒ 触发 `initialize()` ⇒ 图被实例化）⇒ 看是否 SIGSEGV。

```
n2w       [probe] combinator built OK, now constructing Simulator...   ⇒ 💥 SIGSEGV
w2n       [probe] combinator built OK, now constructing Simulator...   ⇒ 💥 SIGSEGV
generic   [probe] combinator built OK, now constructing Simulator...   ⇒ 💥 SIGSEGV
extend    [probe] Simulator constructed OK => 该组合子**可仿真**        ✔（但**零消费者**）
truncate  [probe] Simulator constructed OK => 该组合子**可仿真**        ✔（但**零消费者**）
```

⇒ ⇒ **`stream_narrow_to_wide` / `stream_wide_to_narrow` / `stream_width_adapter`
三个对外 API 全部「构建能过、一仿真就崩」** ⇒ ⇒ **组件当前完全不可用**。

### 为什么既有测试完全没发现（零判别力的证据）

`tests/test_stream_width_adapter.cpp` 有 **8 个 TEST_CASE**（`narrow_to_wide 8→32` ·
`wide_to_narrow 32→8` · 各种 ratio · `Transfer Ratio` · `Type Compilation`）⇒ 看着很密
⇒ ⇒ **但它从不构造 `Simulator`**（实测：文件里 `Simulator` 出现 **0** 次）⇒ ⇒
只做**裸 context** 的**类型/宽度**检查 ⇒ ⇒ 图**从不被实例化** ⇒ ⇒
**空操作数的 mux 永不被兑现** ⇒ ⇒ 缺陷**完全不可见** ⇒ ⇒ 长期「假绿」。

⚠️ 这与「**裸 context ⇒ 命中 R6 排除项**」是同一条线索的另一面：**裸 context 的测试
连「图能否被实例化」都验证不了** ⇒ ⇒ **组件级覆盖必须至少有一个「真跑仿真」的用例**。

## 2. What Changes (变更范围)

- **`stream_narrow_to_wide`**：MUST 先给 `result.ready` 建**默认驱动**
  （`ch_bool default_ready(true); result.ready <<= default_ready;`），
  **再**读它 ⇒ ⇒ 与 `stream_arbiter_priority` 的既有纪律**对齐**。
- **`stream_wide_to_narrow`**：同类问题（`:147` 的 `input.ready = !valid_flag;` 用
  `operator=` ⇒ **节点重绑定** ⇒ 调用方的 `ready` **收不到反压** ⇒ 静默失效）
  ⇒ ⇒ MUST 改成 `<<=`（**数据流边**）⇒ ⇒ 调用方真收到反压。
  ⚠️ **实测**：读未驱动的 `src.ready` ⇒ **SIGSEGV**（空节点解引用）。
- **`stream_width_adapter`**（`:163` 的泛型版）：它分派到上面两个 ⇒ ⇒ 随它们一起修。
- ⚠️ **另有两个「零消费者」的辅助函数**：`stream_extend`（`:191`）· `stream_truncate`（`:213`）
  ⇒ **可仿真但零引用** ⇒ ⇒ 按 Zero-Debt（「No dead code」）⇒ ⇒ **物理删除**
  （而不是给它们补测试）。
- ⚠️ **ADR 需求**：这是**行为变更**（反压真的开始到达上游）⇒ ⇒ 需独立 ADR
  ⇒ ⇒ **不在 parity 批里做**。

## 3. Acceptance (验收标准)

- [ ] `stream_narrow_to_wide` / `stream_wide_to_narrow` / `stream_width_adapter`
      **构造 `Simulator` 不再崩溃**（⇒ ⇒ 必须有「真跑仿真」的用例）
- [ ] 三个适配器的**上游 `ready` 真的被驱动**（⇒ ⇒ 调用方能收到反压）
      ⇒ ⇒ 判据 MUST 用**调用方持有的 handle**（不是组合子内部的副本）
- [ ] **累积语义**被钉住（`narrow_to_wide` 的宽字内容与输出 `valid` 的拍次）
      ⇒ ⚠️ **读实现时发现的疑点**（**尚未实测确认**，修的时候必须核对）：
      `result.payload = accumulator` **不含当前窄字**（当前窄字在**同一沿**才进累加器）
      ⇒ 且 `accumulator` **从不清零** ⇒ ⇒ 「宽字缺最后一个窄字」+「下一组与上一组重叠」
      这两点**疑似**是缺陷 ⇒ ⇒ **修的时候必须先手推期望值再对比实测**
- [ ] `stream_extend` / `stream_truncate` **物理删除**（零消费者 ⇒ Zero-Debt）
- [ ] **变异证据**：把「先建默认驱动」那一步去掉 ⇒ ⇒ 用例 MUST 转红（不得再崩）
- [ ] 双后端 parity：修好后三个适配器 MUST 进 `DUAL_BACKEND_TEST_PAIR`
      （本组件的 parity 覆盖**当前刻意不做** ⇒ 见 §5）

## 4. Capabilities (契约归属)

- 建议新建 capability `chlib-stream-width-adapter`（或并入 `chlib-stream-semantics`）
  ⇒ **ADDED ×1**，含「宽度适配器 MUST 可被仿真（构建成功 ⇒ 图可实例化）」
  与「宽度适配器 MUST 把上游 `ready` 用**数据流边**驱动」两条 Requirement。
- ⚠️ **命名诚实性**：`stream_extend` / `stream_truncate` 删除后，登记表若收录过它们
  MUST 同步删除（R1 会检出「登记的组件已不存在」⇒ ⇒ 自检会红 ⇒ ⇒ 这是**预期**的）。

## 5. Impact (影响面)

- **生产消费者**：**0**（全仓只有 `include/chlib/stream_width_adapter.h` 自身 +
  `tests/test_stream_width_adapter.cpp`）⇒ ⇒ **爆炸半径为零** ⇒ ⇒ 可以独立修。
- ⚠️ **本 parity 批刻意避开本组件**：dual parity 会**在构建时崩溃**
  （verilator 后端同样要 `initialize()`）⇒ ⇒ **先修缺陷，再做覆盖**。
- ⚠️ **顺带登记的文档缺陷**（同一族，见姊妹 draft
  `priority-encoder-doc-contradicts-implementation-2026-11.md`）：
  `include/chlib/converter.h` 的 `priority_encoder` 注释写「**优先处理低位**」，
  而实现 `result = select(bit_at_i, current_value, result)` ⇒ ⇒ **最高**置位胜
  ⇒ ⇒ **实测 `priority_encoder(0b1010) == 3`**（注释说 1）⇒ ⇒ 文档与实现**相反**。
