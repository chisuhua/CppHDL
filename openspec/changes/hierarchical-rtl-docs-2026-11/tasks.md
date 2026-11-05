# 任务清单：`hierarchical-rtl-docs-2026-11`

> 判据来源：proposal.md §Acceptance。
> ⚠️ **前置**：**T3-⑤**（等价性验证）—— 文档 MUST 描述**已落地**的行为。
> ⚠️ **无 delta** ⇒ `.openspec.yaml` 显式 `skip_specs: true`。

## T1 用户面文档

- [ ] 1.1 `docs/usage_guide/05-component-design.md` §5 改写
      ⇒ ⇒ **MUST 含 ADR-058 §4 风险 6 的观测盲区边界段**（等价性只覆盖顶层可见行为 ·
      被移出顶层的 `ch_out` 顶层不可观测 · 输入方向无盲区）—— 这是「三处同步登记」的第三处
      （ADR · capability spec 由 T3-⑤ 的 delta 负责 · 用法指南由本任务负责）
- [ ] 1.2 `HIERARCHICAL-RTL-AUTHORING-GUIDE.md`：删 `CH_MODULE_NAME` ·
      改 `CH_HIERARCHY_LAYER("name")` · 补档 ③ 形态
- [ ] 1.3 合并宏的**签名面 5 处**全部改毕

## T2 架构与索引

- [ ] 2.1 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` 转正
- [ ] 2.2 根 `AGENTS.md` 的「WHERE TO LOOK」补层次化入口

## T3 零漂移

- [ ] 3.1 文档示例**可编译**（照抄即过）
- [ ] 3.2 `ctest -LE perf` / samples / examples 全绿（文档 change ⇒ 理论零代码影响）

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
openspec validate hierarchical-rtl-docs-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
