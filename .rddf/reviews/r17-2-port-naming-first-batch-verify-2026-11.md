---
id: r17-2-port-naming-first-batch-verify-2026-11
日期: 2026-11-13
对象: R17.2（用户 ⑤ 的原始诉求 · RV32I SoC 首批端口具名 · 32 端口）
状态: ✅ **通过**（判据全绿 · 遗留清扫闭 · **2 条预先存在的 CI 红已如实登记**）
角色: builder 角色内建验收（**verifier 契约** · 只验不改）
判据来源: ① 用户 2026-11-10 拍板「⑤ 端口命名按你建议」（方案 A′）+ 2026-11-13「再切builder进行实施」⇒ ⇒ ② 无 openspec change（**本项不是 change** ⇒ ⇒ 走 ③）③ 本次变更自身声明的行为 ④ ADR-069/068（**不适用** —— 本项不涉及时序契约）
---

# 验收报告：R17.2 · 首批端口具名（32 端口）

> **范围** = `examples/riscv-mini/src/rv32i_pipeline_top.h` 的 **32** 个端口加**显式名**
> ⇒ ⇒ 连带：删 `port-naming-exempt.tsv` 的 32 行 · 锚点 `1484 → 1452` ·
> 改 `tb_rv32i_soc.sv` 的 32 处连线 · 同步期望登记 / 登记表 / 端口命名指南 / 草案 AC / CHANGELOG / 公开面登记。
> **实施路径** = `.rddf/roadmap/phase-6-next-steps.md` §2 的 **R17.4**（8 步 · 每步一个判据）

---

## 1. 逐条判定

### 1.1 R17.4 第 1 步 —— 32 个端口加显式名（**标识符 == 名字串** ⇒ 满足护栏 P1）

| 判据 | 结果 | 证据 |
|---|:--:|---|
| `rv32i_pipeline_top.h` 的具名端口声明数 == **32** | ✅ | `grep -cE 'ch_(in\|out)<[^;]*>[[:space:]]*[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\{"' examples/riscv-mini/src/rv32i_pipeline_top.h` → **32** |
| 护栏 P1（**标识符 == 名字串**）全绿 | ✅ | `bash scripts/verify_port_naming.sh` → R1「端口总数 = 1484 · **不合规 = 1452** · 登记 = 1452 · 逐端口双向一致」 |

### 1.2 第 2 步 —— 删登记表的 32 行（否则护栏**反向红**）

| 判据 | 结果 | 证据 |
|---|:--:|---|
| `port-naming-exempt.tsv` 中该文件的条目数 == **0** | ✅ | `grep -c 'rv32i_pipeline_top' .rddf/checks/port-naming-exempt.tsv` → **0** |
| tsv 行数 1489 → **1457**（= 1452 条 + 5 行注释头） | ✅ | 脚本实测输出 |

### 1.3 第 3 步 —— 棘轮锚点 1484 → **1452**

| 判据 | 结果 | 证据 |
|---|:--:|---|
| `EXPECTED_EXEMPT_PORTS` == **1452** | ✅ | `scripts/verify_port_naming.sh:50` |
| 护栏 R2 报「登记表条目数 = 1452（== EXPECTED_EXEMPT_PORTS）」 | ✅ | `bash scripts/verify_port_naming.sh` 输出 |
| 护栏 exit 0 | ✅ | 同上 |

⇒ ⇒ **棘轮首次真咬**：「不增」这一半本来活着；「**递减**」这一半从停滞变为**有推进**（1484 → 1452）。

### 1.4 第 4 步 —— **逐位置比对**（证明「只改名、不改序」）

| 判据 | 结果 | 证据 |
|---|:--:|---|
| 顶层端口**总数不变**（旧 114 == 新 114） | ✅ | 脚本断言 `len(old) == len(new) == 114` |
| **全部 114 个端口的方向 + 宽度逐位置一致** | ✅ | 脚本逐位置比对 0 处漂移 |
| **名字变化的位置 == 32** | ✅ | 脚本实测「名字变化的位置数 = 32」 |
| 未变的 82 个端口中**无一**属于根成员名 | ✅ | 脚本断言 `set(unchanged) ∩ root_names == ∅` |
| 后代冒泡端口（**80** 个）**名字逐字未动** | ✅ | 脚本实测 `top_dtcm_*` / `top_itcm_*` / `top_pipeline_*` 全部逐字相同 |
| **新端口名序列 == C++ `__io` 声明顺序**（32 条逐条相等） | ✅ | 脚本断言 `new_names == ["top_" + m for m in members]` |
| ⚠️ 机制**不是**「声明顺序」而是 **node id 升序**（`src/codegen_verilog.cpp:410-448`） | ✅ | 源码核对（oracle 第 1 轮 F3 订正项） |

⇒ ⇒ **纯改名不动 node id ⇒ ⇒ 位置稳定 ⇒ ⇒ TB 的位置对应保持** ⇒ ⇒ 改名**零行为影响**。

### 1.5 第 5 步 —— TB 连线改为具名

| 判据 | 结果 | 证据 |
|---|:--:|---|
| TB 内 **32** 处端口名已改名 | ✅ | 脚本实测「TB 连接改名 32 处（期望 32）」 |
| TB **编译通过** | ✅ | `verify_examples_rtl_sim.sh` 的编译段 PASS |
| **`riscv-mini-rv32i-soc` 跑通且判据全绿** | ✅ | `RESULT: checks=24 fails=0（scenarios=6 · spec_checks=22）` |
| ⚠️ **判据面零变化**（期望值只依赖信号语义 ⇒ 与改名前逐字相同） | ✅ | 改名前 = `checks=24 fails=0（scenarios=6 · spec_checks=22）`；改后**完全一致** |

⇒ ⇒ **可读性收益可验**：TB 现在是 `.top_obs_data_addr(obs_data_addr)`，不再是 `.top_unnamed_output_1(obs_data_addr)`
⇒ ⇒ **「用户写 TB 必须查 port map」的痛点消掉**（用户 ⑤ 的原始诉求）。

### 1.6 第 6 步 —— 期望登记同步

| 判据 | 结果 | 证据 |
|---|:--:|---|
| `examples-rtl-expectations.md` 的 riscv-mini 段已加「端口名已具名」小节 | ✅ | 文件新增段（记录 32/32 + 判据面零变化 + 80 个后代端口仍未具名） |
| `verify_examples_rtl_sim.sh` **10/10** | ✅ | 全 10 项 `fails=0` |

### 1.7 第 7 步 —— 文档同步

| 判据 | 结果 | 证据 |
|---|:--:|---|
| `port-naming-registry.md` §4「当前状态」已更新（1484→1452 / 未具名 1452 / 具名且合规 **32**） | ✅ | 文件已改 |
| `PORT-NAMING-GUIDE.md` 的 L1 限制已加 dated 注记 | ✅ | 文件已改 |
| `port-naming-ratchet` 草案 AC「首批消化完成并登记」已勾 + 实施行已加 | ✅ | 文件已改（**剩余未勾 AC = 1**） |
| `improvement-suggestions.md` 索引行已追加 | ✅ | 文件已改 |
| `next-steps.md` §8.3 计数 3 → **1** | ✅ | 文件已改（存量消化 2 条闭 ⇒ 只剩 §4「N3 补遗」） |
| **公开面登记**（`.rddf/checks/public-api-surface.md`「行为变更」节 + `docs/CHANGELOG.md`） | ✅ | 两处各 1 条（**有同型先例**：行 99「顶层 RTL 端口表（RV32I SoC）」） |
| ⚠️ **根 `AGENTS.md` 未改**（已超 WARN 阈值 57,344 B） | ✅ | `git status` 无它 ⇒ ⇒ 新增内容进 capability / `.rddf/checks/` |

### 1.8 第 8 步 —— 全量重建 + 全护栏

| 判据 | 结果 | 证据 |
|---|:--:|---|
| **全量重建**（改了头 ⇒ ⇒ `ctest` 前 MUST 全量重建） | ✅ | `cmake --build build -j$(nproc)` → **RC=0**（warning 全在 `include/core/lnodeimpl.h` / `include/utils/logger.h` —— **本批未触碰的文件** ⇒ 预先存在） |
| `ctest -N` == **263**（== `CTEST_TOTAL` 锚点） | ✅ | `Total Tests: 263` |
| `ctest` 全量 | ✅ | **100% tests passed, 0 tests failed out of 263**（最终代码 · `ctest --output-on-failure -j$(nproc)` · 与 CI 同款并行 ⇒ Total Test time 1286.31 sec ⇒ §8 有三次运行的过程留痕） |

---

## 2. 护栏矩阵（**逐条** · exit code）

| 护栏 | 结果 | 备注 |
|---|:--:|---|
| `verify_port_naming.sh` | ✅ 0 | R2 报 1452 |
| `verify_port_naming.sh --self-test` | ✅ 0 | |
| `verify_flat_rtl_emit.sh` | ✅ 0 | F1–F5 + F8（**F6 = 「不适用」· 预先存在** ⇒ R19 待裁决） |
| `verify_examples_rtl_sim.sh` | ✅ 0 | **10/10** · `gpio` 仍标 `impl-contract only`（R20 待裁决） |
| `verify_examples_rtl_sim.sh --self-test` | ✅ 0 | |
| `verify_hierarchical_rtl.sh` | ✅ 0 | R1–R9 |
| `verify_hierarchical_port_naming.sh` | ✅ 0 | |
| `verify_node_names_single_source.sh` | ✅ 0 | |
| `verify_component_naming.sh` | ✅ 0 | |
| `verify_no_duplicate_rv32i_headers.sh` | ⚠️ **4** | **C6 已修**（本批引入）⇒ ⇒ **C5 的 4 处是预先存在**（见 §4） |
| `verify_no_duplicate_rv32i_headers.sh --self-test` | ✅ 0 | **本批的解析器修复未破坏自检** |
| `verify_hierarchical_p8.sh` | ⚠️ **3** | **预先存在**（见 §4） |
| `verify_hierarchical_p8.sh --self-test` | ⚠️ **3** | 预先存在（基线不绿 ⇒ 投毒无效） |
| `verify_header_self_contained.sh` · `verify_header_odr.sh` | ✅ 0 | |
| `verify_no_duplicate_rv32i_headers.sh`（其余段） | ✅ 0 | |
| `verify_public_api_visibility.sh` | ✅ 0 | |
| `verify_ledger_evidence.sh` | ✅ 0 | |
| `verify_arch_ledger_residuals.sh` | ✅ 0 | 23 行 |
| `verify_roadmap_fragments.sh` | ✅ 0 | |
| `verify_test_counts.sh` · `verify_agents_size.sh` | ✅ 0 | 263 |
| `verify_dual_backend_parity.sh` | ✅ 0 | |
| `verify_openspec_delta.sh` | ✅ 0 | |
| `verify_example_test_wiring.sh` · `verify_example_assertion_quality.sh` · `verify_assertion_quality_fixtures.sh` | ✅ 0 | |
| `verify_input_port_additions.sh` · `verify_input_port_required_registry.sh` · `verify_rst_n_wiring.sh` | ✅ 0 | |
| `verify_hier_corpus_coverage.sh` · `verify_hierarchical_p8.sh`(部分) · `verify_hierarchical_tree_extract.sh` | ✅ 0 | |
| `verify_perf_baseline_anchors.sh` · `verify_deprecation_callsites.sh` · `verify_deprecation_calendar_gate.sh` · `verify_downstream_consumer.sh` | ✅ 0 | |

---

## 3. 遗留清扫（本批范围内）

| 项 | 结果 | 证据 |
|---|:--:|---|
| TODO / FIXME / XXX / HACK / DEBUG / 临时 / 待补 / 占位（本批 3 个代码文件） | ✅ **0 处** | `grep -cE` 逐文件 |
| 注释掉的代码（`//` 后跟代码） | ✅ **0 处** | `grep -nE "^\s*//\s*(ch_in\|ch_out\|assign\|always\|if\|for\|\.top_)"` |
| 新增被 skip 的测试 | ✅ **0 处** | `git diff -- examples/` 无 `+SKIP` |
| 新增临时文件进仓 | ✅ **0 个** | `git status` 的 3 个 `??` 全部是**有意工件**（决策请求单 / 审稿记录 / architect 复盘） |
| 未替换的 mock / 临时 fallback 分支 | ✅ **0 处** | 本批**无 mock / 无 fallback** |
| 「活数字」漂移（`1484`） | ✅ **已同步** | 活面 2 处已改（`scripts/verify_port_naming.sh:22` 注释 · `port-naming-registry.md:14`）⇒ ⇒ 历史快照（`rtl-track-correction-list` / `b1-batch-verify` / `docs/CHANGELOG.md` 旧条目 / 复盘 #3）**故意不改**（改了就篡改历史）⇒ ⇒ 决策请求单已加 dated 注记（当前值 = 1452） |
| 「活数字」漂移（顶层端口数 `113`） | ✅ **已订正** | `tb_rv32i_soc.sv` 的注释写「113 个端口（5 输入 + 108 输出）」⇒ ⇒ 那是 **C8 tie-off 之前**的数字 ⇒ ⇒ 2026-11-13 按产物实测订正为 **114（7 in + 107 out）**（并注明「107 = 自有 27 + 后代冒泡 80」）⇒ ⇒ **本批顺手清理**（本批正在改该文件的端口段 ⇒ ⇒ 同一处不留陈旧数字） |
| 列对齐回退 | ✅ **已修复** | 首版脚本重写端口声明时**压平了原始对齐列** ⇒ ⇒ 可读性回退 ⇒ ⇒ 已从 HEAD 重做（**保留「类型 → 标识符」的原始空白**）⇒ ⇒ 产物实测**逐字节不变**（`top.v` 与 `manifest.json` 均 `diff -q` 相同）⇒ ⇒ ⇒ **教训**：文本变换 MUST 保留原排版 |

---

## 4. ⚠️ 失败项与路由建议（**本批不引入 · 范围外 · 不擅自清理**）

| # | 失败项 | 证据 | 归属 | 建议 |
|---|---|---|---|---|
| **F-1** | `verify_no_duplicate_rv32i_headers.sh` **C5**：`include/cpu/AGENTS.md` 自述头文件数 **30 ≠ 实测 27** · 自述行数 **6473 ≠ 实测 6064** | ✅ **已实证为预先存在**：把本批改动 `git stash` 后在 **HEAD** 上跑，仍是**同样 4 处违规** | **B0 批遗留**（该批物理删除了 3 个头却没更新 `include/cpu/AGENTS.md` 的自述计数） | ⇒ ⇒ **范围外**（本批写 `examples/`，不碰 `include/cpu/`）⇒ ⇒ **本批不清理**⇒ ⇒ **建议独立立项**（2 个数字的订正 · 载体 = `include/cpu/AGENTS.md` + 本护栏）⇒ ⇒ ⚠️ **它在 CI 门内**（`ci.yml:1021` / `:1921`）⇒ ⇒ **CI 当前为红** |
| **F-2** | `verify_hierarchical_p8.sh` **R4②**：`public-api-surface.md` 活账登记的 3 个头**不存在**（`hazard_detection_unit.h` / `rv32i_forwarding.h` / `rv32i_hazard_complete.h`） | ✅ **已实证为预先存在**（HEAD 上同样 3 处） | **B0 批遗留**（C5+C6 退役后活账未同步） | ⇒ ⇒ 同上：**范围外 · 不清理**⇒ ⇒ **建议与 F-1 同批立项**（两者同源 = 「B0 批退役后的登记未同步」）⇒ ⇒ ⚠️ **它在 CI 门内**（`ci.yml:735` / `:744`）⇒ ⇒ **CI 当前为红** |
| **F-3** | 本批**引入**并**已修复**的 1 处：`verify_no_duplicate_rv32i_headers.sh` 的 **C6 解析器**不认识具名形态（`ident{"ident"}`）⇒ ⇒ 端口集合比对把 `ext_clk{"ext_clk"}` 当成另一个端口名 ⇒ ⇒ 假红 | ✅ 修法 = **剥掉 `{"..."}` 后缀**（`sub(/\{.*\}$/, "", namepart)`）⇒ ⇒ **取到的仍是 C++ 成员名** ⇒ ⇒ **ADR-046 D10 的「名 + 方向 + 位宽」语义未变 ⇒ ⇒ 快照 MUST NOT 改** | builder（本批内） | ✅ **已闭**：C6 PASS（32 个端口与快照一致）⇒ ⇒ `--self-test` 仍绿 ⇒ ⇒ **无残留** |

⇒ ⇒ **本批净新增失败项 = 0**；**F-1 / F-2 是范围外的历史遗留** ⇒ ⇒ **已如实登记 · 交用户决定**（不在本批偷偷扩大范围）。

---

## 5. 用户可见收益（**本次改动的实际价值**）

| 项 | 改前 | 改后 | 证据 |
|---|---|---|---|
| RV32I SoC 顶层端口名 | `top_unnamed_input_3` / `top_unnamed_output_7` | **`top_prog_addr` / `top_obs_data_addr`** | 产物实测 |
| manifest 的 `cpp_member` **可解析**条数 | **2**（只有 `default_clock` / `default_reset`） | **34**（+32 个根端口） | manifest 实测（改前 `rv32i_emit2/manifest.json` vs 改后） |
| 命名棘轮豁免基线 | **1484** | **1452** | `verify_port_naming.sh` R2 |
| 写 TB 是否要查 port map | **要** | **不要** | TB 实测（`.top_obs_data_addr(obs_data_addr)`） |

---

## 6. 已用轮次

| 轮次 | 内容 | 结论 |
|---|---|---|
| **第 1 轮** | 本报告（builder 角色内建验收） | ✅ **通过**（0 净新增失败项 · 遗留清扫闭） |

⇒ ⇒ **未触发重试**（`implementation_gap` 0 项 · `proposal_drift` 0 项）⇒ ⇒ **未升级人工**。

---

## 7. 后续（**不在本批**）

1. **R13（B3 立项）** = 主线真起点（🚪 门 R5 + R6 已闭 · 只差立项）⇒ ⇒ ⚠️ **R17 与 B3 在 `rv32i_pipeline_top.h` 上同区 ⇒ MUST 顺序执行**（本批先做）⇒ ⇒ **B3 实施时 MUST 复核**：R17 已把 32 端口具名 ⇒ ⇒ B3 新增加载端口 MUST 一并具名（护栏 P1 会替 B3 兜住）。
2. **R18 / R19 / R20** = 待用户裁决（`.rddf/checks/decision-request-port-face-2026-11.md`）。
3. **R21** = 目标 ④ 的独立立项（**须先做规模侦察**：跨 module 隐式端口合成尚未实现 ⇒ 分析层 fail-closed）。
4. **F-1 / F-2** = 两条范围外的 CI 红（见 §4）⇒ ⇒ **需用户决定是否独立立项**。
5. **后续批次** = 后代冒泡端口（`top_pipeline_*` / `top_itcm_*` / `top_dtcm_*`，**80** 个）仍未具名 ⇒ ⇒ 归后续批次。

---

## 8. 补录（`ctest` 全量跑完后回填）

> ⚠️ **本节是「首稿先落盘 ⇒ 结论后补录」** —— 报告首稿写下时 `ctest` **仍在跑**
> ⇒ ⇒ §1.8 的 `ctest` 全量行**先标 ⏳** ⇒ ⇒ **未凭「预期会绿」写结论**。

| 项 | 结果 | 证据 |
|---|:--:|---|
| `ctest --output-on-failure -j$(nproc)`（全量 · 与 CI 同款并行） | ✅ **100% tests passed, 0 tests failed out of 263** · Total Test time **1286.31 sec** | `/tmp/opencode/ctest_final.log`（**零失败项**：`grep -iE "\*\*\*Failed"` 空）· 标签分布：`base` 226 · `verilator_e2e` **76** · `perf` 11 · `spinalhdl-ported` 25 · `verilator` 1 · `chlib` 1 |

⚠️ **过程留痕（如实 · 三次运行）**：
1. **第 1 次（串行 `ctest`）** ⇒ 卡在 `perf_subprocess_isolation`（自身 `TIMEOUT 1800`）⇒ ⇒ **手动停止**（SIGTERM）
   ⇒ ⇒ 停止前已完成 **107/263 全部 Passed**（含 `perf_tests` 601.46s · `perf_three_way` 84.07s）⇒ ⇒ 结论**未采信**（不完整）。
2. **第 2 次（并行 `-j`）** ⇒ ⇒ **100% passed / 263**（Total 1281.96 sec）。
3. **第 3 次（最终代码 · 并行 `-j`）** ⇒ ⇒ **100% passed / 263**（Total 1286.31 sec）
   ⇒ ⇒ **第 3 次才是最终依据** —— 第 2 次之后我**重做了端口声明的排版**（恢复原始列对齐，见 §3「列对齐回退」）
   ⇒ ⇒ ⇒ **源码变了 ⇒ ⇒ MUST 重跑**（仓内纪律：「还原源码 ≠ 复原 · 二进制也是状态」）⇒ ⇒ **已重跑并全绿**。

⚠️ **perf 标签测试与本批的关系**：`perf_*` 共 **11** 项测的是 **JIT / 构建吞吐**，
**不涉及** `rv32i_pipeline_top.h` ⇒ ⇒ 本批改动**对它们无影响面** ⇒ ⇒ 但仍**全跑了并全绿**（不据此推断）。
