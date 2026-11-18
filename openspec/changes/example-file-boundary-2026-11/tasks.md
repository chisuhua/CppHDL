# tasks — `example-file-boundary-2026-11`

> ⚠️ **设计不在本文件**：决策面 = ADR-074 / ADR-046 / ADR-075 + 蓝图 §4.3。
> ⚠️ **顺序**：本 change 在 `axi4-verif-driven-cpp` → `emit-cli-into-example` **之后**（G-2）。
> ⚠️ **3.2（批次 F）受 G-1 约束**：**F1 本身就是 G-1 的交付物**（architect 执行）⇒ ⇒
> F1 未闭前 **F2–F9 MUST NOT 开工**；**3.1（批次 A–E）可独立推进**。

## 批次 A — 头文件归位（AC1 · AC2 · AC3 · AC4 · AC7）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | `git mv` 两个顶层头到 `examples/axi4/`（保留历史） | `include/axi4/` → `examples/axi4/` | AC1 | ☐ |
| A2 | **3 个 examples 消费者改裸 include**：`tb_axi4_lite_example.cpp` · `axi_soc_example.cpp` · `emit_rtl_soc.cpp`（**`emit_rtl.cpp` 已被前序 change 删除**） | `examples/axi4/` | AC3 | ☐ |
| A3 | 库公开面归零核验 | `include/**` | AC2 | ☐ |
| A4 | 根 `tests/` 不再引用这两个头（由 B1–B4 完成后核验） | `tests/**` | AC7 | ☐ |
| A5 | **搬入的 2 个测试**（`examples/axi4/tests/*.cpp`）也用裸 include（靠 helper 的 `-I examples/axi4`） | `examples/axi4/tests/` | AC3 | ☐ |
| A6 | **C2 扩展（最小面 + 剥注释）**：扫描面仍只 `tests/CMakeLists.txt`，匹配串 `examples/riscv-mini/src` → `examples/`，**移植 C7 的剥注释 + 括号配平**；新增 fixtures：① 注释里出现 `examples/` ⇒ **绿** ② `target_include_directories(… examples/axi4)` ⇒ **红** ⇒ 并在 AC4 里点名 C7 一并绿 | `scripts/verify_no_duplicate_rv32i_headers.sh` | AC4 | ☐ |

## 批次 B — 消费者归位 / 换 DUT（AC5 · AC6 · AC8 · AC9）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | `tests/test_axi_lite_verilator_e2e.cpp`：删顶层头 include ⇒ `using` 改库组件 ⇒ DUT 换 `AxiLiteSlave<32,32,4>` ⇒ 顶部注释同步（**把陈旧的「10 inputs / 7 outputs」订正为 12 / 8**）⇒ 路径 / 用例名 / SECTION / 断言**零改动** | 该文件 | AC5 | ☐ |
| B2 | 该 e2e 仍绿（verilator 在 `build/verilator-install/bin` 或 PATH） | ctest | AC6 | ☐ |
| B3 | `tests/test_axi_lite_test_master.cpp` → `examples/axi4/tests/`（**CTest 条目名不变**） | `examples/axi4/tests/` | AC9 | ☐ |
| B4 | `tests/test_verif_single_source.cpp` **按准则拆分**：凡以 `ch_device<axi4::example::AxiLiteTop>` / `LateAxiLiteTop` 为 DUT 的用例 → `examples/axi4/tests/test_verif_axi4_lite.cpp`；其余（扫描器用例）留 `tests/` ⇒ **共享 helper 一并搬**（`verif_path()` / `LateAxiLiteTop` / `read_file()`） | 两处 | AC8 | ☐ |
| B5 | **TEST_CASE 名集合守恒（判据）**：拆分前后差集 == ∅（源码解析或 `ctest -N` 对照）⇒ 结果记录在 note | 记录 | AC8 | ☐ |

## 批次 C — 构建与锚点（AC8 · AC9 · AC10 · AC20）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | `examples/axi4/CMakeLists.txt` 增 `axi4_catch_test` helper：链 `catch_amalgamated` + include 集（`include` · `tests` · `examples/axi4`）+ `add_test` + **`LABELS base`** + `TIMEOUT 300` + **`CPPHDL_SOURCE_DIR` 编译定义** | 同上 | AC8 · AC9 | ☐ |
| C2 | 根 `tests/CMakeLists.txt` 去掉迁走的注册项（**不得**重复注册） | `tests/CMakeLists.txt` | AC9 | ☐ |
| C3 | **锚点按实测回填**：`CTEST_TOTAL` 268 → **269** · non-perf **257 → 258** · `-L base` **231 → 232** · `EXAMPLES_TEST_SOURCES` 18 → **20** | 根 `AGENTS.md` | AC10 | ☐ |
| C4 | **手工改 `tests/test_*.cpp` 计数行**：现写 **145** 但实测 **159** ⇒ 搬走 1 个后为 **158**（**MUST 用实测值，禁「原值 −1」**） | 根 `AGENTS.md` | AC10 | ☐ |
| C5 | **登记 `CPPHDL_BUILD_EXAMPLES=OFF` 的接受项**（被搬走的 2 个条目消失 ⇒ 锚点只在默认配置成立）⇒ 写在该锚点旁 + 指向本 change | 根 `AGENTS.md` / `.rddf/checks/` | AC20 | ☐ |

## 批次 D — 登记面（AC11 · AC12 · AC13 · AC14）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| D1 | 棘轮豁免表 **23 行**只改第一列路径（**不删行**） | `.rddf/checks/port-naming-exempt.tsv` | AC11 | ☐ |
| D2 | **公开面移除登记 5 条**：2 条 `nofile`（两个头的 glob）+ 3 条 `removed`（类名 · `scope=include/`）⇒ 同步 `EXPECTED_PROBES` 46 → **51**、`EXPECTED_REMOVAL_ROWS` 25 → **30**（登记行 MUST 用 `**移除**`）⇒ `public-api-surface.md` 同章逐条登记 | `.rddf/checks/public-api-removals.tsv` · `public-api-surface.md` | AC12 | ☐ |
| D3 | **输入端口登记表的 3 行保留**：把 `input_port_registry.py` 的 `header_of()` 搜索根从 `include/**` 扩到 `include/**` + `examples/**`（否则 R1 报「登记过时」）⇒ 只更新这 3 行的**消费者路径**列 | `scripts/lib/input_port_registry.py` + 登记表 | AC13 | ☐ |
| D4 | **零缩面核对**：`verify_rst_n_wiring.sh` 的 `COMPONENTS_W_RST_N` 组件数 **搬家前后相同**（3 个组件仍在表内）⇒ 若数量变化 ⇒ 说明走了「删行」路线 ⇒ 回退 | `scripts/verify_rst_n_wiring.sh` | AC13 | ☐ |
| D5 | `CH4_ANCHOR` **+2 行**（两个新 `examples/axi4/tests/*.cpp` 的断言数下限） | `scripts/verify_example_assertion_quality.sh` | AC14 | ☐ |
| D6 | **新建 capability 的护栏**：`scripts/verify_example_only_headers.sh`（末段匹配 · 作用域 `include/ src/ tests/ examples/ tools/` · 三态区分孤儿/歧义/违规 · 登记表双向**且反向为红**）+ 登记表 `.rddf/checks/example-only-headers.tsv` + `--self-test` **6 条投毒**（含 2 条**必绿**：孤儿头 / 歧义 ⇒ 退出码 0）⇒ 并挂进 CI | 新增脚本 + 登记表 | AC21 | ☐ |

## 批次 E — 文档与产物等价（AC15 · AC16）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| E1 | 文档同步 + **旧路径归零**：三份 `AGENTS.md` · `docs/usage_guide/**` · `.rddf/checks/**`（新增记录）；**排除** `openspec/changes/**`（含本 change 与**仍活跃的**前置 change）· `docs/archive/**` · `docs/architecture/**` · `docs/adr/**` · `docs/reports/**` · **整份 `docs/CHANGELOG.md`** · `.rddf/improvements/**` · `.rddf/reviews/**` | 同上 | AC15 | ☐ |
| E2 | **产物等价基线**：搬前产 4 个 sha256（AXI4-Lite / SoC × `top.v` + `manifest.json`）+ 复算命令 + 前提（同 build dir / 同 emitter / 前置已完成的 HEAD）记入 `.rddf/checks/` | `.rddf/checks/` | AC16 | ☐ |
| E3 | 搬后重产 ⇒ 4 个 sha256 **相同** | diff | AC16 | ☐ |

## 批次 F — 3.2 SoC checker 机制下沉（**F1 = G-1 的交付物 · architect 执行 ⇒ 未闭前 F2–F9 不开工**）

| # | 任务 | 判据 | 状态 |
|---|---|---|---|
| F0 | **开工前准备（不受 G-1 约束 ⇒ 可与 F1 并行；MUST 在任何代码改动之前完成）**：① **采基线**：MUST 在本 change 任何代码改动之前、且 MUST 在 `emit-cli-into-example` **之后**（SoC 入口形态已变）⇒ **分模式两套**（默认模式 / `--check-products` 模式）+ 比较面 = 该 `--out` 目录**全部落盘物**（含 `eq/`）+ 记录 verilator 可用性 ⇒ 前提 = 同 build 配置 + **唯一变量 = `examples/axi4/emit_rtl_soc.cpp`**（**不是**「同 emitter」）② **订正上游活跃 change** `rtl-verification-single-source-2026-11` 的同款陈旧目标（`proposal.md` 的 `:21` / `:34` / `:46` + `tasks.md:29`）⇒ ⇒ 该 change **尚未归档** ⇒ **不是**「历史记录」豁免 | AC23 · AC25 | ☐ |
| F1 | **architect 裁决（architect 交付 · builder 只验证）**：① 蓝图 §4.3 **6 处**订正（**按锚点文本**定位 · 行号仅辅助）——「G-发射 总表行」·「缺口 ② 行」（含悬空 `415-483`）·「§4.3 行 1」·「§4.3 行 2」（机制落点：`src/rtl_emit/` ⇒ **公开头** `include/rtl_emit/`）·「§4.3 末尾『不动』条」（与 ⑤ 冲突）·「§5.2 M4 行」② **9 个符号的逐符号切分表**（`kExpectedModules` · `kExpectedTopPorts` · `product_modules` · `find_verilator` · `make_inputs` · `make_observations` · `make_stimulus` · `tamper_hier` · `check_products`）—— 逐个定归属（库层 / example / 不下沉），**禁**「待定」，「下沉」**须给具名第二消费者 + 调用点** ③ **形态裁决**（默认模式是否恒发层次化 ⇒ 分支 A / B）④ **ADR-074「不覆盖」条**是否需同步措辞 ⑤ **`.rddf/improvements/rtl-verification-single-source-2026-11.md`（draft 活文档）同批订正** + **2026-11-17 蓝图 §7 登记「默认模式扁平产物」为已知残留** ⇒ ⇒ ⚠️ 蓝图属 **architect 写集** ⇒ ⇒ builder 不可写 ⇒ ⇒ **MUST NOT** 由 builder 代改 | AC22 · AC23 | architect 交付 |
| F2 | **按 F1 的切分表下沉**到**公开头** `include/rtl_emit/*.h`（实现落 `src/rtl_emit/*.cpp`）⇒ ⇒ ⚠️ **该头的实现 TU 必然 include 它 ⇒ 按 R2 定义不算「消费者」**（实现 ≠ 使用）⇒ ⇒ 故 R2 反向判据不会误红 ⇒ ⇒ **linter 路径作为参数**（**不下沉** `find_verilator`）⇒ ⇒ **只下沉编排，判据值留 example**（ADR-074「不覆盖」）⇒ ⇒ 下沉范围**由切分表定**，不是写死的 3 个 | AC17 | ☐ |
| F3 | `emit_rtl_soc.cpp` 的**壳层**（= `main()` 本体 + 它调用的**库层**入口）退化为「组装参数 + 调用」⇒ **结构判据**：被裁下沉的符号在 example 侧**无定义** + 新 API 调用点 == 2（**不用行数靶**）⇒ ⇒ ⚠️ example 侧产品专属 helper（`make_inputs` / `make_observations` / `check_products` 的判据值）**无论是否被 `main` 直接调用，一律归 example**（归属以 F1 切分表为准） | AC17 | ☐ |
| F4 | **第二消费者**：把 `examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` 的 module 集/端口计数改为调用新 API；做不到 ⇒ 明写为带门的 follow-up（否则下沉 = 第 5 份实现） | AC17 | ☐ |
| F5 | **常驻判别力回归**：4 项判据各自注入错期望 ⇒ 必红；④″ 仍执行；退出码语义不变 ⇒ ⇒ ⚠️ **已知边界**：`emit_rtl_soc.cpp:263-273` 的 CHECK 3 在 verilator 不可用时打印 `SKIP` 且**不计 failures** ⇒ ⇒ 基线记录 MUST 含 verilator 可用性 / 版本 | AC18 | ☐ |
| F6 | **公开面登记（真载体）**：新增公开头 MUST 进 **`.rddf/checks/example-only-headers.tsv` 豁免通道** + `docs/CHANGELOG.md` 一行 ⇒ ⇒ ⚠️ **实现 TU 的 include 不算「消费者」**（R2 定义）⇒ ⇒ 该形态**可登记** ⇒ ⇒ ⚠️ **如实声明**：`public-api-surface.md` 的扫描范围 = **顶层 `include/*.h`** + 两个已裁决 core 头 ⇒ ⇒ `include/rtl_emit/*.h` 属**子目录头 ⇒ 不在扫描范围** ⇒ ⇒ 登记到那里**零判别力**（只作记录，不构成护栏）⇒ ⇒ 该事实 MUST 写进登记条目本身 | AC19 | ☐ |
| F7 | **产物形态（分支 A 专属）**：SoC 发射入口**恒发层次化** ⇒ ⇒ 护栏 ③ 对 `kind=soc` 行的**发射语义 MUST 同步**（它现在发扁平产物）+ 产物形态 **MUST 登记** ⇒ ⇒ 护栏 ⑧（顶层端口表）在**层次化**产物上仍绿 ⇒ ⇒ 分支 B 下本条**不执行** | AC24 | ☐ |
| F8 | **产物等价核对**：用 F0 采的基线逐项比对（**分模式两套** + **全部落盘物**）⇒ ⇒ 差异 MUST 为**零**；改动过产物 ⇒ 必红 | AC25 | ☐ |
| F9 | **切分表为空的合法终态**：若 F1 裁定**无符号可下沉** ⇒ ⇒ 本批次零代码改动 ⇒ ⇒ 实质结论 MUST 由 **F1** 在 **2026-11-17 蓝图 §7「已知残留与边界」** 留一条**已收口**残差（闭口证据 = **存在的路径** · 逗号分隔 · 无空格 · 无反引号）⇒ ⇒ ⚠️ **不是** `.rddf/checks/arch-residual-registry.md`：那本护栏的台账**硬编码**为**另一本**蓝图（`scripts/lib/ledger_residual.py:17`）⇒ ⇒ 往它加行**必红**（「结算行数不一致」）⇒ ⇒ 本条判据 = **蓝图 §7 段落自证** | AC26 | ☐ |
## 复核命令

```bash
cmake --build build -j$(nproc)

# include 文本归零（AC3）
grep -rn '"axi4/axi4_lite_example_top\.h"\|"axi4/axi_soc_example_top\.h"' examples tests || echo 'OK'

# 依赖方向（AC4：C2 扩展后 + C7）
bash scripts/verify_no_duplicate_rv32i_headers.sh

# 计数（AC9 · AC10；默认配置）
ctest --test-dir build -N | tail -1
ctest --test-dir build -N -LE perf | tail -1
ctest --test-dir build -N -L base | tail -1
bash scripts/verify_test_counts.sh

# 登记面（AC11–AC14）
bash scripts/verify_port_naming.sh
bash scripts/verify_public_api_visibility.sh
bash scripts/verify_input_port_required_registry.sh
bash scripts/verify_rst_n_wiring.sh
bash scripts/verify_example_assertion_quality.sh

# 运行
bash scripts/verify_example_test_wiring.sh
bash scripts/verify_examples_rtl_sim.sh
./run_all_ported_tests.sh
```

## 复核命令 —— 批次 F（**仅 G-1 闭后执行**）

```bash
BP=docs/architecture/2026-11-17-rtl-verification-single-source.md

# AC17 —— 壳层侧无被下沉的符号（示例：product_modules）
if grep -q '^std::set<std::string> product_modules' examples/axi4/emit_rtl_soc.cpp; then
  echo 'FAIL: 壳层侧仍有 product_modules 的定义'; exit 1
fi
echo 'OK: 壳层侧无定义'

# AC22 —— 切分表覆盖 9 个符号（**限定在蓝图 §4.3 段内** · 命中表格行 · **带退出码**）
SEC=$(sed -n '/^### 4.3/,/^### 4.4/p' "$BP")
for s in kExpectedModules kExpectedTopPorts product_modules find_verilator \
         make_inputs make_observations make_stimulus tamper_hier check_products; do
  n=$(printf '%s\n' "$SEC" | grep -c "^| .*$s")
  if [ "$n" -lt 1 ]; then
    echo "FAIL: §4.3 段缺 $s 的切分表行"; exit 1
  fi
done
if printf '%s\n' "$SEC" | grep -q '待定'; then
  echo 'FAIL: 切分表含「待定」'; exit 1
fi
echo 'OK: 切分表 9 符号齐备且无待定'

# AC23 —— 陈旧表述清零（**按位置**限定；豁免 = 蓝图 §5.1 的估算段 ⇒ 不做全文一刀切）
if sed '/^### 5.1/,/^### 5.2/d' "$BP" | grep -q '483\|~60\|415-483'; then
  echo 'FAIL: 蓝图仍有陈旧表述（483 / ~60 / 415-483）'; exit 1
fi
if grep -q '483\|~60\|415-483' .rddf/improvements/rtl-verification-single-source-2026-11.md; then
  echo 'FAIL: draft 活文档仍有陈旧表述'; exit 1
fi
echo 'OK: 陈旧表述清零'

# AC24 —— 形态（分支 A：默认模式恒层次化 ⇒ module 数 == 5 = 4 成层 IP + 根 top）
d=$(mktemp -d /tmp/opencode/soc_flat.XXXXXX)
./build/examples/axi4/emit_rtl_axi_soc --out "$d" --emit-hygiene
if test "$(grep -c '^module ' "$d/top.v")" -eq 5; then
  echo 'OK: 默认模式恒层次化'
else
  echo "FAIL: 默认模式 module 数 = $(grep -c '^module ' "$d/top.v")（期望 5）"; exit 1
fi

# AC26 —— 切分表为空的终态 ⇒ 登记在**蓝图 §7**（不是 arch-residual-registry：其台账硬编码另一本蓝图）
if sed -n '/^## 7\./,$p' "$BP" | grep -q '§4.3'; then
  echo 'OK: 蓝图 §7 已登记'
else
  echo 'FAIL: 蓝图 §7 未登记该残差'; exit 1
fi
```

## ⚠️ 未做 / 本次不做

| 项 | 原因 |
|---|---|
| 3.2（checker 机制下沉） | **G-1 未闭 ⇒ MUST NOT 开工**（architect 域）⇒ ⇒ 其中 **F1 本身**就是 G-1 的交付物（architect 执行）⇒ ⇒ **MUST NOT** 由 builder 代改蓝图 |
| `find_verilator()` 下沉 | 仓内已有裁决（fork 外部 lint 污染失败归因）⇒ linter 路径作为参数 |
| **默认模式扁平产物的合规化**（分支 B） | 分支 B = 改 `rtl-soc-output` 既有 MUST ⇒ ADR 修订 + MODIFIED ⇒ **不在本 change 实施面** |
| **module 集 / 顶层端口数的多载体收敛** | 现状 3 处载体（`kExpectedModules` / `kExpectedTopPorts` · `soc-rtl-expectations.md` · tsv 散文）⇒ ⇒ ADR-074「判据值留 example」⇒ example 侧常量 MUST NOT 被删 ⇒ ⇒ **登记为已知残留** ⇒ 独立立项 |
| 给 `AxiLiteSlave` 的端口具名 | 公开面变更 ⇒ 需 architect（ADR-075 域） |
| 给 `AxiLiteTestMaster` 的 23 个端口具名 | 成层子 module 端口 ⇒ 独立立项 |
| 把 `AxiLiteTop` / `AxiSocTop` 提升为库组件 | 与本 change 方向相反 ⇒ 需 ADR |
| RV32I 侧顶层组件归位 | 独立立项 |
| 删 `emit_rtl.cpp` | 属 `emit-cli-into-example`（前序 change） |
