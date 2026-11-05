# 任务清单：`hierarchical-rtl-equivalence-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（hierarchical **ADDED ×2** + **MODIFIED ×5**）。
> ⚠️ **前置**：**T3-④**（层次发射）。
> ⚠️ **工作量 ~7–9 天**（oracle R13 实测：backend 的「只增不改入口」远不止加一个函数）。

## T1 backend 只增不改入口

- [x] 1.1 入口接收**显式端口集合**（`node_id` + 顶层端口名 + 宽度 + 方向）⇒ ⇒
      集合 MUST **⊇ {clock, reset}**
      ⇒ ⇒ **B3 已落地**：实际签名 `try_initialize_with_verilog(verilog_path, ports, data_map)`
      （`include/core/verilator_backend.h`）——**无 `work_dir` 形参**：work dir 来自构造器
      （`VerilatorBackend(verilog_path)` 的实参即 work dir）⇒ 两条路径的物理隔离由调用方
      **各构造一个实例**承担（见 1.6）；端口值类型 = `ch::VerilatorPortSpec`
      （`include/core/verilator_port_set.h`，value-only 中立头）· `⊇ {clock, reset}`
      在**碰 I/O 之前**校验（`[hier][negative]` 用例）
- [x] 1.2 `build_port_access_table` 改造：从 ctx 全量推 ⇒ 改为**显式集合**
      ⇒ ⇒ **B3 已落地**：新增 `build_port_access_table_from_specs()`
      （`port_access_.size() == ports.size()` 在 `[hier]` 用例断言）；
      既有 `build_port_access_table()` 保留且**未被替换**
- [x] 1.3 `emit_sim_main_postlude` 同上
      ⇒ ⇒ **B3 已落地**：新增 `emit_sim_main_postlude_for_ports(path, ports)`（**不读 ctx**）；
      固定前导抽成 `kSimMainPrelude`，两条路径共用（文本与既有逐字节**相同** —— 缓存 key 的输入）
- [x] 1.4 **既有 `initialize(ctx, …)` 行为逐字节不变**
      ⇒ ⇒ **B3 已验收**：`sim_main.cpp` 生成文本与原实现 **IDENTICAL**
      （`/tmp/opencode/cmp_prelude5.py` 的字面量级比对）+ `ctest -LE perf` 238/238 ·
      总时长 363s（与 B2 基线一致 ⇒ cache key 未漂 ⇒ 无冷编译）
- [x] 1.5 **E1 来源约束**：上述集合 MUST 由 **emitter 产物**（映射表 ∪ `port_provenance`）派生
      ⇒ ⇒ **MUST NOT** 由 harness / backend 独立撰写或手工列举（ADR-058 v2.14 §2.3 E1）
      ⇒ ⇒ **B3 已落地**：`ch::backend_port_set(emitted, root_module)`（`src/hierarchy_verilog.cpp`）；
      负向对照 = 抹掉映射表里一个顶层端口名 ⇒ 派生集合**跟着少一个**
- [x] 1.6 **E2 物理隔离**：与既有路径 **两个独立 work_dir**（共享 `verilator_work_dir_` ⇒ 互踩）
      ⇒ ⇒ **B3 已落地**：`[hier]` 用例用 `make_temp_dir("_hier_v")`（Verilog 产物）
      与 `make_temp_dir("_hier_b")`（backend work dir）两个目录，且与既有路径的实例**不同**
- [x] 1.7 **E3**：失败 MUST **致命**（返回 `VerilatorError`，**MUST NOT** 沿用
      `CHWARN` best-effort）；harness MUST 断言 backend **真初始化成功**（非 SKIP 即过）
      ⇒ ⇒ **B3 已落地**：`compile_and_load(/*fatal=*/true)` 路径；`[hier]` 用例 `REQUIRE(init.has_value())`
      （**不 SKIP**）+ 值判据 0x2A / 0x5C（手推：若 clock/reset 未接 ⇒ 寄存器冻结 ⇒ 恒 0 ⇒ 转红）
      ⏳ **顶层名 fail-early 归 B4**（本批未做：`root_module != "top"` ⇒ 入口致命拒绝 + 点名 + 不留文件）

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

## T2b 顶层端口集合判据（结构面 · 零仿真）

- [x] 2b.1 输入端口集合 MUST **相等**（名字 + 宽度逐项配对 · **双向**）+ 输出端口集合 MUST
      **等于**「扁平输出集 − 被移出顶层的那批」（**双向**）⇒ ⇒ 判据函数落在
      `src/hierarchy_port_set_check.h`（**发射器侧与单测共用同一份** ⇒ 否则单测只能证明
      「自己和自己一致」）⇒ ⇒ ⚠️ 期望集**手推**（MUST NOT 从层次化产物反推 ⇒ 那是
      「实际集 − 实际集」的**循环定义** ⇒ 恒绿 ⇒ 「不减法」这类缺陷抓不住）
- [x] 2b.2 产物级载体：护栏 **R8**（`scripts/verify_hierarchical_rtl.sh`）—— 从
      `verilator --json-only` **结构化提取**顶层端口表
      （`scripts/lib/verilog_tree_extract.py --ports` ⇒ `isPrimaryIO` + dtype 的 `range`
      ⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器）⇒ ⇒ 与**手推期望集**逐项比对
      （名字 + 方向 + 宽度）⇒ ⇒ 同时判掉「成层子树的 `ch_out` 跑到顶层」
      ⇒ ⇒ 投毒对照：改顶层端口宽度 ⇒ R8 红（而 R1 仍绿 ⇒ 证明不是 R1 的重复）

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
