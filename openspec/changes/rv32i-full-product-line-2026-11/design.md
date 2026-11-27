# Design: S4c-0 —— 双产品线骨架

> **本文只写「推导与机制」**（决策见 ADR-078 · 任务清单见 `tasks.md` · 判据见 `proposal.md` 的
> `## 验收标准`）。

---

## 1. 为什么「登记面」必须是派生物而非手写中央表

| 形态 | 加一条线的成本 | 多会话并行 | 半创建 / 忘登记 | 结论 |
|---|---|---|---|---|
| 手写中央登记表 | 改中央文件 1 处 | ❌ 同文件冲突（仓内 `git add -A` 跨会话事故先例） | ⚠️ 需额外校验 | ❌ |
| 纯自动发现（`examples/*/` glob） | 0 处 | ✅ | ❌ **静默不覆盖新线**（本仓最恐惧的失败族） | ❌ |
| **per-line marker + 派生物登记面** | **写自己的 marker + 重生成派生物** | ✅（各写各的目录） | ✅ **元护栏 C1 必红** | ✅ **采用** |

⇒ 单点真相 = **各产品线目录内的 `.rddf-line.yaml`**；中央只有**派生物**
（`.rddf/checks/product-lines.md`）⇒ ⇒ 生成物陈旧 ⇒ 护栏 C1 红。

## 2. marker schema

⚠️ **一条产品线 = 两个成员目录**（IP 级 + SoC 级）⇒ 例：产品线 `riscv-mini` 含
`examples/riscv-mini`（IP 级）+ `examples/riscv-soc`（SoC 级）⇒ ⇒ **marker 放在该线的
**IP 级目录**（产品线的锚点）**，**每条产品线恰好一个** ⇒ ⇒ 「发现集」= marker 集。

```yaml
# examples/riscv-mini/.rddf-line.yaml
line: riscv-mini        # 产品线标识 == 所在目录名（锚点 = IP 级目录）
layer: ip               # 本 marker 所在目录的层（恒为 ip · 锚点语义）
members:                # 该产品线的全部成员目录（每个 MUST 存在）
  - examples/riscv-mini
  - examples/riscv-soc
adrs:                   # 关联 ADR（文件 MUST 存在 ⇒ C2 可判）
  - ADR-035
  - ADR-046
specs:                  # 关联 capability（目录 MUST 存在）
  - rv32i-isa-semantics
```

**必填字段** = `line` / `layer` / `members` / `adrs`（`specs` 可空）。
**C2 的四条子判据**：`line` == 所在目录名（⇒ 目录改名必改 marker ⇒ 改名不可能静默）·
`layer` ∈ {`ip`}（锚点语义）· `members` 列的**每个目录 MUST 存在** · `adrs` 指向的 ADR 文件 MUST 存在。
**归属完备性**（C2 的第五子判据）：`examples/` 下的**每个**产品线成员目录 MUST 要么是某 marker 的
`members` 成员、要么**自己带 marker** ⇒ ⇒ 「新建了产品线目录却忘了登记」当场红。

## 3. 生成器与登记面

- `scripts/gen_product_lines.py` ⇒ 输出 `.rddf/checks/product-lines.md`（**登记面**）：
  §1 产品线表（标识 / 层 / 目录 / 关联 ADR / 关联 capability）· §2 锚点分组表 ·
  §3 脚本处置表（**14 行** · 由 `scripts/product_line_scope.tsv` 派生 ⇒ 见 §5）。
- `--check` 模式 = 「生成物 == 发现集」⇒ **AC2** 的判据。
- ⚠️ 生成器**不写**任何**手写**内容 ⇒ 登记面里的一切都可从 marker / tsv 重建。

## 4. 元护栏（`scripts/verify_product_lines.sh`）的四条判据

| 判据 | 内容 | 红的情形 |
|---|---|---|
| **C1** | 发现集 == 生成集 | marker 缺失 / marker 多出 / 生成物陈旧 |
| **C2** | marker 完备性 | 必填字段缺 / `layer` 非法 / **`line` != 目录名** / `adrs` 指向的 ADR 文件不存在 |
| **C3** | 覆盖完备性 | 某条线**未被任何**护栏登记面引用（登记了但不覆盖 ⇒ 红） |
| **C4** | `--self-test` 投毒自证 | 投毒（改 marker / 删 marker / 改 `layer`）⇒ **必红** |

⇒ C3 的「护栏登记面引用」载体 = 各护栏的**登记文件**（如 tsv 的行、处置表的行）
⇒ ⇒ 「加线漏登记 = 当场红」（**AC11**）。

## 5. 脚本处置表（`scripts/product_line_scope.tsv` · 14 行）

| 脚本 | 写死内容 | 类 | **S4c-0 的动作** | 何时才需要按线分组 |
|---|---|---|---|---|
| `verify_no_duplicate_rv32i_headers.sh` | 34 处 · C1/C4/C5/C6 扫描面 | **B** | **扫描面改为按 marker 发现**（`examples/*/` 下各产品线）；C1 因 full 线 basename 独立而天然绿；**C6 快照仍只对 mini**（full 线尚无核） | C6 的 per-line 快照 ⇒ **S4c-1**（full 线有核后） |
| `verify_riscv_soc_core_ports.sh` | 10 处 · 53 行表 + V4 basename | **B** | 加 `--line` 参数；**full 线无核 ⇒ 本脚本对 full 线无对象**（不误红） | 53 行表的 per-line 版本 ⇒ **S4c-1** |
| `rv32i_obs_vector.py` | 4 处 · `CORE_HEADER` / `VECTOR_SRC` | **B** | 改为**按 marker 发现**核头（找不到 ⇒ 该线无观测向量 ⇒ 跳过而非误红） | full 线的向量源 ⇒ **S4c-1** |
| `dual_backend_parity.py` | 2 处 · 测试源清单 + 全局锚点 | **B** | 清单按 marker 发现；锚点改**分组** | full 线有 dual pair ⇒ **S4c-1** 起 |
| `verify_example_assertion_quality.sh` | 24 处 · 测试源清单 + 期望描述 | **B** | 新线的测试源 MUST 登记 ⇒ **full 线的最小测试源必须登记** | 持续 |
| `verify_example_test_wiring.sh` | 归属守恒 | **B** | 守恒面按 marker 发现 ⇒ full 线测试源 MUST 在其 CMake 注册 | 持续 |
| `verify_examples_rtl_sim.sh` | tsv + self-test 临时清单 | **B** | **S4c-0 的 full 线无 RTL 产物 ⇒ tsv 无行是合法的**；「有产物但未登记」⇒ 红 | full 线进 tsv ⇒ **S4c-1** 起 |
| `verify_hierarchical_rtl.sh` | 5 处 · `RV32I_HIER_EMITTER`（**已可覆盖**） | **B** | 无需改（S4c-0 的 full 线不参与层次化判据） | full 线语料 ⇒ **S4c-1** 起 |
| `verify_rv32i_official_rtl.sh` | 5 处 · riscv-tests 语料 | **A** | **只登记不改**（mini-only by design） | — |
| `verify_dtcm_read_timing.sh` | 2 处 | **A** | 只登记不改 | — |
| `verify_rv32i_fw_init_rtl.sh` | 3 处 | **A** | 只登记不改 | — |
| `verify_rv32i_elf_e2e.sh` | 2 处 | **A** | 只登记不改（full 线有对应物时再加行） | full 线有 ELF 流程 ⇒ 另议 |
| `rv32i_rtl_sim_env.sh` | 1 处 | **A** | 只登记不改 | — |
| `probe_r21_hier_granularity.sh` | 1 处 | **A** | 只登记不改（一次性探针） | — |

⇒ **B 类 8 个的动作在 S4c-0 各不相同** ⇒ ⇒ 任务清单 MUST 逐个列（不可写成「参数化护栏」一句）。

## 6. 锚点分组

根 `AGENTS.md` 的 CI anchor 段从单值改为分组：

```yaml
- CTEST_TOTAL: mini=285, full=1
- VERILATOR_E2E_TOTAL: mini=82, full=0
- DUAL_BACKEND_PARITY_TOTAL: mini=68, full=0
- DUAL_BACKEND_PARITY_PAIRS: mini=307, full=0
- EXAMPLES_TEST_SOURCES: mini=26, full=1
```

- `verify_test_counts.sh` 改为**分组比对**（逐组 × `ctest -N` 的对应子集）。
  ⚠️ **分组子集的划定口径 MUST 明确**：`ctest -N` 的测试名**不含产品线标识** ⇒ ⇒
  分组比对**只能**用「该线**拥有**的测试 target 清单」（由 marker / CMake 派生）⇒ ⇒
  ⇒ **实现形态 = 由 marker 派生的「该线的 ctest target 清单」+ 按 target 名分组计数**。
- ⚠️ 由此产生一条**新不变式**：**每个 ctest target MUST 归属唯一一条产品线** ⇒
  未归属 ⇒ 元护栏 C3 红 ⇒ ⇒ 「新 target 忘了归线」当场红。

## 7. 逐线 scope 标注

- 根 `AGENTS.md` 与 `include/cpu/AGENTS.md` 的「imem MUST `sread`（1 拍）」类红线
  ⇒ 标注「**mini 线专属**（full 线见 ADR-078 D14 的取指接口 fork）」。
- capability `rv32i-data-forwarding` 的 load-use 停顿矩阵 ⇒ 同款标注
  （⚠️ capability 是**活的单一真相** ⇒ 该标注进 `openspec/specs/**` ⇒ 属 spec 变更 ⇒
  ⇒ 本 change 的 delta MUST 含该 capability 的 `## MODIFIED Requirements` ⇒
  ⇒ 标题 MUST 与主 spec **逐字一致**（delta 护栏 R3））。

## 8. full 线空壳的最小形态

| 项 | S4c-0 的形态 | 理由 |
|---|---|---|
| `examples/riscv-full/` | marker + README（说明 S4c-1 才建核） | ADR-078 AC2 只要求目录存在 |
| `examples/riscv-full-soc/` | marker + **最小 SoC 顶层**（框架 2 + 2 个 `ch_out` 计数）+ **1 个 Catch2 测试源** | 让新线**立即进入** test-wiring / assertion-quality 的扫描面（AC11 的登记对象） |
| RTL 产物 / 层次化 | **无** | ⇒ tsv 无行是合法的；RTL 判据从 S4c-1 起 |
| 互连 / 外设 / 核 | **无** | ⇒ S4c-0 的 full 线**零功能** |

⇒ ⚠️ 「最小 SoC 顶层」的**端口面**（框架 2 + 2 个 `ch_out`）MUST **具名**（ADR-075 E1）
⇒ 且**不与** mini 线的任何端口名重复（`ch_out` 名不同）⇒ ⇒ C1 判据天然绿（AC4）。

## 9. 失败模式与缓解

| # | 失败模式 | 缓解 |
|---|---|---|
| F1 | **加线漏登记** ⇒ 该线的测试/产物不被任何护栏覆盖 ⇒ 静默缩面 | 元护栏 C3（每条线 MUST 被登记面引用）+ AC11 |
| F2 | **分组口径错** ⇒ 锚点计数对不上或错配到别的线 | 由 marker 派生的「该线的 ctest target 清单」⇒ 口径唯一；未归属 target ⇒ C3 红 |
| F3 | **生成物陈旧** ⇒ 登记面与 marker 不一致 | C1（生成器 `--check` 模式进 CI） |
| F4 | **A 类脚本被误当「漏参数化」** ⇒ 无谓改动 ⇒ mini 线回归面膨胀 | 处置表分类固定；A 类 6 个**只登记不改**（表行数 == 14 ⇒ 漏行红） |
| F5 | **full 线空壳被误当「功能线」** ⇒ 后续批次以为 I-L1/DMA 已就绪 | README + 目录命名 + 处置表注明「零功能」；ADR-078 的 D8 分批表是唯一权威 |
| F6 | **逐线 scope 标注漏一处** ⇒ 未来会话读到自相矛盾的「红线」 | AC7 用 **grep 可判**（三处 MUST 各含「mini 线」标记） |
