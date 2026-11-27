# 产品线登记面（**派生物** —— 由 `scripts/gen_product_lines.py` 生成）

> ⚠️ **本文件 MUST NOT 手工编辑** —— 它是每条产品线 marker 的**派生物**
> （marker = `examples/<line>/.rddf-line.yaml`，是产品线的**唯一真相**）。
> 手改 ⇒ `scripts/gen_product_lines.py --check` 报红 ⇒ 元护栏 C1 报红。
>
> **来源**：ADR-078（`docs/adr/ADR-078-dual-product-lines-and-full-line-scope.md`）
> 的 D8「产品线抽象」· S4c-0。

## 1. 产品线表

| 产品线 | 锚点目录（IP 级） | 成员目录 | 关联 ADR | 关联 capability |
|---|---|---|---|---|
| `riscv-full` | `examples/riscv-full` | `examples/riscv-full`<br>`examples/riscv-full-soc` | ADR-078 | `rv32i-product-lines` `rv32i-data-forwarding` |
| `riscv-mini` | `examples/riscv-mini` | `examples/riscv-mini`<br>`examples/riscv-soc` | ADR-035 ADR-046 ADR-047 ADR-069 ADR-077 | `rv32i-isa-semantics` `rv32i-data-forwarding` `rv32i-memory-timing` `rv32i-dcache-timing` `cpu-cache-subsystem` `rv32i-mmio-channel` |

## 2. CI 锚点分组

| 锚点 | 值 | 归组形态 |
|---|---|---|
| `CTEST_TOTAL` | `286` | **未归组**（T4.2） |
| `VERILATOR_E2E_TOTAL` | `82` | **未归组**（T4.2） |
| `DUAL_BACKEND_PARITY_TOTAL` | `68` | **未归组**（T4.2） |
| `DUAL_BACKEND_PARITY_PAIRS` | `307` | **未归组**（T4.2） |
| `EXAMPLES_TEST_SOURCES` | `27` | **未归组**（T4.2） |

## 3. 脚本处置表

**权威源** = `scripts/product_line_scope.tsv`（14 行）⇒ 本节只是其**摘要视图**。

| 脚本 | 类 | S4c-0 的动作 | 何时才需按线分组 |
|---|---|---|---|
| `scripts/probe_r21_hier_granularity.sh` | **A** | 只登记不改（一次性探针） | 不分组 |
| `scripts/rv32i_rtl_sim_env.sh` | **A** | 只登记不改（mini 线专有 TB） | 不分组 |
| `scripts/verify_dtcm_read_timing.sh` | **A** | 只登记不改（mini 线专有场景） | 不分组 |
| `scripts/verify_rv32i_fw_init_rtl.sh` | **A** | 只登记不改（mini 线专有场景） | 不分组 |
| `scripts/verify_rv32i_elf_e2e.sh` | **A** | 只登记不改（full 线有对应物时再加行） | full 线有 ELF 流程时另议 |
| `scripts/verify_rv32i_official_rtl.sh` | **A** | 只登记不改（语料在 mini 线） | 不分组 |
| `scripts/verify_no_duplicate_rv32i_headers.sh` | **B** | 扫描面改为按 marker 发现；C1 因 full 线 basename 独立而天然绿；C6 快照仍只对 mini | C6 的 per-line 快照 => S4c-1 |
| `scripts/verify_riscv_soc_core_ports.sh` | **B** | 加 --line 参数；full 线无核 => 本脚本对 full 线无对象（不误红） | 53 行表的 per-line 版本 => S4c-1 |
| `scripts/verify_example_assertion_quality.sh` | **B** | **实测形态**：SRC_LIST **自动收集**（含 examples/**/tests/）⇒ ⇒ 新线测试源**自动进扫描面**；但护栏**主动要求**「新增测试源 MUST 登记一行（含真断言数下限）」⇒ ⇒ **登记 1 行**（漏登记当场红） | 持续 |
| `scripts/verify_example_test_wiring.sh` | **B** | **实测形态**：源集合已是 `find examples -path '*/tests/*'`（**全扫**）⇒ ⇒ 新线**自动覆盖** ⇒ ⇒ **S4c-0 零改动**（只需实测新线被覆盖） | 持续 |
| `scripts/verify_examples_rtl_sim.sh` | **B** | S4c-0 的 full 线无 RTL 产物 => tsv 无行是合法的；「有产物但未登记」=> 红 | full 线进 tsv => S4c-1 起 |
| `scripts/verify_hierarchical_rtl.sh` | **B** | 无需改（S4c-0 的 full 线不参与层次化判据） | full 线语料 => S4c-1 起 |
| `scripts/lib/rv32i_obs_vector.py` | **B** | 改为按 marker 发现核头；找不到 => 跳过而非误红 | full 线的向量源 => S4c-1 |
| `scripts/lib/dual_backend_parity.py` | **B** | 清单按 marker 发现；锚点改按产品线分组 | full 线有 dual pair => S4c-1 起 |

⇒ **A 类 6 个**（该线专属 · 只登记不改）· **B 类 8 个**（MUST 参数化）
