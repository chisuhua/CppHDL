# 验收 —— `examples/spinalhdl-ported/` 8 个产品面 target 迁移到 `.verif` 单一真相源

- **change**: `openspec/changes/archive/2026-11-21-spinalhdl-ported-verif-migration-2026-11/`
  + 框架前置 `openspec/changes/archive/2026-11-21-verif-history-primitives-2026-11/`
- **裁决**: **PASS**（0 失败项 · 已用轮次 **1**）
- **取证日**: 2026-11-21 · 取证环境 = 本机 `main` · 提交 `eefa890` / `f94d9cd`

---

## 1 判据来源（按优先级）

| # | 来源 | 内容 |
|---|---|---|
| 1 | **用户明确验收条件**（2026-11-21 裁决） | 8 个已登记产品面 target 全迁 · 三角色分离（`*_top.h` + `tb_*.cpp` + `tb_*.verif`）· 端口全具名 · `.verif` 单一真相源 · 删除 `emit_rtl*.cpp` 薄壳 · 同步产品面清单/期望源/文档 · **不得降级判据面** |
| 2 | `proposal.md` 的 **AC1–AC9** | 见 proposal（归档副本已被勾选，逐条由本报告的 §2 取证） |
| 3 | 变更自身声明（commit message / `docs/CHANGELOG.md`） | 断言数、method 变更登记、已知局限 |
| 4 | `ADR-074`（`.verif` 单一真相源）/ `ADR-076`（历史/序列原语）的 Verification 章 | 原语语义、两端同源、扫描期良构性 |

---

## 2 逐条判定 + 证据

### 2.1 用户验收条件（逐 target）

| target | 设计头 | 驱动 / 验证源 | `RESULT`（解释器） | RTL 侧 | 结论 |
|---|---|---|---|---|---|
| `tb_pwm_example` | `pwm/pwm_top.h` | `tb_pwm_example.cpp/.verif` | `checks=29 fails=0` | `checks=29 fails=0` | ✅ |
| `tb_counter_simple_example` | `counter/counter_simple_top.h` | 同名 | `checks=258 fails=0` | `258/0` | ✅ |
| `tb_counter_example` | `counter/counter_top.h` | 同名 | `checks=9 fails=0` | `9/0` | ✅ |
| `tb_uart_tx_example` | `uart/uart_top.h` | 同名 | `checks=49 fails=0` | `49/0` | ✅ |
| `tb_uart_rx_example` | `uart_rx/uart_rx_top.h` | 同名 | `checks=32 fails=0` | `32/0` | ✅ |
| `tb_fifo_stream_example` | `fifo/fifo_top.h` | 同名 | `checks=20 fails=0` | `20/0` | ✅ |
| `tb_gpio_example` | `gpio/gpio_top.h` | 同名 | `checks=11 fails=0` | `11/0` | ✅ |
| `tb_quadrature_encoder_example` | `quadrature_encoder/quadrature_encoder_top.h` | 同名 | `checks=15 fails=0` | `15/0` | ✅ |

证据（一次运行产出，非推断）：

```
bash scripts/verify_examples_rtl_sim.sh   →  rc=0
  ✅ tb_counter_example            RESULT: checks=9 fails=0（scenarios=5 · spec_checks=8）
  ✅ tb_counter_simple_example     RESULT: checks=258 fails=0（scenarios=2 · spec_checks=258）
  ✅ tb_fifo_stream_example        RESULT: checks=20 fails=0（scenarios=4 · spec_checks=18）
  ✅ tb_uart_tx_example            RESULT: checks=49 fails=0（scenarios=3 · spec_checks=45）
  ✅ tb_uart_rx_example            RESULT: checks=32 fails=0（scenarios=6 · spec_checks=30）
  ✅ tb_pwm_example                RESULT: checks=29 fails=0（scenarios=5 · spec_checks=29）
  ✅ tb_gpio_example               RESULT: checks=11 fails=0（scenarios=6 · spec_checks=0）
  ✅ tb_quadrature_encoder_example RESULT: checks=15 fails=0（scenarios=5 · spec_checks=12）
```

`emit_rtl*.cpp` 已全部消失（`ls examples/spinalhdl-ported/*/emit_rtl*.cpp` 空）；
8 个旧 `<impl>.cpp` 已 `git rm`；target 改名后 CTest 条目名不变
（`ctest -N -L spinalhdl-ported` 仍是 24 条）。

### 2.2 AC1 —— `generated=yes` × 8（⑥⁗）

- `awk -F'\t' '!/^#/ && NF>3' .rddf/checks/examples-rtl-sim-targets.tsv`：8 行的
  `generated` 列全部 `yes`；`generated=no` 的只剩 `riscv-mini-*`（2 行，`kind=dut/hier`）
  与 `tb_phase3a_example`（1 行，手写 TB —— 见 §5 的 fixture 换载体）。
- **③ 判定**：✅ 满足。

### 2.3 AC2 —— 三方等式 + 双侧各自登记（⑥ / ⑨）

| target | `min_checks` | `interp_checks` | `VERIF-CHECKS` | 运行期 `checks` |
|---|---|---|---|---|
| pwm | 29 | 29 | 29 | 29 |
| counter | 9 | 9 | 9 | 9 |
| counter_simple | 258 | 258 | 258 | 258 |
| uart_tx | 49 | 49 | 49 | 49 |
| uart_rx | 32 | 32 | 32 | 32 |
| fifo | 20 | 20 | 20 | 20 |
| gpio | 11 | 11 | 11 | 11 |
| quadrature | 15 | 15 | 15 | 15 |

- `verify_examples_rtl_sim.sh` 的 ⑥（断言 == 登记）与 ⑨（解释器条数）均绿。
- **③ 判定**：✅ 满足。

### 2.4 AC3 —— 锚点双向 1:1 · TB 无 TODO 残留（⑥'' / ⑥‴）

- 期望源每节的 `#### scenario:` 行数与 `.verif` 的 `@scenario` 数逐 target 相等
  （2/3/4/5/5/6/6/5 与上表的 `scenarios` 列一致）。
- `grep -rn "TODO\|FIXME\|XXX\|HACK" examples/spinalhdl-ported/{pwm,counter,uart,uart_rx,fifo,gpio,quadrature_encoder}/`
  → **0 命中**（排除生成物 `.sv`）。
- **③ 判定**：✅ 满足。

### 2.5 AC4 —— 新鲜度 / 端口表 / 生成物形态（⑥' / ⑧ / ⑧''）

- ⑥'（逐字节重现）：未命中「生成物与验证源不一致」。
- ⑧（四件套）：manifest 端口表 == 产物端口表，全部 target 一致；产物端口名全部
  `top_<C++ 成员名>`（无 `top_unnamed_*`）⇒ 由 **⑦ 投毒 6/7/13** 反向证明该判据有判别力。
- ⑧''（形态）：module 集与顶层端口数符合各节「产物形态（手推）」段。逐 target 实测
  （从各 TB 的 manifest `port_table` 计数，= 框架 2 + 根自有）：
  counter_simple **3** · counter **5** · pwm **5** · uart_tx **6** · uart_rx **6** ·
  fifo **8** · gpio **7** · quadrature **9** ⇒ 与期望源各节登记的根自有端口数
  （1 / 3 / 3 / 4 / 4 / 6 / 5 / 7）逐一相符；module 集均为单 `top`。
- **③ 判定**：✅ 满足。

### 2.6 AC5 —— 每条被测性质至少有 1 条变异使其转红

全部变异实测（临时产物上注入，仓内产物零改动；每条先确认 `compile_rc=0`）：

| target | 变异 → 实测 | 抓到的判据 |
|---|---|---|
| pwm | M1 比较恒 0 → 4 · M2 enable 直通 → 6 · M3 `<=` → 4 | 段长 / 恒低 / 两条 metamorphic |
| counter | M1 去 clear 优先级 → 6 · M2 去 enable 门控 → 4 · M3 步长 2 → 5 | 绝对 + 相对判据 |
| counter_simple | M1 `+0` → 255 · M2 `+2` → 255 | 逐拍绝对计数 |
| uart_tx | M1 → 16 · M2 起始位高 → 25 · M3 位宽减半 → 16 · M4 停止位低 → 2 | 位窗 + 空闲电平 |
| uart_rx | M1 位移方向 → 7 · M2 采样点硬编码 → 2 · M3 `valid` 恒 1 → 16 | 解码 / 相位夹逼 / 单拍性 |
| fifo | M1 `full` 恒 0 → 1 · **M1b 旧 D5 公式 → 1** · M2 pop 不推进 → 11 · M3 push 不推进 → 14 | 容量边界 + 序列恒等 |
| gpio | M1 中断恒 0 → 1 · M2 忽略使能 → 1 · M3 输出不寄存 → 2 | 边沿 / 使能 / 1 拍寄存 |
| quadrature | M1 无加法 → 3 · M2 Z 清零失效 → 10 · M3 方向取反 → 3 | 4 倍频 / 清零 / 方向 |

逐 target 的变异表与 `check_id` 见 `.rddf/checks/examples-rtl-expectations.md` 的各节。
**无效变异：0 条**（无「恒绿」条目需要如实登记）。
- **③ 判定**：✅ 满足。

### 2.7 AC6 —— 判据面不降级

| 原形态 | `.verif` 形态 | 是否降级 |
|---|---|---|
| counter_simple 周期 256（相对） | `WAIT value low` 锚回绕 + `EXPECT_SEQ_SELF value 256 256` | 否（**已知局限**：128 整除 256 ⇒ 抓不到步长 2，由绝对判据承担；与原手写 TB 同局限） |
| counter `Δ == enable`（逐拍） | `EXPECT_SEQ_STEP value enable 40` + `EXPECT_SEQ_SELF value 63 1` | 否 |
| pwm 窗口内高电平数 | 段长（先 `WAIT low` 再 `WAIT high`）+ 整周期 `EXPECT_SEQ_COUNT` | 否 |
| uart_tx 边沿时间戳测位宽 | **434 锚定位窗 + 每窗首/尾双端断言** | **方法变更**（已登记）：判别力不降（每位被钉在恰好 434），代价 = 434 成为 impl-contract |
| uart_rx 采样相位夹逼 | 同形态（`prescale=99` + 3 探针） | 否 |
| fifo 序列恒等 | `EXPECT_SEQ_*` | 否 |
| quadrature 往返恒等 | 正向 +4 / 反向 −4 ⇒ 回 0 | 否 |

- **③ 判定**：✅ 满足（方法变更已在 `proposal.md` §已知局限 3 与
  `examples-rtl-expectations.md` 的 uart_tx 节登记）。

### 2.8 AC7 —— 护栏三件套

```
bash scripts/verify_examples_rtl_sim.sh              → rc=0 · PASS（17 项）
bash scripts/verify_examples_rtl_sim.sh --self-test  → PASS（基线绿 · 21 投毒各自转红 · 2 条必绿仍绿 · 还原复绿）
bash scripts/verify_examples_rtl_sim_coverage.sh     → PASS
```

- **③ 判定**：✅ 满足。

### 2.9 AC8 —— 运行面 + 棘轮 + 归属/断言质量

```
./run_all_ported_tests.sh                       → PASS: 27 · FAIL: 0 · STALE: 0 · MISSING: 0
ctest -LE perf（全量重建后）                     → 262/262 PASS（rc=0）
bash scripts/verify_port_naming.sh              → PASS（端口 1521 · 不合规 1311 == 登记 1311）
bash scripts/verify_example_test_wiring.sh      → PASS
bash scripts/verify_example_assertion_quality.sh→ PASS
bash scripts/verify_assertion_quality_fixtures.sh → PASS
bash scripts/verify_test_counts.sh              → PASS
```

- 棘轮锚点回填：`EXPECTED_EXEMPT_PORTS=1311`（删 14 + 16 + 12 + 10 + 14 条已删文件的豁免）。
- **③ 判定**：✅ 满足。

### 2.10 AC9 —— 文档同步

```
git status --short                       → 干净
bash scripts/verify_docs_links.sh         → PASS
bash scripts/verify_public_api_visibility.sh → PASS（含归档 change 的「## 用户可见变更」段）
bash scripts/verify_openspec_delta.sh     → PASS
bash scripts/verify_openspec_purpose.sh   → PASS
bash scripts/verify_roadmap_fragments.sh  → PASS
bash scripts/verify_arch_ledger_residuals.sh → PASS
```

同步落点：`examples/spinalhdl-ported/AGENTS.md`（3 行 + 迁移清单）·
`examples/spinalhdl-ported/README.md`（运行示例路径）· `docs/CHANGELOG.md`（一条）·
`docs/usage_guide/04-testbench-guide.md`（`EXPECT` 位宽边界 + 复位尾缝已对齐 + `EXPECT_STABLE`
共用 `check_id`）· `.rddf/checks/{examples-rtl-expectations.md,examples-rtl-sim-targets.tsv,
port-naming-exempt.tsv}` · `.rddf/checks/public-api-surface.md`（ADR-076 的 4 kind + 5 字段）。
- **③ 判定**：✅ 满足。

---

## 3 失败项与路由建议

**无失败项**（`raise/rework` 队列为空）。

---

## 4 遗留清扫复核（本次范围内）

| 项 | 结果 | 证据 |
|---|---|---|
| TODO / FIXME / XXX / HACK | **0** | `grep -rn "TODO\|FIXME\|XXX\|HACK"` 于 8 个目录（排除生成物 `.sv`） |
| 半截实现 / 注释掉的代码 | **0** | 同上启发式 `^\s*//\s*(assign|always|logic|ch_)` 无命中 |
| 临时调试日志 | **0** | 8 份 `tb_*.cpp` 只打 `RESULT:` / `FAIL` / 发射期 `VERIF-CHECKS:` 与端口诊断 |
| 被 skip 的测试 | **无新增** | 既有 `SKIP()` 全在 `tests/benchmark/**`（依赖 `perf_results.json` 产物），与本 change 无关 |
| 未替换的 mock | **无** | 无 mock 引入 |
| 临时探针 target（`probe_*`） | **已清** | `probe_uart_tx` 已从 CMakeLists 删除并删二进制 |
| 已删 target 的构建残留 | **16 个**（脚本语义：非归属问题） | 8 个旧 target 名 + 8 个 `emit_rtl_*`；`run_all_ported_tests.sh` 明确将其归入 `STALE-ARTIFACT`（「清理 build 目录后即消失，不是归属问题」）⇒ **不删**（非本回合创建，且可再生成） |

**登记而不修的已知局限**（`proposal.md` §已知局限 · `04-testbench-guide.md` 同步）：

1. `EXPECT` 字面量 MUST 落在端口位宽内（渲染侧截断 vs 解释器 64 位比较 ⇒ 越界两端结论相反）。
2. `counter_simple` 的周期 256 抓不到步长 2（128 整除 256；由绝对判据承担，与原手写 TB 同局限）。
3. UART TX 的位宽常量 434 成为 impl-contract（改位宽 MUST 同步改 `.verif`）。

---

## 5 过程中发现并修复的框架缺陷（已带判据载体）

| # | 缺陷 | 修法 | 判据载体 |
|---|---|---|---|
| F1 | 渲染侧把窄端口直接赋给 `logic [63:0]` ⇒ `WIDTHEXPAND` ⇒ 护栏 ④（禁 `-Wno-fatal`）下 TB 编译失败 | `u64_rhs()` 零扩展（`EXPECT_STABLE` / `RECORD` 两处） | `tests/test_verif_wait_level.cpp` 新用例 + pwm 产物真编译 |
| F2 | 解释器框架复位只 `Simulator::reset()`（清 0）⇒ `INIT != 0` 的寄存器起在错误初态 ⇒ 与 RTL 分叉 | 复位后按 `regimpl::get_init_val()` 回填 | 新用例「寄存器 INIT 在框架复位后生效」+ uart_rx 解码 |
| F3 | 自检 fixture 用 `counter_simple` 的**手写** TB，而它现在是**生成式** ⇒ 自检基线红在 ⑥'（新鲜度） | fixture 换载体到仍是手写的 `tb_phase3a_example` | `--self-test` 21 投毒 + 2 必绿全 PASS |

---

## 6 已用轮次

**1**（首次验收即通过，无打回；本轮之后如再发现失败，剩余额度 = 2）。
