# Change: disabled-samples-remediation-2026-10 — 修复 14 个**禁用** sample 的编译

- Status: **草案（尚未实施）**
- 立项自 2026-10-22 的 `samples-regression-guard-2026-10`（其 Non-Goal 5）
- 关联 capability：`sample-regression-guard`（本次**新增**一条 Requirement 到该
  capability —— delta 已起草于 `specs/sample-regression-guard/spec.md`：
  「禁用清单 MUST 可收缩，被修复的 sample MUST 移出清单」）

## Why

`samples-regression-guard-2026-10` 把 14 个**从未被编译**的 sample 登记进了
`samples/disabled_samples.txt`（带根因 + 归位条件），让它们的腐坏从
**「静默」**变成**「可见」**。

但登记 ≠ 修复。这 14 个 sample 是仓库里**唯一的「活文档」**之一（samples 的定位
是「living documentation」，见 `samples/AGENTS.md` 的 Phase Gates），它们**今天
一个都编译不过** ⇒ 它们的文档价值为 0。

按根因分组（实测，`g++ -std=c++23 -fsyntax-only`）：

| 根因 | sample | **全部 error 数** | 修复难度 |
|---|---|---|---|
| **字面量后缀**：十六进制字面量带 `_d` 后缀不被 `lit_dec` 接受 | `stream_arbiter_example` / `stream_demux_example` / `stream_flow_basic_example` / `stream_fork_example` / `stream_join_example`（5 个） | 12 / 5 / 20 / 5 / 5 | 🟢 低（改后缀，见下方「⚠️ 修法不是去后缀」） |
| **POD bundle 分区 API**：`part1` / `part2` 不再是类型名 | `advanced_pod_bundle_serialization` / `large_pod_bundle_example`（2 个） | 19 / 14 | 🟡 中 |
| **context 访问器改名**：`get_default_clk` → `get_default_clock`、`get_default_rst` → `get_default_reset` | `stream_handshake_example`（1 个） | 15 | 🟢 低 |
| **`stream_fifo` 构造签名漂移** | `stream_fifo_example` / `stream_component_example`（2 个） | 14 / 4 | 🟡 中 |
| **头文件 / 命名空间漂移**：`chlib/fragment.h` 不存在；`chlib` 不是命名空间名 | `spinalhdl_stream_example` / `conditional_demo`（2 个） | 1 / 11 | 🟡 中 |
| **组件 API 漂移**：基类名不认识 + `ch_sint` 未声明 | `spinalhdl_component_function_area_example`（1 个） | **98** | 🔴 **高**（见下） |
| **if/else DSL 签名漂移**：字面量宽度不匹配 + `ch_in<ch_bool>` 不能绑到 `const ch_bool&` | `if_demo`（1 个） | 7 | 🔴 高（DSL 语义） |

⇒ 合计 **14 个**，**全部 error 数 229**（不是 14 个「一个问题」）。

> ⚠️ **2026-10-22 实测更正（工作量此前被系统性低估）**：
> 提案原有的根因表是按 **首个 error**（`grep -m1 "error:"`）统计的 ⇒
> **单个 sample 的真实错误面被掩盖**。实测**全部** error 数后：
> - `spinalhdl_component_function_area_example` 独占 **98** 个（占全组 **43%**）
>   ⇒ 它**不是**「🟡 中的 API 改名」，而是**接近重写**（基类名不认识 ⇒ 整个
>   组件骨架要对齐当前 API）⇒ 难度 MUST 上修为 🔴 高。
> - 组 A 的 5 个 sample 也不是「5 个独立的小修」：`stream_flow_basic_example`
>   有 **20** 个 error、`stream_arbiter_example` 有 **12** 个。
> ⇒ **实施顺序 MUST 按 error 数与「是否已有同源可用版本」排，不 MUST 按
> 「首个错误的表面相似度」排**（见 D1 的分组重排）。

> ✅ **2026-10-22 复测**（在 `bundle-ecosystem-hardening-2026-10` 落地**之后**）：
> **14 个的根因全部未变**（逐个重跑 `g++ -fsyntax-only`，首个 error 与上表一致）
> ⇒ 本表**仍然准确**，可照此排期。
> ⇒ 特别地：本次落地改动的 `ch_stream` 连接语义 / prefix 机制 / 工厂语义
> **没有**影响这 14 个中的任何一个（它们的根因分别是字面量后缀、头文件缺失、
> 访问器改名、构造签名、POD 分区 API、组件 API、DSL 语义 —— 与本次改动**正交**）。

> ℹ️ `conditional_demo` / `if_demo` / `stream_*_example` 另有**第二重**问题：
> 它们的**运行时**行为从未被验证过（编译通过 ≠ 行为正确）⇒ 修完编译后
> MUST 由 `scripts/run_all_samples.sh` 接管其**运行**回归（护栏已就位 ✓）。
> ⚠️ 且存在**第三态**（见 D3）：编译过了但**运行失败**（例：缺 `ctx_swap` ⇒
> 建节点返回 nullptr ⇒ 运行期 segfault —— `samples/AGENTS.md` 记录的
> `switch_demo` 同型）⇒ 那是**内容改动**，与 Non-Goal 2 冲突 ⇒ MUST 有处置路径。

> ⚠️ **组 A 的修法不是「去掉 `_d` 后缀」**（原提案写错）：
> `lit_dec::is_escape` 只认 `'\''`（`include/lnode/literal_ext.h:71-79`）⇒
> `0xAB_d` 触发 `static_assert("invalid character in literal")`
> （`literal_ext.h:91-92`）。正确的修法是 **`0xAB_d` → `0xAB_h`**：
> `lit_hex::is_escape` 认 `'x'`（`literal_ext.h:56`）⇒ `0xAB_h` 解析为
> value=171、**width=8**（实测），**保持 `ch_literal_impl` 类型与显式宽度**。
> ⚠️ 而改成**裸** `0xAB` 会退化成 `int`（宽度 32，由操作数推导）⇒
> sample 的信号宽度与打印输出可能随之改变 ⇒ **又是「绿 ≠ 对」**。
> ⚠️ 另注：组 A 里的 `1_d` / `0_d`（`stream_demux_example:23,45`）是
> **十进制**字面量 ⇒ `_d` 后缀**本来就正确** ⇒ **MUST NOT** 一并改掉。

> ⚠️ **组 A 有一条更便宜的路**：这 5 个 sample 与**已存在且 PASS** 的
> `examples/stream/*_demo.cpp` **近乎同源**（`stream_arbiter_example` 72 行 vs
> `stream_arbiter_demo` 54 行；`stream_fork_example` 60 vs 49；
> `stream_demux_example` 59 vs 52）⇒ **优先「按 demo 改造」**而不是逐个调 API。

## What Changes

### D1: 按「根因分组」推进，而不是逐个 sample 推进

同一根因的多个 sample **共享同一次修复** ⇒ 分组推进的边际成本远低于逐个推进。
**建议分组顺序**（按「error 数 × 是否有同源可用版本」排，而非按表面相似度）：

1. **组 A（5 个）**：十六进制字面量后缀 —— `0xNN_d` → `0xNN_h`
   （⚠️ **不是**去掉后缀；⚠️ `1_d` / `0_d` 是十进制、**不动**）
   ⇒ **优先「按 `examples/stream/*_demo.cpp` 改造」**（近乎同源，成本最低）
2. **组 B（1 个）**：context 访问器改名（15 error）
3. **组 C（2 个）**：`stream_fifo` 构造签名（14 + 4 error）
4. **组 D（2 个）**：头文件 / 命名空间漂移（1 + 11 error）
5. **组 E（2 个）**：POD bundle 分区 API（19 + 14 error）
6. **组 F（1 个）**：组件 API 漂移（**98 error**）—— ⚠️ **接近重写**
   ⇒ **建议独立立项**（见 D3）
7. **组 G（1 个）**：`if_demo` 的 DSL 语义（7 error，但语义最难）

⚠️ **「删除」也是合法归宿**：C1（注册一致性）只要求每个 `samples/*.cpp`
**要么已注册、要么登记在禁用清单** ⇒ 若某个 sample 的演示内容已被更好的
版本取代（组 A 的 5 个与 `examples/stream/*_demo.cpp` 近乎同源 ⇒ **可能
就是重复**）⇒ **删除该源文件**也是满足 C1 的合法结局，且优于「把一个重复的
演示修回来」。⇒ 每个 sample MUST 显式回答「修 / 删 / 留」三者之一。

### D2: 每个 sample 修完 MUST **立即**登记进 `CH_SAMPLE_TARGETS` + 从禁用清单移除

⚠️ **MUST NOT** 「一次全修完再统一登记」—— 那会让中途的状态无法被护栏看见。
⇒ 每修好一个，就：① 补进 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`；
② 从 `samples/disabled_samples.txt` 移除该行；③ 跑一遍护栏；
④ ⚠️ **同步两处嵌了计数的头注释**（见 Impact 表 —— 它们每移出一行就漂移）。

### D3: 修不动的 MUST **留在**禁用清单里，并把「归位条件」写得更具体

禁用清单里的 sample 有**三种**归宿，MUST 逐个定性（不得留二义）：

| 状态 | 处置 |
|---|---|
| **编不过，且能改 sample 源码修好** | 修 → 登记（走 D2） |
| **编不过，且需要**框架层**的新能力** | **留在**清单；归位条件改成「依赖 <具体框架变更>」（原提案的裁决） |
| **编不过，且需要**重写 sample 本身**（而非框架能力）** | ⚠️ **新增**：与 Non-Goal 2「不改 sample 的内容」**死结** ⇒ MUST 二选一：① 把该 sample 移出本 change **独立立项**（推荐，例：组 F 的 98 error）；② 把「允许重写该 sample」写进本 change 的 Non-Goal 例外并显式记录 |
| **编过了，但运行失败** | ⚠️ **新增第三态**：归位条件改成「运行期缺陷 #N」并**留在清单**（MUST NOT 移出清单 —— 它一旦移出，护栏就会因它运行失败而红）⇒ 原提案的 D2/D3 只覆盖「编不过」与「修不动」，漏了这一态 |

⇒ **本 change 的目标清单长度**：14 → **≤ 2**（预期剩下：组 F 的 98-error 那
个 + 组 G 的 `if_demo`）⇒ 「最多 38」的旧说法是错的（见 Impact 表）。

### D4: 禁用清单的**格式**保持不变（每行 `名字 <TAB> 理由 <TAB> 归位条件`）

护栏 `scripts/run_all_samples.sh` 的 C1 解析它 ⇒ 格式 MUST NOT 变。

## Impact

| 项 | 影响 |
|---|---|
| `samples/*.cpp`（最多 14 个） | 逐个修 API 漂移（⚠️ 全部 error 数 229 ⇒ 工作量远大于「首个 error」给人的印象） |
| `samples/CMakeLists.txt` | `CH_SAMPLE_TARGETS` **26 → 最多 40**（= 40 源文件 − 0 剩余禁用）；⚠️ 原提案写的「24 → 38」**两个数字都错**：当前实测是 **26**（`samples/CMakeLists.txt:7-33`），而「最多」是 40（不是 38） |
| `samples/CMakeLists.txt:6` | ⚠️ 头注释里嵌了计数（「未注册的 sample 见 samples/disabled_samples.txt（**14 个**，2026-10-22 实测）」）⇒ 每移出一行就漂移 ⇒ **MUST 同步** |
| `samples/disabled_samples.txt` | 14 → 最少 0 行（预期 ≤ 2）；⚠️ `:15` 的判定基准注释同样嵌了计数（「已注册 26 / 源文件 40 / 本清单 **14**」）⇒ **MUST 同步** |
| `samples/disabled_samples.txt`（格式） | 格式**不变**（`名字 TAB 理由 TAB 归位条件`）—— 护栏 C1 解析它 ⇒ 变格式即破护栏 |
| 计数 | **CTEST_TOTAL 不变**（samples 不是 ctest target） |
| 风险 | 🟡 中。① 每修一个 sample 就多一个**运行**回归对象 ⇒ 可能暴露**运行期**失败（编译过了但行为错）⇒ 护栏会报出来（这是**好事**，但要有处置流程 —— 见 D3 第三态）；② 组 F（**98 error**，占 43%）接近重写 ⇒ 建议独立立项；③ 组 G（`if_demo`）可能需要框架变更 ⇒ 触发 D3；④ ⚠️ **「删」是合法归宿** ⇒ 删源文件会改变 `samples/*.cpp` 的**总数**（40 → 39）⇒ 那些嵌了「40」的注释（`samples/CMakeLists.txt:6`、`samples/disabled_samples.txt:15`、根 `AGENTS.md`）MUST 一并同步 |

## Non-Goals

1. **不**为 sample 加**行为断言**（sample 的职责是「演示 + 自检」；行为断言属于
   `tests/`）—— ⚠️ 但「自检」MUST 真的**自检**（退出码 0 ≠ 演示正确 ⇒ 这是
   仓库自己的「绿 ≠ 对」教训，**不得**用「没崩」冒充「自检通过」）
2. **不**改 sample 的**内容**（只修 API 漂移；sample 演示什么不由本 change 决定）
   —— ⚠️ **例外**：D3 允许对「需要重写 sample 本身」的那 1 个独立立项
3. **不**引入框架层新能力（若某 sample 需要 ⇒ 留在禁用清单，见 D3）
4. **不**处理 examples/ 的同类问题（那是 `examples-runtime-coverage-2026-10`）

## 落地顺序

MUST 在 `samples-regression-guard-2026-10` **之后**落地（护栏是它的前提：
没有护栏，修好的 sample 照样会静默腐坏）。

⚠️ **与另两个 change 的关系**：
- ⚠️ **MUST 在 `examples-runtime-coverage-2026-10` 之后**（两者都写
  `ci.yml` + 根 `AGENTS.md` ⇒ 并行合并必冲突 ⇒ 串行合并）；
- ⚠️ **建议在 `fifo-read-data-path-2026-10` 之前**：那个会改
  `samples/fifo_bundle.cpp` / `minimal_fifo_bundle.cpp` 的运行行为 ⇒
  若本 change 采纳了「删源文件」的归宿（`samples/*.cpp` 总数会变）⇒
  两个 change 同时动 `samples/CMakeLists.txt` ⇒ 归因困难 ⇒ 串行更安全。

⇒ **推荐总序**：① examples-runtime-coverage → ② disabled-samples-remediation
→ ③ fifo-read-data-path（三者可并行**开发**，但 MUST 串行**合并**）。
