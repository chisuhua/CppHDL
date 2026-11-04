# 任务清单：`hierarchical-rtl-emitter-split-2026-11`

> 判据来源：proposal.md §Acceptance。
> ⚠️ **前置**：**T3-①b**（归属真源）—— ①b 之后的扁平产物是本 change 的**基线**。
> ⚠️ **无 delta**（纯重构）⇒ `.openspec.yaml` 显式 `skip_specs: true`。

## T1 拆分

- [x] 1.1 `ModuleEmitter(ctx, ModuleView)` —— `ModuleView{nodes, ports, names, module_name,
      clock_name, reset_name}`（`include/codegen_verilog.h`）
- [x] 1.2 `verilogwriter` 变成薄适配器（`unique_ptr<ModuleEmitter>` + `make_flat_module_view`）
      ⇒ 端口选择/排序/过滤逻辑**逐字搬进** `make_flat_module_view`

## T2 时钟名参数化

- [x] 2.1 四处参数化：`emit_always_ff` · `print_reg`（无 next 分支）· `print_mem_read_port` ·
      `print_mem_write_port`
- [x] 2.2 `make_flat_module_view` 传 `default_clock` / `default_reset` ⇒ 产物逐字节不变（见 3.1）

## T3 判据

- [x] 3.1 外部基线比对（3 个设计：RegChain / MemTop / Rv32iPipeline 145 KB）⇒ `cmp` **全 0**
- [x] 3.2 **M1 实测**：单测 RED + lint **exit 1**（`Can't find definition of variable:
      'default_clock'`）；对照组 lint **exit 0**
- [x] 3.3 `ctest -LE perf` **237/237** · samples 39/39 · examples 28/28 ·
      计数锚点同步（247 → 248 / non-perf 236 → 237 / `tests/test_*.cpp` 139 → 140）

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
openspec validate hierarchical-rtl-emitter-split-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
