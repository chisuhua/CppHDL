# 任务清单：`hierarchical-rtl-naming-fix-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（ownership ADDED ×2 · hierarchical ADDED ×1）。
> ⚠️ **零语义变更**：本 change **只改名字**，**不碰**归属 / 粒度 / emitter。
> ⚠️ **前置**：`header-odr-inline-fix-2026-11`（公共头 ODR）—— ✅ **已归档 2026-11-04**。

## T1 命名模型（`include/component.h` + `src/component.cpp`）

- [x] 1.1 `Component` 新增 `local_name_` 成员（**先于** `name_` 声明 ⇒ 初始化顺序正确）
- [x] 1.2 新增访问器 `local_name()` / `module_type_name()`；`name()` 语义**不变**（全路径）
- [x] 1.3 **构造函数入参 = 局部名**，全路径由构造函数自算（design.md §2.1.1）
      ⇒ ⇒ 不需要 `set_local_name`（初稿方案已被推翻）
- [x] 1.4 `module_type_name()` = 类级字面量（`layer_name_literal()` 虚函数）否则回落
      `typeid(*this).name()`（⇒ 同类型 ⇒ 同值 ⇒ 满足 R3；⇒ 跨平台**不可复算**
      ⇒ 契约限定「同 ABI 内」；⚠️ **未 sanitize** —— sanitize 归发射侧，见 design.md §5 偏离 3）
- [x] 1.5 **删除** `Component::hierarchical_name()`（唯一消费者在 T2 被替换 ⇒ 留着 = 第二个真源）
- [x] 1.6 move ctor / move assignment 一并搬运 `local_name_`

## T2 路径拼接（`include/component.h` + `include/module.h`）

- [x] 2.1 `build_hierarchical_name()` 改用 `parent->name()`（**MUST NOT** 用 `hierarchical_name()`）
- [x] 2.2 函数**从 `module.h` 移到 `component.h`**（唯一真源与计算处同文件）
- [x] 2.3 `ch_module` ctor 传**局部名**（不再自己拼全路径）
      ⇒ ⇒ **派生组件 ctor 签名零变更**

## T3 合并宏（`include/component.h`）

- [x] 3.1 `CH_HIERARCHY_LAYER("name_literal")`：**成层 + 命名一体**
      ⇒ 暴露**编译期可见**的名字（`static constexpr const char* kChHierarchyLayerName`）
      + `static constexpr bool kChIsLayer`
      ⇒ ⇒ 「成层却忘了命名」**编译期即不可表达**
- [x] 3.2 宏体 **MUST NOT** 触碰 `this`（类级声明 ⇒ 实例级名字归 ①c）
- [x] 3.3 宏内声明 `layer_name_literal()` 覆写（**virtual** ⇒ 运行期才取到派生类版本）

## T4 判据

- [ ] 4.1 **深度 ≥3 无重复前缀**：三层组件树 ⇒ Leaf 路径形如 `top.m.lf`（**MUST NOT** `top.top.m.lf`）
- [ ] 4.2 `local_name()` 返回**局部**名；`module_type_name()` 返回**类型**名（同类型多实例同值）
- [ ] 4.3 `hierarchical_name()` **MUST NOT** 仍存在（编译期判据：`grep` 命中即红）
- [ ] 4.4 **文本变化面逐条登记**：ADR-059 §5 的 4 处实测依赖
      （`tests/test_axi_spi.cpp:213-214` · `tests/benchmark/verilator_harness_tc07.cpp:33` ·
      `tc08.cpp:51` · `tests/test_verilator_backend.cpp` 的 `module top` / 端口名）
      **+** trace 输出（信号名变 ⇒ trace 文件也变）
- [ ] 4.5 **零语义变更**：仿真结果**逐位不变**
- [ ] 4.6 `ctest -LE perf` / `run_all_samples.sh` / `run_all_ported_tests.sh` 全绿 · 计数锚点同步

## T5 护栏与 CI

- [x] 5.1 `scripts/verify_component_naming.sh`：R0 fail-closed（缺 fixture 报「缺 fixture」）
      · R1 正向编译通过 · R2 「缺名」MUST 编译失败 · R3 `hierarchical_name()` MUST 不存在
      · R4 源码级（独立于 R3）· `--self-test` 中和 R2/R3 自证
- [x] 5.2 CI 挂 **两处**（主路径 + `--self-test`）

## T6 交接（**不在本 change 内**）

- [ ] 5.1 **T3-①b**（归属）：作用域栈 / `node_owner()` / `ch_module` parent / build ctx / 组件树
- [ ] 5.2 **T3-①c**（粒度）：`ch_layer<T>` / `ch_inline<T>` / `is_layer()` / 无 parent 装配点
      ⇒ ⇒ **四档来源的 ②③④** 在 ①c 落地（档 ① + 回落在本 change）

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
openspec validate hierarchical-rtl-naming-fix-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
