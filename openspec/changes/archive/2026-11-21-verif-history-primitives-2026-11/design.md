# design: `.verif` 原语集扩展 —— 历史/序列判据（ADR-076）

> **决策面在 ADR**（`docs/adr/ADR-076-verif-history-primitives.md`）⇒ ⇒ 本文件是**推导与实现细则**。
> ⚠️ **不变量守恒**：ADR-074 D1/D2 的「**值在生成期全部求值成字面量 ⇒ IR 里没有表达式 ⇒
> 两个执行后端不需要各自解释表达式**」**逐字保持** ⇒ ⇒ 新原语的**全部参数都是字面量**
> （`n` / `lag` / `value` / `count` 在扫描期求值）⇒ ⇒ **不引入任何表达式**。

---

## 1. 为什么需要（问题陈述）

`.verif` 的封闭原语集（ADR-074 D2）能表达「**当前采样点**的观测」，但**不能表达「对过去若干个
采样点的关系」的观测」。⇒ ⇒ 三类判据**不可表达**：

| 判据形态 | 具体实例（`examples/spinalhdl-ported/`） | 缺什么 |
|---|---|---|
| **自滞后恒等** | `counter_simple` 的「周期 256」：`v[n] == v[n+256]` | 历史 + 「与 N 拍前的那份逐项比」 |
| **相邻差 vs 他端口** | `counter` 的「Δ == enable」：`v[n+1]-v[n] == enable[n]` | 历史 + 「相邻差」+「与另一端口同拍值比」 |
| **窗口计数** | `pwm` 的「256 拍内高电平数 == duty」 | 历史 + 「窗口内等于某值的项数」 |

⇒ ⇒ 这三条都是**metamorphic（相对判据）**⇒ ⇒ 仓内对它们的定位是「**不依赖任何手推绝对期望值**」
⇒ ⇒ **降级成绝对判据 = 判据面实质收窄** ⇒ ⇒ 本 change 扩原语集，**保住判据面**。

⚠️ **为什么不是「加一条万能原语」**：ADR-074 D2 的封闭集是**fail-closed 的前提**（集合外构造 ⇒
扫描器非零退出）⇒ ⇒ 每条新原语 MUST 有**单一、无歧义、可两端逐字同构**的语义 ⇒ ⇒ 本 change
只加**4 条**（`RECORD` + 3 条 `EXPECT_SEQ_*`），每条都**有实际消费者**（Zero-Debt：禁死代码）。

---

## 2. 新原语（封闭集增补 · 4 条）

### 2.1 `RECORD <port>`

- **语义**：把 `<port>` 在**当前采样点**（沿后结算后 · 与 `EXPECT` 同一采样窗）的值
  **追加**到该端口的**历史序列**末尾。
- **不产生断言**（`checks` 不增 · `check_id` 不分配）⇒ ⇒ 它是「记录」不是「判断」。
- **历史归属**：按**端口标识符**（`.verif` 里写的名字）⇒ ⇒ 两端用**同一 key** ⇒ ⇒ 一致。
- **跨 scenario**：**不清空**（全局累积）⇒ ⇒ 与 DUT 状态跨 scenario 持续**同构**
  ⇒ ⇒ 文档 MUST 写明（否则作者会误以为 scenario 隔离）。
- **上限**：`HISTORY_DEPTH = 1024` ⇒ ⇒ **扫描期**静态判定（见 §4）⇒ ⇒ 运行期**不可能**超限。
  ⚠️ **该承诺成立的充要前提**（oracle 闭合项 1）：
  ① 静态计数器**一份**（**不是** per-scenario 局部）· **跨 scenario 不清空**；
  ② 按**展开后**的程序顺序累计（`REPEAT` 在扫描期展开 ⇒ 追加顺序 == 运行期顺序）；
  ③ **key = 端口标识符字符串**（与运行期历史缓冲的 key **逐字同一**）；
  ④ 运行期的追加次数 **==** 静态计数（`RECORD` 无条件追加 ⇒ 不存在「静态记了但运行期没记」的路径）。
  ⇒ ⇒ 四条同时成立 ⇒ ⇒ 运行期历史长度 **恒等于** 静态计数 ⇒ ⇒ 超限与不足**都不可达**。

### 2.2 `EXPECT_SEQ_SELF <port> <n> <lag>`

- **语义**：令 L = 该端口历史序列长度。断言：对 i = 0..n-1，
  `h[L-n+i] == h[L-n-lag+i]`（**最近 n 项** 与 **再往前 lag 项** 的对应项**逐项恒等**）。
- **前置**：L ≥ n + lag（**扫描期**静态判定）。
- **产生 1 条断言**（一个「关系」= 一个判断 ⇒ 一条 check_id）。

### 2.3 `EXPECT_SEQ_STEP <portV> <portE> <n>`

- **语义**：令 LV / LE = 两端口**各自**的历史长度。对 i = 0..n-1，断言：
  ```
  (hV[LV-n+i] - hV[LV-n-1+i]) mod 2^64 == hE[LE-n-1+i]
  ```
  ⚠️ **对齐的精确定义**（**实施期修正 · 差一错位**）：
  - V 侧取的是**最后 n+1 项**的 **n 个相邻差**（⇒ 下标 `LV-n+i` 与 `LV-n-1+i`）
    ⇒ ⇒ ⚠️ **不是** `hV[LV-n+i+1]`（那会**越界**读 `hV[LV]` ⇒ UB）。
  - E 侧取的是**同一窗口的同位项**（⇒ 下标 `LE-n-1+i`）⇒ ⇒ 与 diff 的**较早端**同拍
    ⇒ ⇒ 因为「造成 `V[t] → V[t+1]` 这次跃迁的 E 值」正是**采样在 t 时刻**的那个值。
- ⚠️ **「同位」的精确定义**（oracle 闭合项 2 · 防止作者成为「第三个解释器」）：
  **「同位项」= 两端口历史序列「各自尾部的第 (n+1-i) 项」** ⇒ ⇒ 当 **LV ≠ LE** 时，
  本断言比较的是**两条不同尾部的窗口** ⇒ ⇒ 它**不是**「Δv[k] == en[k]」的同拍关系。
  ⇒ ⇒ **使用前提（规范义务）**：两端口 MUST 在**同一采样点集合**上 `RECORD`
  ⇒ ⇒ 写法范式 = **每拍交错记录**：`TICK 1` → `RECORD v` → `RECORD en`
  ⇒ ⇒ 且两者的 `RECORD` 次数 MUST **相等**（⇒ LV == LE）⇒ ⇒ 才是同拍关系。
  ⇒ ⇒ **LV ≠ LE 时断言仍良构可执行**（不报错）⇒ ⇒ 但它表达的是「各自尾部的错位关系」
  ⇒ ⇒ **作者责任** ⇒ ⇒ 文档 MUST 显式警告（并在迁移 change 的写法指南里给范式）。
- **前置**：LV ≥ n+1 且 LE ≥ **n+1**（**扫描期**静态判定 · **两个端口各判**）。
- **产生 1 条断言**。

> ⚠️ **实施期发现（2026-11-21 · 由判别力夹具抓出）**：本节的初版公式把 V 侧写成
> `hV[LV-n+i+1] - hV[LV-n+i]`、E 侧写成 `hE[LE-n+i]` ⇒ ⇒ **两端一致地错**
> （⇒ ⇒ 正是「两端一致不能当正确性证据」那条铁律的活标本）⇒ ⇒ 已按上面的
> 「对齐的精确定义」修正 ⇒ ⇒ **实现 / 渲染 / 扫描期充足性**三处同批改。

### 2.4 `EXPECT_SEQ_COUNT <port> <value> <n> <count>`

- **语义**：令 L = 该端口历史序列长度。断言：**最近 n 项**中**等于 `<value>`** 的项数 == `<count>`。
- **前置**：L ≥ n（**扫描期**静态判定）。
- **产生 1 条断言**。

⚠️ **算术口径**：历史值一律按 **64 位无符号**存取 ⇒ ⇒ `EXPECT_SEQ_STEP` 的差值按 **mod 2^64 回绕**
⇒ ⇒ 解释器（`std::uint64_t`）与渲染器（`logic [63:0]`）**逐字同语义**。

---

## 3. IR 扩展（`include/rtl_emit/verif.h`）

```cpp
enum class VerifOpKind {
    /* …既有… */,
    Record,        // RECORD <port>
    ExpectSeqSelf, // EXPECT_SEQ_SELF <port> <n> <lag>
    ExpectSeqStep, // EXPECT_SEQ_STEP <portV> <portE> <n>
    ExpectSeqCount // EXPECT_SEQ_COUNT <port> <value> <n> <count>
};

struct VerifOp {
    /* …既有… */,
    std::string   port2;    // ExpectSeqStep 的第二端口
    std::uint32_t seq_n = 0;     // n
    std::uint32_t seq_lag = 0;   // lag（仅 ExpectSeqSelf）
    std::uint64_t seq_value = 0; // value（仅 ExpectSeqCount）
    std::uint32_t seq_count = 0; // count（仅 ExpectSeqCount）
};
```

⚠️ **不复用** `VerifOp::value` / `VerifOp::count` ⇒ ⇒ 理由 = ADR-074 §7.2 的同型坑
（`WAIT` 的 `level` 若复用 `value`，「缺省 0」会被读成 `low` ⇒ 语义翻转）⇒ ⇒
**新语义一律新字段**。

---

## 4. 扫描期静态良构性（fail-closed · **两端同源**）

> ⚠️ **这是本设计最重要的鲁棒性论点**：历史充足性由**扫描器**判定 ⇒ ⇒
> **两个执行后端读同一份 IR ⇒ 不存在「历史不足」的运行期分歧** ⇒ ⇒
> 不会出现「一端 PASS 一端 FAIL」的分叉。

| 检查 | 判定 |
|---|---|
| 参数个数 | `RECORD` = 1 · `EXPECT_SEQ_SELF` = 3 · `EXPECT_SEQ_STEP` = 3 · `EXPECT_SEQ_COUNT` = 4 ⇒ 不符 ⇒ **非零退出** |
| 取值域 | `n ≥ 1` · `lag ≥ 1`（`lag = 0` ⇒ 恒真 ⇒ 禁）· `count ≤ n` ⇒ 不符 ⇒ **非零退出** |
| **32 位上溢** | `n` / `lag` / `count` 的表达式求值 MUST 复用**既有**的 32 位上溢检查形态（`verif_scan.cpp` 的「计数超出 32 位」）⇒ ⇒ 超界 ⇒ **非零退出**（否则 `uint32` 字段静默回绕 ⇒ ⇒ 窗口语义漂移） |
| 历史深度 | 每端口 `RECORD` 累计次数 ≤ `HISTORY_DEPTH`（1024）⇒ 超限 ⇒ **非零退出** |
| 历史充足性 | 到该语句为止，**各端口**的 `RECORD` 累计次数 ≥ 该语句所需（`EXPECT_SEQ_SELF` = 1 个端口 · `EXPECT_SEQ_STEP` = **两个端口各判**（`portV` ≥ n+1 · `portE` ≥ n）· `EXPECT_SEQ_COUNT` = 1 个端口）⇒ 不足 ⇒ **非零退出**（消息点名端口与所需/已有） |
| **端口位宽** | 被 `RECORD` / 被序列断言引用的端口 **width > 64** ⇒ ⇒ **fail-closed**（渲染期已知 width ⇒ 一行判掉；解释器侧从**同一个 manifest** 查 width ⇒ ⇒ 两端同源 ⇒ ⇒ 一致）。⚠️ **理由**：解释器的 `get_value_by_node_id` 只返回 `uint64_t`（**低 64 位**）⇒ ⇒ 宽端口上两端会「大概都截断」⇒ ⇒ 「大概同构」正是 D8 要消灭的 ⇒ ⇒ 与其登记差异，不如**直接拒** |
| 端口解析 | 渲染器已有（`PortTable::resolve` fail-closed）⇒ ⇒ 解释器侧 MUST 同样 fail-closed（`resolve_or_fail` 已有） |

⚠️ **`REPEAT` 展开顺序**：`REPEAT` 的 body 在 `END` 处展开 ⇒ ⇒ 累计计数按**展开后**的顺序进行
⇒ ⇒ 静态充足性判定对 `REPEAT` 内的序列断言同样正确。
⚠️ ⚠️ **已知限制：`RECORD` 与 `EXPECT_SEQ_*` 同处一个 `REPEAT` body ⇒ ⇒ 过拒**（critic NIT）：
静态充足性按**展开前**的 body 计数判定 ⇒ ⇒ 「`REPEAT { RECORD v; EXPECT_SEQ_SELF v n lag }`」
这种**自举**形态会被**误拒**（fail-closed）⇒ ⇒ 方向是**安全**的（宁可拒不可放）
⇒ ⇒ 但它是**限制**不是缺陷 ⇒ ⇒ **登记** ⇒ ⇒ 迁移 change 的写法 MUST 把
「记录」与「比较」**分到不同拍次**（先 `REPEAT` 记录完，再单独一条序列断言）。

⚠️ **静态计数器的生命周期**（oracle 闭合项 1 · **核心承诺的成立前提**）：
计数器**一份**（**不是** per-scenario 局部变量）· **跨 scenario 不清空** ·
按**展开后**的程序顺序累计 · key = **端口标识符字符串**。
⇒ ⇒ **运行期历史长度恒等于静态计数** ⇒ ⇒ 「深度超限」与「历史不足」**都不可达**。
⇒ ⇒ ⚠️ **实施期反模式**（必须避免）：把计数器声明在 per-scenario 循环内
⇒ ⇒ ① 深度计数 per-scenario 重置 ⇒ ⇒ 跨 scenario 累计越界（渲染侧数组越界写 · 解释器 `vector` UB）
⇒ ⇒ ② 充足性 per-scenario 判定 ⇒ ⇒ 误拒「scenario B 引用 scenario A 记录」的合法用法（安全但不可用）。

⚠️ **时序锚点纪律**（`enforce_settle_after_drive`）**扩到 `RECORD`**：
`RECORD` 读的是**当前**端口值 ⇒ ⇒ `SET`/`RESET` 之后紧邻 `RECORD` 与紧邻 `EXPECT` **同型**
⇒ ⇒ MUST 先 `SETTLE`（或 `TICK`/`WAIT`）⇒ ⇒ 扫描器静态强制。
⚠️ **但 `EXPECT_SEQ_*` 不受该纪律约束**：它们只读**历史**（过去的采样点）⇒ ⇒ 与当前结算状态
**无关** ⇒ ⇒ 约束它们会制造无谓的 `SETTLE` 噪声 ⇒ ⇒ **明确不约束**（先例：`OBSERVE` 同样读当前值
且不受约束 —— 但它是纯诊断；`RECORD` 会喂给判断 ⇒ 故 `RECORD` **必须**受约束）。

---

## 5. 两个执行后端（同语义 · 逐条对照）

### 5.1 IR 解释器（`src/rtl_emit/verif_run.cpp`）

- 状态：`std::unordered_map<std::string, std::vector<std::uint64_t>> history;`（key = 端口标识符）。
- `RECORD` ⇒ resolve ⇒ 读当前值 ⇒ `push_back`。
- `EXPECT_SEQ_*` ⇒ 按 §2 的公式计算 ⇒ 1 条 check ⇒ 失败消息**点名首个不符项的索引与两端的值**
  ⇒ ⇒ 诊断可行动（不是「序列不匹配」这种无信息文案）。

### 5.2 Verilog 渲染器（`src/rtl_emit/verif_render.cpp`）

- 为**每个被 `RECORD` 的端口**声明：
  ```verilog
  logic [63:0] hist_<id>[0:1023];
  integer      hist_idx_<id> = 0;
  ```
  （`<id>` = 端口标识符 sanitize 后 + 前缀 `hist_` ⇒ 规避「首字符是数字」的 Verilog 限制。）
  ⚠️ **sanitize MUST 是单射**（oracle 闭合项 8）：两个不同标识符 sanitize 成同一个名字 ⇒ ⇒
  两条历史**共用一个数组** ⇒ ⇒ 静默串数据 ⇒ ⇒ **撞名 ⇒ fail-closed**。
  ⚠️ **该检查落在「扫描期」而非渲染期**（critic NIT）：渲染期报错虽也是 fail-closed（响 ≠ 静默），
  但解释器路径会「先跑成功、再在渲染侧失败」⇒ ⇒ **两端不同步**⇒ ⇒ 改为**扫描期**计算 sanitize
  并检测撞名 ⇒ ⇒ **两端同源拒绝**⇒ ⇒ sanitize 函数 MUST 由**扫描器与渲染器共用**
  （落点 = `src/rtl_emit/verif_internal.h` 内部头 ⇒ ⇒ **不扩公开面**）。
- `RECORD` ⇒ `hist_<id>[hist_idx_<id>] = <port>; hist_idx_<id> = hist_idx_<id> + 1;`
- `EXPECT_SEQ_*` ⇒ 用 `for (wi = 0; wi < n; wi = wi + 1)` 逐项比较 ⇒ 汇总成 1 条 CHECK 行。
  ⚠️ 索引是**变量**（`hist_idx_<id> - 1 - wi`）⇒ ⇒ Verilog 合法。
  ⚠️ **64 位减法回绕**：`logic [63:0]` 的减法天然回绕 ⇒ ⇒ 与解释器同语义。
  ⚠️ **X 态**：渲染侧用 `!==` ⇒ X 判为不等 ⇒ FAIL（fail-closed）；解释器侧无 X
  ⇒ ⇒ **两端对 X 的处理不对称** ⇒ ⇒ **登记**（与既有 `EXPECT` 的 X 差异**同型** ·
  `verif-wait-level` 已登记过同一族差异）。
  ⚠️ **width > 64 的端口**：渲染期已知 width ⇒ ⇒ **fail-closed**（不渲染）⇒ ⇒ 与解释器同决策。

### 5.3 已知差异登记（必须写进文档）

| 差异 | 表现 | 处置 |
|---|---|---|
| X 态（`EXPECT_SEQ_SELF` / `STEP`） | 渲染侧 `!==` ⇒ X ⇒ FAIL；解释器无 X | 登记 · 不修（同既有 `EXPECT`） |
| **X 态（`EXPECT_SEQ_COUNT`）** | 渲染侧用 `===` 计数 ⇒ **X 项不计入** ⇒ ⇒ 计数**偏少** ⇒ ⇒ 渲染侧 FAIL；解释器侧无 X ⇒ ⇒ 若端口出 X 则两端结论可能不同 ⇒ ⇒ **登记为已知差异**（本仓 Verilator 会把 X 优化成确定值 ⇒ 实际不触发） | 登记 · 不修 |
| **复位尾缝（场景首样本）** | 渲染侧 `write_tb_reset_pulse` 末尾多一个 `@(posedge clk);`（**无 `#1`**），解释器 `apply_frame_reset` 只做 `sim.reset()`（**无该沿**）⇒ ⇒ **场景首条 `RECORD`** 继承这条接缝 ⇒ ⇒ 首样本在两端可能差 1 拍。⚠️ 这是 **wait-level change 已实测登记**的既有差异（非本 change 引入） | **继承登记** + **夹具 MUST 沿用 `SET` + `TICK` 前缀范式**（先例 `tests/test_verif_wait_level.cpp` 的 `kPrelude`）⇒ ⇒ 避开而非修复 |
| 失败文案 | 两端文案本就不同（解释器带端口名与索引 · 渲染侧带端口名） | 比对粒度 = **对** `(check_id, PASS/FAIL)`（同 `verif-wait-level`） |

⚠️ **「同物理端口的两个标识符 ⇒ 两条独立历史」**（oracle 闭合项 8）：
历史按**标识符字符串**分桶 ⇒ ⇒ 同一物理端口若用两个不同标识符 `RECORD` ⇒ ⇒
得到**两条互不相干的历史** ⇒ ⇒ 跨桶比较恒不成立 ⇒ ⇒ **文档 MUST 写明**
「一个物理端口 MUST 只用一个标识符 `RECORD`」。

---

## 6. D8 证据义务（两执行后端同结论）

新测试源 `tests/test_verif_history_primitives.cpp`（标签 `verilator_e2e`）⇒ ⇒ 复用
`tests/test_verif_wait_level.cpp` 的装置形态：

- **同一份 `.verif`** ⇒ ① IR 解释器（`run_verif` + `Simulator`）② 渲染 TB 真 verilator 编译运行
  ⇒ ⇒ 逐条 `(check_id, PASS/FAIL)` **对**比对相等。
- ⚠️ **不是**静态 check_id 集合比对（对时序/序列敏感原语**结构性失明**）。
- 每条构例另带**手推期望**（禁抄仿真输出 ⇒ 两端一致**不是**正确性证据）。
- **变异自证**：变异投在**夹具的 `.verif` 输入**上（不改实现代码）⇒ ⇒ 至少一条构例必红。

⚠️ **变异点名清单**（oracle 闭合项 6 · 比 wait-level 先例更严 —— wait-level 只点名了 1 条）：

| # | 变异 | 预期转红 |
|---|---|---|
| M1 | `EXPECT_SEQ_SELF` 的 `lag` 256 → 255 | 自滞后恒等构例 FAIL（窗口错位 ⇒ 不恒等） |
| M2 | `EXPECT_SEQ_COUNT` 的 `count` 由 `duty` → `duty ± 1` | 窗口计数构例 FAIL |
| M3 | `EXPECT_SEQ_STEP` 的两个端口**对调**（`portV` ↔ `portE`）⇒ ⇒ 断言变成「`portE` 的相邻差 == `portV` 的同位项」⇒ ⇒ 语义变了 ⇒ ⇒ 构例 FAIL | 相邻差构例 FAIL |
| M4 | 把 `TICK 1` + `RECORD v` 改成 `SET v <x>` + `RECORD v` | **扫描期**红（`SET` 后紧邻 `RECORD` ⇒ 违反时序锚点纪律）⇒ ⇒ 证明该纪律**有判别力**（不是摆设） |
| M5 | `EXPECT_SEQ_COUNT` 的 `value` 由 `1` → `0` | 窗口计数构例 FAIL（计的是「高」而非「低」） |

⚠️ ⚠️ **已撤回的变异**（critic SHOULD_FIX #1 · **原 M3 无效**）：
「`EXPECT_SEQ_STEP` 的 `n` 由 40 → 39」**恒绿** ⇒ ⇒ 原因是：该原语只产生 **1** 条 check，
且 `n` 减 1 后比较项是原窗口的**严格子集**（`i = 0..38 ⊂ i = 0..39`）⇒ ⇒ 原断言 PASS 则变异后**仍 PASS**
⇒ ⇒ 「运行期条数不等」的预期**不成立**（运行期不存在「条数」维度）⇒ ⇒ **无效变异**
⇒ ⇒ 依仓内「变异纪律」第 ④ 条**如实登记** ⇒ ⇒ **不计入**判别力条数。

⚠️ **无效变异 MUST 如实登记**（仓内「变异纪律」第 ④ 条）⇒ ⇒ 恒绿的变异**如实登记**且**不计入**判别力条数。

---

## 6.1 D5 / ⑥'' 锚点义务对新原语的裁决（oracle 闭合项 5）

新原语携带**推导层字面量**（`lag=256` · `count=duty` · `value=1`）⇒ ⇒ 这些是「期望值」的一部分。

- ⑥''（`verify_examples_rtl_sim.sh`）当前只做 **scenario 粒度**双向 1:1 + `VERIF-CHECKS:` 条数等式
  ⇒ ⇒ **本 change 不改 ⑥''**（它是**护栏**判据，改它属另一个 change 的范围）。
- **裁决**：D5 的「期望值机器层在 DSL · 推导层在 `.md`」义务**自动覆盖**新原语
  ⇒ ⇒ `EXPECT_SEQ_*` 的字面量 MUST 同样在 `.md` 的期望登记里有**推导**
  ⇒ ⇒ **迁移 change（`spinalhdl-ported-verif-migration-2026-11`）MUST 为每条 `EXPECT_SEQ_*`
  在 `.md` 里写出推导**（含 `lag` / `count` / `value` 的由来）⇒ ⇒ 否则「机器层有值、推导层无据」
  ⇒ ⇒ 那正是 D5 要禁的形态。
- ⇒ ⇒ **本 change 的登记义务**：在 `.md` 的期望登记**范式**里加一句「序列类断言的推导 MUST 含
  `lag` / `count` / `value` 三项的由来」⇒ ⇒ 具体登记由迁移 change 落地。

---

## 7. 与 ADR-074 的关系（不变量守恒）

| ADR-074 的条款 | 本 change 的处置 |
|---|---|
| D1「一份 IR → 两个后端」 | **不动** ⇒ ⇒ 新原语照此实现 |
| D2「封闭原语集」 | **扩展**（+4 条）⇒ ⇒ **这是决策变更 ⇒ ⇒ 独立 ADR-076**（§7 头部已自禁「继续追加」⇒ ⇒ MUST 拆独立 ADR） |
| D2「值在生成期全部求值成字面量 ⇒ IR 里没有表达式」 | **逐字保持** ⇒ ⇒ 新原语的参数全是字面量（扫描期求值）⇒ ⇒ **不引入表达式** |
| D2「fail-closed：集合外构造 ⇒ 非零退出」 | **保持并加强** ⇒ ⇒ 新原语的静态良构性检查 = 更多 fail-closed 点 |
| D4「生成物新鲜度 + 条数等式」 | **不动** ⇒ ⇒ 新原语的 `EXPECT_SEQ_*` 各计 1 条 ⇒ ⇒ `verif_check_count` 计入 ⇒ ⇒ 三方等式照常成立 |
| D7「保留手写 TB 逃生口」 | **不动** ⇒ ⇒ 无法忠实表达的判据仍可保留手写 TB（并登记理由） |
| D8「逐原语两后端同结论 + 变异自证」 | **扩展到新原语** ⇒ ⇒ §6 |

---

## 8. 消费者（为什么这 4 条都有人用）

| 新原语 | 消费者（`examples/spinalhdl-ported/` 迁移） |
|---|---|
| `RECORD` | counter_simple · counter · pwm |
| `EXPECT_SEQ_SELF` | counter_simple（「周期 256」） |
| `EXPECT_SEQ_STEP` | counter（「Δ == enable」） |
| `EXPECT_SEQ_COUNT` | pwm（「256 拍内高电平数 == duty」） |

⚠️ **没有消费者的原语不加**（Zero-Debt「No dead code」）⇒ ⇒ 本设计**不含** `EXPECT_SEQ_EQ`
（两端口历史逐项恒等）—— 因为 fifo / uart_rx 的「逐项恒等」其**比较对象是字面量序列**
⇒ ⇒ 绝对判据即可 ⇒ ⇒ 不需要它。

⚠️ **该否决的举证义务**（oracle 闭合项 · 对 `EXPECT_SEQ_EQ` 否决的加固）：
「fifo / uart_rx 的比较对象是字面量序列」是**本设计单方断言的负载判断** ⇒ ⇒
**迁移 change MUST 以 fifo / uart_rx 现有判据的逐条映射表举证** ⇒ ⇒
若映射过程中出现「**port-vs-port** 的历史恒等」型消费者 ⇒ ⇒ **STOP** ⇒ ⇒ 走 ADR-076 的增补条款
⇒ ⇒ **不**在本 change 内偷加原语。
⚠️ **顺带澄清**：字面量序列判据用**既有**文法即可表达（`REPEAT(n){ EXPECT out f(_i) }`）
⇒ ⇒ 即使某处需要序列恒等，**也不必然**需要 `EXPECT_SEQ_EQ` ⇒ ⇒ 加它的门槛更高。

---

## 9. 风险

| # | 风险 | 缓解 |
|---|---|---|
| R1 | **历史数组的 Verilog 内存**：1024 × 64bit × 端口数 ⇒ 每端口 64KB | 可忽略（TB 一次性分配） |
| R2 | **扫描期计数的复杂度**：每条语句 O(端口数) | 端口数 ≤ 数十 ⇒ 可忽略 |
| R3 | **两端语义漂移**（最坏：同构缺陷） | ① 静态充足性判定 ⇒ 消除「历史不足」这一类分叉；② D8 的运行期逐条比对；③ 变异自证 |
| R4 | **`RECORD` 的采样窗歧义**（记的是驱动前还是结算后） | 钉死「沿后结算后」⇒ ⇒ 与 `EXPECT` 同窗 ⇒ ⇒ 两端同源 |
| R5 | 新原语让 `.verif` 复杂度上升 ⇒ ⇒ 作者更易写错 | fail-closed 的静态检查（参数个数/取值域/充足性）⇒ ⇒ 错在**扫描期**而不是**运行期** |
