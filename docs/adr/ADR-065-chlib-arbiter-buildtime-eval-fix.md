# ADR-065: chlib 仲裁器族的两处「建图期求值」缺陷 —— 修法形态与锁存语义补全

- Status: **Accepted**（2026-11-03，用户授权「按你建议走」）
- Date: 2026-11-03
- Author: build-agent（architect role）
- Parent: **ADR-051**（`if` 语句块：「条件量在建图期被求值」的同族）· **ADR-050**（FSM 动作语义）
  · **ADR-054**（ready 形态 E）· **ADR-049 §1.4**（同一陷阱在 CLINT 上的先例）
- 关联 improvement: `.rddf/improvements/stream-arbiter-lock-selects-channel-zero-2026-11.md`
  · `.rddf/improvements/selector-round-robin-not-rotating-2026-11.md`
- 关联 capability: `chlib-stream-arbitration`（**ADDED ×1**）⇒ 归档**禁** `--skip-specs`
- 前置证据：独立探针 `/tmp/opencode/probe_arb.cpp`（**不入仓**），实测输出见 §1.3 / §1.4

---

## 1. Context

### 1.1 触发

路线 B 的 4 问① 只读查重逐个读 `include/chlib/*.h` 的**实现**时发现（不是读注释），
并用独立探针实测确认。两个 improvement 已登记但都**未修**（当时它们带缺陷 ⇒ 做
dual parity 会给缺陷**盖章**）。

### 1.2 共同根因（**同一处陷阱，两处实例**）

`ch_bool::operator uint64_t()`（`src/lnode/bool.cpp:43-49`）**只对 const 节点返回
真值，非 const 节点恒返 0** ⇒ 任何 `if (ch_bool_expr)` 都是**建图期**求值 ⇒
**恒假** ⇒ `if` 体**从不执行**。同族已登记：`_if`（ADR-051）· FSM（ADR-050）·
cache wrapper（ADR-045 批）· `add_with_carry` 的 `cin` · `edge_detector` ·
`fixed_point_divider` · CLINT 的 `carry ? 1 : 0`（ADR-049 §1.4）。

### 1.3 实例 A：`stream_arbiter_lock` 恒选通道 0 **且输出错误通道的数据**

`include/chlib/stream_arbiter.h:62-68` 的选择逻辑写成**普通 C++ `if/else`**：

```cpp
if (current_locked_valid) { selected_idx = locked_channel; }      // 建图期恒假
else if (has_valid)       { selected_idx = priority_encoder(...);
                            locked_channel = selected_idx; }       // ← C++ 赋值
else                      { selected_idx = locked_channel; }       // ← 恒走这里
```

**实测**（3 路输入，**只有通道 1 有效**、payload `0x22`）：
`selected=0  out.valid=1  out.payload=11`（期望 `selected=1` / payload `0x22`）
⇒ **不只是选错通道，而是输出了另一个通道的 payload** ⇒ **静默数据损坏**
（`out.valid` 仍是对的 ⇒ 更难发现）。

⇒ ⇒ **三个缺陷叠在同一个组件上**：
① 选择由建图期求值的 `if` 决定 ⇒ **恒选通道 0**；
② `locked_channel = selected_idx;` 是**对 `ch_reg` 的 C++ 赋值**（既非 `->next`、
也非数据流边）⇒ **锁存永不生效** ⇒ 头注释承诺的「stays selected until the
transaction completes」**完全没有实现**；
③ **实施期才发现**：`out.valid = valid_vector != 0_d;` 用的是「**任一**通道
valid」而非「**选中**通道的 valid」⇒ 锁定通道撤销 valid 而其它通道有效时
`out.valid=1` 但 payload 属于另一个通道 ⇒ 同型的数据/valid 不匹配。

### 1.4 实例 B：`round_robin_arbiter` 不轮转（`next_ptr` 恒 0）

`include/chlib/selector_arbiter.h:98-117` 的 `UpdatePriority::process` 有一个
**外层 `if (result.valid)`**（条件是 `select(...)` 产出的**非 const 组合节点**）
⇒ ⇒ 建图期恒假 ⇒ ⇒ **整个 if 体（含递归展开）从不执行** ⇒ ⇒ `next_priority`
永不更新 ⇒ ⇒ `ptr_reg->next` 恒写回原值 ⇒ ⇒ **恒从位置 0 扫描** ⇒ ⇒
**不是轮转，而是「固定从最低位开始」的优先级仲裁** ⇒ ⇒ 持续有效的低位通道
**饿死**高位通道（正是 capability 第 1 条 Requirement 禁止的）。

**实测**（`request` 恒 `0b0001`，连续 3 拍）：`next_ptr` 恒 `0`（期望 `1/2/3`）。

### 1.5 既有测试零判别力

| 测试 | 现状 |
|---|---|
| `tests/test_stream_arbiter.cpp` 的 2 个 lock 用例 | 只断 `.width` ×3 + **1 条** `output_stream.valid == true` ⇒ **`selected` 与 `payload` 从未被断言** |
| `tests/chlib/test_selector_arbiter.cpp` 的 12+ 个用例 | 全部裸 `context`（无 `Simulator`）⇒ **`next_ptr` 的动态推进不可观测** |

⇒ ⚠️ 名字叫「holds lock until transaction」的用例**从未测过锁** ⇒ 又一个「绿 ≠ 对」。

### 1.6 消费者 = **0**

两者都只有测试消费者 ⇒ **生产消费者为零** ⇒ 爆炸半径为零。⚠️ **`round_robin_arbiter`
不是 `stream_arbiter_round_robin` 的重复**：后者用 `round_robin_selector`（无内部
状态）+ 外部 `last_grant_reg`；前者**自持 `ptr_reg`** 并暴露 `next_ptr` ⇒ 层级不同。


---

## 2. Decision

### D1. 两处都**修**（不删除）

删除先例 `fwft_fifo` / `stream_arbiter_sequence` 的共同点是「**能力已被别处覆盖**」
（`sync_fifo.q_async` / `stream_arbiter_priority`）—— 此处**不成立**：没有任何组件
提供「锁存仲裁」或「自持状态的轮转原语」⇒ 删除 = 造能力缺口。

### D2. 实例 B 的修法：**删掉外层 `if`，select 链无条件执行**

```cpp
// ⚠️ `if (result.valid)` 是建图期求值的 C++ 控制流 ⇒ 恒假 ⇒ MUST NOT 保留
ch_uint<compute_idx_width(N)> pos = (priority_ptr + Iter) % N;
ch_bool req_at_pos = bit_select(request, pos);
ch_uint<compute_idx_width(N)> next_pos = ((pos + 1) % N);
next_priority = select(req_at_pos & bit_select(result.grant, pos),
                       next_pos, next_priority);
if constexpr (Iter + 1 < N) {   // ⚠️ 编译期展开，保留
    UpdatePriority<N, Iter + 1>::process(result, next_priority, request, priority_ptr);
}
```

⚠️ **`if constexpr` 与 `if` 的区别是本 ADR 的承重点**：前者**编译期**展开（合法且
必需），后者是**运行期**条件（非法 —— 条件量是硬件值）。
⚠️ **判据 MUST 用 `bit_select(grant, pos)`**（`grant` 是 one-hot ⇒ 与
`grant == (1 << pos)` 等价），**MUST NOT** 用后者 —— 它的 `<<` 会生成
**N + (N-1) 位**的中间节点（N=64 ⇒ **127 位**），而本仓 interpreter 的 `eq`
在该位宽组合且**移位量 ≥ 2** 时**判错**（见 §5.2 的实施期发现）。

### D3. 实例 A 的修法：选择逻辑改成 `select` 链 + `->next`

- 选择：`selected_idx = select(locked, locked_channel, select(has_valid, prio_sel, locked_channel))`
- 锁存更新：MUST 用 `locked_channel->next`（**MUST NOT** C++ 赋值）
- ⚠️ **`out.valid` MUST 是「选中通道的 valid」**，**MUST NOT** 是「任一通道 valid」
  （§1.3 的第三个缺陷）⇒ `out_valid = mux(valid_vector, selected_idx)` 或等价形态。

### D4. 实例 A 的锁存语义补全（**新增 `locked` 寄存器**）

头注释承诺「stays selected until the transaction completes」⇒ MUST 真正实现：
**未锁定**且 `has_valid` ⇒ 仲裁出 `prio_sel` 并**锁定**到它（`locked<=1`,
`locked_channel<=prio_sel`）；**已锁定** ⇒ **保持**选中 `locked_channel`（**不管**
其它通道是否更优先）；**已锁定**且 **`output.fire`**（`valid & ready`）⇒ **释放**。

⇒ 与「`locked_channel` 仍 valid 就继续」**不同**：后者看**输入 valid**，前者看
**事务能否完成** ⇒ 后者会在「通道中途撤销 valid」时**换人**（打断已送出的数据）。
⚠️ 前提：master MUST 保持 `valid` 到 `ready`（Stream/AXI 通行要求）⇒ 不遵守是
**协议违规**，不是本组件缺陷。

### D5. 既有测试 MUST 补**行为断言**（否则「绿但没测到」）

- `stream_arbiter_lock`：MUST 断言 `selected` 与 `output_stream.payload`
  （≥2 个不同通道各自唯一的用例 ⇒ 能区分「选错」与「数据错」）+ **锁存保持**
  + **释放**；`round_robin_arbiter`：MUST 用 `Simulator` 观测 `next_ptr` 的**跨拍推进**

### D6. 修好后 MUST 补 dual parity（两组件各 +1 pair 组）⇒ 路线 B 的候选补完

### D7. capability：`chlib-stream-arbitration` **ADDED ×1**

新增 Requirement：「租借式（锁存）仲裁 MUST 保持选中到事务完成」+「selector 层轮转
仲裁器的 `next_ptr` MUST 每拍推进」+ 4 个 Scenario。⚠️ **不 MODIFIED 既有条款**：
第 1 条「轮转仲裁器 MUST 真正轮转」的**适用面**是 `stream_arbiter_round_robin`
（stream 层），`round_robin_arbiter` 是 selector 层 ⇒ 在 ADDED 里显式声明适用范围，
避免改既有条款的 Scenario 标题（CLI 按标题匹配 ⇒ 改标题 = 丢弃 + 新增 ⇒ 拒绝归档）。

### D8. 明确**不在**本次范围

| 项 | 为什么不一起做 |
|---|---|
| `priority_encoder` 注释「优先处理低位」与实现（高位优先）相反 | 在 `include/chlib/converter.h`，独立立项 |
| `(priority_ptr + Iter) % N` 当 `N` 非 2 的幂时要真实除法器 | 现状语义正确（只是成本），无缺陷 |
| `chlib` 全仓「C++ 控制流作用于硬件值」的**普查** | 4 简述中已扫过，无第三处同型；**不做全仓普查**（B8 已否决该义务） |


---

## 3. 选项对比

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| A | **只修 A-4**，保留 `stream_arbiter_lock` 现状 | ❌ 否决 | 留下**静默数据损坏**（输出别的通道的 payload）⇒ 比「不轮转」严重得多 |
| B | **只删 A-3 的 `if`**，**不**加 `locked` 寄存器 | ❌ 否决 | 「锁定到事务完成」**仍未实现** ⇒ 名字仍说谎；`out.valid` 仍是「任一通道 valid」 |
| C | **A-4 删 `if` + A-3 改 select 链 + 补 `locked` 寄存器 + 修 `out.valid`**（D2–D4） | ✅ **选中** | 两处根因同一形态（无条件 select 链）；`locked` 让名字兑现；`out.valid` 消除数据/valid 不匹配 |
| D | 物理删除两个组件 | ❌ 否决 | 两者都**无替代**（层级不同：`stream_arbiter_round_robin` ≠ `round_robin_arbiter`；锁存仲裁在仓内唯一）|
| E | 冻结现状 + 文档化「不可用」| ❌ 否决 | 组件**静默返回错误数据**；`include/chlib.h` 已导出 ⇒ 文档化不能替代修复 |

---

## 4. 后果与代价

### 正面

- 消除**静默数据损坏**（A-3）与**饿死**（A-4，违反 capability 第 1 条）；
  `stream_arbiter_lock` 的**名字/注释首次兑现**；顺带修掉**实施期才发现的** `out.valid` 缺陷。

### 负面 / 代价

- **两个组件的行为都会变**（这正是修复）⇒ 本仓内 **0 消费者**，风险仅在仓外（无证据）。
- 加 `locked` 寄存器 ⇒ `stream_arbiter_lock` 多一个 `ch_reg`（组合逻辑不变）。
- ⚠️ `out.valid` 语义收紧后，「锁定通道撤销 valid 但其它通道有效」返回 `valid=0`
  （旧实现返回 1 + 错数据）⇒ 更保守但**更正确**。

### 风险

| 风险 | 缓解 |
|---|---|
| 锁存引入死锁（锁定通道永不 `ready`） | 契约写明「master MUST 保持 `valid` 到 `ready`」；§5 测「释放」路径 |
| A-4 删 `if` 后 `next_priority` 的 select 链序错 | 只有**被授予**的那一位 cond 为真 ⇒ 无歧义；§5 用多请求者验证 |
| 既有 12+ 个裸-context 用例因语义变化转红 | **已发生**：8 处转红 ⇒ **全部**是「钉住了缺陷」⇒ 按正确语义更新（§5.4） |

---

## 5. Verification（前瞻验收判据 —— **可勾选**；实施后逐条回填）

### 5.1 A-3（`stream_arbiter_lock`）—— ✅ 实施后回填

- [x] 唯一有效 = 通道 `k` ⇒ `selected == k` **且** `payload == 通道 k 的 payload`
      （`k = 0/1/2` 各一例 ⇒ 能区分「选错」与「数据错」）—— 探针实测
      `selected=1` / `payload=0x22`（修复前 `selected=0` / `payload=0x11`）
- [x] 锁存：A 被授予后 B 变有效 ⇒ `selected` **仍为 A**；`fire` 后可选 B
- [x] 无请求 ⇒ `out.valid == 0`（`selected` 保持，不影响下游）
- [x] **锁定通道撤销 valid、其它通道有效** ⇒ `out.valid == 0`

### 5.2 A-4（`round_robin_arbiter`）—— ✅ 实施后回填

- [x] `request` 恒 `0b1111` ⇒ `next_ptr` 每拍推进且 `grant` 跟着轮转
- [x] 偏请求（bits 0 与 3）⇒ 两个请求者**交替**被服务（无饿死）
- [x] `request == 0` ⇒ `grant == 0`、`valid == 0`、`next_ptr` 保持
- [x] ⚠️ **实施期新增发现**：N=64 时「被授予位」判定 MUST 用 `bit_select`
      而**非** `grant == (1 << pos)` —— 后者的 `<<` 生成 **127 位**节点，而
      interpreter 的 `eq` 在「64 位 vs 127 位」且**移位量 ≥ 2** 时**判错**
      ⇒ ⇒ 表现为 64 位下 `next_ptr` 恒 0 ⇒ ⇒ 该**框架缺陷独立立项**
      （本 change 只绕开，不修框架）。

### 5.3 变异验证

变异清单与逐条实测结果 **MUST NOT 进本正文**（architect 产出契约：可枚举的
变异清单属「推导与证据」）⇒ ⇒ 落在
`openspec/changes/chlib-arbiter-buildtime-eval-fix-2026-11/tasks.md` §T4。

⚠️ 本轮实测的两条纪律教训（值得留在正文）：
① **还原「原始缺陷形态」时 MUST 用原始写法** —— 第一版 M5 写成
   `if (!result.valid) return;`，而 `!` 是**硬件算子**（产出节点）⇒
   `if (节点)` 在**建图期恒假** ⇒ ⇒ **永远不 return** ⇒ 该变异**恒绿**
   （等价）⇒ ⇒ 改回原始的 `if (result.valid) { ... }` 才转红；
② 变异注入器的 `text=True` 会因 Verilator 的**非 UTF-8 输出**抛
   `UnicodeDecodeError` ⇒ MUST 用 `errors="replace"`。

### 5.4 契约与计数

- [x] capability `chlib-stream-arbitration` ADDED ×1 已起草（归档后合并；禁 `--skip-specs`）；
      命名诚实性登记表 +2（`stream_arbiter_lock` / `round_robin_arbiter`）
- [x] 两组件进 `DUAL_BACKEND_TEST_PAIR`（**7** pair / **184 == 2 × 92** 断言）⇒ parity 源文件数 +1
- [x] 既有裸-context 用例逐个核对：**8 处转红** ⇒ 全部是「钉住了缺陷」⇒ 按**正确语义 +
      手推读模型**更新（84 → 120 断言）；`ctest -LE perf` **229/229** 全绿


---

## 6. 参考与审查记录

- ADR-051 · ADR-050 · ADR-054 · ADR-049 §1.4（同陷阱）
- capability `chlib-stream-arbitration` 第 1 条 —— **`round_robin_arbiter` 一直在违反它**
  （只是没人测）；同族已登记未修：`priority_encoder` 注释与实现相反 ⇒ 独立议题
- 审查记录：**build-agent 自查一轮**；critic 因 Token Plan 配额不可用（外部限制），
  **未过外部评审** ⇒ 实施前如有异议请指出
