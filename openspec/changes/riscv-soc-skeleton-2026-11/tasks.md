# Tasks: S1 结构骨架（`riscv-soc-skeleton-2026-11`）

- **判据来源**: `proposal.md` 的 AC1–AC13 + `design.md` 的实施规格 + ADR-077 的 V1/V2/V4
- **完成定义**: 每个 Task 的「验证」命令绿 → 才勾选；AC 层面由 `verifier` 独立复核
- **零回归红线**: `examples/riscv-mini/**` 与 `include/**` 一字不动（AC5）

---

## T1 骨架：目录 + `SocTraceCounter` + 根装配（编译通过）

- [x] 1.1 建 `examples/riscv-soc/{src,tests}/` + `CMakeLists.txt`（`tb_riscv_soc` target，**不** `add_test`）
- [x] 1.2 `src/soc_trace_counter.h`：2 `ch_in` + 2 `ch_out<ch_uint<16>>` · 饱和计数 · 复位清零
- [x] 1.3 `src/rv32i_soc_top.h`：根 6 根 `__io` + `ch_layer<Rv32iPipelineTop>` + `ch_layer<SocTraceCounter>` + 5 条连线（4 根驱动 + 2 兄弟边）+ 2 根输出
- [x] 1.4 `examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS` 加 `tb_riscv_soc`（28 → 29）

**验证**: `cmake --build build --target tb_riscv_soc -j$(nproc)` ⇒ 0 error / 0 warning

---

## T2 默认模式（解释器路径 · 手推断言 A1–A6）

- [x] 2.1 TB 骨架：模式剥离（`strip_mode_flags`）/ 未知参数 exit 2 / 缺 `--out` exit 2
- [x] 2.2 7 字程序（复用核级常量 + `riscv::ADDI/ADD/SW/JAL` + `static_assert` 交叉校验）
- [x] 2.3 装载 7 字（`ext_rst=1` 期）+ 填 NOP 至 64 字 + 释放复位
- [x] 2.4 断言 A1–A6（初始 0 / 装载期保持 0 / `store_count==1` / `writeback_count==3` / 单调不减 / 尾段不增）
- [x] 2.5 打印 `RESULT: checks=<L> fails=0`

**验证**: `build/examples/riscv-soc/tb_riscv_soc` ⇒ `RESULT: checks=6 fails=0`
**反向自证**: 把 `retire_event` 接到 `obs_data_write_en`（同源）⇒ A4 必红

---

## T3 发射 + `check_products` 四件套

- [x] 3.1 `make_emit_design`（根名常量 `kSocRootName = "top"`）
- [x] 3.2 `--emit-rtl [--hierarchical] --out <dir>` 走库层 CLI
- [x] 3.3 `--check-products`：值侧 `kExpectedModules`(10) / `kExpectedTopPorts`(8) / `kMinVaryingObservations`(2) + 激励面 4 根 + 观测面 2 根 + 激励 ≥10 拍值不同
- [x] 3.4 `find_verilator` / `run_lint` 平台胶水（照 `tb_axi_soc_example.cpp`）

**验证**:
```
build/examples/riscv-soc/tb_riscv_soc --emit-rtl --hierarchical --out /tmp/opencode/soc_hier
⇒ 产物 10 个 module（grep -c '^module ' ⇒ 10）
build/examples/riscv-soc/tb_riscv_soc --check-products --out /tmp/opencode/soc_chk
⇒ PRODUCTS-CHECK: PASS（CHECK 1..4 全过）
```
**反向自证**: ① 从观测面移出 `writeback_count` ⇒ CHECK 4 必红 ② `kExpectedModules` 删一个名字 ⇒ CHECK 1 必红

---

## T4 独立 Verilog TB（`tb_riscv_soc.sv`）

- [x] 4.1 只读 8 根根端口 · 装载 7 字 → 释放 → 60 拍
- [x] 4.2 判据 A1–A6 的 Verilog 版（与 C++ 侧同一组手推字面量）
- [x] 4.3 `EXPECT-SRC:` 头注释指向 `.rddf/checks/soc-rtl-expectations.md#riscv-soc`

**验证**: `bash scripts/verify_examples_rtl_sim.sh` 跑 riscv-soc ⇒ `RESULT: checks=L fails=0`（与 C++ 侧同 L）
**反向自证**: 把某条期望改 1 位 ⇒ 该条必红

---

## T5 登记 8 处 + 新登记面

- [x] 5.1 `.rddf/checks/examples-rtl-sim-targets.tsv` 新增 `kind=soc` 行（`interp_checks == min_checks` · `scenarios`/`spec_checks` 必填）
- [x] 5.2 `examples-rtl-sim-exclusions.tsv` 守恒 `29 == 17 ∪ 12`
- [x] 5.3 锚点 `REGISTERED_SOC` 1 → 2（`REGISTERED_HIER` 不动）
- [x] 5.4 `.rddf/checks/soc-rtl-expectations.md` 新增 `## riscv_soc {#riscv-soc}`（手推）
- [x] 5.5 根 `AGENTS.md`：examples main() 32 → 33（`CTEST_TOTAL` 不动）· Structure 树加 `riscv-soc/`
- [x] 5.6 `improvement-suggestions.md` 索引行（AC10 · 状态词 `**draft**`）
- [x] 5.7 ~~`.rddf/checks/riscv-core-complex-ports.md`~~ ⇒ **改落 `examples/riscv-soc/AGENTS.md`**（单一真相源 · 见 design §6.1）
- [x] 5.8 `docs/CHANGELOG.md` + `examples/riscv-soc/AGENTS.md`（含 34 端口契约表）+ `docs/developer_guide/rv32i-rtl-delivery-and-sim.md`

**验证**: `verify_examples_rtl_sim_coverage.sh` · `verify_test_counts.sh` · `verify_agents_size.sh` · `verify_improvement_index.sh` · `verify_docs_links.sh` 全绿

---

## T6 护栏（AC11 新面 + AC12 回归面）

- [x] 6.1 新护栏 `scripts/verify_riscv_soc_core_ports.sh`：V1 端口表 == AGENTS.md 的 34 行 · V2 == 34 · V3 与 C6 闭环 · V4 无微架构副本 · **V5 两条兄弟边（AC8）** · 7 条投毒各自必红 · 已挂 CI
- [x] 6.2 `verify_no_duplicate_rv32i_headers.sh` 的扫描面扩到 `examples/riscv-soc/src/`（ADR-077 R4 的缓解）
- [x] 6.3 「登记面 ↔ TB 期望集逐字比对」= `verify_examples_rtl_sim.sh` 的既有 ⑥（`RESULT` 条数等式）+ ⑧（manifest 端口表 == 产物）⇒ riscv-soc 行已覆盖
- [x] 6.4 AC11 其余护栏：`check_products` · `verify_example_only_headers` · `verify_downstream_consumer` · `verify_port_naming` · `verify_component_naming`
- [x] 6.5 AC12 回归面：`verify_example_test_wiring` · `verify_flat_rtl_emit` · `verify_hierarchical_rtl` · `verify_example_assertion_quality` · `verify_guard_reachability` · `verify_roadmap_fragments` · `./run_all_ported_tests.sh`（29 全 PASS）

**验证**: 逐条跑，全绿

---

## T7 收口

- [x] 7.1 AC5 回归锚点：`git diff 4c04821 -- examples/riscv-mini/ include/` **为空** + `git status --porcelain` 为空 + `tb_rv32i_soc` 仍 `checks=24 fails=0`
- [x] 7.2 AC13 平台声明：`examples/riscv-soc/AGENTS.md` 的「已知边界」+ `developer_guide` §0 均已写「verilator 依赖 ⇒ CI 证据仅 Ubuntu」
- [x] 7.3 遗留清扫：`grep` 四类（TODO / `SKIP()` / 注释代码 / DEBUG 输出）全部为空
- [ ] 7.4 `openspec archive riscv-soc-skeleton-2026-11 --yes --skip-specs`（无 spec delta · 先例 6/6）
