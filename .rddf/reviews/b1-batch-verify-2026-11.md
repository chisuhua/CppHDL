# 验收报告：B1 批 11 项（`rtl-emit-product-tooling` C1/C3/C4/C5/C6 + `port-naming-ratchet` N1/N2/N3/N4/N5(a)/N6 + `port-name-literal-parity` P1）

- **验收对象**：`852c38a`（10/11 项）+ `ab354bf`（C6）+ `1e1d1e8` / `aa3ebef`（roadmap）+ `a8b79d7`（归档）
- **验收角色**：verifier（**只验不改实现**）
- **待验收树**：`aa3ebef`（工作区 clean）
- **判据来源**（按 verifier 产出契约的优先级）：
  ① 用户本次明确给出的验收条件 → **无**（用户给的是「做 B1 验收」）
  ② `openspec/changes/<name>/proposal.md` 的 `## 验收标准` → **不适用**（`openspec/changes/` 下只有 `archive/`，无 active change）
  ③ **本次变更自己声明了什么** → **`.rddf/roadmap/phase-6-rtl-productization.md` 的 B1 表（每行的「内容」列 = 交付承诺）** ⇒ ⇒ **本报告的主判据**
  ④ ADR 的 Verification 章 → 本批无新 ADR
- **补充判据**（第 ③ 项的原始来源）：`rtl-emit-product-tooling-2026-11.md` 的 `### C6` 与 `## Acceptance` 段 —— 批次表每项都注明「来源 = 某草案的某条」，故草案原文是判据的最细颗粒度
- **分流**：`rdd-verifier` 的 canonical preconditions **不满足**
  （① `grep -lE '^## (验收标准|Acceptance Criteria)$' openspec/changes/*/proposal.md` = **空** ⇒ 无 active change ·
  ② `.rddf/state/iteration.json` **不存在** ⇒ P3 archive gate 未触发）⇒ ⇒ **走本地验收 1–5 步**
  （③ `skill` 可加载 `rdd-verifier` 已实测 = 是，但 ①② 不满足 ⇒ 不委托）

**结论：❌ 不通过**（1 条 BLOCKER + 4 条判据子项未交付 + 3 条 SHOULD_FIX）

---

## 1. 验证顺序（早失败早停，逐条有工具证据）

| 步 | 内容 | 结果 | 证据 |
|---|---|---|---|
| **1** | 编译 / 静态检查 | ✅ | `cmake --build build -j$(nproc)` ⇒ **0 error** |
| **2** | 单元测试 | ✅ | `ctest --output-on-failure` ⇒ **100% tests passed, 0 tests failed out of 261**（base 224 · chlib 1 · perf 11 · spinalhdl-ported 25 · verilator 1 · **verilator_e2e 76**）· 总耗时 **1462.65 s**（全量重建后跑，非增量） |
| **3** | 集成 / 端到端（**真跑**） | ✅ | `./run_all_ported_tests.sh` ⇒ **PASS 28 / FAIL 0 / STALE 0 / MISSING 0** |
| **4** | 边界与错误路径 | ⚠️ | 见 §3 的「边界」列（CLI 三态退出码 ✅ · R0 fail-closed ✅ · **F1 无判别力自证 ❌**） |
| **5** | 遗留清扫复核 | ✅ | 见 §5 |
| **+** | 护栏复跑（12 条） | ✅ | 全部 PASS（`verify_test_counts` / `verify_dual_backend_parity` / `verify_public_api_visibility` / `verify_roadmap_fragments` / `verify_arch_ledger_residuals` / `verify_hierarchical_rtl` / `verify_hierarchical_port_naming` / `verify_port_naming` / `verify_flat_rtl_emit` / `verify_openspec_delta` / `verify_example_test_wiring` / `verify_example_assertion_quality`） |
| **+** | **两产物 lint 门** | ❌ | **默认产物 lint exit=0 · 卫生产物 lint exit=1**（`Can't find definition of variable`）⇒ ⇒ **BLOCKER-1** |

> ⚠️ **12 条护栏全绿 + 261/261 全绿，但卫生产物不可编译** —— 这正是「护栏只测工具小设计」的盲区（SF-8）。绿不能替代真实设计上的判据。

---

## 2. 11 项逐条验收

### ✅ 通过（7 项）

| 项 | 判据（批次表内容列） | 证据 |
|---|---|---|
| **C4 manifest schema** | 10 字段 + `rtl-emit-manifest-v1` | `tests/test_rtl_manifest.cpp`（**58 断言 / 5 用例**）· 实测产物 10 字段全在（`name`/`dir`/`width`/`role`/`driven`/`effective`/`owner`/`is_top_io`/`cpp_member`/`semantic`）· `schema = rtl-emit-manifest-v1` |
| **C1 CLI** | 库 + 薄 CLI · 二进制 MUST NOT 链 `examples/**` · 退出码 `0`/`1`/`2` fail-closed | `tools/CMakeLists.txt:15` 只 `target_link_libraries(cpphdl-emit-rtl cpphdl)` · 实测：正常 ⇒ **0** · `--verilog /proc/1/...`（写失败）⇒ **1** · `--bogus-option` / `--out`（缺值）⇒ **2** |
| **N1 具名用法 + 探针** | ① 用法约定 ② MUST 探针 ③ ctest ④ 文档 | `tests/test_port_name_named_ctor.cpp`（**27 断言 / 5 用例**）· `PORT-NAMING-GUIDE.md`（108 行：三条用法约定 + 已知限制 L1–L6 + 判别力自证方法） |
| **N2 基线棘轮** | 登记表 + `verify_port_naming.sh`（**先于 N4**） | `R1` 逐端口双向一致 **1484 / 1484** · `R2` 计数锚点 `EXPECTED_EXEMPT_PORTS=1484` 与 tsv 行数一致 · `--self-test` **4 类**全绿（M1 正向对照 / M2 P1 转红 / M3 正向转红 / M4 反向转红 + 还原复绿）· `R0` 缺登记表 ⇒ **exit 1**（fail-closed 实测）<br>⚠️ 「先于 N4」：实测两者**同时**可满足（spec 条款 + 豁免表均已存在，`verify_port_naming` PASS）⇒ ⇒ 判据意图（条款当场可满足）已达成，时序倒置不构成缺陷 |
| **N3 名字稳定性** | 显式命名端口绕开 eval-list 计数器 · 存量走兼容路径（否则 3103 处端口名全体变） | `include/codegen_naming.h:190-235` 两遍扫描（先占位后发名）· `is_explicit_named_port` 只对 `type_input`/`type_output` 且名字末段非 `unnamed_*` 生效 · **存量具名构造调用点实测 = 0**（`git grep` 空）⇒ ⇒ 新路径无存量消费者 ⇒ **零 churn 结构性成立** · 默认产物与 B1 前基线逐字节相同（见 C6 行） |
| **N4 规范落点** | 命名规则写进 `rtl-hierarchical-output` · guard MUST NOT 复述 | `openspec/specs/rtl-hierarchical-output/spec.md:608` `### Requirement: 顶层端口名的 C++ 成员可解析性 MUST 按端口三类分别规定`（A/B/C 按序判定 + `owner == nullptr` 显式排除 + C 类 MUST 登记棘轮豁免表） |
| **N5(a) manifest 双字段** | `name`（RTL 全名）+ `cpp_member`（C++ 成员路径） | 实测产物：`name=top_leaf_data_in` / `cpp_member='data_in'`；`name=top_prog_we` / `cpp_member='prog_we'` |
| **N6 公开面登记** | 第 1 步零公开 API 变更 ⇒ 无需登记；第 2 步若成立 ⇒ MUST 走登记 | `.rddf/checks/public-api-surface.md:85` 「行为变更（非符号移除）」4 行（`F-A` 前缀口径 / 根组件空名 / `mem_is_load` / manifest 新 capability）· `verify_public_api_visibility` R1–R6 PASS |
| **P1 探针固化** | 三探针逐字搬进 `test_port_name_parity.cpp`（5 用例 + 退化用例） | **25 断言 / 5 用例**（Ratchet 1/2/3 + Device 4 + **退化 6**）· ⑤ 落台账 `id=21b`（`.rddf/checks/arch-residual-registry.md:57` + 架构文档 `:300`） |

### ❌ 不通过（2 项）

#### C6 codegen 卫生 —— **4 个子项中 2 项未实现、1 项部分实现，且实现的 2 项有 BLOCKER**

批次表内容列（逐字）：`` `default_nettype none `` + 生成元数据头（时间只进 manifest）· 剪零引用信号 + proxy 恒等直通 · **复位写法规范化** · **`lint_off` 改按节点白名单**

| 子项 | 判定 | 证据 |
|---|---|---|
| ① `` `default_nettype none `` + 元数据头 | ⚠️ **部分** | 头部有（`emit_hyg/top.v:1-2`）· 元数据头确定性（产物无 `2026`）✅<br>🔴 **缺**「末尾 `` `default_nettype wire ``，**每文件一次**」（草案 `### C6` 原文）⇒ ⇒ 实测 `grep -c default_nettype emit_hyg/top.v` = **1** ⇒ ⇒ `none` 不恢复，宏状态**跨文件延续** |
| ② 剪零引用 + proxy 恒等直通 | 🔴 **BLOCKER** | 见 **BLOCKER-1** |
| ③ **复位写法规范化** | ❌ **未实现** | 草案原文要求 `if (!reset) q <= next; else q <= INIT;` ⇒ 规范化成 `if (reset) q <= INIT; else q <= next;`<br>实测产物 **仍是旧写法**：`hier_hyg.v:951-954` = `if (!default_reset) ... <= next; else ... <= 32'h80000000;`<br>全仓 `grep -rn "复位写法\|reset_style\|reset_norm"` ⇒ **空** |
| ④ **`lint_off` 改按节点白名单** | ❌ **未实现**（实现策略偏离） | 草案原文 = 「**保留**既有 `lint_off` 但**收窄**为按节点白名单」<br>实现 = 「卫生模式**完全不发**」（`src/codegen_verilog.cpp:456-467`）⇒ ⇒ 不是同一策略<br>⚠️ 且该策略**当前不可用**：卫生产物 lint exit=1（BLOCKER-1）⇒ ⇒ 「完全不发」要求产物自身绝对干净，而真实设计做不到 |

#### C5 产物护栏 —— F1 的「两段式基线」未实现

| 判据 | 判定 | 证据 |
|---|---|---|
| F1 剥 blanket `lint_off` 后重 lint **· 两段式基线** | ⚠️ **单段** | `verify_flat_rtl_emit.sh` 的 F1 = 一段（剥掉后重 lint）· **无**「基线不绿也能自检」的第二段 |
| F2 / F3 / F4 / F5 / F6 / F7 | ✅ | F2 确定性 · F3 三方比对（端口集 == 清单 6 个）· F4 diagnostics 空 · F5 无自赋值 · F6 **不适用**（工具设计无寄存器 ⇒ backend 不产 `top.v`，有 notice 说明解除条件）· F7 自检 M1/M2/M3 各转红 + 还原复绿 |
| 🔴 **F7 的 M4 是空操作** | ❌ | 实现里 `if lint 通过; then : # 自赋值 lint 不一定报错 ⇒ 不作为 F1 的判别力来源 ⇒ 只记录; fi` —— **两条分支都不失败** ⇒ ⇒ 「M4」只打印 `ok` 名称却**不判定任何东西**<br>⇒ ⇒ **F1 至今没有判别力自证**，而草案 Acceptance 明确：「⚠️ **F1 的基线不绿也能自检**（投毒 = 手工往白名单外造一条 ⇒ 必红）⇒ ⇒ **禁「首轮只报告不失败」这种无判据状态进入验收**」 |

---

## 3. BLOCKER-1（阻断级，路由回 builder）

### 现象

C6 的「字面量内联特例」剪枝**误剪仍被引用的字面量** ⇒ 卫生产物**无法编译**。

### 证据（RV32I SoC，**扁平与层次化都失败**）

```
$ verilator --lint-only --top-module top flat_hyg.v
%Error: flat_hyg.v:1006:62: Can't find definition of variable: 'top_pipeline_uint_lit_7'
%Error: flat_hyg.v:1006:88: Can't find definition of variable: 'top_pipeline_uint_lit_8'
%Error: flat_hyg.v:1007:64: Can't find definition of variable: 'top_pipeline_uint_lit_5'
...（同类 ≥12 处）
exit=1

$ verilator --lint-only --top-module top flat_hyg.v   # 默认产物（hygiene off）
exit=0                                                # ⇒ 默认路径**不受影响**（零 churn 结论仍成立）
```

误剪点（产生侧 vs 消费侧）：

```verilog
// 标准产物：声明 + 赋值都在，且 mux 引用它
//   hier_std.v:158   logic [4:0] top_pipeline_uint_lit_7;
//   hier_std.v:1508  assign top_pipeline_uint_lit_7 = 5'h10;
//   hier_std.v:1510  assign top_pipeline_mux_select = top_pipeline__eq_eq_2 ? top_pipeline_uint_lit_7 : ...
//
// 卫生产物：声明与 assign 被剪，**引用仍在** ⇒ 未声明标识符
//   hier_hyg.v:1006  assign top_pipeline_mux_select = top_pipeline__eq_eq_2 ? top_pipeline_uint_lit_7 : ...
```

### 根因（代码级）

`src/codegen_verilog.cpp` 的剪枝有一条特例：

> 「字面量若**全部使用者都内联常量**（`op` / `mux` / `bitsupdate`）⇒ ⇒ 可剪」

前提**假**：`print_mux` **不内联**字面量 —— 它引用信号名（`? top_pipeline_uint_lit_7 : ...` 形式）。`op` 是否内联取决于具体算子（`== 5'h0` 内联、mux 选择不内联）。

⇒ ⇒ 正确判据不能按「使用者类型」猜，必须**按实际发射形态**判（例如：把「是否内联」做成 `print_*` 与剪枝共用的**唯一**判定函数，或在发射后做**引用完备性**闭环）。

### 最小复现（**14 行设计**，比 RV32I 更适合当判据）

一个 `select()` 就足以触发（`/tmp/opencode/probe_mux_lit.cpp` 实测）：

```cpp
io().dout <<= select(io().sel, ch_uint<8>(5_d), ch_uint<8>(7_d));
```

卫生产物（**产物本身自相矛盾**：引用两个从未声明的信号）：

```verilog
`default_nettype none
module top (
    input default_clock, input default_reset, input top_sel, output [7:0] top_dout
);
    logic [7:0] top_mux_select;
    assign top_mux_select = top_sel ? top_uint_lit : top_uint_lit_1;   // ← 两个 lit 被剪
    assign top_dout = top_mux_select;
endmodule
```

```
$ verilator --lint-only --top-module top muxlit_hyg.v    # hygiene=on
%Error: Can't find definition of variable: 'top_uint_lit'
%Error: Can't find definition of variable: 'top_uint_lit_1'
exit=1

$ verilator --lint-only --top-module top muxlit_std.v    # hygiene=off
exit=0
```

⇒ ⇒ **这不是 RV32I 特有的形态**：任何含「mux 选字面量」的设计都会触发。`tests/test_rtl_emit_hygiene.cpp` 的 `FlatDut` 恰好没有这个形态（只有 `prog_data + 1` 与直通）⇒ ⇒ 测试结构性漏掉。

### 复现命令

```bash
# ① 最小复现（真跑过）
g++ -std=c++23 -w -I include -I include/core -I include/utils -I include/ast -I include/bv \
    /tmp/opencode/probe_mux_lit.cpp -o /tmp/opencode/probe_mux_lit \
    build/libcpphdl.a -lgmp
/tmp/opencode/probe_mux_lit
build/verilator-install/bin/verilator --lint-only --top-module top /tmp/opencode/muxlit_hyg.v   # ⇒ exit=1

# ② 真实设计（真跑过）
build/tools/cpphdl-emit-rtl --out /tmp/opencode/emit_hyg --emit-hygiene
build/verilator-install/bin/verilator --lint-only --top-module top /tmp/opencode/emit_hyg/top.v  # ⇒ exit=1
```

⚠️ **为什么 261/261 + 12 条护栏全绿仍然漏掉**：`tests/test_rtl_emit_hygiene.cpp` 与 C5 的 F8 都只跑**工具自己的小设计**（7 端口、无 mux-选字面量形态）⇒ ⇒ 对「真实设计才出现的引用形态」**结构性失明**。

### 路由

`implementation_gap`（判据明确、实现有缺陷）⇒ **回 builder**。修复后 MUST 至少：
1. 剪枝判据与「是否内联」共用**同一处**判定（禁两套）；
2. F8 增加一条**真实设计**判据（RV32I SoC 或含 mux-选字面量的最小复现设计）⇒ ⇒ 否则同一盲区仍在；
3. 卫生产物 MUST 过 `verilator --lint-only`（RV32I SoC 层面）。

---

## 4. SHOULD_FIX（记录，不阻断）

| # | 项 | 说明 |
|---|---|---|
| **SF-6** | C3 偏离「MUST 复用 `diagnose_drive()`」 | 批次表 C3 与草案 Capabilities 都要求「复用 `diagnose_drive()`，MUST NOT 另造驱动状态判定」。实现改用 `inputimpl::driver()`（`src/rtl_emit/manifest.cpp:194-201`），注释论证「`ch_in` 的 `num_srcs()` 恒为 0，用 `diagnose_drive` 会把**每个**被正常驱动的子端口都误报悬空」——**论证正确**（`include/core/lnodeimpl.h:264-270` 对 `type_input` 判 `num_srcs()==0 ⇒ undriven` 确实恒假）<br>⇒ ⇒ 但**判据侧没登记这个偏离**（批次表仍写「复用 `diagnose_drive()`」）⇒ ⇒ 下一个人照判据改会引入全误报。**建议**：把偏离理由写进批次表/草案的 C3 行 |
| **SF-7** | CLI 的 manifest 写失败返回 **2**（而非 1） | `src/rtl_emit/cli.cpp` 对不可写 manifest 路径返回 2。批次表只要求「`0`/`1`/`2` fail-closed」⇒ ⇒ **不违反**（非 0），但与「2 = 用法错误」的定义有偏差 |
| **SF-8** | C5 的 F8 判别力不足 | F8 六项判据全部只跑工具小设计。**BLOCKER-1 的直接成因**。建议随 BLOCKER-1 一并加「真实设计 / mux-选字面量形态」用例 |
| **SF-9** | `PORT-NAMING-GUIDE.md:92` 的示例输出陈旧 | 文档写 `./build/tests/test_port_name_named_ctor` ⇒ 「**18 assertions in 4 test cases**」，实测 **27 assertions in 5 test cases** ⇒ ⇒ 命令输出示例未随测试增补同步 |
| **SF-10** | `AGENTS.md:514-519` 的「Test counts」表**全面陈旧且与锚点矛盾** | 实测 vs 表格：CTest **261** vs 257 · non-perf **250** vs 246 · `verilator_e2e` **76** vs 77 · parity 源文件 **65** vs 66 · parity macro 对 **284** vs 290<br>该表自己注明「单一真相在上面的 CI 锚点」⇒ ⇒ 它是**冗余副本**且已漂移；`verify_test_counts.sh` 只校验两个锚点，不校验此表 ⇒ ⇒ 结构性漏检。**建议**：删表或改成「见上方锚点」 |

---

## 5. 遗留清扫复核

| 检查 | 结果 | 证据 |
|---|---|---|
| `TODO` / `FIXME` / `XXX` / `HACK` / `#if 0` / 调试打印 | ✅ **0 处** | 扫 B1 全部 23 个改动文件（`git diff --name-only 67926cf..HEAD`）⇒ 仅 2 处 `XXXXXX`（`mktemp` 模板，非 TODO） |
| 被 skip / 注释掉的测试 | ✅ **0 处** | 4 个新测试文件 `grep SKIP(` / `// TEST_CASE` ⇒ 空 |
| 未替换的 mock / 临时 fallback | ✅ **0 处** | 无 mock 框架；`emit_hygiene` 默认 off 是**设计**而非 fallback |
| 全量构建 + ctest | ✅ | **261/261 PASS**（全量重建后跑，非增量） |
| 12 条护栏 | ✅ | 全绿 |

---

## 6. 失败项汇总与路由

| # | 项 | 级别 | 路由 |
|---|---|---|---|
| **BLOCKER-1** | C6 剪枝误剪仍被引用的字面量 ⇒ 卫生产物不可编译（扁平 + 层次化） | 🔴 BLOCKER | **builder**（`implementation_gap`） |
| **FAIL-2** | C6 子项「复位写法规范化」未实现 | 🔴 判据未交付 | **builder** |
| **FAIL-3** | C6 子项「`lint_off` 收窄为按节点白名单」未实现（实现成「完全不发」） | 🔴 判据未交付 | **builder**（或 planner 改判据 —— 需用户裁决） |
| **FAIL-4** | C6 缺「末尾 `` `default_nettype wire ``，每文件一次」 | 🟡 判据未交付 | **builder** |
| **FAIL-5** | C5 的 F1「两段式基线」未实现；F7 的 M4 是空操作 | 🟡 判据未交付 | **builder** |
| SF-6 / SF-7 / SF-8 | 见 §4 | 🟢 记录 | 不阻断 |

**已用轮次**：本 change **1 / 3**（`RDDF_VERIFIER_MAX_LOOPS=3`）

---

## 7. 需要用户裁决的一点

`FAIL-3`（`lint_off` 按节点白名单 vs 完全不发）在 BLOCKER-1 修好**之后**才可判：

- 若真实设计的卫生产物**能**做到「自身零告警」⇒ ⇒ 「完全不发」是**比判据更强**的方案，可把批次表判据更新为「不发 blanket ⇒ 产物自身零告警」（需 planner 记录判据变更）
- 若做不到（大概率：真实 SoC 的面积/宽度告警不可全消）⇒ ⇒ 必须按原判据实现「按节点白名单」

⇒ ⇒ 建议：**先修 BLOCKER-1（含 F8 真实设计判据）**，用实测结果再定 `FAIL-3` 的走向，不要现在就改判据。

---

## 8. 返工记录（builder · 2026-11-11）

> 用户裁决「**先修 BLOCKER + 4 项 FAIL**，再进 B2」⇒ 本节记录改建方的闭合证据，
> 由 verifier 复审（不替代验收结论）。

### BLOCKER-1 闭合

| 项 | 返工内容 | 闭合证据 |
|---|---|---|
| 剪枝判据的前提错误 | 新增 `is_literal_referenced_by(user, lit)` —— **逐使用者、逐位置**判定「文本里会不会出现这个名字」（`binary` 只内联 rhs · `concat` 两侧 · `rotate`/`bit_sel`/`bits_extract` 的 rhs · `mux`/`bitsupdate` 不内联 · 不发射的 `op` 路径） | RV32I 卫生产物 **未声明引用 0**（返工前扁平/层次化均 ≥12 处 `Can't find definition`） |
| 三条「非图边引用」保护 | ① 模块端口；② `body_epilogue` + **`port_drives_node` 的目标名**；③ **端口 assign 的终点 driver** | 每条都是本轮实测踩出来的（依次命中 6 条 / 6 条 / 1 条误剪） |
| walk 逻辑三处不一致 | 抽出 `port_driver_endpoint()`，`print_logic` 的 output/input 分支与剪枝**共用** | 默认路径逐字节不变（三处语义等价） |
| 子组件输入端口 | 扁平化后是内部 wire ⇒ 无消费者即可剪（模块端口由 `view_.ports` 保护） | RV32I 最后 6 个零引用归零 |

**关键实测（返工后）**

```
flat_hyg.v / hier_hyg.v（RV32I SoC）
  零引用信号 = 0 · 恒等直通 = 0 · 未声明引用 = 0
  verilator --lint-only ⇒ 告警 0 · exit=0        （返工前：exit=1）
默认产物（hygiene off）与 B1 前基线 diff ⇒ 空（零 churn）
行为等价：同一 TB 驱动 flat_std.v 与 flat_hyg.v ⇒ 200 拍 × 107 输出端口逐拍一致
```

### 4 项 FAIL 闭合

| 项 | 返工内容 |
|---|---|
| **FAIL-2** 复位写法规范化 | `if (reset) q <= INIT; else q <= next;` —— `emit_always_ff` + `print_reg` 的「无 next」分支两处（仅 hygiene 模式） |
| **FAIL-3** `lint_off` 收窄 | 卫生模式**不发**任何 blanket ⇒ 改为**显式 size cast**（`N'(expr)`，新增 `hygiene_cast()`）把隐式宽度转换写成意图 ⇒ ⇒ **RV32I 的 23 条宽度告警归零** |
| **FAIL-4** `default_nettype wire` | `print_footer` 在 `endmodule` 之后恢复（每文件一次，与头部 `none` 配对） |
| **FAIL-5** F1 两段式 + F7 M4 | F1 改两段式（① 原样基线 · ② 剥掉后仍绿）且靶改为**卫生产物**；F7 的 M4 从空操作改为**真判定**（用 `lint_off` 盖住一条真实宽度截断 ⇒ 剥掉后必红；含「投毒有效性」前置校验）<br>⚠️ **F1 靶变更的判据理由**：原 F1 跑默认产物，其隐含前提「工具设计恰好无宽度告警」已破 —— 而**默认产物在真实设计上必然有 23 条宽度告警**（那正是 blanket 要盖的东西）⇒ ⇒ 「剥掉 blanket 后干净」对默认产物**不可满足** |

### SF-8 闭合

- `--undeclared-refs` 新模式（**不依赖 verilator**）⇒ F8 第 ⑥ 项 + `test_rtl_emit_hygiene` 的 C++ 版
- 工具自带设计加入 **mux 选字面量**形态 ⇒ F8 成为 BLOCKER-1 的**常驻回归靶**

### 判别力自证（3 条变异 · 各自转红 · 还原后重建复绿）

| 变异 | 命中用例 |
|---|---|
| M1 恢复「mux 内联字面量」 | 用例 5（mux 选字面量）转红 |
| M2 去掉「链上中间 proxy」死节点种子 | 用例 6（中间 proxy 剪除）转红 |
| M3 不发 `default_nettype wire` | 用例 7（尾部恢复）转红 |

### 返工后的全量验证

- `ctest` ⇒ **261/261 PASS**（base 224 · verilator_e2e 76）· 1460.27 s
- 12 条护栏 ⇒ 全绿；`verify_flat_rtl_emit --self-test` ⇒ M1–M4 各自转红 ✓；
  `verify_port_naming --self-test` ⇒ 4 类 ✓
- `run_all_ported_tests.sh` ⇒ **28/28 PASS**
- `tests/test_rtl_emit_hygiene.cpp` ⇒ **38 断言 / 7 用例**

### 仍未处理（如实登记）

- **SF-6**（C3 偏离「复用 `diagnose_drive()`」）：**未改** —— 该偏离的论证是正确的
  （`ch_in` 的 `num_srcs()` 恒 0 ⇒ 复用会把每个被正常驱动的子端口误报悬空），
  但**判据侧仍写着「复用」**。建议在批次表 C3 行补记偏离理由（planner 域，未越权改）。
- **SF-9 / SF-10**（文档数字陈旧）：**未改** —— 属文档维护，不在 BLOCKER 修复范围。

---

## 9. 复审（verifier · 第 2 轮 · 2026-11-11）

> **范围限定**（评审预算：本 change 2/3 轮）：只验「§3 的 BLOCKER-1 + §6 的 4 项 FAIL
> 是否闭合」+「新 diff 是否引入新 BLOCKER」。**不做**全量重审、不重新评判 §4 的
> SHOULD_FIX（除 SF-8）。

### 闭合判定

| 上轮项 | 判定 | 证据（本轮独立取证） |
|---|---|---|
| **BLOCKER-1** 剪枝误剪仍被引用的字面量 | ✅ **闭合** | RV32I 卫生产物（**扁平 + 层次化**）⇒ 未声明引用 **0** · 零引用 **0** · verilator 告警 **0** · `--lint-only` **exit=0**<br>最小复现（`select()` 选两个字面量）⇒ `exit=0` · 未声明引用 **0**<br>§3 的复现命令重跑：不再有 `Can't find definition of variable` |
| **FAIL-2** 复位写法规范化 | ✅ **闭合** | 卫生产物 `if (default_reset) pc_reg <= 32'h80000000; else pc_reg <= <next>;`（`hier_hyg.v:1028-1031`）<br>默认产物仍是旧写法（`hier_std.v:1441`）⇒ 开关隔离正确 |
| **FAIL-3** `lint_off` 收窄 | ✅ **闭合**（且**更强**） | 卫生产物 `grep -c "verilator lint_off"` = **0**；而宽度告警 **0 条**<br>对照：默认产物剥掉 blanket 后 **23 条**告警 ⇒ ⇒ 返工用**显式 size cast** 把 23 条全部消除，不是掩盖 |
| **FAIL-4** `default_nettype wire` | ✅ **闭合** | `hier_hyg.v` 尾部 `endmodule` 之后有 `` `default_nettype wire ``；`default_nettype` 计数 **2**（`none` + `wire` 配对） |
| **FAIL-5** F1 两段式 + F7 M4 | ✅ **闭合** | `F1-a 卫生产物基线 lint 干净` · `F1-b 剥掉 lint_off 后仍干净` · `自检 M4：真实宽度告警被 lint_off 盖住 ⇒ 剥掉后 F1-b 转红 ✓`（M4 不再是空操作） |
| **SF-8** F8 判别力不足 | ✅ **闭合** | F8 新增「无未声明引用」（`--undeclared-refs`，不依赖 verilator）· 工具设计加入 **mux 选字面量**形态 ⇒ F8 成为 BLOCKER-1 常驻回归 |

### 不回归（本轮实测）

| 判据 | 结果 |
|---|---|
| 默认路径**逐字节不变** | ✅ RV32I 层次化产物与 B1 前基线 `diff` 为空 |
| **行为等价** | ✅ 同一 TB 驱动 `flat_std.v` 与 `flat_hyg.v` ⇒ **200 拍 × 107 个输出端口逐拍一致** |
| `ctest` | ✅ **261/261 PASS**（base 224 · verilator_e2e 76）· 1460.27 s |
| 12 条护栏 + 2 条 self-test | ✅ 全绿（含 `verify_flat_rtl_emit --self-test` 的 M1–M4 各自转红） |
| `run_all_ported_tests.sh` | ✅ **28/28 PASS** |
| 新 diff 是否引入新 BLOCKER | ✅ **无** —— 剪枝的 `while(changed)` 每轮至少新增一个死节点（有限集 ⇒ 终止）· `hygiene_cast` / `print_footer` / `port_driver_endpoint` 的改动全部被 `emit_hygiene` 隔离（零 churn 实测）· `is_prunable_type(type_input)` 的放宽由 `view_.ports` 保护模块端口，且行为等价实测覆盖 |

### 结论

**APPROVE** —— 上轮 1 条 BLOCKER + 4 项 FAIL + SF-8 **全部闭合**，无新增 BLOCKER。

**放行条件**（不影响本次放行，记入 backlog）：
- **SF-6**（C3 偏离「复用 `diagnose_drive()`」）→ 需在批次表 C3 行补记偏离理由（**planner 域**）
- **SF-9 / SF-10**（`PORT-NAMING-GUIDE.md` 示例输出 · `AGENTS.md` Test counts 表）→ 文档维护

**已用轮次**：本 change **2 / 3**
