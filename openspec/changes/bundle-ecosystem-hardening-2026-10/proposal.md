# Change: bundle-ecosystem-hardening-2026-10 — bundle 生态边角设施的语义契约化

## Why

`bundle-master-slave-io-2026-10`（ADR-042）把 bundle 字段改成**一等节点**、连接改成
**数据流边**，并给 `mem-read-timing` / `dual-backend-test-coverage` 等能力补了护栏。
但那次改动**只覆盖了「单个 bundle、叶子字段、master↔slave 直连」这条主路径**；
bundle 生态的**边角设施**（嵌套 bundle / `flip()` / 工厂函数 / 序列化 / `ch_stream`）
**既无契约、也无断言**，且其中若干处在 ADR-042 之后**语义已变或已失效**。

本 change 是对「`bundle-master-slave-io-2026-10` 之后全仓遗留与遗漏」的一次
系统审计（2026-10-22）的收敛。

### 审计结论：10 项遗留 / 遗漏（逐项有取证）

| # | 遗留 / 遗漏 | 取证 | 严重度 |
|---|---|---|---|
| **L1** | **`bundle_base(const std::string &prefix)` 在「基类构造期」调用虚函数 `set_name_prefix()`** ⇒ 此时派生类成员（嵌套 bundle 字段）**尚未构造** ⇒ 对未构造对象调虚函数 = **UB / 段错误** | gdb 精确帧：`bundle_traits.h:135` ← `bundle_base.h:34`；`samples/nested_bundle_demo.cpp` **segfault**（基线同样崩 ⇒ 既有缺陷） | 🔴 高 |
| **L2** | `bundle_slice_view` / `bundle_concat`（`bundle_operations.h:14,58`）**全仓零使用**（连测试都没有）⇒ ADR-042 后它们的字段与原 bundle **断连** ⇒ **静默失效** | grep：除定义处外**零引用** | 🟡 中 |
| **L3** | `deserialize_bits_to_fields()`（`bundle_serialization.h:85-97`）**全仓零使用** ⇒ 仍用 `bit_select`/`bits<>` **切片重绑定**字段 ⇒ 与「字段一等节点」原则冲突 | grep：除定义处外**零引用** | 🟡 中 |
| **L4** | `bundle_base::operator=`（`bundle_base.h:170-175`）的**「软件拷贝 / 节点重绑定」语义**只被 `tests/test_bundle_connection.cpp` 的**注释**钉住 ⇒ 无 spec / ADR ⇒ 下次「统一连接语义」的重构会误删它 | 断言与注释的对照 | 🟡 中 |
| **L5** | `master()` / `slave()` 工厂（`bundle_utils.h:10-17`）**按值拷贝**穿过 `operator=` ⇒ 副本与原件**共享字段节点**，但方向被重新贴标签 ⇒ 语义无任何契约 | 3 个测试文件在用，但无契约 | 🟡 中 |
| **L6** | `flip()`（`bundle_base.h:181-188`）**硬编码 `as_slave()`**（不记忆原方向）且返回**全新未连接** bundle ⇒ 语义无契约 | `include/chlib/stream_pipeline.h` + `samples/nested_bundle_demo.cpp` + 4 个测试在用 | 🟡 中 |
| **L7** | **`ch_stream` 仍走「重绑定」连接语义**（`include/chlib/stream_operators.h:25-33` 的**自由** `operator<<=` 在重载决议中胜出，内部是字段级 `=`）⇒ 框架内**两套连接语义并存** | 源码 + 重载决议 | 🟡 中 |
| **L8** | `get/set_bundle_value` 在「顶层不用 bundle」之后**接近死 API** ⇒ 去留未决 | ADR-042 Decision 5 | 🟢 低 |
| **L9** | `internal_direction`（`include/core/direction.h:10`）+ `is_internal_v` = **死代码**（全仓零使用） | grep：除定义处外**零引用** | 🟢 低 |
| **L10** | FIFO 的**读数据通路**（`rd_data` 字段，公开 API 布局变更） | 用户已裁决**不在本 change 处理** ⇒ 独立 follow-up | ⛔ 排除 |

### 测试完备性审计：5 项缺口

| # | 缺口 | 证据 |
|---|---|---|
| **T1** | **40 个 sample 零自动化覆盖**（0 个在 ctest、0 个在 `run_all_ported_tests.sh`）⇒ 坏了没人知道 | `samples/*.cpp` = 40；`grep -c 'samples/' tests/CMakeLists.txt` = 0（仅注释）；`run_all_ported_tests.sh` = 0 |
| **T2** | `samples/switch_demo.cpp` **既有 segfault**：`make_uint<8>(0)` 在**无 `ctx_swap`** 的情况下创建节点 ⇒ 节点创建失败 ⇒ 崩溃 | 基线同样崩；日志「No active context for literal creation」 |
| **T3** | bundle 能力的**覆盖缺口**：嵌套 bundle 字段 / `flip()` / 工厂函数 / `deserialize` 路径 / `ch_stream` 的连接语义 —— **零断言** | `tests/test_bundle_master_slave.cpp` 的 8 个用例只覆盖叶子字段 + 直连 |
| **T4** | `ch_stream` **不在双后端矩阵**里（`test_verilator_stream_e2e` 测的是 handshake，不是 bundle 连接的 parity） | grep：`dual_backend_*` / `test_dual_backend_*` 里无 `ch_stream` |
| **T5** | `bundle-master-slave-io` 能力的 6 条 Requirement | ✅ **完备**（23 条 Requirement→测试 映射全覆盖，8 项变异验证全部被抓到） |

⇒ **T1/T2 归独立的 `samples-regression-guard-2026-10`**（覆盖面是 40 个程序，
不只 bundle）；本 change 收敛 **L1-L9 + T3/T4**。

## What Changes

### D1: bundle 的 prefix 命名 MUST NOT 在基类构造期触碰派生类成员（修 L1）

`bundle_base(const std::string &prefix)`（`bundle_base.h:32-35`）MUST NOT 调用
**虚函数** `set_name_prefix()` —— 基类构造期间派生类成员**尚未构造**
（`CustomNested` 的 `ch_stream` 成员还是未初始化内存）⇒ 对它调虚函数 = UB。

**裁决**：把「命名」**推迟到字段节点建立的那一刻**，即 `set_role()`（`as_master()` /
`as_slave()`）—— 那时派生类成员已构造、字段节点正要建立 ⇒ **命名与建节点同时做**。

⇒ 附带效果：ADR-042 里「字段节点创建 MUST 幂等」的约束**保持不变**（命名同样幂等）。

### D2: 嵌套 bundle 字段的连接语义 MUST 有明确契约（修 L2 / L3）

`bundle_concat` / `bundle_slice_view` / `deserialize_bits_to_fields()` 三者都
**依赖「字段由总线切片派生」**这一**已被 ADR-042 取代**的模型。

**裁决**（逐个明确，不留「看起来能用」的状态）：

| 设施 | 裁决 |
|---|---|
| `bundle_concat` / `bundle_slice_view` | **实现正确语义**：拷贝后**逐字段 `<<=` 连接**（与 `bundle_base::operator<<=` 的方向感知逻辑一致）⇒ 副本真正「连着」原件。若无法在本次达成，则 MUST **标记 deprecated** 并从公开派生类清单中移除（MUST NOT 留一个静默失效的公开类型） |
| `deserialize_bits_to_fields()` | 改为**字段一等节点**（与 ADR-042 一致）⇒ 序列化路径不再用切片重绑定 |

### D3: `flip()` 的方向语义 MUST 明确（修 L6）

现状：硬编码 `as_slave()` ⇒ `master.flip()` 得到 slave ✓，但 `slave.flip()` 也得到
slave（**不记忆原方向**）。

**裁决**：`flip()` MUST 返回**方向取反**的副本（master→slave，slave→master），
且 MUST NOT 硬编码 `as_slave()`。

### D4: `master()` / `slave()` 工厂的语义 MUST 明确（修 L5）

现状：按值拷贝 ⇒ 副本与原件**共享字段节点**，方向却被重新贴标签。

**裁决**：工厂 MUST 产出**独立**的 bundle（字段节点独立），方向按 master/slave 设定；
MUST NOT 与原件共享字段节点。

### D5: `bundle_base::operator=` 的「软件拷贝」语义 MUST 进 spec（修 L4）

`operator=` 保留为**显式的软件拷贝 / 节点重绑定**语义（ADR-042 的遗留债务 #1），
MUST 在 capability 里写明（当前只有测试注释钉住 ⇒ 下次重构会误删）。

### D6: `ch_stream` 的连接语义 MUST 收敛或写明 + 补双后端覆盖（修 L7 / T4）

`ch_stream` 的自由 `operator<<=` 内部用**字段级 `=`（重绑定）** ⇒ 与其他 bundle 的
**数据流边**语义不一致 ⇒ 框架内两套语义并存。

**裁决**：MUST 收敛到**数据流边**（与 ADR-042 一致）；若因 `ch_stream` 的
「ready 反向流动」特性无法直接收敛，则 MUST **显式写明差异**并补**双后端 parity 覆盖**
（`ch_stream` 进双后端矩阵）。

### D7: bundle 生态边角 MUST 有可执行断言（修 T3）

新增/扩展 Catch2 覆盖：**嵌套 bundle 字段**的连接、**`flip()`** 的方向语义、
**工厂函数**的节点独立性、**`deserialize`** 路径、**`ch_stream`** 的连接语义。

### D8: 死代码清理（修 L9）

`internal_direction` + `is_internal_v` 全仓零使用 ⇒ 删除（ZERO-DEBT POLICY：
「No dead code」）。

### D9: `get/set_bundle_value` 的去留 MUST 显式裁决（修 L8）

ADR-042 Decision 5 之后，TB 侧改用普通端口 ⇒ 打包式 API 只对**未连接** bundle 有用。
**裁决**：保留（调试用途）+ 在 spec / 文档中写明其**适用范围**（未连接 bundle）。

## Impact

| 项 | 影响 |
|---|---|
| `include/core/bundle/bundle_base.h` | prefix 命名推迟（D1）；`flip()` 方向取反（D3）；`operator=` 语义注释（D5） |
| `include/core/bundle/bundle_operations.h` | `bundle_concat` / `bundle_slice_view` 的语义实现或废弃（D2） |
| `include/core/bundle/bundle_serialization.h` | `deserialize_bits_to_fields()` 改字段一等节点（D2） |
| `include/core/bundle/bundle_utils.h` | `master()` / `slave()` 工厂产出独立 bundle（D4） |
| `include/chlib/stream_operators.h` | `ch_stream` 的连接语义收敛（D6） |
| `include/core/direction.h` | 删 `internal_direction` + `is_internal_v`（D8） |
| `openspec/specs/bundle-master-slave-io/spec.md` | MODIFIED：边角设施的契约（D2-D6, D9） |
| `tests/test_bundle_master_slave.cpp` | 扩展：嵌套 / flip / 工厂 / deserialize / ch_stream 的断言（D7） |
| `samples/nested_bundle_demo.cpp` | **不再 segfault**（D1 的直接效果） |
| 风险 | ⚠️ **中**。① D1 改的是**基类构造期**的行为（影响所有 bundle 的命名时机）⇒ MUST 全量回归；② D2 若选「废弃」则影响**公开 API**（`bundle-base-integer-ctor` capability 的 22 个派生类清单）⇒ MUST 走 ADR；③ D6 收敛 `ch_stream` 的连接语义可能影响 **stream / AXI 子系统**（`include/chlib/stream_pipeline.h` 等）⇒ MUST 跑双后端矩阵；④ D7/D8 是纯增量 |

## Non-Goals

1. **FIFO 的读数据通路**（L10）—— 用户已裁决不在本 change 处理 ⇒ 独立 follow-up
2. **不**改 `bundle-master-slave-io` 已确立的**主路径**契约（字段一等节点 / 数据流边 /
   方向感知 connect）—— 本 change 只补**边角**
3. **不**把 samples 纳入 CI（T1/T2）—— 归独立的 `samples-regression-guard-2026-10`
4. **不**引入 bundle 的新能力（协议层 bundle / 打包 API 的新形态）

## 附录 A：审计的复现命令

```bash
# L1（精确根因）
g++ -std=c++23 -g -O0 -Iinclude ... samples/nested_bundle_demo.cpp build/libcpphdl.a \
    -L/usr/lib/llvm-18/lib -lLLVM-18 -o /tmp/nbd
gdb -batch -ex run -ex "bt 15" --args /tmp/nbd
#   ⇒ #0 bundle_field_traits<ch_stream>::set_name at bundle_traits.h:135
#      #8 bundle_base::bundle_base at bundle_base.h:34   ← 基类构造期调虚函数

# L2 / L3 / L9（零使用）
grep -rn "bundle_slice_view\|bundle_concat" --include=*.cpp --include=*.h . | grep -v third_party
grep -rn "deserialize_bits_to_fields" --include=*.cpp --include=*.h . | grep -v third_party
grep -rn "internal_direction\|is_internal_v" --include=*.cpp --include=*.h . | grep -v third_party

# T1（samples 零覆盖）
ls samples/*.cpp | wc -l                        # 40
grep -c 'samples/' tests/CMakeLists.txt         # 0（仅注释）
grep -c 'samples/' run_all_ported_tests.sh      # 0

# T2
./build/samples/switch_demo                     # segfault（基线同样）
#   日志：「No active context for literal creation」⇒ 缺 ctx_swap
```
