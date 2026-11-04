# 任务清单：`hierarchical-rtl-analysis-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（hierarchical ADDED ×1）。
> ⚠️ **前置**：**T3-①b**（归属真源）· **T3-①c**（成层判定）。

## T1 数据结构

- [x] 1.1 `HierarchyModule` / `HierarchyInstance` / `HierarchyPort`（+ `HierarchyBodyNode`）/
      `HierarchyEdge` / `HierarchyAnalysis`（`include/hierarchy_analysis.h`）
- [x] 1.2 组件树 → 模块清单（post-order ⇒ 叶子在前、根最后）；inline 组件**不产生** module
- [x] 1.3 节点 → module 归属（`owner` + `nearest_layer()` 折叠到最近成层祖先）

## T2 端口与边

- [x] 2.1 端口表：`Self` / `ClockReset` / `InlineFolded` / `Bubbled` 四类 origin
      ⚠️ 名字取自**扁平发射器的全局命名表**（`include/codegen_naming.h`）⇒ 去重 + sanitize
      ⇒ 同一 module 内端口名唯一，且顶层端口名与扁平产物**逐字相同**（ADR-061 决策④）
- [x] 2.2 隐式端口**识别并响亮拒绝**（ADR-060 §2.1 方向矩阵 + 第三轴 pass-through）
      ⚠️ **合成**归 T3-④ ⇒ ⇒ 本层 MUST NOT 产出半成品端口表（fail-closed）
- [x] 2.3 悬空输入冒泡（inline 组件的无 driver `ch_in` ⇒ 逐级向上到根 module）

## T3 前置校验（MUST 在分析之前）

- [x] 3.1 已 build 未 `add_child` ⇒ 抛（错误信息给出 `top.add_child(std::move(unit))` 修法）
- [x] 3.2 已登记从未 build ⇒ 抛（判据 `!is_built()`；`Component::is_built()` 为本次新增的只读访问器）
- [x] 3.3 成层组件自驱动 `ch_in` ⇒ 抛（判据：driver 的 `nearest_layer` == 该组件）

## T4 golden

- [x] 4.1 结构化 golden **4 份**落 `tests/golden/hierarchy/`：`two_layers` · `three_layers` ·
      `inline_folding` · `inline_instance_path`（另 6 个 DUT 走**断言**而非 golden：fail-closed /
      前置校验 / 实例名 —— 它们的期望是「抛」或单值，golden 无增益）
- [x] 4.2 **手推 golden** = `three_layers.txt`（推导表见 `design.md` §4.1，按 ADR-060 §2.1 逐项）
- [x] 4.3 比对在测试二进制内（`CPPHDL_GOLDEN_DIR` 注入 + 逐字节相等；`UPDATE_GOLDEN=1` 供重生成）
      + **负向对照 12/12**（含 M12「端口方向写反」⇒ 手推那份 golden **必红**）

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
openspec validate hierarchical-rtl-analysis-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
