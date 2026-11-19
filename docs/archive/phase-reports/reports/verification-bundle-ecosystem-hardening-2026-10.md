# 验证记录 — `bundle-ecosystem-hardening-2026-10`

> 对应 change：`openspec/changes/bundle-ecosystem-hardening-2026-10`
> 记录日期：2026-10-22
> 执行环境：Linux x86_64 / g++-14 / OpenSpec CLI **1.4.1** / Verilator（子模块）

---

## 1. 缺陷修复验证

| 缺陷 | 现象（修复前） | 修复后 | 验证命令 |
|---|---|---|---|
| **L1** 基类构造期调虚函数 | `samples/nested_bundle_demo` **segfault**（exit 139） | **exit=0** | `./build/samples/nested_bundle_demo` |
| **L11** prefix 机制是「空壳」 | 叶子节点名**从来**不带 prefix | 叶子节点名 = `prefix.字段名`；嵌套 = `outer.inner.field` | `test_bundle_ecosystem` 的 prefix 用例（5 条） |
| **L2** `bundle_concat` / `bundle_slice_view` | 公开 API 静默失效 | **物理移除**（ADR-043） | `grep -rn "bundle_concat\|bundle_slice_view\|bundle_cat\|bundle_slice\b\|bundle_operations" --include=*.h --include=*.cpp . \| grep -v third_party \| grep -v openspec` ⇒ **0 行** |
| **L3** `deserialize_bits_to_fields()` 切片重绑定 | 字段 = 位串切片 | 字段 = **自己的**节点（`type_proxy`），切片只作驱动源 | `test_bundle_ecosystem` 的 deserialize 用例（2 条） |
| **L6** `flip()` 硬编码 `as_slave()` | 方向不取反 | master→slave / slave→master / **unknown→slave** | `test_bundle_ecosystem` 的 flip 用例（2 条） |
| **L5** 工厂共享字段节点 | 副本与原件共享字段节点 | 字段节点**独立**（总线节点仍共享） | `test_bundle_ecosystem` 的工厂用例 |
| **L9** `internal_direction` 死代码 | 全仓零使用 | 已删 | `grep -rn "internal_direction\|is_internal_v" --include=*.cpp --include=*.h . \| grep -v third_party \| grep -v openspec` ⇒ **0 行** |

---

## 2. 变异验证（4 类，MUST 各自被断言抓到）

方法：改坏实现 ⇒ 对应断言 MUST 变红 ⇒ 还原。

| # | 变异 | 期望 | 实测 |
|---|---|---|---|
| **M1** | `ch_stream` 的自由 `operator<<=` 退回**节点重绑定**（`sink.valid = source.valid`） | 连接语义断言变红 | ✅ `test_bundle_ecosystem` **2 处红**（`test_bundle_ecosystem.cpp:333` 节点身份、`:341` src 边）+ `test_stream_operators` **1 处红** |
| **M2** | `flip()` 硬编码 `as_slave()`（slave 分支改成 `as_slave()`） | flip 方向断言变红 | ✅ `test_bundle_ecosystem.cpp:292` 红 |
| **M3** | 工厂去掉 `detach_field_nodes()`（副本与原件共享字段节点） | 工厂独立性断言变红 | ✅ `test_bundle_ecosystem.cpp:327` 红 |
| **M4** | `deserialize_bits_to_fields()` 退回**切片重绑定** | 节点身份断言变红 | ✅ `test_bundle_ecosystem.cpp:357` 红 |

### ⚠️ 变异验证当场抓到**测试自身**的零信号（已修）

**M1 第一次跑时 `test_bundle_ecosystem` 全绿** ⇒ 该文件根本没测到自由运算符。

根因：自由 `operator<<=` **只在声明可见的 TU 里**参与重载决议 ⇒
`tests/test_bundle_ecosystem.cpp` 当时**没有** include `chlib/stream_operators.h`
⇒ `sink <<= source` 落到了**成员** `bundle_base::operator<<=`（逐字段连接，
本来就是数据流边）⇒ 改自由运算符对它**毫无影响**。

⇒ 已修：补 include + 在文件头写明「不 include 它 ⇒ 本文件的『自由运算符』
用例就测不到自由运算符」。

### ⚠️ 顺带查明的事实（**与提案的判断相反**）

提案称 D6 的爆炸半径覆盖「**所有**嵌套 stream 的 bundle 连接」。实测：

```
grep -rn "stream_operators.h" --include=*.h --include=*.cpp include/ tests/ examples/ samples/
  → include/chlib.h（聚合）
  → tests/test_stream_operators.cpp
  → tests/test_stream_connect_dual.cpp（本次新增）
```

⇒ 自由运算符**只在** include 了 `chlib/stream_operators.h`（或 `chlib.h`）的 TU 里
可见；全仓**唯一**的真实消费方是 `tests/test_stream_operators.cpp`
（`samples/bundle_top_example.cpp` 等走的是**成员** `bundle_base::operator<<=`）。

⇒ 由此得出一个**此前未被记录**的缺陷：改前**同一份源码**因 include 不同而走
**两套连接语义**（自由函数 = 节点重绑定 / 成员 = 数据流边）⇒ D6 的收敛恰好
**消除**了这个不一致。

---

## 3. 双后端 parity

| 项 | 结果 |
|---|---|
| `tests/test_stream_connect_dual.cpp`（2 个 `DUAL_BACKEND_TEST_PAIR` = 4 个 TEST_CASE） | ✅ **12 断言全过**（interpreter + **verilator** 两半都跑通，verilator 命中缓存） |

⚠️ 该文件覆盖的是**自由 `operator<<=` 连接路径**—— 它是仓库里唯一走该运算符的
路径，此前**零 parity 覆盖**（`tests/test_ch_stream.cpp` 的 2 个双后端用例覆盖的是
「裸端口的数据流」）。

⚠️ 该文件是**parity** 护栏（两后端一致），**不是**「语义区分」护栏 ——
区分性断言在 `test_bundle_ecosystem`（M1/M4）。

---

## 4. 全量回归

| 项 | 结果 |
|---|---|
| `ctest -LE perf` | ✅ **173/173 全绿**（新增 2 个 target：182 → 184） |
| `ctest -L perf` | 见下方「perf 回归」 |
| `./run_all_ported_tests.sh` | ✅ **28/28** |
| `bash scripts/run_all_samples.sh` | ✅ **26/26**（`nested_bundle_demo` 的豁免**已摘除** —— 护栏先报 **STALE** 再被摘除，正是该机制的设计目的） |
| `openspec validate --specs --strict` | ✅ **18/18**（CLI 1.4.1） |
| `bash scripts/verify_test_counts.sh` | ✅ 通过（`CTEST_TOTAL: 184`） |

### samples 护栏的豁免摘除（跨 change 耦合）

`samples-regression-guard-2026-10` 落地时为 `nested_bundle_demo` 建了**显式豁免**
（失效条件 = 本 change archive）。本 change 修好根因后：

1. 护栏**先**报 `STALE EXEMPTION (已转绿)` 并 **exit=1** ⇒ 证明 stale 机制有效；
2. 摘除 `KNOWN_FAILING` 条目；
3. 护栏恢复 **26/26 全绿**。

⇒ 这是两个 change 的**串行依赖**被真正走通的证据。

---

## 5. 计数与文档同步

| 项 | 变更 |
|---|---|
| `CTEST_TOTAL` | 182 → **184**（+`test_bundle_ecosystem` +`test_stream_connect_dual`） |
| non-perf CTest | 171 → **173** |
| `tests/test_*.cpp` | 112 → **114** |
| `scripts/verify_test_counts.sh` | ✅ 仍绿 |
| 根 `AGENTS.md` | 「已消除的过时信息」新增本 change 条目；**顺带订正** `bundle-master-slave-io-2026-10` 条目里「5 用例 / 19 断言」的**计数漂移**（实际 **8 用例 / 30 断言**） |
| `docs/adr/ADR-043-bundle-operations-removal.md` | **新增**（D2 的移除决策） |
| `docs/adr/ADR-012-io-port-dual-api.md` | 同步 `internal_direction` 的删除 |
| `docs/adr/ADR-042-bundle-field-first-class-nodes.md` | 同步「遗留债务」段（本 change **就是**那个 follow-up） |
| `tests/test_bundle_literal.cpp` | 「all 22 derived classes」注释 → 20 |
| `include/simulator.h` | `get/set_bundle_value` 的**适用范围**（D9） |
| `include/core/bundle/bundle_base.h` | prefix 三步 + `flip()` + `operator=` 语义 + 公开派生类清单 22 → 20 |

---

## 6. 遗留（转交 follow-up）

- **L10** FIFO 的读数据通路（`rd_data` 字段，公开 API 布局变更）—— 用户已裁决独立立项。
- 提案的「爆炸半径」判断被实测修正（见 §2）⇒ **提案文本里的该判断已被本记录更正**，
  但提案本身是历史文档，不回改。
