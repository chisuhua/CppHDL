---
# ⚠️ 本文件是**活账**（★ 每次新增公开头 MUST 同步登记）⇒ ⇒ ⇒ 护栏
#    `scripts/verify_hierarchical_p8.sh` 的 **R4②** 对它做**双向核对**。
evidence: probe:scripts/verify_hierarchical_p8.sh
---

# 公共 API 面活账（`scripts/verify_hierarchical_p8.sh` 的 R4②）

> **权威**：本文件是「**哪些头文件构成公开 API 面**」的**唯一登记处** ⇒ ⇒
> 护栏 `scripts/verify_hierarchical_p8.sh` 的 **R4②** 对它做**双向核对**：
> ① 登记了**不存在**的头 ⇒ **红** · ② **存在未登记**的公开头 ⇒ **红**。

## 为什么需要这个文件

⚠️ **P8 之前**，「公开 API 面」只有一条判据 —— `verify_hierarchical_p8.sh` 的 R4①：
「**T3-005 那一段**的公开面 ⊆ 已裁决的 4 个公共头」⇒ ⇒ ⚠️ 但它的 base/head
**两端都钉死**在 T3-005 的两个 commit ⇒ ⇒ ⇒ **P8 新增的公共头不在窗口内**
⇒ ⇒ 「白名单 4 → 5 扩容」是**空操作**（判别力零变化）⇒ ⇒ ⇒ 需要一条**活的**判据。

⚠️ **P8 之后**，本文件成为那条活判据的**数据源** ⇒ ⇒ ⇒ **新增顶层公共头 MUST 登记**。

## 扫描范围

护栏扫的范围 = **顶层 `include/*.h`** + 这两个已裁决的 core 头：

| 头 | 为什么在扫描范围内 |
|---|---|
| `include/core/verilator_backend.h` | T3-005 已裁决的公共头（公开类型 `VerilatorResult`） |
| `include/core/verilator_port_set.h` | T3-005 已裁决的公共头（公开类型 `VerilatorPortSpec`） |

⚠️ **`include/` 的子目录头**（`core/` `chlib/` `ast/` … 的其余部分）**不在**扫描范围内
⇒ ⇒ 理由：它们是**实现细节级**的公开头（用户通常经由顶层头间接到达）⇒ ⇒ 登记它们
⇒ ⇒ 会让活账变成 167 行的清单 ⇒ ⇒ ⇒ **失去可审性** ⇒ ⇒ ⇒ ⇒ **顶层头**才是
「用户会直接 `#include` 的入口」⇒ ⇒ ⇒ 登记门槛落在**真正会扩张承诺**的地方。

## 登记清单

| 公共头 | 说明 | 登记日期 |
|---|---|---|
| `include/ch.hpp` | 核心聚合入口 | 2026-11-07 |
| `include/chlib.h` | chlib 组件聚合入口 | 2026-11-07 |
| `include/codegen_dag.h` | DAG codegen 入口 | 2026-11-07 |
| `include/codegen_naming.h` | 命名表入口（**两侧共享**同一份实现） | 2026-11-07 |
| `include/codegen_verilog.h` | 扁平 Verilog 发射入口（T3-005 已裁决） | 2026-11-07 |
| `include/component.h` | 组件基类 + `Granularity` | 2026-11-07 |
| `include/device.h` | 顶层设备封装 | 2026-11-07 |
| `include/hierarchy_analysis.h` | 层次化分析 + **诊断 API**（P6） | 2026-11-07 |
| `include/hierarchy_equivalence.h` | **等价性 helper**（P8 · 本 change 新增） | 2026-11-07 |
| `include/hierarchy_verilog.h` | 层次化 Verilog 发射入口（T3-005 已裁决） | 2026-11-07 |
| `include/module.h` | 模块生命周期入口 | 2026-11-07 |
| `include/simulator.h` | 仿真器入口 | 2026-11-07 |
| `include/core/verilator_backend.h` | Verilator backend（T3-005 已裁决） | 2026-11-07 |
| `include/core/verilator_port_set.h` | Verilator 端口集（T3-005 已裁决） | 2026-11-07 |

## ⚠️ **已移除 / 已改名的公开符号**（**跨仓迁移指引** · 2026-11-09 起登记）

> **为什么需要这一节**（issue #28 的根因）：「零消费者 ⇒ 直接删」这条判决规则的
> **证据源只有仓内 `grep`** ⇒ ⇒ 对**跨仓下游**结构性失明 ⇒ ⇒ 下游**编译失败**
> 才第一次知道某个公开符号没了（真实发生：下游 ChipForge，2026-11-08）。
> ⇒ ⇒ 本节把「**哪些公开符号在什么时候没了 / 改名成什么了**」变成**可查的活账** ⇒ ⇒
> ⇒ 与 `docs/CHANGELOG.md` 的「用户可见变更」条目**互补**（本节 = **符号级** ·
> CHANGELOG = **变更级**）。

| 符号 / 头 | 移除或改名 | 日期 | 依据 | 迁移指引 |
|---|---|---|---|---|
| `chlib::ch_state_machine::is_built()` | **移除** | 2026-10-30 | ADR-050 D2（`c7b0b28`） | 改断言**可观测产物**：`entry_state()` + 状态寄存器节点已发射 |
| `chlib::ch_state_machine::on_exit` / `exit_action` | **移除** | 2026-10-30 | ADR-050 D2 | 改用 `on_active` 闭包 + `transition_when` |
| `chlib::ch_state_machine::on_entry` / `entry_action` | **移除** | 2026-10-30 | ADR-050 D2 | 运行时 entry 语义**未实现** ⇒ ⇒ 移出状态动作（独立立项） |
| `chlib::ch_state_machine::then()` / `next_state_on_exit()` / `has_exit_transition()` | **移除** | 2026-10-30 | ADR-050 D2 | 改用 `transition_when(cond, target)` |
| `chlib::ch_state_machine::state_changed` | **移除** | 2026-10-30 | ADR-050 D2 | 该字段**从不赋值** ⇒ ⇒ 改用 `is_in(state)` |
| `chlib::ch_state_machine::tick_state()` | **移除** | 2026-10-30 | ADR-050 D2 | **说谎**的 API（重复发射节点）⇒ ⇒ 改用 `is_in()` / `current_state_uint()` |
| `chlib::ch_state_machine::current_state()` | **改名** → `entry_state()` | 2026-10-30 | ADR-050 D3 | ⚠️ 旧名返回**编译期** entry ⇒ ⇒ 运行时状态用 `current_state_uint()` / `is_in()` |
| `CH_STATE_MACHINE` / `CH_STATE` / `CH_STATE_MACHINE_BUILD` 宏 | **移除** | 2026-10-30 | ADR-050 D2 | 改用 `chlib::ch_state_machine<State, N>` 类模板 |
| `chlib::ch_state_machine::state_def::parent`（+ `friend`） | **移除** | 2026-10-30 | ADR-050 D2 | 只服务 `then()` ⇒ ⇒ 随 `then()` 一并移除 |
| `get_lnode` 的「无约束通用入口」②（`include/core/bundle/bundle_base.h`） | **移除** | 2026-10-30 | ADR-053 决策 C | 改用 bundle 自带的 `impl()`（约束偏序下**恒胜** ⇒ ⇒ 爆炸半径 0） |
| `bundle_concat` / `bundle_slice_view` | **移除** | 2026-10-22 | ADR-043 | 改用 bundle 的**字段直连**（ADR-042 把字段改成一等节点 ⇒ 拼接/切片不再需要）；两者当时均为**静默失效**（`bundle_concat` 字段不在位布局里 · `bundle_slice_view` 根本不切片） |
| `conditional_reg_assign` | **改名** → `conditional_assign` | 2026-10-30 | ADR-051 | 旧名暗示「只对寄存器」⇒ 而**输出端口同样**需要条件化 ⇒ 改用 `conditional_assign(target).next() = v` |
| `include/cpu/cache/*_stub.h`（pass-through stub 族） | **物理删除** | 2026-10-20 | ADR-037 | 语义不构成合法 cache 行为 ⇒ ⇒ 改用 `i_cache_{sync,async}.h` / `d_cache_{sync,async}.h` |
| `WriteBufferEntry` / `WriteBufferConfig`（`include/cpu/cache/d_cache_write_buffer.h`） | **移除** | 2026-11-09 | ZERO-DEBT（phase-6 **P9** · 死数据：`buffer[4]` 零消费者） | 下游若引用 ⇒ ⇒ **自建**等价类型；wrapper 的 `__io` 签名与可观测行为**不变**；**真 FIFO 写缓冲 = 独立立项** |
| `prefetch_data` / `prefetch_addr` / `prefetch_valid` / `PrefetchConfig`（`include/cpu/cache/i_cache_prefetch.h`） | **移除** | 2026-11-09 | ZERO-DEBT（phase-6 **P9** · 死数据：预取缓冲零消费者） | 下游若引用 ⇒ ⇒ **自建**；**本组件现在不含任何预取状态** ⇒ **真预取 = 独立立项**；wrapper 的 `__io` 签名与可观测行为**不变** |
| `rv32i_forwarding.h` / `riscv::ForwardingUnit`（`include/cpu/pipeline/rv32i_forwarding.h`） | **物理删除** | 2026-11-11 | `rv32i-dead-component-consolidation` **C5**（零交付路径消费者 + 独有语义为空集：WB 档无覆盖 · `id_reg_read` 死输入） | 下游若引用 ⇒ ⇒ 改用仓内权威的 `riscv::HazardUnit`（**已接入真实流水线**）+ `chlib::ForwardingMux`（ADR-047 D2） |
| `hazard_detection_unit.h` / `riscv::HazardDetectionUnit`（`include/cpu/pipeline/hazard_detection_unit.h`） | **物理删除** | 2026-11-11 | `rv32i-dead-component-consolidation` **C6**（零交付路径消费者 + 其 MEM 级检测点在流水线里**永不触发**） | 下游若引用 ⇒ ⇒ 改用 `riscv::HazardUnit`（`include/cpu/pipeline/hazard_unit.h`） |
| `rv32i_hazard_complete.h` / `riscv::HazardUnitComplete`（`include/cpu/pipeline/rv32i_hazard_complete.h`） | **物理删除** | 2026-11-11 | `rv32i-dead-component-consolidation` **C6**（零交付路径消费者 + 2 个死输入 + 3 档语义；唯一存在理由是 parity 夹具） | 下游若引用 ⇒ ⇒ 改用 `riscv::HazardUnit`；⚠️ **这是显式批准的覆盖缩面**（`DUAL_BACKEND_PARITY_PAIRS` −6） |

## ⚠️ **行为变更（非符号移除）**（2026-11-11 起登记）

> **为什么需要这一节**：有些变更**不移除任何符号**，但**改变可观测行为** ⇒ ⇒
> 只查「符号还在不在」的护栏（R2/R5）对它**结构性失明** ⇒ ⇒ 下游「编译通过但行为变了」
> ⇒ ⇒ ⇒ 本节把这类变更也变成**可查的活账**。
>
> ⚠️ **与上面那节的分工**：上面 = **符号级**（没了 / 改名了）· 本节 = **行为级**（还在，但变了）。

| 符号 / 头 | 行为变更 | 日期 | 依据 | 迁移指引 |
|---|---|---|---|---|
| `ch::build_hierarchical_name()` / `ch::Component::name()`（`include/component.h`） | **子组件全路径的拼接不再按父路径的「字面值」剥离路径段** | 2026-11-11 | `node-name-prefix-consistency` **P1**（F-A）· capability `component-ownership-and-naming` | ⚠️ **唯一受影响的情形**：根组件的 name 为空（`local_name_` 归一成 `"unnamed"`）⇒ ⇒ 其子组件的 `name()` 从**裸局部名**（`"kid"`）变为**带父前缀**（`"unnamed.kid"`）⇒ ⇒ **按 `name()` 精确匹配裸局部名的下游代码 MUST 更新**；常规路径（父名非空，如 `"top"`）**逐字节不变** |
| `ch::Component::name()` 的**根组件空名**语义 | **空名与用户真名 `"unnamed"` 在 `name()` 上永久不可区分**（**已登记代价**） | 2026-11-11 | 同上（F-A 的已知代价） | 空名根组件现在与「真名 `"unnamed"`」的根组件**行为完全相同** ⇒ ⇒ 若下游需要区分 ⇒ ⇒ 须**另立 item**（引入独立的「无名」标记而非复用字面名）⇒ ⇒ **不在本项** |
| 顶层 RTL 端口表（RV32I SoC） | `mem_is_load` 的装配点**补上 driver** ⇒ ⇒ 该 `ch_in` **不再是无 driver 的顶层 input** ⇒ ⇒ **顶层 input 少 1 个**（8 → 7） | 2026-11-11 | `rv32i-dead-component-consolidation` **C8** · 契约相容（契约说的是「**无 driver 的** input」⇒ 补 driver = 修缺陷） | 下游若按端口**数量/序号**硬编码顶层 input ⇒ ⇒ MUST 改为**按名**取；**行为零变化**（MEM 级那一路在流水线里永不触发） |
| 端口清单（manifest）schema `rtl-emit-manifest-v1`（**新** capability `rtl-emit-portability`） | **新增**一份 manifest schema（10 个字段：`name` / `dir` / `width` / `role` / `driven` / `effective` / `owner` / `is_top_io` / `cpp_member` / `semantic`）⇒ ⇒ `driven`（节点级）与 `effective`（功能级）**拆成两个独立字段**（「有连线但无功能」这一类**单布尔表达不了**） | 2026-11-11 | `rtl-emit-product-tooling` **C4**（B1 批地基）· change `rtl-emit-manifest-schema-2026-11` | ⚠️ **无库代码变更**（纯规格）⇒ ⇒ 下游若实现 RTL / TB / 命名工具，MUST 按这 10 个字段产出 manifest；**端口面与命名规则一字未改** ⇒ ⇒ 现有产物解析不受影响 |
| UART RX demo 的 `data` / 采样点（`examples/spinalhdl-ported/uart_rx/uart_rx.cpp`） | **① 解出的字节不再位反转**（旧：线上 LSB first ⇒ 解出的是**位反转**的字节，例：线上 `0x10` ⇒ 解出 `0x08`）· **② 采样点随 `prescale` 缩放**（旧：硬编码 `div == 4` ⇒ 只对 `prescale == 9` 成立 ⇒ 别的 `prescale` 在**位的前沿**采样）| 2026-11-12 | D6 + D7（实测缺陷 · 见 `.rddf/checks/decision-request-uart-rx-defects-2026-11.md`） | **无需迁移**（旧行为是错的）· ⚠️ 下游若**依赖**旧的位反转结果 ⇒ 那是在依赖一个缺陷 ⇒ 需改 |\n| 手写 FIFO demo 的 `full` 端口（`samples/fifo_example.cpp` + `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp`） | **`full` 的门限修正**：旧写法在**完整指针**空间里比 `wr+1 == rd` ⇒ 门限 = `2^(addr_width+1)-1`（N=16 ⇒ 31）⇒ **永不置位** ⇒ 第 N+1 次写入起地址混叠 ⇒ ⇒ 改为经典方案（**地址位相等 + wrap 位不同** ⇒ 容量 = N）| 2026-11-12 | D5（实测缺陷 · 见 `.rddf/checks/decision-request-fifo-full-threshold-2026-11.md`） | **无需迁移**（旧行为是错的）；若曾依赖「`full` 恒 0」，该依赖本就不成立 ⇒ 现在 `full` 会在满时正确置位。⚠️ 库组件 `chlib::sync_fifo` **未变**（它一直是对的）|

### **已弃用（尚未移除）** —— 下游现在就该迁移

| 符号 | 弃用日 | 依据 | 移除窗口 | 迁移指引 |
|---|---|---|---|---|
| `ch_bool` / `port<ch_bool>` 上的 `&&` / `\|\|` | 2026-10-19 | ADR-036 | ⚠️ **日历锁：最早 2027-04-17** | 改用 `&` / `\|`；mixed 语义 MUST 写成显式 nonzero-test（`(lhs != 0_d) & rhs`） |
| `ICacheComplete` / `DCacheComplete` | 2026-10-20 | ADR-039 / ADR-041 | （尚无窗口） | 改用 `ICacheSync` / `DCacheSync`（或 `_async` 变体） |

> ⚠️ **本节的规则**：
> - **移除 / 改名**任何**公开符号** ⇒ ⇒ MUST 在本节加一行（符号 · 日期 · 依据 · 迁移指引）
>   ⇒ ⇒ 否则护栏 `scripts/verify_public_api_visibility.sh`（R2）**红**。
> - 本节登记的符号 MUST **确实不存在**于当前源码（⇒ ⇒ R2 的「登记了仍存在的符号 ⇒ 红」）
>   ⇒ ⇒ ⇒ 「删了又加回来」必须同步删登记行。
> - 本节 MUST **非空**（⇒ ⇒ 防「登记表被清空 ⇒ 静默缩面」）⇒ ⇒ R2。
> - **机器探针**在 `.rddf/checks/public-api-removals.tsv`（逐符号 / 逐文件族 · 四种 kind ·
>   判定在**剥掉注释与字符串字面量之后**进行）⇒ ⇒ 本节是**人读**的（含迁移指引），
>   tsv 是**机器读**的 ⇒ 两者由护栏 **R5/R6** 双向绑定（探针 id MUST 出现在本节）。

## 维护规则

- **新增**顶层公共头 ⇒ ⇒ **MUST** 在上表加一行（否则护栏 R4② **红**）。
- **删除**顶层公共头 ⇒ ⇒ **MUST** 从上表删行（否则护栏 R4② **红**：登记了不存在）。
- ⚠️ **上表是「承诺」而不是「清单」** ⇒ ⇒ 加一行 = **接受**该头的语义承诺
  （⇒ ⇒ 公共 API 面**扩张** ⇒ ⇒ 需要 review ⇒ ⇒ ⇒ 与 P6 的「公开 API 面登记机制」
  backlog 同源 ⇒ ⇒ **本文件是该机制的第一版落地**）。

## 相关

- 护栏：`scripts/verify_hierarchical_p8.sh`（R1 / R2 / R2' / R3 / R4① / R4②）
- capability：`hierarchical-equivalence-public-helper`（P8 的公共面契约）
- 既有 capability：`rtl-hierarchical-output`（产物契约 —— **不**由本文件登记）
