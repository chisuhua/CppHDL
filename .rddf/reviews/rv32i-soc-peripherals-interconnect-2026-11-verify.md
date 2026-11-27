# 验收报告 — `rv32i-soc-peripherals-interconnect-2026-11`（S4b）

- **角色**：`verifier`（`source: user` · 2026-11-27）
- **判据来源**：`openspec/changes/rv32i-soc-peripherals-interconnect-2026-11/proposal.md` 的
  `## 验收标准`（AC1–AC9）· 用户本次要求（S4b P0–P5 落地）
- **已用轮次**：**1**（本报告为第 1 轮）
- **结论**：**PASS — 9/9 AC 满足**（AC9 附 1 条测试面残留，见 §AC9）
- **验证方式**：本地验收（`rdd-verify --dry-run` 报 `No eligible changes` ⇒ 不走 skill）

> ⚠️ `rdd-verifier` skill 未启用（**条件 2 不满足**）：`rddf rdd-verify --dry-run` 输出
> `no change has all tasks checked (1 change(s) incomplete)` —— 未勾的 6 项是 **T5.5（本次验收本身）
> + T6.1–T6.3（归档）**，属**流程项**而非实现项 ⇒ ⇒ 按 verifier 产出契约走本地 1–5 步验收。

---

## 1. 逐条判定 + 证据

### AC1 路由完备（12 对）— **PASS**

| 证据 | 内容 |
|---|---|
| `./build/tests/test_axi_lite_crossbar` | `All tests passed (508 assertions in 20 test cases)` |
| 用例名（`tests/test_axi_lite_crossbar.cpp:371,391`） | `AxiLiteCrossbar fanout: all 12 (master,slave) write pairs` · `... read pairs` |

**判别力**：两个用例**逐对**（2 master × 6 slave）驱动并检查 `awaddr/wdata` 落到对应 `sX_*` 与
`bresp/rdata` 回到 `m*_*` ⇒ 任一配对缺失即红。

### AC2 锁存 / 在途 — **PASS**

| 证据 | 内容 |
|---|---|
| 用例（`:410,452`） | `write route latch: changing AWADDR after handshake` · `read route latch: changing ARADDR after handshake` |
| 用例（`:484`） | `in-flight: awready low, next AW not swallowed` |
| 源码 `include/axi4/axi_lite_crossbar.h:313,320,338-344,477-480` | `busy_of(act, route, j)` + `wr_active_m0/m1` / `rd_active_m0/m1` 寄存器；`wr_route` / `rd_route` 为锁存值 |

**判别力**：握手后改地址 ⇒ 用例断言仍按**锁存值**转发（改地址无效）；在途期间 `m*_awready` 为 0
且第二条 AW 不被吞（用例显式发第二条并检查其被接受于后续拍）。

### AC3 仲裁 + 同 slave 串行 — **PASS**

| 证据 | 内容 |
|---|---|
| 用例（`:537`） | `arbitration: grants rotate across masters` |
| 用例（`:723`） | `same-slave serialization: second AW waits for the first B` |
| 源码 `:28` | `每方向独立 round-robin（chlib::round_robin_selector），握手后推进指针` |
| 源码 `:519-526` | `*s_awvalid[j] = aw_demux.output_streams[j].valid & !wr_busy[j]`（**valid 与 ready 一起门控**）|
| 源码 `:663-667` | `*s_arvalid[j] = ... & !rd_busy[j]`（读方向同构） |

**判别力**：`grants rotate` 用例连发两 master 同 slave 请求并检查**授权在两个 master 间轮转**
（固定优先级会恒选同一 master ⇒ 红）；`same-slave serialization` 检查第二个 AW 的 `awvalid` 在
`wr_busy` 期间被钉 0（不只 `awready`）。

### AC4 DECERR — **PASS**

| 证据 | 内容 |
|---|---|
| 用例（`:609`） | `DECERR: unmapped write answers 0b11 and does not hang` |
| 用例（`:645`） | `ERR window: DECERR without touching any slave` |
| `./build/examples/riscv-soc/tb_riscv_soc` | `RESULT: checks=16 fails=0` —— **A8** 判 `writeback_count == 3`（DECERR 单拍完成、程序继续） |
| 源码 `include/axi4/interconnect_common.h:83` | `inline constexpr uint32_t kErr = 0xFFFF'FFFFu;` |
| 源码 `axi_lite_crossbar.h:59,349-352` | `kErrRoute` + `err_b_pending_m0/m1` / `err_r_pending_m0/m1`（互连自应答） |

**判别力**：`ERR window` 用例同时断言**没有任何从口被触碰**（`sX_*valid` 恒 0）⇒ 「路由到错误从口
但恰好返回 DECERR」也会红。A8 从**软件可见面**（写回次数）验证「不挂死」。

### AC5 窗口表同源 — **PASS**

| 证据 | 内容 |
|---|---|
| 源码 `examples/riscv-soc/src/rv32i_soc_top.h:71-94` | **8 条** `static_assert`：逐段 `windows[i].base/size == riscv::kXxx.base/size` |
| 源码 `:97-110` | **8 条** `static_assert`：逐窗口 `target` 意图钉死（CLINT/DMA → `kErr`；6 外设 → s0..s5） |
| `./build/examples/riscv-mini/test_address_decoder` | `All tests passed (164 assertions in 10 test cases)`，含 `address_decoder: MMIO 全表 MUST 派生 + 8 段 MUST 各自命中` |

**判别力**：把派生基址换成手写字面量（如 `0x80102000`）⇒ `static_assert` 失败 ⇒ **编译失败**
（不是「编译通过但静默错位」）。运行期侧由 `test_address_decoder` 的「8 段各自命中」覆盖。

### AC6 外设回环（总线级）— **PASS**

| 证据 | 内容 |
|---|---|
| `./build/examples/riscv-soc/tb_riscv_soc` | `RESULT: checks=16 fails=0` |
| `tb_riscv_soc.cpp` 判据面 | **A10** UART `CTRL(0x0C)`=0x53 · **A11** TIMER `LOAD(0x00)`=0x6A · **A12** SPI `BAUD(0x10)`=0x17 · **A13** I2C `PRESCALE(0x10)`=0x2C · **A14** PWM `PERIOD(0x00)`=0xE1 · **A15** PWM `PERIOD` 复位值=0xFF |
| 期望值**手推** | 源码注释含手推表（段基址 / 寄存器 / 字节偏移 / **外设复位值**）；段基址全部派生自 `rv32i_mem_map.h`（`static_assert` 保 4 KiB 对齐） |

**判别力**：5 个写入值**互不相同且都 ≠ 目标外设复位值**（SPI/I2C 复位 0x64 · PWM 复位 0xFF）⇒
「写没生效」不会与期望撞值。

> ⚠️ **已知残留**（源码 + 台账均已登记）：A10–A14 对「窗口**单点**错位到**同偏移也可写**的外设」
> **结构性失明**（写和读都落到同一个错误外设 ⇒ 读回值一致）。由 **A15**（PWM 的 0xFF 是 word 0 中
> 唯一非零复位值）闭合 PWM 面；UART/TIMER/SPI/I2C 面由 P1 逐从口单测 + `static_assert` 承担。

### AC7 零回归 — **PASS**

| 证据 | 内容 |
|---|---|
| `ctest`（P5 全量 · `/tmp/opencode/p5_ctest.log`） | `100% tests passed, 0 tests failed out of 285`（2535 s，含 perf） |
| `rv32i_rtl_sim_env.sh`（`/tmp/opencode/g_rv32i_env.log`） | `[flat] RESULT: checks=24 fails=0` · `[hier] RESULT: checks=24 fails=0` |
| `verify_test_counts.sh` | `CTEST_TOTAL (285) matches ctest -N (285)` ✓ · `VERILATOR_E2E_TOTAL (82) matches` ✓ |
| `tb_riscv_soc` | 原 **10 条**（A1a/A1b/A2–A9）全绿（16/16 包含它们） |
| A8 语义 | 探针仍是 `kClint.base`（`LUI(1, 0x80100)`），`kClint` 的 `target` 仍为 `kErr`（`static_assert` 钉住）⇒ **语义不变** |

**parity 面**：`DUAL_BACKEND_PARITY_TOTAL=68` / `PAIRS=307` 锚点本批**未改动**；`verilator_e2e`
82 条在 ctest 全量中全绿。

### AC8 形态登记 — **PASS**

| 证据 | 内容 |
|---|---|
| `./build/examples/riscv-soc/tb_riscv_soc --check-products` | `PRODUCTS-CHECK: PASS` |
| 明细 | `CHECK 1 module 集: PASS (期望 17 个 · 实际 17 个)` · `CHECK 2 顶层端口: PASS (端口 10/10 · 后代未驱动 0)` · `CHECK 3 lint: PASS` · `CHECK 4 双形态等价: PASS (变化量=2/2 · 自证=PASS)` |

**module 数推导核对**：`17 = 12（S4a）− 1（AxiLiteMatrix2x2 退役）+ 1（AxiLiteCrossbar）+ 5（新外设）`
⇒ 与 `soc_product_face.h` / tsv / 期望台账三处登记**一致**。根端口 **10 不变**（D-8）。

### AC9 反向判据 — **PASS（附 1 条测试面残留）**

| 支路 | 证据 | 结论 |
|---|---|---|
| 「窗口派生改回手写字面量」 | 源码 `:71-94` 的 8 条 `static_assert`（`grep` 逐条可取）⇒ 手写字面量破坏相等 ⇒ **编译失败** | ✅ 满足 |
| 同类实测（P4 · builder 侧执行，记录在台账 §4） | **M-P4-2** UART 窗口 1→2 ⇒ A10 红；**M-P4-3** SPI 窗口 3→`kErr` ⇒ A12 红；**M-P4-1** PWM 窗口 5→0 ⇒ 只有 A15 红 | ✅ 佐证「窗口改错 ⇒ 判据转红」 |
| ⚠️ **残留** | 「`ERR` 指向 s0」这一**具体形态**未在 verifier 侧实测 —— verifier 写集不含 `examples/**`（角色边界），不能自行改实现做变异 | 如实登记 |

**判定**：AC9 原文是「…（**或**把 ERR 指向 s0）⇒ AC4/AC5 **至少一条**转红」⇒ **static_assert 支路
已满足**（且 M-P4-2/3 提供了运行期同类证据）⇒ AC9 **PASS**。
**建议**（非阻断）：若需闭合最后一条，由 builder 补一次 `windows[0].target: kErr → 0u` 的变异。

---

## 2. 边界与错误路径复核

| 路径 | 覆盖 | 证据 |
|---|---|---|
| 未映射地址 | ✅ | `DECERR: unmapped write answers 0b11 and does not hang` · `tb_riscv_soc` A8 |
| `ERR` 窗口（CLINT/DMA） | ✅ | `ERR window: DECERR without touching any slave` |
| 在途期间新请求 | ✅ | `in-flight: awready low, next AW not swallowed` |
| 同一 slave 并发读写 | ✅ | `same-slave serialization: second AW waits for the first B` |
| 同偏移可写外设错位 | ⚠️ | 仅 PWM 面由 A15 覆盖（已登记） |

## 3. 遗留清扫复核（独立取证）

| 检查 | 命令 | 结果 |
|---|---|---|
| TODO / FIXME / XXX / HACK | `grep -rnE "TODO:\|FIXME\|XXX\|HACK"`（`examples/riscv-soc/` + crossbar + mem_map + decoder） | **0 命中** |
| 调试输出残留 | `grep -rnE "\[DBG\]\|DBG recv\|std::cerr << \"DBG"` | **0 命中** |
| 注释掉的代码块 | `grep -nE "^\s*//\s*(if\|for\|while\|return\|[a-z_]+\(|sw \|lw )"` | **0 命中** |
| 被 skip 的测试 | `grep -nE "SKIP\(\|\[!mayfail\]\|\[\.\]"`（crossbar + address_decoder） | **0 命中** |
| 临时调试补丁 | `git diff --stat include/core/io.h` | **空**（已还原） |
| 工作区 | `git status --short` | **干净** |

## 4. 失败项与路由建议

**无失败项**（9/9 AC 满足）。

非阻断建议（不影响本次验收结论）：

1. **AC9 的最后一条支路**：`ERR` → s0 变异未实测（verifier 写集限制）。若追求完备，由 builder
   补一次（预计 2 分钟：改 target + 改 `static_assert` + 重建 + 跑 `tb_riscv_soc` + 还原）。
2. **UART/TIMER/SPI/I2C 的「单点窗口错位」**：SoC 层无判别力（已在源码 / 台账 / CHANGELOG 三处登记），
   由 P1 逐从口单测承担。若要提升，可在后续批次用「外设复位值差异」补地址特征判据
   （目前只有 PWM 有唯一非零复位值）。

## 5. 验收结论

**PASS** —— AC1–AC9 全部满足，遗留清扫通过，零回归（285/285 · flat/hier 24/24）。

后续流程项（不在本次验收判定内）：

- **T5.5 勾选**需由 `builder` 执行（verifier 写集不含 `openspec/changes/*/tasks.md`）。
- **P6 归档**（`openspec validate --strict` ✅ · `verify_openspec_delta.sh` 4/4 ✅ 已在 P3/P4 期间核对）
  待 builder 执行 `openspec archive … --yes`。
