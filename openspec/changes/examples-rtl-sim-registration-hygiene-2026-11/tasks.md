# Tasks: examples-rtl-sim-registration-hygiene-2026-11

> **task 定义与判据的唯一权威** = `proposal.md`（§验收标准 · §What Changes）。
> ⚠️ **无 spec delta**（覆盖登记面 + 护栏 ⇒ 零 capability 契约变更）。

## 执行序

`T1`（落选表转机器可读 + 补登 9 项）→ `T2`（`spec_checks` 列纯化）→ `T3`（CI 接线 + 可达性登记）
→ `T4`（新护栏 + `--self-test`）→ `T5`（既有护栏复跑）→ `T6`（回归）→ `archive`

**硬约束**

1. **T4 MUST 晚于 T1 / T2** —— 守恒判据要读这两份表（表未就位 ⇒ 护栏必红）。
2. **T3 MUST 晚于 T4** —— 护栏文件必须先存在（可达性判据 ①「引用存在」查文件）。
3. **写点零交集** —— **MUST NOT** 碰 `verilator-pch-toolchain-fix-2026-11` 的写集
   （并发会话持有）⇒ ⇒ 特别是 `scripts/verify_examples_rtl_sim.sh`（**只读**，不改）。

---

## T1 — 落选表转机器可读（TSV）+ 补登 9 个漏项

- [x] 新增 `.rddf/checks/examples-rtl-sim-exclusions.tsv`：**18 行**（28 被跟踪 − 10 已覆盖）
      ⇒ ⇒ 列：`target` · `category` · `reason` · `return_condition`（**理由与归位条件 MUST 非空**）
- [x] 既有 9 行的理由**逐字迁入** TSV（信息不丢）
- [x] **补登 9 项**：`phase1c_test` · `stream_mux_demo` · `stream_demux_demo` ·
      `stream_arbiter_demo` · `stream_fork_demo` · `phase3a_complete` ·
      `rv32i_phase1_test` · `rv32i_phase2_test` · `rv32i_phase3_test`
- [x] `.rddf/checks/examples-rtl-sim-exclusions.md`：「落选名单」表 ⇒ ⇒ **改为指向 TSV 的指针**
      ⇒ ⇒ 文档内「共 19 项 / 落选 10」的**陈旧计数**订正为 **28 / 18**
      ⇒ ⇒ 保留：入选准则 / 首批的未覆盖项 / 墙钟实测

验证：

```bash
awk -F'\t' '!/^#/ && NF>0 {print NF}' .rddf/checks/examples-rtl-sim-exclusions.tsv | sort -u
grep -n "落选" .rddf/checks/examples-rtl-sim-exclusions.md
```

---

## T2 — `spec_checks` 列纯化（裸整数）

- [x] `.rddf/checks/examples-rtl-sim-targets.tsv` 的 4 行（`fifo_stream` / `uart_tx` / `uart_rx` /
      `axi4_lite`）：`spec_checks` ⇒ ⇒ **裸整数**；散文 ⇒ ⇒ 迁到该行 `note` 列（**信息不丢**）
- [x] **零信息丢失**：迁入的散文 MUST 仍在该行的 `note` 列里（可 grep 到）

验证：

```bash
awk -F'\t' '!/^#/ && NF>0 {print $1"\t"$10}' .rddf/checks/examples-rtl-sim-targets.tsv
```

---

## T3 — CI 接线 + 护栏可达性登记

- [x] `.github/workflows/ci.yml` 的 **`build-and-test`** job 加一步（**静态 ⇒ 不需要 verilator**）
      ⇒ ⇒ ⚠️ **MUST NOT** 碰 `pr-verilator-fast` 段（并发会话持有）
- [x] `.rddf/checks/guard-reachability.md`：登记表**新增一行**（双向 1:1 棘轮）
- [x] `scripts/verify_guard_reachability.sh` PASS

验证：

```bash
scripts/verify_guard_reachability.sh
```

---

## T4 — 新护栏 `scripts/verify_examples_rtl_sim_coverage.sh`

- [x] ① **守恒**：被跟踪 target（从 `examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS` 解析）
      == 覆盖（tsv 里 `kind ∈ {target, soc}` ∩ 被跟踪）∪ 落选（TSV）⇒ ⇒ 且三者互斥
- [x] ② **落选登记完备**：每行 `reason` + `return_condition` 非空 + target 真在被跟踪集合里
- [x] ③ **列纯度**：覆盖表的 `spec_checks` 列 MUST 是裸整数
- [x] ④ **fail-closed**：源文件缺失 / 解析不出集合 ⇒ exit 2（禁静默跳过）
- [x] ⑤ **`--self-test`**：3 条投毒（删落选行 · 清空理由 · 污染 `spec_checks`）各自**转红**
      ⇒ ⇒ 还原复绿
- [x] ⑥ **可执行位** = `100755`（`verify_guard_reachability.sh` 的判据 ②）

验证：

```bash
bash scripts/verify_examples_rtl_sim_coverage.sh
bash scripts/verify_examples_rtl_sim_coverage.sh --self-test
```

---

## T5 — 既有护栏复跑（**只读** `verify_examples_rtl_sim.sh`）

- [x] `scripts/verify_examples_rtl_sim.sh` **11 项 PASS** + `--self-test` 绿
- [x] ⚠️ **零** `integer expression expected`（⇒ ⇒ ②'' 判据恢复生效）
- [x] `gpio_example`（`spec_checks = 0`）**仍**打出 ②'' 告警
- [x] `verify_example_test_wiring.sh` / `verify_example_assertion_quality.sh` 绿

验证：

```bash
bash scripts/verify_examples_rtl_sim.sh 2>&1 | tee /tmp/examples_rtl_sim.log
grep -c "integer expression expected" /tmp/examples_rtl_sim.log   # 期望 0
```

---

## T6 — 回归

- [x] `ctest --test-dir build -LE perf -j$(nproc)` = **255/255**（实测 `100% tests passed, 0 tests failed out of 255`）
- [x] 既有护栏全绿（port-naming / flat-emit / hierarchical / public-api / counts / parity /
      ledger / roadmap / examples 两条 / delta / guard-reachability）

验证：

```bash
ctest --test-dir build -LE perf -j"$(nproc)"
```

---

## archive

- [ ] `openspec archive examples-rtl-sim-registration-hygiene-2026-11 --yes`（**无 spec 合并**
      ⇒ ⇒ 只移动 change 目录）
