# Change: bundle-ecosystem-hardening-2026-10 — bundle 生态边角设施的语义契约化

## Why

`bundle-master-slave-io-2026-10`（ADR-042）把 bundle 字段改成**一等节点**、连接改成
**数据流边**，并给 `mem-read-timing` / `dual-backend-test-coverage` 等能力补了护栏。
但那次改动**只覆盖了「单个 bundle、叶子字段、master↔slave 直连」这条主路径**；
bundle 生态的**边角设施**（嵌套 bundle / `flip()` / 工厂函数 / 序列化 / `ch_stream`）
**既无契约、也无断言**，且其中若干处在 ADR-042 之后**语义已变或已失效**。

本 change 是对「`bundle-master-slave-io-2026-10` 之后全仓遗留与遗漏」的一次
系统审计（2026-10-22）的收敛。

> ⚠️ **本提案经 2026-10-22 的双路评审（架构 + 差距分析）修订**：修正了审计的
> **4 处取证错误**（L2 / L3 / L6 / T4）、**新增 1 项缺陷**（L11：prefix 机制是
> 「空壳」）、把 **D1 / D2 / D6 的裁决改写为可落地形态**，并补齐了 spec delta 的
> `MODIFIED` 段（ADDED-only 归档会让主 spec 残留**假陈述**）。
> 修订点逐条标注在下方。

### 审计结论：11 项遗留 / 遗漏（逐项有取证）

| # | 遗留 / 遗漏 | 取证 | 严重度 |
|---|---|---|---|
| **L1** | **`bundle_base(const std::string &prefix)` 在「基类构造期」调用虚函数 `set_name_prefix()`** ⇒ 此时派生类成员（嵌套 bundle 字段）**尚未构造** ⇒ 对未构造对象调虚函数 = **UB / 段错误** | gdb 精确帧：`bundle_traits.h:135` ← `bundle_base.h:34`；`samples/nested_bundle_demo.cpp` **segfault**（基线同样崩 ⇒ 既有缺陷） | 🔴 高 |
| **L2** | `bundle_slice_view` / `bundle_concat`（`bundle_operations.h:14,58`）**无存活测试** ⇒ ADR-042 后它们的字段与原 bundle **断连** ⇒ **静默失效** | ⚠️ **取证更正**：唯一引用点 `tests/test_bundle_advanced_features.cpp:72` 位于**被注释掉的** `/* */` 块内 ⇒ 「零存活测试」成立，但「连测试都没有」不准确。**追加事实**：`bundle_slice_view` 的 `Start`/`Count` 是**死参数**（`bundle_operations.h:21`；`slice_fields` 零调用点，`:39-53`；它是**从未实例化**的成员模板 ⇒ 其 `C > 3` 的 `static_assert` 今天不会触发，**若将来实例化**则编译期拒止）⇒ 它**根本不做切片**，宽度 = 整个原 bundle 宽度 | 🟡 中 |
| **L3** | `deserialize_bits_to_fields()`（`bundle_serialization.h:85-97`）仍用 `bit_select`/`bits<>` **切片重绑定**字段 ⇒ 与「字段一等节点」原则冲突 | ⚠️ **取证更正（原结论错误）**：它**并非**「全仓零使用」——它被 `deserialize<>()` 调用（`bundle_serialization.h:114`），而 `deserialize<>` 有**活跃消费方**：`include/core/bundle/tlm/tlm_bundle_converter.h:54`、`tests/test_bundle_serialization.cpp:47,149`、`tests/test_tlm_bundle_phase1.cpp:80,96`、`samples/tlm_bundle_demo.cpp:121` ⇒ 它是**活的路径** ⇒ 修它是**行为变更**，**不是**死代码清理。⚠️ **追加更正（critic 实测）**：该路径的既有断言**全是宽度断言**（`.width ==` / `bundle_width_v<>`，**零** `impl()` 身份断言）⇒ 它们对「切片重绑定」与「字段一等节点」两种语义**同样通过** ⇒ 既**不需要**反转，也**证明**该路径此前**零行为覆盖** ⇒ MUST **新增**行为级断言 | 🔴 高（原判 🟡 低） |
| **L4** | `bundle_base::operator=`（`bundle_base.h:176-181`）的**「软件拷贝 / 节点重绑定」语义**只被 `tests/test_bundle_connection.cpp` 的**注释**钉住 ⇒ 无 spec / ADR ⇒ 下次「统一连接语义」的重构会误删它 | 断言与注释的对照 | 🟡 中 |
| **L5** | `master()` / `slave()` 工厂（`bundle_utils.h:10-17`）**按值拷贝**穿过 `operator=` ⇒ 副本与原件**共享字段节点**，但方向被重新贴标签 ⇒ 语义无任何契约 | **12** 个调用点（`tests/test_bundle.cpp:169,170`、`tests/test_bundle_advanced.cpp:107,108`、`tests/test_bundle_connection.cpp:402,403,429,430,525,526,579,580`）在用，但无契约。**追加事实**：这些调用点**全部传临时对象**（源字段无节点）⇒ 今天已平凡满足「字段节点独立」；但**总线节点** `node_impl_` 按拷贝构造语义**仍共享**（`bundle_base.h:38-41`，被 `tests/test_bundle_node_management.cpp:164` 钉住）⇒ 契约措辞 MUST 只约束**字段**节点 | 🟡 中 |
| **L6** | **`flip()`（`bundle_base.h:187-194`）硬编码 `as_slave()`**（不记忆原方向）且返回**全新未连接** bundle ⇒ 语义无契约 | ⚠️ **取证更正**：`include/chlib/stream_pipeline.h` **没有** `flip()` 调用（grep 无命中；`:117` 的 "flips" 是注释）⇒ 真正的消费方是 8 处：`samples/axi_lite_demo.cpp:171`、`samples/nested_bundle_demo.cpp:81`、`tests/test_bundle.cpp:90`、`tests/test_bundle_advanced.cpp:119`、`tests/test_bundle_connection.cpp:166,422`、`tests/test_nested_bundle.cpp:102`、`tests/test_axi_lite_bundle.cpp:150`。其中 **2 个**翻转的是**已设定 `master` 角色**的 bundle（`samples/axi_lite_demo.cpp:171`（`:156` 已 `as_master()`，同一对象）、`tests/test_bundle_connection.cpp:166`（`:162` 已 `as_master()`，同一 TEST_CASE）），**6 个**翻转的是**从未设定角色**的 bundle（⚠️ `tests/test_axi_lite_bundle.cpp:150` 属此类：`:139` 的 `as_master()` 在**前一个** TEST_CASE 里、作用于**另一个** `master_axi` 对象；`:149` 的 `master_axi` 是新对象；`tests/test_nested_bundle.cpp:102` 同理） | 🟡 中 |
| **L7** | **`ch_stream` 仍走「重绑定」连接语义**（`include/chlib/stream_operators.h:25-32` 的**自由** `operator<<=` 在重载决议中胜出，内部是字段级 `=`）⇒ 框架内**两套连接语义并存** | ⚠️ **重载决议已实测**（附录 A 的探针 ⇒ 打印 `FREE`）：自由函数两个参数均 Exact Match，成员 `bundle_base::operator<<=` 的隐式对象参数需 derived→base 转换（劣）⇒ 按 [over.match.best] 自由函数胜出 ⇒ 原结论成立。**追加事实（爆炸半径）**：不只 `ch_stream <<= ch_stream` —— `bundle_base::connect_field_based_on_direction_and_source`（`bundle_base.h:144-146`）对 **bundle 类型字段**递归调 `left_field <<= right_field` ⇒ **任何嵌套 ch_stream 字段**（`CustomNested.data_stream`、AXI / stream 子系统）的 bundle 级连接同样落到自由函数上 | 🟡 中 |
| **L8** | `get/set_bundle_value` 在「顶层不用 bundle」之后**接近死 API** ⇒ 去留未决 | ADR-042 Decision 5 | 🟢 低 |
| **L9** | `internal_direction`（`include/core/direction.h:10`）+ `is_internal_v` = **死代码**（全仓零使用） | grep：除定义处外**零引用** | 🟢 低 |
| **L10** | FIFO 的**读数据通路**（`rd_data` 字段，公开 API 布局变更） | 用户已裁决**不在本 change 处理** ⇒ 独立 follow-up | ⛔ 排除 |
| **L11** | 🆕 **prefix 命名机制是「空壳」** —— `set_field_name()`（`bundle_base.h:259-265`）对普通信号字段是 **no-op**（`ch_uint` / `ch_bool` **没有** `set_name`；该方法只存在于 `lnodeimpl.h:104`），而代理节点建名用的是 `std::string(field_info.name)`、**不含 prefix**（`bundle_base.h:348`）⇒ **prefix 机制今天唯一的可观测效果就是 L1 的 UB 递归**，叶子节点名**从来**不带 prefix | 逐行核对 `bundle_base.h:259-265` / `:348` + `ch_uint`/`ch_bool` 无 `set_name` 的 grep 事实 | 🔴 高（**评审新增**） |

### 测试完备性审计：5 项缺口

| # | 缺口 | 证据 |
|---|---|---|
| **T1** | **40 个 sample 零自动化覆盖**（0 个在 ctest、0 个在 `run_all_ported_tests.sh`）⇒ 坏了没人知道 | `samples/*.cpp` = 40；`grep -c 'samples/' tests/CMakeLists.txt` = 1（唯一一处是注释行 ⇒ 0 个被覆盖）；`run_all_ported_tests.sh` = 0 |
| **T2** | `samples/switch_demo.cpp` **既有 segfault**：`make_uint<8>(0)` 在**无 `ctx_swap`** 的情况下创建节点 ⇒ 节点创建失败 ⇒ 崩溃 | 基线同样崩；日志「No active context for literal creation」 |
| **T3** | bundle 能力的**覆盖缺口**：嵌套 bundle 字段 / `flip()` / 工厂函数 / `deserialize` 路径 / `ch_stream` 的连接语义 —— **零断言** | `tests/test_bundle_master_slave.cpp` 的 8 个用例只覆盖叶子字段 + 直连。**追加事实**：`tests/test_stream_operators.cpp` 的 10 个用例**全是结构性断言**（`impl() != nullptr`，**无一次** `tick()`）⇒ 在「重绑定」与「数据流边」两种语义下**都会绿** ⇒ **零信号**，必须补行为级断言 |
| **T4** | ⚠️ **取证收窄**：`ch_stream` **已在**双后端矩阵里（`tests/test_ch_stream.cpp:190,239` 有 **2 个** `DUAL_BACKEND_TEST_PAIR`：`SimulationDataFlow` / `SimulationDataConsumption`）⇒ 真正缺的是**自由 `operator<<=` 连接路径**的 parity | 原表述「`ch_stream` 不在双后端矩阵」**不准确** |
| **T5** | `bundle-master-slave-io` 能力的 6 条 Requirement | ✅ **完备**（23 条 Requirement→测试 映射全覆盖，8 项变异验证全部被抓到） |

⇒ **T1/T2 归独立的 `samples-regression-guard-2026-10`**（覆盖面是 40 个程序，
不只 bundle）；本 change 收敛 **L1-L9 + L11 + T3/T4**。

## What Changes

### D1: prefix 命名 MUST 内生于「字段节点建立」，MUST NOT 在基类构造期触碰派生类成员（修 L1 / L11）

⚠️ **评审修正**：原裁决是「把命名**推迟到 `set_role()`**」。该裁决**只能修 L1、修不了
L11** —— 叶子节点名仍然不带 prefix ⇒ 本 change 的 spec Scenario
「字段的节点名 MUST 带上该 prefix」会**必然红**。D1 拆成两个不同的问题：

- **L1（未定义行为）**：基类构造期对**尚未构造**的派生类成员调虚函数。
- **L11（能力缺失）**：prefix **从来没真正落到过叶子节点名上**。

**裁决（三步，缺一不可）**：

1. **ctor 只存 prefix**：`bundle_base(const std::string &prefix)`（`bundle_base.h:32-35`）
   MUST NOT 调用虚函数 `set_name_prefix()`；MUST 只把 prefix 存进基类成员
   ⇒ **不触碰任何派生类成员** ⇒ 构造期不再有虚调用 ⇒ **L1 的 UB 根除**。
2. **命名内生于建节点**：`create_field_proxy_nodes()`（`bundle_base.h:321-356`）建代理
   节点时 MUST 用 `prefix.empty() ? field_name : prefix + "." + field_name` 命名
   （改 `bundle_base.h:348` 一处）⇒ 「命名与建节点同时完成」**自然成立** ⇒ **L11 修复**。
3. **嵌套 prefix 非虚下推**：`set_role()` MUST 把 prefix **非虚**地传给嵌套 bundle 字段
   （MUST NOT 走 `set_name_prefix()` 虚函数）⇒ 嵌套 bundle 的叶子节点在自己的
   `set_role()` 里拿到 `outer.inner.field` 形式的完整名字。
   ⚠️ **「非虚」的真实理由**（评审更正）：在 `set_role()` 路径上调虚函数本身**是
   安全的**（彼时对象已完全构造）⇒ 问题不是未定义行为，而是**语义不统一** ——
   派生类可以 override `set_name_prefix()`（既有例子：`bundle_concat` 的 override
   只处理自己的两个成员、**不下推** prefix）⇒ 若下推走虚通道，行为将取决于
   「派生类 override 是否恰好调用了基类实现」⇒ MUST 用非虚通道保证
   「所有嵌套 bundle 一律拿到完整层级前缀」。

**幂等**：`create_field_proxy_nodes()` 的既有 guard（`bundle_base.h:341`，字段已有节点
就跳过）⇒ 节点与名字都**只生成一次** ⇒ 「prefix MUST 不被重复拼接」**免费成立**。

⇒ 附带效果：ADR-042 的「字段节点创建 MUST 幂等」约束**保持不变**（命名同样幂等）。

**prefix 生命周期 MUST 明确（否则实现者只能猜）**：

| 决策点 | 裁决 |
|---|---|
| prefix 存哪 | 基类成员 `std::string prefix_`（`bundle_base` 新增） |
| 拷贝构造是否复制 prefix | **复制**（拷贝出的 bundle 与原件同名 ⇒ 否则「拷贝 → `as_master()`」的节点名无 prefix） |
| 公开的 `set_name_prefix()` 事后调用 | **保留为「设置 prefix」语义**（MUST NOT 立即改名 —— 节点已建立）⇒ 事后调用**不生效**于已建节点 ⇒ MUST 在注释中写明，**且** MUST 有对应的 spec Scenario（见 delta 的「事后改 prefix MUST NOT 改名已建立的节点」） |
| 「角色从未设定」的 bundle | 字段**没有节点**（被 `tests/test_bundle_node_management.cpp:169-171` 钉住）⇒ **无节点可命名** ⇒ prefix 在该路径上**不产生可观测效果**（spec Scenario 的 WHEN MUST 限定「角色已设定」） |

### D2: 嵌套 bundle 设施的语义 MUST **逐项裁决**，MUST NOT 留析取（修 L2 / L3）

⚠️ **评审修正**：原裁决写成「实现正确语义 **或** 显式废弃」⇒ 归档进主 spec 后是一条
**不可判定**的 Requirement（verifier 无法判 done），且 delta 的 Scenario 与主体的
fallback 分支**内部矛盾**（选 fallback 则 Scenario 必败）⇒ 本 change 的存在理由
（「把语义钉进 spec」）被自身的析取抵消。

**逐项裁决（每项唯一分支）**：

| 设施 | 裁决 | 理由 |
|---|---|---|
| `bundle_concat` | **物理移除**（含公开工厂 `bundle_cat()`） | 唯一引用点 `tests/test_bundle_advanced_features.cpp:72` 在**被注释掉**的块内；`bundle_cat` 走 `std::move` 拷贝 ⇒ 副本与原件共享字段节点，语义自相矛盾；「实现正确语义」要先回答「副本写回原件吗」这类**没有消费者需要**的问题。⚠️ **只标记 deprecated 不够**：留着它就会让「已废弃但仍计入清单」与新 Requirement 并存矛盾；且工厂是它的唯一入口 ⇒ 不一并移除则调用方一用即触发 deprecation 告警（仓库先例：`deprecate-hdl-logical-operators` 首轮 315 warning 回滚） |
| `bundle_slice_view` | **物理移除**（含公开工厂 `bundle_slice()`） | `Start`/`Count` 是**死参数** ⇒ 它根本不做切片；「修正确语义」= **先实现切片**，属**新增能力**（Non-Goal 4） |
| `deserialize_bits_to_fields()` | **改为字段一等节点**（**保留**，不废弃） | ⚠️ 它**不是死代码**（见 L3）⇒ 有活跃消费方 ⇒ MUST 修。⚠️ 既有断言**全是宽度断言**（零 `impl()` 身份断言）⇒ **不需要**反转，也**不会**因此变红 ⇒ 但这正说明该路径**零行为覆盖** ⇒ MUST **新增**行为级断言（字段节点 ≠ 源位串切片节点 + 值正确） |

⇒ 移除路径 MUST 走 **ADR** 流程（公开 API 变更）+ 同步
`bundle-base-integer-ctor` capability 的**派生类清单（22 → 20）** ⇒ 本 change 的
delta MUST 附带该 capability 的 `## MODIFIED Requirements` 段（否则归档后主 spec
仍列着已移除的类型）。

### D3: `flip()` 的方向语义 MUST 明确，且 MUST 覆盖「未设角色」路径（修 L6）

**裁决**：

- `flip()` MUST 返回**方向取反**的副本（master→slave，slave→master），
  MUST NOT 硬编码 `as_slave()`。
- **未设角色（`role_ == unknown`）的 bundle** MUST 按 **unknown→slave** 处理。
  理由：8 个现存调用点里 **6 个** flip 的是从未设角色的 bundle、**2 个** flip 的是
  已设 `master` 角色的 bundle（后者 master→slave 本来就与现状一致）⇒ unknown→slave
  让这 8 处**全部保持零行为变化**。
- 返回的副本 MUST 是**未连接**的（调用方负责连接）。

### D4: `master()` / `slave()` 工厂的语义 MUST 明确（修 L5）

**裁决**：工厂 MUST 产出**字段节点独立**的 bundle，方向按 master/slave 设定。

- 「独立」的**确切含义**：**字段**节点 MUST NOT 与源 bundle 的对应字段共享节点。
  ⚠️ **总线节点**（`node_impl_`）按 `bundle_base` 拷贝构造语义**仍然共享**
  （`bundle_base.h:38-41`，被 `tests/test_bundle_node_management.cpp:164` 钉住）
  ⇒ 本契约 **MUST NOT** 要求总线节点独立（那会破坏既有断言）。
- 现状下 **12** 个调用点全部传**临时对象**（源字段无节点）⇒ 今天已平凡满足该契约；
  实现者 MUST NOT 因此认为「无需改动」—— 契约是**面向未来**的（调用方可能传入
  已设角色的 bundle）。

### D5: `bundle_base::operator=` 的「软件拷贝」语义 MUST 进 spec（修 L4）

`operator=` 保留为**显式的软件拷贝 / 节点重绑定**语义（ADR-042 的遗留债务 #1），
MUST 在 capability 里写明（当前只有测试注释钉住 ⇒ 下次重构会误删）。

### D6: `ch_stream` 的连接语义 MUST 收敛到数据流边（修 L7 / T4）—— **两阶段**，不得一步到位

⚠️ **评审修正（两处，均影响可执行性）**：

1. **爆炸半径**：自由 `operator<<=` 不只被 `ch_stream <<= ch_stream` 命中 ——
   `bundle_base::connect_field_based_on_direction_and_source`（`bundle_base.h:144-146`）
   对 **bundle 类型字段**递归调 `left_field <<= right_field` ⇒ **任何嵌套 ch_stream 字段**
   （`CustomNested.data_stream`、AXI / stream 子系统）的 bundle 级连接同样落到自由函数上
   ⇒ 收敛 = 改**所有嵌套 stream 的 bundle 连接** ⇒ MUST 全量双后端回归。
2. **现有测试零信号**：`tests/test_stream_operators.cpp` 的 10 个用例**全是结构性断言**
   ⇒ 在两种语义下**都会绿** ⇒ 收敛前 MUST 先有**行为级**断言钉住现状。

**阶段 1（表征测试）**：新增**行为级**断言（驱动源字段 → sink 字段读到值）+ 双后端
parity，钉住**当前**语义（字段级 `=` 重绑定）。

**阶段 2（收敛）**：把自由 `operator<<=` 改为三条数据流边
（`sink.valid <<= source.valid; sink.payload <<= source.payload; source.ready <<= sink.ready;`）
⇒ 与 ADR-042 一致；`ready` 的**反向流动**由「两条边」表达。

### D7: bundle 生态边角 MUST 有可执行断言 + 变异验证（修 T3）

新增/扩展 Catch2 覆盖：**嵌套 bundle 字段**的连接、**`flip()`** 的方向语义（含 unknown
路径）、**工厂函数**的节点独立性、**`deserialize`** 路径、**`ch_stream`** 的连接语义
（**行为级**，非结构性）。

⚠️ 每条断言 MUST **能单独抓住**对应的回归 ⇒ MUST 做**变异验证**（把实现改坏 ⇒ 对应
断言 MUST 变红），并把变异结果写进**验证记录文件**
（`docs/reports/verification-bundle-ecosystem-hardening-2026-10.md`，见 D13；
对齐 `bundle-master-slave-io` 的
6 项变异验证先例）。

### D8: 死代码清理（修 L9）

`internal_direction` + `is_internal_v` 全仓零使用 ⇒ 删除（ZERO-DEBT POLICY：
「No dead code」）。

### D9: `get/set_bundle_value` 的去留 MUST 显式裁决（修 L8）

ADR-042 Decision 5 之后，TB 侧改用普通端口 ⇒ 打包式 API 只对**未连接** bundle 有用。
**裁决**：保留（调试用途）+ 在 spec / 文档中写明其**适用范围**（未连接 bundle）。

### D10: 主 spec 的两条备注 MUST 随本次裁决更新（评审新增）

⚠️ 本 change 的 delta 若只用 `## ADDED Requirements`，归档后主 spec
（`openspec/specs/bundle-master-slave-io/spec.md`）会**残留两条假陈述**：

- `:35-39`（R1 备注）：点名 `deserialize_bits_to_fields()`「要么一并修，要么 MUST 被
  **显式排除**」⇒ D2 已裁决「一并修」⇒ 该条件句成为**悬置**；
- `:75-78`（R2 备注）：点名「`ch_stream` 的自由 `operator<<=` 与
  `deserialize_bits_to_fields()` **仍在用重绑定**」⇒ D2/D6 落地后这句变成**过时事实**，
  与新增的「连接语义 MUST 唯一」Requirement **并置自相矛盾**。

⇒ delta MUST 补 **`## MODIFIED Requirements`** 段，重述这两条 Requirement 的
**全文**（含全部 Scenario），把备注更新为收敛后的状态。

### D11: 测试计数与文档 MUST 同步（评审新增）

新增 `tests/test_bundle_ecosystem.cpp` ⇒ `CTEST_TOTAL` 与 `tests/test_*.cpp` 计数
**各 +1** ⇒ 根 `AGENTS.md` 的 CI 锚点行 MUST 同步 ⇒ `scripts/verify_test_counts.sh`
（CI 2 个 job 都跑）MUST 保持绿。

### D12: 被移除的派生类 MUST 同步 `bundle-base-integer-ctor` 的清单（评审新增）

`bundle-base-integer-ctor` capability 的主 spec（`## Purpose` 段与 Requirement 正文）
都列着「22 个 bundle 派生类」（含 `bundle_slice_view` / `bundle_concat`）⇒ D2 移除
这两个类型后，该清单 MUST 变成 **20 个**。

⚠️ 仓库规则**禁止**直接改 `openspec/specs/` ⇒ 本 change 的 delta MUST 附带
`bundle-base-integer-ctor` 的 **`## MODIFIED Requirements`** 段（完整重述那条
Requirement，把清单改成 20 个）⇒ 否则归档后两个主 spec 会并存互斥。

⚠️ `## Purpose` 段**不是** Requirement，OpenSpec 的 delta **不合并**它 ⇒ archive
之后 MUST 人工订正该 capability 主 spec 的 `## Purpose`（仓库既有先例：
`axi4-full-slave-burst` 的 Purpose 即「archive 后已补写」）。

### D13: 验证记录 MUST 落在**可寻址的文件**上（评审新增）

「实施记录」「变异验证结果」这类 MUST 有**明确路径**，否则 verifier 无法判 done。

**约定**：本次变更的验证记录落在 `docs/reports/verification-<change-name>.md`
（`docs/reports/` 是既有目录）⇒ 变异验证结果、护栏基线清单等 MUST 写进该文件，
并在 tasks 的 verify 里用 `test -f` 断言它存在。
（本 change 的**具体文件名**见上表；上式的 `<change-name>` 只是**约定模板**。）

> ⚠️ **MUST NOT 在 spec delta 里写含 change 名的具体文件名** —— delta 会被**逐字**
> 并入主 spec ⇒ 归档后主 spec 会残留 change 名（仓库既有约定：主 spec 内的 change
> 名引用必须全部改为指向 capability，见 `AGENTS.md`）。spec 里只能用
> `<change-name>` 占位形式。

## Impact

| 项 | 影响 |
|---|---|
| `include/core/bundle/bundle_base.h` | prefix 改为「ctor 只存 + 建节点时命名 + 非虚嵌套下推」（D1）；`flip()` 方向取反 + unknown→slave（D3）；`operator=` 语义注释（D5） |
| `include/core/bundle/bundle_operations.h` | `bundle_concat` / `bundle_slice_view` + 工厂 `bundle_cat` / `bundle_slice` **物理移除**（D2） |
| `include/core/bundle/bundle_serialization.h` | `deserialize_bits_to_fields()` 改字段一等节点（D2） |
| `include/core/bundle/bundle_utils.h` | `master()` / `slave()` 工厂产出字段节点独立的 bundle（D4） |
| `include/chlib/stream_operators.h` | `ch_stream` 的连接语义收敛（D6，两阶段） |
| `include/core/direction.h` | 删 `internal_direction` + `is_internal_v`（D8） |
| `openspec/specs/bundle-master-slave-io/spec.md` | **ADDED**（边角设施的新契约）+ **MODIFIED**（R1 / R2 两条失效备注，D10） |
| `openspec/specs/bundle-base-integer-ctor/spec.md` | **MODIFIED**（派生类清单 22 → 20，D12）⇒ 本 change 的 delta **MUST 附带**该 capability 的 delta 文件 |
| `docs/reports/verification-bundle-ecosystem-hardening-2026-10.md` | **新增**（验证记录：变异验证结果等，D13；路径由 tasks 的 `test -f` 钉住） |
| `docs/adr/ADR-043-bundle-operations-removal.md` | **新增**（D2 的移除决策：公开 API 变更 ⇒ MUST 先有 ADR，仓库先例 ADR-036 / ADR-037） |
| `docs/adr/ADR-012-io-port-dual-api.md` | `internal_direction` 的引用需同步（D8） |
| `docs/adr/ADR-042-bundle-field-first-class-nodes.md` | 「`bundle_concat`/`bundle_slice_view` 记为遗留债务，独立 follow-up」的段落变陈旧 ⇒ MUST 同步（本 change **就是**那个 follow-up） |
| `tests/test_bundle_literal.cpp` | 「all 22 derived classes」注释随移除同步（D2） |
| `tests/test_bundle_ecosystem.cpp`（新增） | D7 的行为级断言 + 变异验证 |
| `tests/test_bundle_serialization.cpp` / `tests/test_tlm_bundle_phase1.cpp` | 既有断言**全是宽度断言** ⇒ **不需要**反转；MUST **新增**行为级断言（字段节点 ≠ 源位串切片节点）（D2 / L3） |
| `tests/test_stream_operators.cpp` | 补行为级断言（D6 阶段 1） |
| `samples/nested_bundle_demo.cpp` | **不再 segfault**（D1 的直接效果） |
| 计数 | `CTEST_TOTAL` 182 → **183**；`tests/test_*.cpp` 112 → **113**（D11） |
| 风险 | ⚠️ **中偏高**。① D1 改的是**所有 bundle 的命名时机**⇒ MUST 全量回归；② D2 的**移除**路线影响**公开 API**（`bundle_concat` / `bundle_slice_view` + 工厂 + `bundle-base-integer-ctor` capability 的派生类清单 22 → 20）⇒ MUST 走 ADR；③ D2 的**修复**路线改的是**活的路径**（`deserialize<>` 的 4 处消费方）⇒ 既有断言**不需要**反转（全是宽度断言）⇒ MUST **新增**行为级断言；④ D6 的爆炸半径覆盖**所有嵌套 stream 的 bundle 连接**⇒ MUST 跑**全量**双后端矩阵；⑤ D7/D8/D11 是纯增量 |

## Non-Goals

1. **FIFO 的读数据通路**（L10）—— 用户已裁决不在本 change 处理 ⇒ 独立 follow-up
2. **不**改 `bundle-master-slave-io` 已确立的**主路径**契约（字段一等节点 / 数据流边 /
   方向感知 connect）—— 本 change 只补**边角**
3. **不**把 samples 纳入 CI（T1/T2）—— 归独立的 `samples-regression-guard-2026-10`
4. **不**引入 bundle 的新能力（协议层 bundle / 打包 API 的新形态）—— ⚠️ 含
   「让 `bundle_slice_view` 真的做切片」（它今天**根本不做**切片 ⇒ 那属新能力）
5. **不**重写 `bundle_base` 的整体连接模型 —— 只改**边角**设施与 prefix 命名路径

## 落地顺序（与 `samples-regression-guard-2026-10` 的耦合）

⚠️ 两个 change **必须串行**（它们都会改根 `AGENTS.md`，且 B 的护栏依赖 A 的
`nested_bundle_demo` 修复）：

1. **先** `samples-regression-guard-2026-10` 落地（护栏带 `nested_bundle_demo` 的
   **显式豁免**，失效条件 = 本 change archive）
2. **再** 本 change 落地 ⇒ archive 时 MUST **移除该豁免**并全量重跑护栏
   （Task 7 有对应 checkbox）

## 附录 A：审计的复现命令

```bash
# L1（精确根因）
g++ -std=c++23 -g -O0 -Iinclude ... samples/nested_bundle_demo.cpp build/libcpphdl.a \
    -L/usr/lib/llvm-18/lib -lLLVM-18 -o /tmp/nbd
gdb -batch -ex run -ex "bt 15" --args /tmp/nbd
#   ⇒ #0 bundle_field_traits<ch_stream>::set_name at bundle_traits.h:135
#      #8 bundle_base::bundle_base at bundle_base.h:34   ← 基类构造期调虚函数

# L7（重载决议 —— 已实测，非推理）
cat > /tmp/opencode/overload_probe.cpp <<'EOF'
#include <cstdio>
template <typename D> struct bundle_base {
    D &operator<<=(D &src){ std::puts("MEMBER"); return src; }
};
namespace ch {
template <typename T> struct ch_stream : bundle_base<ch_stream<T>> {};
template <typename T> ch_stream<T> &operator<<=(ch_stream<T> &s, ch_stream<T> &r){
    std::puts("FREE"); return s;
}}
int main(){ ch::ch_stream<int> a,b; a <<= b; }
EOF
g++ -std=c++23 /tmp/opencode/overload_probe.cpp -o /tmp/opencode/op && /tmp/opencode/op
#   ⇒ FREE   ← 自由函数胜出（成员隐式对象参数需 derived→base 转换，劣于 Exact Match）

# L11（prefix 机制是空壳）
sed -n '259,265p' include/core/bundle/bundle_base.h   # set_field_name 对普通信号字段 no-op
sed -n '348p'     include/core/bundle/bundle_base.h   # 建名用 field_info.name，不含 prefix
grep -rn "set_name" include/core/uint.h include/core/bool.h   # ⇒ 0 命中（无 set_name）

# L2 / L3 / L9（零使用 / 死代码）
grep -rn "bundle_slice_view\|bundle_concat" --include=*.cpp --include=*.h . | grep -v third_party
grep -rn "deserialize_bits_to_fields\|deserialize<" --include=*.cpp --include=*.h . | grep -v third_party
grep -rn "internal_direction\|is_internal_v" --include=*.cpp --include=*.h . | grep -v third_party

# L6（flip 的真实消费方 —— 不含 stream_pipeline.h）
grep -rn "flip()" --include=*.h --include=*.cpp . | grep -v third_party

# T1（samples 零覆盖）
ls samples/*.cpp | wc -l                        # 40
grep -c 'samples/' tests/CMakeLists.txt         # 1（唯一一处是注释行）
grep -c 'samples/' run_all_ported_tests.sh      # 0

# T2
./build/samples/switch_demo                     # segfault（基线同样）
#   日志：「No active context for literal creation」⇒ 缺 ctx_swap
```
