# 任务清单：`hierarchical-rtl-analysis-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（hierarchical ADDED ×1）。
> ⚠️ **前置**：**T3-①b**（归属真源）· **T3-①c**（成层判定）。

## T1 数据结构

- [ ] 1.1 `ModuleDef` / `InstanceDef`（模块清单 / 端口表 / 连接表）
- [ ] 1.2 组件树 → 模块清单（自底向上）；inline 组件**不产生** module
- [ ] 1.3 节点 → module 归属（`owner` + inline 折叠到最近成层祖先）

## T2 端口与边

- [ ] 2.1 端口表：自身 `ch_in` / `ch_out` + 隐式端口 + 冒泡端口 + clock/reset
- [ ] 2.2 隐式端口识别（ADR-060 §2.1 四行方向矩阵 + 第三轴 pass-through）
- [ ] 2.3 悬空输入冒泡识别（ADR-060 §2.2）

## T3 前置校验（MUST 在分析之前）

- [ ] 3.1 已 build 未 `add_child` ⇒ 抛（给 `add_child` 修法）
- [ ] 3.2 已登记从未 build ⇒ 抛（判据 MUST 是 `not built()`）
- [ ] 3.3 成层组件自驱动 `ch_in` ⇒ 抛

## T4 golden

- [ ] 4.1 结构化 golden（5 个 DUT）落 `tests/golden/`
- [ ] 4.2 **手推 golden** 至少 1 份（三层端口表 · 人手推导）
- [ ] 4.3 比对脚本 + 负向对照（改坏分析层 ⇒ 手推那份必红）

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
openspec validate hierarchical-rtl-analysis-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
