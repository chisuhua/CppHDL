# 审稿记录：`p8-hierarchical-equivalence-public-helper-2026-11`（P8 · 公共 API + 护栏改口径）

- **日期**：2026-11-07
- **审查者**：critic（profile `build` + 公开 API 面按 `arch` 档的公开契约强度）
- **轮次**：**1 / 2**（预算未用尽 ⇒ 无 BLOCKER ⇒ 不需复审）
- **最终结论**：✅ **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false`
- **统计**：**0 BLOCKER** · **2 SHOULD_FIX** · **4 NIT**

## 1. 三条 focus 的逐条判定（critic 原文要点）

### Focus 1 · 公共面契约是否自洽且最小

| 子项 | 判定 | 依据 |
|---|---|---|
| **1(a)** 「闭合集合 = 7 个符号」 | ✅ **成立** | `make_observation` 是头文件内模板 ⇒ 用户可见/可 ODR 使用 ⇒ **计入白名单是正确的**；枚举器对**当前**头的枚举是准确的（4 struct + 2 函数 + 1 模板，无 `using`/宏/`constexpr` 变量/成员函数） |
| **1(b)** `flat_trace` 改名 + 取消 `Trace` 别名 | ✅ **语义被遵守** | `src/hierarchy_equivalence_impl.h:300` 的 `row.push_back(vf)` **只存扁平侧**（`vh` 只用于比较、不落盘）⇒ 改名是语义保持的；旧 `Trace` 别名**无仓内残留消费者** |
| **1(c)** 用 `ports`（而非 `signal_map`）判可见性 | ✅ **等价且唯一正确** | `backend_port_set` 的文档契约（`include/hierarchy_verilog.h:131-141`）明确「只取根 module 端口」⇒ 「`node_id` ∈ `ports`」⇔「层次化顶层可见」；且 `compare` 允许 `hier_path` 指向**被篡改副本** ⇒ ⇒ 用 `ports` 是**唯一正确**选择（映射表对应的是**原始**产物） |

### Focus 2 · 护栏改口径是否真的「强度不减」

| 子项 | 判定 |
|---|---|
| **R1 是超集** | ✅ 对匹配 `*equivalence*`/`*hier_eq*` 的文件名，新判据 ⊇ 旧判据；唯一放松点 = 豁免文件本身（**意图内**） |
| **改名绕法被联合覆盖** | ✅ `hierarchy_api.h` 类 ⇒ R1 不匹配 ⇒ 但 **R2'（namespace 内容维度）** 兜住；不同名新头 ⇒ **R4②（活账）** 兜住；顶层头维度**无洞** |
| **R4② 子目录不在扫描范围** | ⚠️ **接受为边界** ⇒ critic 判定：该边界对**本 capability**（只管 helper 的公共面）**自洽**；`public-api-surface.md:25-28` 的理由（可审性 vs 167 行清单）**站得住**；且这是 P6「公开 API 面登记机制」的**第一版**而非全仓治理 ⇒ ⇒ 残余风险已在文档**明示** |
| **R2 双向核对值得** | ✅ 把「grep 无命中 ⇒ 绿」的**静默失效**变成可行动的维护信号；代价（合法重命名须同步改表）**正是该机制的意图** |
| **7 条投毒真的证明判别力** | ✅ 投毒② 专门覆盖「∀ 否定在**找不到匹配文件**时**真空通过**」这个**已实测的真缺口**；`judge()` 聚合判定的修法**是对的** |

### Focus 3 · 零回归的真实强度

- ✅ **近结构性证据**（不只是「跑一遍没坏」）：critic **逐段 diff 了 HEAD 旧 harness 与新 impl** ⇒ tick/比对主循环**逐行相同**，差异**仅限**：① 错误串加前缀 ② `trace`→`flat_trace` 改名 ③ 新增的 fail-closed 早退校验 ⇒ ⇒ alias 层是纯 `using` **编译期转发**（运行路径**无**间接层）⇒ ⇒ 新增校验对既有 9 用例**不发射**（它们的观测面非空且随激励变化）⇒ ⇒ 加上 3 个产物**逐字节不变** ⇒ ⇒ 「零回归」主张**可信**。
- ✅ **`no-varying-signal` 误报风险可接受**：critic 判定它是 **spec 明文要求的行为**（Requirement ② Scenario「观测集合不得静默退化」）⇒ ⇒ helper 拒绝是**契约一致**而非缺陷 ⇒ ⇒ 且判据有**逃生口**（单拍激励豁免）+ 错误信息可行动 ⇒ ⇒ 合法的「恒定输出」场景会被拒但**可接受**。

## 2. SHOULD_FIX（**2 条 · 已全部修复**）

| # | critic 的发现 | 判定 | 处置 |
|---|---|---|---|
| **SF1** | `tests/test_hierarchy_verilog.cpp` 的 `kOutDir` 修复与其注释**声明不匹配**：注释声称「判据自足」，但实现是**单层** `mkdir("/tmp/opencode/hieremit")` ⇒ ⇒ 本机 `/tmp/opencode` 预存在所以全绿，而**全新 runner 上 `/tmp/opencode` 不存在时 mkdir 仍失败** | ✅ **成立**（★ 这是我自己 fix 得**不彻底**，正是「静默留存」形态） | ✅ **已修**：改为**逐级建**（`/tmp` → `/tmp/opencode` → 目标）⇒ ⇒ 已存在时 `EEXIST` 无害 ⇒ ⇒ **隔离实测**：深路径 `/tmp/opencode_mkdirtest/a/b` 创建成功（`deep dir created = YES`） |
| **SF2** | 文档 §5.4-ter 示例**照抄运行会在 ① 步失败**：`work_dir = "/tmp/my_hier_eq"` **从未被创建**，而 `emit` **不建** `work_dir`（impl 只建 `work_dir/flat`、`work_dir/hier` 两个子目录） | ✅ **成立**（对**新 API 的第一条上手路径**而言是硬伤） | ✅ **已修**：示例补 `::mkdir(work_dir.c_str(), 0755)` + `<sys/stat.h>` + 一行说明「`work_dir` MUST 由调用方创建」⇒ ⇒ 重新编译 **0 错误** |

## 3. NIT（4 条 · 已全部处置）

| # | critic 的发现 | 处置 |
|---|---|---|
| **NIT1** | deny 条目 `ch_test::hier::impl` 已是「**历史名**」（其「仍存在」核对靠 `tests/hier_equivalence.h:6` 的**注释字符串**满足）⇒ ⇒ 条目性质已从「活符号」变为「历史名」 | ✅ **已在脚本里注明**：「防旧命名空间**复辟**的 tripwire ⇒ ⇒ **不是**活符号」 |
| **NIT2** | 符号枚举器的**形态覆盖有已知边界**（struct 内成员函数 / `using` / `typedef` / 宏 / 命名空间级 `constexpr` 变量均不可见） | ✅ **已写进脚本头注释**：它是 **tripwire**，**不是**完备门 ⇒ 完备性仍由「新增符号 MUST 改 spec」这条**流程义务**承担 |
| **NIT3** | R4② 的失败措辞「公开 API 面静默扩张」**略大于实际口径**（扫描范围 = 顶层头 + 2 个 core 头） | ✅ **已改为**「**扫描范围内的**公开 API 面静默扩张」 |
| **NIT4** | `no-varying-signal` 的**单拍激励豁免**只写在 impl 注释 ⇒ ⇒ **公共头用户看不到** | ✅ **公共头已补**：「`no-varying-signal` 只在激励 ≥ 2 拍时判 ⇒ 单拍**豁免** ⇒ 但单拍比较的判别力天然很弱 ⇒ 想真正验证 MUST 给多拍且激励值不同的激励」 |

## 4. critic 的验证建议（已由 builder 执行）

| 建议 | 执行结果 |
|---|---|
| 在**没有** `/tmp/opencode` 的环境跑 `test_hierarchy_verilog` | ✅ **已用隔离探针**验证「逐级建」在**深路径不存在**时成功（`deep dir created = YES`）；并从**空的** `/tmp/opencode/hieremit` 起跑该 target ⇒ **34 cases / 914 assertions 全绿** |
| 照抄文档 §5.4-ter 示例编译运行 | ✅ **编译 0 错误**（`-fsyntax-only`）；⚠️ **运行**需要 `verilator` + 建 `work_dir` ⇒ ⇒ 示例已补建目录 ⇒ ⇒ 这条**已由 SF2 的修复闭合** |
| `--self-test` 的 7 条投毒输出归档核对（重点确认投毒②/④ 的红来自**预期判据**） | ✅ 已核对：投毒② 由 **R2'（内容维度）+ R3（存在性）** 抓（★ 只跑 R1 时**真空通过** ⇒ ⇒ 这就是必须用聚合 `judge()` 的原因，已在脚本注释里记录）；投毒④ 由 **R4②** 抓（★ 名字**刻意不命中** `*equivalence*`/`*hier_eq*` ⇒ ⇒ 证明红来自**活账机制**而非旧模式） |
| 对 `symbols.py` 的实际枚举输出做快照存档 | ✅ 已做：枚举结果 = `DrivenInput` `Observation` `ProductSet` `Result` `compare` `emit` `make_observation`（**7 项**，与白名单**双向一致**） |

## 5. 结论

**无 BLOCKER** ⇒ 按 §评审预算规则 **不触发返工与复审**（2 SHOULD_FIX + 4 NIT 已全部处置）。

⚠️ **critic 的自述边界**（如实转录）：「未发现架构级 BLOCKER，但这不代表没有 —— 以上结论基于**静态阅读与 git 历史比对**，**未实跑**构建/测试。」⇒ ⇒ builder 侧的实证由 `.rddf/reviews/p8-...-verify.md` 承担（242/242 ctest · 5 条变异转红 · 7 条投毒转红 · 3 产物逐字节不变）。
