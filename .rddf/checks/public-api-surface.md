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
