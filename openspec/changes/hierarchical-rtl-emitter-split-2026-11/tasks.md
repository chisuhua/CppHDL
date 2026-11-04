# 任务清单：`hierarchical-rtl-emitter-split-2026-11`

> 判据来源：proposal.md §Acceptance。
> ⚠️ **前置**：**T3-①b**（归属真源）—— ①b 之后的扁平产物是本 change 的**基线**。
> ⚠️ **无 delta**（纯重构）⇒ `.openspec.yaml` 显式 `skip_specs: true`。

## T1 拆分

- [ ] 1.1 `ModuleEmitter`（接收节点子集 + 端口表 + 名字表）
- [ ] 1.2 `verilogwriter` 委托给它；扁平路径行为**一字不改**

## T2 时钟名参数化

- [ ] 2.1 `print_reg` / `emit_always_ff` 的 `default_clock` / `default_reset` 改为入参
- [ ] 2.2 扁平路径传原值 ⇒ 产物逐字节不变

## T3 判据

- [ ] 3.1 **基线脚本**：相对 T3-①b 之后的基线 `cmp` 退出码 **0**
- [ ] 3.2 **mutation**：去掉参数传递 ⇒ `verilator --lint-only` 必报错（未声明信号）
- [ ] 3.3 `ctest -LE perf` 全绿 · 计数锚点同步

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
openspec validate hierarchical-rtl-emitter-split-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
