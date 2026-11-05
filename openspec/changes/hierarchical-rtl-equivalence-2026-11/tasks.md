# 任务清单：`hierarchical-rtl-equivalence-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（hierarchical **ADDED ×2** + **MODIFIED ×5**）。
> ⚠️ **前置**：**T3-④**（层次发射）。
> ⚠️ **工作量 ~7–9 天**（oracle R13 实测：backend 的「只增不改入口」远不止加一个函数）。

## T1 backend 只增不改入口

- [ ] 1.1 `initialize_with_verilog(path, port_set, data_map, work_dir)`：接收**显式端口集合**
      （`node_id` + 顶层端口名 + 宽度 + 方向）⇒ ⇒ 集合 MUST **⊇ {clock, reset}**
- [ ] 1.2 `build_port_access_table` 改造：从 ctx 全量推 ⇒ 改为**显式集合**
- [ ] 1.3 `emit_sim_main_postlude` 同上
- [ ] 1.4 **既有 `initialize(ctx, …)` 行为逐字节不变**
- [ ] 1.5 **E1 来源约束**：上述集合 MUST 由 **emitter 产物**（映射表 ∪ `port_provenance`）派生
      ⇒ ⇒ **MUST NOT** 由 harness / backend 独立撰写或手工列举（ADR-058 v2.14 §2.3 E1）
- [ ] 1.6 **E2 物理隔离**：与既有路径 **两个独立 work_dir**（共享 `verilator_work_dir_` ⇒ 互踩）
- [ ] 1.7 **E3 + 顶层名**：失败 MUST **致命**（返回 `VerilatorError`，**MUST NOT** 沿用
      `CHWARN` best-effort）；harness MUST 断言 backend **真初始化成功**（非 SKIP 即过）；
      顶层 module 名 ≠ `top` ⇒ 入口**致命拒绝 + 点名**且**不留文件**（fail-early）

## T2 映射表与观测集

- [x] 2.1 emitter 产出**四元组**映射表（`node_id` → 名 → 扁平位置 → 层次位置集合），
      载体 = `HierarchyEmitResult` 的**新增公开字段**（value-only · `hier_top_port_name` 允许为空）
      ⇒ ⇒ **B1 已落地**（`SignalMapEntry` / `SignalLocation` · `include/hierarchy_verilog.h`）：
      发射器**在同一次调用内**产出（不另开 manifest 入口）· 顺序 = `node_id` 升序（确定性）·
      发射前过 `check_signal_map`（`src/hierarchy_signal_map_check.h`）⇒ 不满足 ⇒ `ok=false`
      且**不产出文件**；判据 = 顶层端口全覆盖 · `flat.is_port` ⇔ 顶层端口名非空 ·
      「声称在顶层 ⇒ MUST 真的有那个位置」· 「顶层端口名为空 ⇒ MUST 真的不在顶层」
      （变异 M-B1：漏记一个顶层端口的位置 ⇒ 转红 ✓）
- [ ] 2.2 `O` 由测试**手写声明**；MUST ⊇ DUT 全部 `ch_out`；MUST 非空
- [ ] 2.3 **「随激励变化」的机械判据**（两组激励 ⇒ 取值集合不同）

## T3 等价性用例

- [ ] 3.1 5 个 DUT（**清单锁定**：两层 / 三层+兄弟连线 / bundle 成员跨层 / 含 `ch_mem` /
      真实最深设计 ⇒ ⇒ 后两者的粒度矩阵见 design §6.2：**只跑 `RespectDeclarations`**）
- [ ] 3.2 逐拍全等；**多拍激励** + **至少一个寄存器**
- [ ] 3.3 `ForceInline` vs `ForceLayer` 逐拍全等 ⇒ ⚠️ **仅用于不含跨模块 `mem` 读的小 DUT**
      （含 `ch_mem` / 深设计在 ForceLayer 下会被**合法拒绝** ⇒ 无可执行对象，见 design §6.2）
- [ ] 3.4 同一 `node_id` 名字逐字相同（按映射表索引比对）
- [ ] 3.5 判别力自证 **≥2 形态**（删连线 · 改隐式端口方向）⇒ 两红 ⇒ 还原复绿

## T4 确定性与零回归

- [ ] 4.1 同一设计两次生成**字节相同**（同 ABI 内）
- [ ] 4.2 产物通过 **ADR-060 §2.5 的 lint 门**（退出码 + 无 `%Error` + `lint_off` 白名单；
      ⚠️ **不加 `-Wall`**、**禁 `-Wno-fatal`**）
- [ ] 4.3 `ctest -LE perf` / samples / examples 全绿 · 计数锚点**按新增用例同步更新**
      （⚠️ 等价性用例带 `verilator_e2e` 标签 ⇒ `VERILATOR_E2E_TOTAL` 必变；
      parity 两锚点（66 / 289）**不变** —— 等价性用例不是 `DUAL_BACKEND_TEST_PAIR`）

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

## 归档前判据（V1 · D4 的闭环 · 正反两面）

> 台账 §11.11.2 的 V1：主 spec 的 5 条 Requirement 已由本 change 的 delta 以 **MODIFIED** 修订
> ⇒ ⇒ 这两条 grep 是**归档门**，任一条不满足 ⇒ **不得归档**。

```bash
S=openspec/specs/rtl-hierarchical-output/spec.md
# 正面：断言面的 7 行必须清零（本 change 开工前 = 7）
grep -cE -- '-Wall|`strict`|MUST \*\*(抛异常|拒绝)|MUST 改为抛异常' "$S"
# 反面：合法的「MUST NOT 依赖抛异常」必须仍在（防「为清零而误删」）
grep -c '抛异常' "$S"
```

⇒ ⇒ 归档时 MUST 分别为 **0** 与 **1**（1 = 仅 `:398` 的静态析构期禁令）。
