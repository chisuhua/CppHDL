# Change: disabled-samples-remediation-2026-10 — 修复 14 个**禁用** sample 的编译

- Status: **草案（尚未实施）**
- 立项自 2026-10-22 的 `samples-regression-guard-2026-10`（其 Non-Goal 5）
- 关联 capability：`sample-regression-guard`（本次**不改**该 capability 的契约）

## Why

`samples-regression-guard-2026-10` 把 14 个**从未被编译**的 sample 登记进了
`samples/disabled_samples.txt`（带根因 + 归位条件），让它们的腐坏从
**「静默」**变成**「可见」**。

但登记 ≠ 修复。这 14 个 sample 是仓库里**唯一的「活文档」**之一（samples 的定位
是「living documentation」，见 `samples/AGENTS.md` 的 Phase Gates），它们**今天
一个都编译不过** ⇒ 它们的文档价值为 0。

按根因分组（实测，`g++ -std=c++23 -fsyntax-only`）：

| 根因 | sample | 修复难度 |
|---|---|---|
| **字面量后缀**：`0xAB_d`（十六进制 + `_d` 后缀）不被 `lit_dec` 接受 | `stream_arbiter_example` / `stream_demux_example` / `stream_flow_basic_example` / `stream_fork_example` / `stream_join_example`（5 个） | 🟢 低（去掉后缀即可） |
| **POD bundle 分区 API**：`part1` / `part2` 不再是类型名 | `advanced_pod_bundle_serialization` / `large_pod_bundle_example`（2 个） | 🟡 中 |
| **context 访问器改名**：`get_default_clk` → `get_default_clock`、`get_default_rst` → `get_default_reset` | `stream_handshake_example`（1 个） | 🟢 低 |
| **`stream_fifo` 构造签名漂移** | `stream_fifo_example` / `stream_component_example`（2 个） | 🟡 中 |
| **头文件 / 命名空间漂移**：`chlib/fragment.h` 不存在；`chlib` 不是命名空间名 | `spinalhdl_stream_example` / `conditional_demo`（2 个） | 🟡 中 |
| **组件 API 漂移**：基类名不认识 + `ch_sint` 未声明 | `spinalhdl_component_function_area_example`（1 个） | 🟡 中 |
| **if/else DSL 签名漂移**：字面量宽度不匹配 + `ch_in<ch_bool>` 不能绑到 `const ch_bool&` | `if_demo`（1 个） | 🔴 高（DSL 语义） |

⇒ 合计 **14 个**：**7 个低/中难度**（API 改名 / 字面量后缀）+ **7 个中/高难度**。

> ℹ️ `conditional_demo` / `if_demo` / `stream_*_example` 另有**第二重**问题：
> 它们的**运行时**行为从未被验证过（编译通过 ≠ 行为正确）⇒ 修完编译后
> MUST 由 `scripts/run_all_samples.sh` 接管其**运行**回归（护栏已就位 ✓）。

## What Changes

### D1: 按「根因分组」推进，而不是逐个 sample 推进

同一根因的多个 sample **共享同一次修复** ⇒ 分组推进的边际成本远低于逐个推进。
**建议分组顺序**（低 → 高）：

1. **组 A（5 个）**：字面量后缀 —— 去掉 `0xAB_d` 的 `_d` 后缀
2. **组 B（1 个）**：context 访问器改名
3. **组 C（2 个）**：`stream_fifo` 构造签名
4. **组 D（2 个）**：头文件 / 命名空间漂移
5. **组 E（2 个）**：POD bundle 分区 API
6. **组 F（1 个）**：组件 API 漂移
7. **组 G（1 个）**：`if_demo` 的 DSL 语义（最难，可能需要重新设计该 sample）

### D2: 每个 sample 修完 MUST **立即**登记进 `CH_SAMPLE_TARGETS` + 从禁用清单移除

⚠️ **MUST NOT** 「一次全修完再统一登记」—— 那会让中途的状态无法被护栏看见。
⇒ 每修好一个，就：① 补进 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS`；
② 从 `samples/disabled_samples.txt` 移除该行；③ 跑一遍护栏。

### D3: 修不动的 MUST **留在**禁用清单里，并把「归位条件」写得更具体

若某个 sample 需要**框架层**的新能力才能编译（例：`if_demo` 可能需要
`ch_in<ch_bool>` 能绑到 `const ch_bool&` 的新 API）⇒ 那属**框架**变更，
不在本 change 范围 ⇒ 留在禁用清单，并把归位条件改成「依赖 <具体框架变更>」。

### D4: 禁用清单的**格式**保持不变（每行 `名字 <TAB> 理由 <TAB> 归位条件`）

护栏 `scripts/run_all_samples.sh` 的 C1 解析它 ⇒ 格式 MUST NOT 变。

## Impact

| 项 | 影响 |
|---|---|
| `samples/*.cpp`（最多 14 个） | 逐个修 API 漂移 |
| `samples/CMakeLists.txt` | `CH_SAMPLE_TARGETS` 24 → 最多 **38**（= 40 − 2 剩余禁用） |
| `samples/disabled_samples.txt` | 14 → 最少 0 行 |
| 计数 | **CTEST_TOTAL 不变**（samples 不是 ctest target） |
| 风险 | 🟡 中。① 每修一个 sample 就多一个**运行**回归对象 ⇒ 可能暴露**运行期**失败（编译过了但行为错）⇒ 护栏会报出来（这是**好事**，但要有处置流程）；② 组 G（`if_demo`）可能需要框架变更 ⇒ 触发 D3 |

## Non-Goals

1. **不**为 sample 加**行为断言**（sample 的职责是「演示 + 自检」；行为断言属于
   `tests/`）
2. **不**改 sample 的**内容**（只修 API 漂移；sample 演示什么不由本 change 决定）
3. **不**引入框架层新能力（若某 sample 需要 ⇒ 留在禁用清单，见 D3）
4. **不**处理 examples/ 的同类问题（那是 `examples-runtime-coverage-2026-10`）

## 落地顺序

MUST 在 `samples-regression-guard-2026-10` **之后**落地（护栏是它的前提：
没有护栏，修好的 sample 照样会静默腐坏）。
