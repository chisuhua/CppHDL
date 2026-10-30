# Tasks: get-lnode-entry-set-2026-10

## 0. 只读核实（实施前）

- [x] 0.1 核实两个 `get_lnode` 签名完全相同（`lnode.h:24-28` 有约束 /
      `bundle_base.h:551-553` 无约束）
- [x] 0.2 核实 `bundle_base` **继承**了 `logic_buffer::impl()`（`bundle_base.h:67`）
      ⇒ `HardwareType<Bundle>` = true
- [x] 0.3 核实仓内 `lnode<Bundle>` / `ch_width_v<Bundle>` 使用数均为 **0**
- [x] 0.4 核实 `samples/if_demo.cpp` 的报错**指向** `bundle_base.h:552`
- [x] 0.5 核实**无** `get_lnode<` 显式模板实参 / `&get_lnode` 取址用法

## 1. 实现（**同一次提交**内完成，否则中间态编译不过）

- [x] 1.0 ⚠️ **实施期订正**：初稿写「入口集合 = 2」⇒ ⇒ 实测活声明是 **3** 个
      （`uint.h:197-200` 的 `get_lnode(const ch_uint<N>&)` **实测生效**）⇒ ⇒
      契约精确化为「**通用**入口 = 1」+「MUST NOT 有无约束的**通用**入口」⇒ ⇒
      **裁决不变**；ADR-053 + capability spec + `lnode.h` 注释**同批**订正
- [x] 1.1 **删除** `include/core/bundle/bundle_base.h:551-553`
- [x] 1.2 `include/core/lnode.h`：在两个入口**之后**加
      `static_assert(!requires(no_impl_tag x) { get_lnode(x); }, "ADR-053: ...")`
      ⚠️ 位置 MUST 在两入口之后（否则探测不到 ⇒ 断言恒真 ⇒ 无判别力）
- [x] 1.3 `include/core/lnode.h`：加「入口集合 = 2」注释 + 引用 ADR-053
- [x] 1.4 同批处置 `lnode.h:42-58` 的注释死块（删或写明保留理由，不允许沉默）
- [x] 1.5 **MUST NOT** 用 `= delete` 兜底（实测两编译器均为硬错误 ⇒ 更坏）
- [x] 1.6 **MUST NOT** 用函数体 `static_assert` 陷阱（体不实例化 ⇒ 真毒化）

## 2. 验证（每条必须能**转红**）—— **两层**护栏

### 2.1 编译期层（`tests/test_lnode_entry_set.cpp`）

- [x] 2.1.1 **V1** 存活探针（**编译期** `static_assert`）：`get_lnode(bundle)` /
      `get_lnode(ch_uint<8>)` / 恒等版 MUST 全部解析 ⇒ 把「零合法消费者」从
      **grep 结论**升级为**可执行复核**
- [x] 2.1.2 **V2** 权威断言：`static_assert(!can_get_lnode<no_impl_tag>)`
      ⇒ ⚠️ **必须放在依赖上下文**（模板参数）⇒ 非依赖的 requires-expression 里
      失败是**硬错误**而非求值 `false`
- [x] 2.1.3 **正向对照**：`can_get_lnode<ch_uint<8>>` MUST 为 `true`
      ⇒ 否则断言可能恒真 ⇒ 失去判别力
- [x] 2.1.4 运行期：bundle 的 `get_lnode` MUST 返回**它自己**的那个节点

### 2.2 源码级扫描层（**必需** —— 实施期实测的关键发现）

- [x] 2.2.1 ⚠️ **只靠编译期层会漏报** —— 它**只能看到 fixture 自己 include 的**
      那些头文件 ⇒ 缺陷真正的所在地（`core/bundle/bundle_base.h`）若**没被**
      include ⇒ ⇒ **漏**（实测：恢复那个无约束重载后，编译期判据**仍全绿**）
- [x] 2.2.2 `scripts/lib/get_lnode_entry_set.py`：扫描 `include/**` + `src/**` 的
      **全部** `get_lnode` 声明点 ⇒ 判定「通用」+「无约束」⇒ 违规 ⇒ exit 1
- [x] 2.2.3 分析器**自身**的自检（6 个内联 fixture）：通用无约束 ⇒ 违规；
      通用有约束 ⇒ 合法；类型专用 ⇒ 合法；恒等版 ⇒ 合法；注释里 ⇒ 合法；调用点 ⇒ 合法
- [x] 2.2.4 `scripts/verify_get_lnode_entry_set.sh`：R1 存在性 · R2 判别力 ·
      R3 诊断质量（**GCC + Clang 各一次**）· R4 源码级扫描
- [x] 2.2.5 `--self-test`：把**真实头文件**投毒 ⇒ 分析器 MUST 报违规 ⇒ 还原

### 2.3 回归

- [x] 2.3.1 **V5** 零回归：全量构建 · `ctest -LE perf` · `run_all_samples.sh` ·
      `run_all_ported_tests.sh` 全绿；计数锚点同步
- [x] 2.3.2 全部护栏（含 `--self-test`）exit 0

## 3. 变异（每条**先确认编译成功**）

- [x] 3.1 **M1** 恢复无约束 `get_lnode` ⇒ **R4** 源码级扫描 MUST 报违规
      （实测 ✅ exit 1）⇒ 红
- [x] 3.2 **M2** 把 `lnode.h:24-28` 的约束改成恒真（等价于无约束）⇒ R4 转红
- [x] 3.3 **M3**（分析器自身）把 `scripts/lib/get_lnode_entry_set.py` 的
      「无约束」判定去掉 ⇒ 分析器自检 MUST 转红
- [x] 3.4 记录「归一化 / 单分支」等 **N4 侧**变异不属本 change

## 4. 验收与归档

- [x] 4.1 计数锚点核对并同步（若新增测试文件 ⇒ ctest 条目数 +1）
- [x] 4.2 归档（**禁** `--skip-specs`；新建 capability ⇒ ADDED ×1 合并）
- [x] 4.3 ⚠️ **归档后 MUST 补写** `openspec/specs/core-lnode-entry-set/spec.md`
      的 `## Purpose`（archive **不合并** delta 的 `## Purpose`；先例
      `axi-interconnect-routing`）
- [x] 4.4 更新 ADR-053 的「关联 OpenSpec change」为**已归档**路径
