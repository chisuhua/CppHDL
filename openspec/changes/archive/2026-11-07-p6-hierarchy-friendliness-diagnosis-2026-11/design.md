# Design: 层次友好度诊断 API（`p6-hierarchy-friendliness-diagnosis-2026-11`）

> 本文是 **builder 域**的定稿设计（`design.md` 无 skill owns）。推导与证据在
> `proposal.md` 附录 A；可勾选判据在 `tasks.md`。
> ⚠️ 本 change **新增公开 API 面**（`include/hierarchy_analysis.h`）⇒ ⇒ 归档前须过
> **critic**（触及公开 API ⇒ 2 轮预算）。

## 1. API 形态（落在 `include/hierarchy_analysis.h` · **不新增公共头**）

```cpp
namespace ch {

/// 层次友好度的**三档**结论（★ 不是 5 档 —— 5 大项里 4 项是**正交事实**，
/// 谁更糟**没有客观依据** ⇒ ⇒ 5 档是**伪全序** ⇒ ⇒ 收敛为 3 档）
enum class HierarchyFriendliness {
    kClean = 0,          ///< 无隐式边 / 无悬空输入 / 无未消费子输出 / 无跨模块 mem
    kNeedsWork = 1,      ///< 至少一项「有代价的发现」
    kNotAnalyzable = 2,  ///< 分析层拒绝 ⇒ ⇒ 报告仍返回（**不抛**）
};

/// 一层 module 的规模（★ 三个数**分开给** ⇒ pass-through 层 body=0 但端口很多
/// ⇒ ⇒ 合并成单一 "scale" **必误导**）
struct HierarchyModuleSize {
    std::string module;
    std::size_t port_count = 0;
    std::size_t instance_count = 0;
    std::size_t body_count = 0;
};

/// 一条诊断明细（★ **value-only** ⇒ 不带 `lnodeimpl*` ⇒ 报告不耦合 IR 生命周期）
struct HierarchyDiagnosisDetail {
    std::string category;   ///< "implicit_edge" / "dangling_input" /
                            ///< "unconsumed_child_output" / "cross_module_mem" /
                            ///< "not_analyzable"
    std::string module;     ///< 所属 module（空 = 根 / 全局）
    std::string node;       ///< ★ **Verilog 名**（★ 与产物同一套名字 ⇒ 用户能直接在
                            ///< 产物里定位；★ `lnodeimpl::name()` 是 `.` 分隔的**组件
                            ///< 路径** ⇒ 拿它搜产物搜不到）
    std::string reason;     ///< 人类可读成因（★ 各隐式边 reason 不同 ⇒ 用户才知道
                            ///< 该「加一个端口」还是「加一条 N 跳链」）
};

/// 诊断报告（★ **value-only**）
struct HierarchyDiagnosis {
    HierarchyFriendliness verdict = HierarchyFriendliness::kClean;

    // ── 5 大项（★ 全部 `std::optional` ⇒ 「未测得」与「实测为 0」
    //    **结构上可区分** ⇒ ⇒ 不可分析时 MUST NOT 全是 0）
    std::optional<std::size_t> implicit_edge_count;
    std::optional<std::size_t> synthesized_port_count;
    std::optional<std::size_t> dangling_input_count;
    std::optional<std::size_t> unconsumed_child_output_count;
    std::optional<std::size_t> cross_module_mem_count;

    std::size_t module_count = 0;
    std::vector<HierarchyModuleSize> module_sizes;
    std::vector<HierarchyDiagnosisDetail> details;

    /// **确定性快照**（★ 排序固定 ⇒ 两次调用逐字节相同 ⇒ 「字段只增不改」这条
    /// 兼容承诺**可机械核验** ⇒ ★ 判据来源 = delta Requirement 3）
    std::string to_text() const;
};

/// **只读**诊断入口（★ 唯一判定实现）
HierarchyDiagnosis diagnose_hierarchy(Component& root);

}  // namespace ch
```

⚠️ **`hierarchy_analysis.h` 不在 `ch.hpp` 聚合器里** ⇒ 文档必须给显式 include。

⚠️ **`std::optional` ⇒ 需 `#include <optional>`**（C++17；仓用 C++23 ✓）。

## 2. 报告项的推导口径（**每项必须可机械判定**）

| 项 | 推导 | 精度边界 |
|---|---|---|
| `implicit_edge_count` | `analysis.pending_implicit_edges.size()` | ✅ **精确**（分析层直出） |
| `synthesized_port_count` | 隐式边按 **(子侧 module key, 子侧节点)** 去重后的数量（★ 与发射器**共用**去重 key 计算 ⇒ §4） | ✅ **精确**（与发射器的隐式合成端口**逐项相等**）<br>⚠️ **不含向上 pass-through（`__up_`）** —— 它的端口数 = 链长 ⇒ **不可由边数推导** ⇒ ⇒ MUST 在文档写明 |
| `dangling_input_count` | `analysis.bubbled_inputs` 中 `b.module != root_key` 的部分（★ 判法同 `src/hierarchy_verilog.cpp:146-150`） | ✅ 精确 |
| `unconsumed_child_output_count` | §2.1 的并集推导 | ✅ 精确（口径见下） |
| `cross_module_mem_count` | 正常路径 = `0`（★ 能返回就必为 0：`mem` 的读写端口都把 mem 加为 `src` ⇒ 跨模块 mem **必产生**跨模块边 ⇒ 分析层**必抛**）；不可分析路径 = **独立预扫描**的真实计数（§2.2） | ⚠️ **粒度相关**（`ForceInline` 下折进同层 ⇒ 0）⇒ MUST 写进文档 |

### 2.1 `unconsumed_child_output_count` 的口径（四条排除规则）

对每个非根 module `m`、每个 `!p.is_input` 且 `origin ∈ {Self, InlineFolded}` 的端口 `p`：

1. `edges` 有 `e.from_node == p.name && e.from_module == m.key` ⇒ **已消费**
2. `pending_implicit_edges` 有 `pe.from_node == p.name && pe.from_module == m.key`
   ⇒ **已消费**
   （⚠️ **必须并集**：兄弟/非相邻边在 `src/hierarchy_analysis.cpp:482-504`、隐式边在
   `:519-553` 都是 `continue` **在** `edges.push_back` **之前** ⇒ ⇒ 只看 `edges`
   一定把「被兄弟层消费」误判成未消费）
3. 排除 `into_child == true` 的连接（父驱动子 ⇒ 不是「消费子输出」）
4. **按 `(instance.module_key, instance.instance_name, child_port)` 判**，**不是**按
   `module_key`（同一 module 多实例时，按 module 去重会把「只有一个实例消费」算成
   「已消费」）

⇒ ⇒ **MUST NOT** 重新遍历 IR/ctx（= 第二份判定 ⇒ 与发射器漂移）。

⚠️ **实施期订正**：v1 草稿写「`node->name()` 恒为 `unnamed_input`」⇒ **错**
（那是 `port::name()`）⇒ ⇒ `lnodeimpl::name()` = **组件路径 + 局部名**
（`src/core/lnodeimpl.cpp:32`）⇒ ⇒ 两个不同组件的 input **本就不同名**。
⇒ ⇒ 「用 Verilog 名」的理由改为「**与产物同一套名字** ⇒ 用户能在产物里定位」。

### 2.2 跨模块 `mem` 的独立预扫描（A1 · 用户已拍板）

正常路径下该项必为 0 ⇒ ⇒ 「有跨模块 mem」这个事实**只存在于异常里** ⇒ ⇒ 不可分析时
由**一次独立预扫描**报出真实计数：

- 遍历 ctx 的节点 ⇒ 找 `mem_port_impl`（读/写端口）⇒ 取其 `parent()`（= mem 节点）
- 用 `analysis.module_of_node` 比对 mem 节点与 mem 端口的**归属 module**
- 不同 ⇒ **跨模块访问** ⇒ 计数 +1，明细记 module 与 Verilog 名

⚠️ **代价（已接受）**：多一处 `src/**` 内实现依赖（`ast/mem_port_impl.h`）。
⚠️ **失效形态**：`mem_port_impl` 若无 `parent()` ⇒ 不属跨模块 ⇒ 须在代码注释写明。

## 3. 异常处理与「不可分析」

### 3.1 只捕 `std::runtime_error`

- 分析层的 **15 个抛点全部**是 `CHFATAL_EXCEPTION`（`include/utils/logger.h:235-244`）
  ⇒ ⇒ 全部抛 `std::runtime_error` ⇒ ⇒ 捕它即覆盖全部「用户设计问题」。
- ⚠️ **不捕** `std::out_of_range`（`module_index.at()`）与 `std::bad_alloc`
  ⇒ ⇒ **框架 bug 不会被伪装成「你的设计不可分析」** ⇒ ⇒ 必须**重抛**。

### 3.2 副作用（MUST 写进文档）

| 副作用 | 实测依据 | 处置 |
|---|---|---|
| **stderr 出现 `FATAL:`** | `CHFATAL_EXCEPTION` **先 `log_message(fatal)` 再 throw** | ⇒ 选 **(a) 文档写明**（**不**改宏 ⇒ 零回归 · 零范围扩张）⇒ ⇒ 「给宏加静默开关」记 backlog |
| **静态析构期不抛** | 宏的 `in_static_destruction()` 守卫 | ⇒ ⇒ 该期可能得到**部分结果** ⇒ MUST 写进文档 |

## 4. ★ 与发射器**同源**：共享去重口径（`src/` 内部头 · **零公开面扩张**）

⚠️ **问题**：`synthesized_port_count` MUST 与发射产物的隐式合成端口数**相等**
⇒ ⇒ 若诊断**自己**复刻「哪个端点是子侧」「行 1 是否可合成」⇒ ⇒ **第二份判定**
⇒ ⇒ 与发射器漂移 ⇒ ⇒ 违反 delta Requirement 3。

⇒ ⇒ **解法**：新增**内部头** `src/hierarchy_synth_count.h`（★ 仓内已有同款先例：
`src/hierarchy_chain.h` · `src/hierarchy_port_check.h` ·
`src/hierarchy_port_set_check.h` · `src/hierarchy_signal_map_check.h`
⇒ ⇒ **不**进 `include/` ⇒ ⇒ **公开 API 面零扩张**），提供**两个共享原语**：

| 原语 | 内容 | 消费者 |
|---|---|---|
| `synth_port_key(edge)` | 隐式边的**合成端口去重 key** = **(子侧 module key, 子侧节点指针)**<br>（★ 行 2 `child_drives_parent==true` ⇒ 子侧 = `(from_module, from_ptr)`；<br>★ 行 1 ⇒ 子侧 = `(to_module, to_ptr)`） | 发射器（建端口时取 key）· 诊断（算去重数） |
| `implicit_input_synthesizable(edge, analysis)` | 行 1 的**可合成性**判定（★ 子侧端点是 `type_reg` / `type_proxy` 且驱动端在父 module 内） | 发射器（决定是否合成）· 诊断（决定该边是否计入） |

⇒ ⇒ 两侧**共用同一份 key 计算与可合成性判定** ⇒ ⇒ 端口数**必然相等**
⇒ ⇒ 满足 delta Requirement 3「与发射器同源」✓

⚠️ **发射器的改动面**：把 `src/hierarchy_verilog.cpp:683-693`（行 2）与 `:729-741`
（行 1）的 key 取法与可合成性判定**换成调用共享原语** ⇒ ⇒ **行为逐字不变**
（★ 判据 = 既有 3 个产物的**逐字节相同**）。

## 5. 确定性（`to_text()`）

- 明细排序：`(category, module, node, reason)` 的**字典序** ⇒ ⇒ 不依赖
  `module_of_node`（`unordered_map`）的遍历序。
- `module_sizes` 排序：按 `module` 字典序。
- 文本形态：每行 `key=value`，末尾一个换行 ⇒ ⇒ 两次调用**逐字节相同**。

## 6. 逐文件改动清单

| 文件 | 改动 | 性质 |
|---|---|---|
| `include/hierarchy_analysis.h` | + `HierarchyFriendliness` · + `HierarchyModuleSize` · + `HierarchyDiagnosisDetail` · + `HierarchyDiagnosis`（含 `to_text()` 声明）· + `diagnose_hierarchy()` 声明 · + `#include <optional>` | **公开 API 面新增**（同文件 ⇒ 不新增头） |
| `src/hierarchy_synth_count.h`（**新**） | 共享去重 key + 行 1 可合成性判定 | **内部头**（不进 `include/`） |
| `src/hierarchy_analysis.cpp` | + `diagnose_hierarchy()` 实现 · + `to_text()` 实现 | 实现 |
| `src/hierarchy_verilog.cpp` | 行 2 / 行 1 的 key 与判定改为**调用共享原语**（★ 行为不变） | **最小侵入**（⇒ 既有 3 产物逐字节不变 = 判据） |
| `tests/test_hierarchy_friendliness_diagnosis.cpp`（**新**） | 判据用例 | 测试 |
| `tests/CMakeLists.txt` | +1 个 `add_catch_test` | 登记 |
| `AGENTS.md` | 计数锚点（`CTEST_TOTAL` +1 · non-perf +1 · `tests/test_*.cpp` +1） | 锚点 |
| `docs/usage_guide/05-component-design.md` | 新增「层次友好度诊断」小节 | 文档 |

⇒ ⇒ **零** `include/` 之外的新公共头 ⇒ ⇒ `verify_hierarchical_p8.sh` 的 R4
（窗口钉死在 T3-005）**不受影响**（本次改 `include/hierarchy_analysis.h` 不在 T3-005
窗口内 ⇒ 不触发）。

## 7. 明确不做

- ✗ 不改发射器的**行为**（只把判定换成共享调用 ⇒ 产物逐字节不变）
- ✗ 不给 `CHFATAL_EXCEPTION` 加静默开关（记 backlog）
- ✗ 不解决「前置校验首错即抛」（文档写明边界）
- ✗ 不覆盖「子 `ch_out` 有端口但体内无驱动」（文档写明不覆盖）
- ✗ 不解决「公开 API 面登记」机制（记 backlog）
- ✗ 不新增 ADR
