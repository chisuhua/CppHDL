# 任务清单：`hierarchical-rtl-equivalence-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（hierarchical ADDED ×3）。
> ⚠️ **前置**：**T3-④**（层次发射）。
> ⚠️ **工作量 ~7–9 天**（oracle R13 实测：backend 的「只增不改入口」远不止加一个函数）。

## T1 backend 只增不改入口

- [ ] 1.1 `initialize_with_verilog(path, data_map)`：接收**显式端口集合**
      （`node_id` + 顶层端口名 + 宽度 + 方向）
- [ ] 1.2 `build_port_access_table` 改造：从 ctx 全量推 ⇒ 改为**显式集合**
- [ ] 1.3 `emit_sim_main_postlude` 同上
- [ ] 1.4 `invoke_verilator` 支持**多文件** + 缓存 key 纳入**全部** source
- [ ] 1.5 **既有 `initialize(ctx, …)` 行为逐字节不变**

## T2 映射表与观测集

- [ ] 2.1 emitter 产出**四元组**映射表（`node_id` → 名 → 扁平位置 → 层次位置集合）
- [ ] 2.2 `O` 由测试**手写声明**；MUST ⊇ DUT 全部 `ch_out`；MUST 非空
- [ ] 2.3 **「随激励变化」的机械判据**（两组激励 ⇒ 取值集合不同）

## T3 等价性用例

- [ ] 3.1 5 个 DUT（**清单锁定**：两层 / 三层+兄弟连线 / bundle 成员跨层 / 含 `ch_mem` /
      真实最深设计）
- [ ] 3.2 逐拍全等；**多拍激励** + **至少一个寄存器**
- [ ] 3.3 `ForceInline` vs `ForceLayer` 逐拍全等
- [ ] 3.4 同一 `node_id` 名字逐字相同（按映射表索引比对）
- [ ] 3.5 判别力自证 **≥2 形态**（删连线 · 改隐式端口方向）⇒ 两红 ⇒ 还原复绿

## T4 确定性与零回归

- [ ] 4.1 同一设计两次生成**字节相同**（同 ABI 内）
- [ ] 4.2 `verilator --lint-only -Wall` **零 error**
- [ ] 4.3 `ctest -LE perf` / samples / examples 全绿 · 计数锚点同步

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
ctest -L verilator_e2e --output-on-failure
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
openspec validate hierarchical-rtl-equivalence-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
