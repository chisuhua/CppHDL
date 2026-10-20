# Change: fix-axi4-peripheral-rst-n-port-2026-10 — 3 个 AXI4-Lite slave 加 rst_n + 收敛到相位分离形态

## Why

### 实测证据（RED，interpreter backend）

`AxiLiteGpio` / `AxiLiteUart` / `AxiLiteTimer` 三个 AXI4-Lite slave 的 AW/W 通道**没有相位分离**，
`bvalid` / `rvalid` 是**组合逻辑、不保持到 BREADY / RREADY**。后果是：**协议合规的 master 会永久死锁**。

复现（`tests/test_axi_gpio.cpp`，本 change 新增，见 Task 0）：

```text
# master 行为：cyc0 同周期拉 AWVALID+WVALID（BREADY=0），
#              握手完成后撤掉 AWVALID/WVALID，改为等 BVALID 并拉高 BREADY
B cyc0 : awready=0 wready=0 bvalid=0 gpio_out=0xab
B cyc1 : awready=0 wready=0 bvalid=0 gpio_out=0xab
B cyc2 : awready=0 wready=0 bvalid=0 gpio_out=0xab
B cyc3 : awready=0 wready=0 bvalid=0 gpio_out=0xab
B cyc4 : awready=0 wready=0 bvalid=0 gpio_out=0xab
B cyc5 : awready=0 wready=0 bvalid=0 gpio_out=0xab
# => BVALID 恒为 0；AWREADY 恒为 0（busy 永久锁）=> 死锁
```

同一探针的第二个场景证明 **AW 与 W 分离两周期时写数据被丢弃**：

```text
# cyc1: 只拉 AWVALID；cyc2: 才拉 WVALID
cyc1 (AW only) : awready=1 wready=0 bvalid=0
cyc2 (W later) : awready=0 wready=0 bvalid=0 gpio_dir=0x0   # DIRECTION 写没有落地
```

### 根因（与前一版 proposal 的诊断不同，需更正）

前一版 proposal（git `3715d5c`）把现象归因为"`busy` 永久锁，因为 axi_uart 是 combinational，没有 clock gating busy"。
这个机制描述是错的：

1. `busy` **有** self-clear 路径 —— `axi_uart.h:142` / `axi_gpio.h:133` / `axi_timer.h:124`
   都是 `busy->next = select(any_handshake, true, select(any_ready, false, busy))`，
   `any_ready = bready || rready`。
2. 真正的死锁是**循环等待**：
   - slave 侧 `io().bvalid = w_handshake`（组合），只在 W 握手当周期拉高 1 个周期；
   - master 侧按 AXI4-Lite 规定，**只有看到 BVALID 才会拉 BREADY**；
   - master 没在 W 握手当周期看到 BVALID → 不拉 BREADY → `any_ready` 恒 0 → `busy` 恒 1
     → `aw_handshake` 恒 0 → `awready` 恒 0 → slave 再也不接受新事务。
3. 第二个独立缺陷：`w_handshake = select(io().wvalid, aw_handshake, false)`
   （`axi_uart.h:87` / `axi_gpio.h:83`）要求 **W 必须与 AW 同周期**，
   违反 AXI4-Lite（master 可以在 AW 之后任意周期送 W）→ 见上面第二个场景。

因此**单纯加 `rst_n` 不能解决问题**：它只是给了一个"手动逃生阀"，
不修协议缺陷 → 下游 `dual-backend-axi4-peripherals-extended-2026-10` 的 parity 测试
仍然会因为 `bvalid` 保持语义 / AW-W 相位而失败或卡死。

### 与仓库自身约定的差距

`include/axi4/AGENTS.md` CONVENTIONS 明文要求：

> All peripherals use `busy/aw_done/r_valid` register trio for AW/W and AR/R phase separation

参考实现（已归档、已通过 dual-backend parity 的 3 个）：

| Peripheral | rst_n | aw_done | r_valid | `bvalid` 语义 |
|---|---|---|---|---|
| `axi_spi.h` | ✅ L98 | ✅ L115 | ✅ L116 | `select(aw_done,true,false)` — **早于 WDATA 拉高**（AW 完成即拉高），且 `aw_done` 在裸 `bready` 上清零 → 常态拉高 BREADY 的 master 会丢写。**非正确参考实现**（2026-10-20 review 更正：先前标注为"唯一正确"是错的） |
| `axi_pwm.h` | ✅ L70 | ✅ L86 | ✅ L87 | L116 被 L153 `w_handshake` 覆盖 → **不保持** |
| `axi_i2c.h` | ✅ L86 | ✅ L99 | ✅ L100 | L290 被 L149（helper，后调用）覆盖 → **不保持** |
| `axi_uart.h` | ❌ | ❌ | ❌ | 组合，不保持 |
| `axi_timer.h` | ❌ | ❌ | ❌ | 组合，不保持 |
| `axi_gpio.h` | ❌ | ❌ | ❌ | 组合，不保持 |

3 个目标 peripheral **全部违反**本仓库自己的 CONVENTIONS。
"加 rst_n 就自洽"是错的 —— 参考实现的一致性核心是 **trio（相位分离 + 响应保持）**，rst_n 只是其中一环。

### 附带发现（不在本 change 修，见 follow-up）

- `axi_timer.h:123` `aw_handshake || ar_handshake` 是 `ch_uint<1> || ch_uint<1>`
  → **ambiguous overload 硬编译错误**；但 `AxiLiteTimer` 在全仓库**从未被实例化**
  （`grep -rn "AxiLiteTimer<"` 只命中头文件本身）→ 该错误至今潜伏，CI 看不见。
  同文件另有 7 处 `-Wdeprecated-declarations`（`&&`/`||` overload，ADR-036）。
- `axi_uart.h:119` / `:123` 对 `status_reg->next` **赋值两次**（后者覆盖前者）
  → TX_EMPTY status 位是死代码；`tx_empty` / `rx_valid` 自身无 next 赋值 → status 恒 0、`irq` 恒 0。
- `axi_timer.h:93` `io().arready = select(busy, ch_bool(false), io().awvalid);`
  → 读通道 ready 用了 **awvalid**（应为 arvalid）。

## 目标

3 个 AXI4-Lite slave（`AxiLiteUart` / `AxiLiteTimer` / `AxiLiteGpio`）收敛到与
`axi_spi.h` 一致的**规范形态**，使其：

1. 具备 `ch_in<ch_bool> rst_n`（低有效异步复位），所有 `ch_reg` 有确定复位值；
2. 具备 `busy` / `aw_done` / `b_valid` / `r_valid` 寄存器组，实现 **AW→W 相位分离**；
3. `bvalid` / `rvalid` **保持到 BREADY / RREADY**（AXI4-Lite 合规）；
4. 不含 `&&` / `||` 逻辑 overload（ADR-036）；
5. 有 interpreter 侧回归测试兜底，为下游 dual-backend parity 测试解锁。

## What Changes

### 1. 规范形态（3 个 peripheral 统一采用）

```cpp
// 端口新增（低有效异步复位）
ch_in<ch_bool> rst_n;

// 状态寄存器组
ch_reg<ch_bool> busy(ch_bool(false));
ch_reg<ch_bool> aw_done(ch_bool(false));   // AW 已接收，等待 W
ch_reg<ch_bool> b_valid(ch_bool(false));   // W 已接收，B 响应待取
ch_reg<ch_bool> r_valid(ch_bool(false));   // AR 已接收，R 响应待取

// ---- 写通道（相位分离）----
auto aw_handshake = select(io().awvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
io().awready = aw_handshake;                                  // busy 期间拉低
auto w_handshake = select(io().wvalid, aw_done, ch_bool(false));  // W 可延后到后续周期
io().wready = w_handshake;
io().bvalid = select(b_valid, ch_bool(true), ch_bool(false));  // 保持到 BREADY
io().bresp = ch_uint<2>(0_d);

// ---- 读通道 ----
auto ar_handshake = select(io().arvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
io().arready = ar_handshake;
io().rvalid = select(r_valid, ch_bool(true), ch_bool(false));  // 保持到 RREADY
io().rresp = ch_uint<2>(0_d);

// ---- 状态更新（全部带 rst_n 复位）----
aw_done->next = select(!io().rst_n, ch_bool(false),
                       select(aw_handshake, ch_bool(true),
                              select(select(b_valid, io().bready, ch_bool(false)),
                                     ch_bool(false), aw_done)));
b_valid->next = select(!io().rst_n, ch_bool(false),
                       select(w_handshake, ch_bool(true),
                              select(io().bready, ch_bool(false), b_valid)));
r_valid->next = select(!io().rst_n, ch_bool(false),
                       select(ar_handshake, ch_bool(true),
                              select(io().rready, ch_bool(false), r_valid)));
busy->next = select(!io().rst_n, ch_bool(false),
                    select(aw_handshake, ch_bool(true),
                           select(ar_handshake, ch_bool(true),
                                  select(select(b_valid, io().bready, ch_bool(false)),
                                         ch_bool(false),
                                         select(select(r_valid, io().rready, ch_bool(false)),
                                                ch_bool(false), busy)))));
```

**关键设计点**

- `aw_done` 只在 **B 握手真正完成**（`b_valid && bready`）时清零。
  若照抄 `axi_spi.h` 的"在 `bready` 上清零"，当 master 常态拉高 BREADY 时
  `aw_done` 会在 W 到达前被清掉 → W 永远不被接受 → 新的死锁。**必须用 `b_valid && bready`。**
- `b_valid` 保证 **BVALID 在 WDATA 被接受之后才拉高**（AXI4-Lite 要求 B 响应在写数据之后），
  同时保持到 BREADY。相比 `axi_spi.h` 的 `bvalid = aw_done`（在 WDATA 到达前就拉高）更严格。
- `busy` 的清零条件同样是"事务真正完成"（`b_valid && bready` / `r_valid && rready`），
  而不是"`bready || rready`"（后者在事务未完成时也会放行新事务）。
- 寄存器写使能 `we_xxx` 仍由 `w_handshake` 驱动 → 写数据只在 AW 之后被接受。

> **对仓库 CONVENTIONS 的显式扩展**：`include/axi4/AGENTS.md` 现写的是
> "`busy/aw_done/r_valid` register trio"。本 change 引入第 4 个寄存器 `b_valid`，
> 原因是 trio 形态下 **BVALID 无法既"在 WDATA 之后拉高"又"保持到 BREADY"**：
> - 用 `bvalid = aw_done`（`axi_spi.h` 现状）→ BVALID 在写数据到达前就拉高
> - 用 `bvalid = w_handshake`（`axi_pwm.h` / `axi_i2c.h` 现状）→ 只维持 1 个周期，不保持
>
> `b_valid` 同时满足两个条件：BVALID 在 `w_handshake` 之后拉高、并在 BREADY 前保持。
> Task 5 会把新的 quartet 形态写回 `include/axi4/AGENTS.md` 的 CONVENTIONS。

### 2. `include/axi4/peripherals/axi_uart.h`

- `__io` 加 `ch_in<ch_bool> rst_n;`
- 按 §1 规范形态重写 AW/W/B/AR/R 通道 + 状态寄存器组
- 4 个数据寄存器（`tx_data_reg` / `rx_data_reg` / `status_reg` / `ctrl_reg`）
  加 `select(!io().rst_n, ch_uint<32>(0_d), ...)` 复位
- `tx_empty` / `rx_valid` 加显式保持 + 复位赋值
  （`tx_empty->next = select(!rst_n, ch_bool(true), tx_empty)`）
- `status_reg->next` 合并为**唯一一处**赋值（现状 L119/L123 双赋值，后者覆盖前者）：
  保留当前**实际生效**的语义（rx_valid 分支），把 TX_EMPTY status 位登记到 follow-up

### 3. `include/axi4/peripherals/axi_timer.h`

- `__io` 加 `ch_in<ch_bool> rst_n;`
- 按 §1 规范形态重写通道；同时修掉现状 L93 `arready` 误用 `awvalid` 的 bug
  （规范形态下 `arready` 由 `ar_handshake` 驱动）
- 4 个寄存器（`load_reg` / `count_reg` / `ctrl_reg` / `status_reg`）加复位
- **修掉全部 8 处 `&&` / `||`**（现状 7 处 deprecated warning + 1 处 ambiguous overload 硬错误）：
  - L77 `io().awvalid && io().awready`（port&&port）
  - L79 `io().wvalid && io().wready`（port&&port）
  - L84 / L85 `w_handshake && sel_load` / `&& sel_ctrl`（ch_uint<1> && ch_bool 混合 overload）
  - L94 `io().arvalid && io().arready`（port&&port）
  - L122 `io().awvalid && io().awready`（port&&port）
  - L123 `aw_handshake || ar_handshake`（**ambiguous overload 硬错误**）
  - L124 `io().bready || io().rready`（port||port）
  改写原则：布尔合取用 `&` / `|`（1-bit 信号，硬件等价），值选择用 `select()`。
  **注意**：规范形态重写后 L77/L79/L94/L122/L123/L124 全部被替换掉，
  L84/L85 改为 `select(w_handshake, sel_load, ch_bool(false))` 形式。

### 4. `include/axi4/peripherals/axi_gpio.h`

- `__io` 加 `ch_in<ch_bool> rst_n;`
- 按 §1 规范形态重写通道
- 3 个寄存器（`data_out_reg` / `direction_reg` / `interrupt_reg`）加复位
- `gpio_out` / `gpio_dir` / `irq` 语义不变

### 5. `examples/axi4/axi_soc_example.cpp`（必要的连带改动）

`AxiLiteGpio` / `AxiLiteUart` 在此实例化（`AxiSocTop::describe()`，L160 / L163）。
新增 `rst_n` 输入端口后必须接线，否则两个 peripheral 会被**永久按住复位**
（`ch_in` 未驱动 = 0 → `!rst_n` = 1）且 CI 察觉不到
（该 example 只打印 `GPIO_OUT`、无 assert，`run_all_ported_tests.sh:110` 只看退出码）。

改法：`AxiSocTop::describe()` 内把两个 peripheral 的 `rst_n` 接到常量 `ch_bool(true)`，
不改变 `AxiSocTop` 的对外 IO 形态（顶层 SoC 是否暴露复位口属于 follow-up）。

### 6. 新增 3 个 interpreter 侧回归测试

| 文件 | 覆盖 |
|---|---|
| `tests/test_axi_gpio.cpp` | AW/W 相位分离、bvalid 保持到 BREADY、rvalid 保持到 RREADY、rst_n 复位语义、AXI 死锁回归 |
| `tests/test_axi_uart.cpp` | 同上 + UART 寄存器读写 |
| `tests/test_axi_timer.cpp` | 同上 + timer 寄存器读写；**同时消除 `AxiLiteTimer` 零实例化导致的潜伏硬错误** |

> `tests/test_axi_{uart,timer,gpio}.cpp` 目前**不存在**（仓库只有 `test_axi_dma.cpp`）。
> 本 change 新建这 3 个文件属于"补齐缺失覆盖"，**不是修改**既有 baseline
> （spec R"原 test_axi_*.cpp 不动"针对的是已存在的文件）。
> dual-backend 的 `_behavior` 测试由下游 change `dual-backend-axi4-peripherals-extended-2026-10` 负责。

### 7. 文档同步

- `include/axi4/AGENTS.md`：
  - CONVENTIONS 记录规范形态（`rst_n` + `busy/aw_done/b_valid/r_valid` + 响应保持语义）
  - ANTI-PATTERN 里 `Found at axi_timer.h:77-81` 的行号锚点改为 grep 模式
    （避免行号漂移）：`grep -nE 'io\(\)\.[a-z_]+ *(&&|\|\|)' include/axi4/peripherals/*.h`
- 根 `AGENTS.md`：`CTEST_TOTAL` 169 → 172（+3 个新 ctest target）

## 非目标（Out of Scope）

以下项**不在本 change**，统一登记到 follow-up change
`axi4-peripheral-followups-2026-10`：

| 项 | 理由 |
|---|---|
| `include/axi4/peripherals/axi_dma.h` 加 `rst_n` | `AxiDma` 是 **AXI4 master**（`araddr/awaddr/wdata` 是 `ch_out`），不是 `AxiLite*` slave；复位语义与 slave 不同；且 13 个 `ch_reg` 的复位值需要单独定义（`state` 必须 = 0 与 `is_idle` 耦合） |
| `axi_pwm.h` / `axi_i2c.h` 的 `bvalid` 双赋值 | 已归档 change 的产物，本 change 不回改已归档 design |
| `axi_spi.h` 的 `bvalid = aw_done`（BVALID 在 WDATA 前拉高） | 同上；规范形态以本 change 的 `b_valid` 为准 |
| `axi_uart` 的 TX_EMPTY / RX_VALID status 位语义 + `irq` 恒 0 | 功能语义决策，超出"通道收敛 + 复位"范围 |
| `examples/riscv-mini/src/rv32i_soc.h` | **孤儿头**（全仓库无任何 TU include，`grep -rn rv32i_soc` 除自身外零命中），改它不会被编译；应先决定纳入构建还是删除 |
| `examples/axi4/axi_soc_example.cpp` 的 `GPIO_OUT` 打印 0x0 既有 bug | 根因在 example 自身的 test master / matrix 路径，与 peripheral 无关；本 change 只做 `rst_n` 接线 |
| 顶层 SoC 暴露 `rst_n` 端口 | 与本 change 的 peripheral 级改造正交 |

**硬约束（不破反）**

- MUST NOT 改 `include/axi4/peripherals/axi_{spi,pwm,i2c}.h`
- MUST NOT 改 `tests/test_axi_{pwm,i2c,spi}_behavior.cpp`（已归档）
- MUST NOT 改 `tests/test_verilator_*.cpp`（spec 锁定的 macro reference impl）
- MUST NOT 改 `tests/test_axi_dma.cpp`（interpreter-only baseline）
- MUST NOT 改 `include/axi4/peripherals/axi_dma.h`

## Acceptance

### Scenario: 编译 0 error 0 warning（含此前潜伏的 hard error）

- **WHEN** `touch tests/test_axi_{uart,timer,gpio}.cpp && cmake --build build -j$(nproc) --target test_axi_uart test_axi_timer test_axi_gpio 2>&1 | tee /tmp/opencode/build.log | grep -cE "error:"`
- **THEN** 输出 `0`
- **AND** `grep -c "test_axi_timer.cpp" /tmp/opencode/build.log` ≥ 1
  （**防假阳性**：`AxiLiteTimer` 原本零实例化，其 `ambiguous operator||` 硬错误
  在未实例化时根本不进编译；必须证明**实例化它的 TU 真的被编译过**。
  `touch` 是必需的 —— 增量构建时目标已是最新，log 里不会有该文件名）
- **AND** `grep -cE "axi_(uart|timer|gpio)\.h.*-Wdeprecated-declarations" /tmp/opencode/build.log` == `0`

### Scenario: 3 个 peripheral 的 AXI 死锁回归测试全绿

- **WHEN** `ctest -R "test_axi_(uart|timer|gpio)" --output-on-failure`
- **THEN** 3 个 target 全 PASS
- **AND** 每个 target 至少含 1 条"协议合规 master 不死锁"的断言
  （BVALID 在 BREADY 拉高前必须已经被 master 观察到）

### Scenario: 地址锁存（AW/AR 握手后 master 改写地址不改变访问目标）

- **WHEN** master 在 AW 握手完成后改写 `AWADDR`（AXI 只要求 AWADDR 保持到 AW 握手），
  再送出 WDATA
- **THEN** 写 MUST 落到 **AW 握手时锁存的地址**指向的寄存器
- **AND** `grep -nE 'addr_bits = io\(\)\.aw?r?addr' include/axi4/peripherals/*.h` → 0 行
  （写侧/读侧地址解码 MUST 基于锁存寄存器）
- **AND** 3 个测试文件各含 1 条"AW 握手后改写 awaddr 写目标不变"的回归用例
- **AND** 该用例 MUST 在移除锁存时变红（**防假阳性**：实测确认）

> 该条是归档后 review 的实测发现：相位分离把 W 的接受窗口从"与 AW 同周期"
> 拉长到"AW 之后任意周期"，而地址解码仍读活的 `io().awaddr` ——
> master 在 AW 握手后改写 AWADDR 会让写静默落到错误寄存器。

### Scenario: 全量 ctest 不退化 + 新增覆盖

- **WHEN** `cd build && ctest -LE perf --output-on-failure`
- **THEN** 161/161 PASS（基线 158 + 本 change 新增 3）
- **WHEN** `cd build && ctest -L verilator_e2e -LE perf --output-on-failure`
- **THEN** 23/23 PASS（不变）

### Scenario: 测试计数同步

- **WHEN** `bash scripts/verify_test_counts.sh`
- **THEN** exit 0
- **AND** 根 `AGENTS.md` 的 `CTEST_TOTAL: 172` 与 `cd build && ctest -N | tail -1` 一致

### Scenario: example 连线正确

- **WHEN** `grep -n "rst_n" examples/axi4/axi_soc_example.cpp`
- **THEN** 命中 2 处（gpio + uart 的 `rst_n` 接线）
- **AND** `./build/examples/axi4/axi_soc_example` exit 0

### Scenario: openspec 合规

- **WHEN** `openspec validate fix-axi4-peripheral-rst-n-port-2026-10 --strict`
- **THEN** valid
- **AND** archive 时**不得**使用 `--skip-specs`（本 change 有 spec delta，
  必须合并进 `openspec/specs/dual-backend-test-coverage/spec.md`）
- **AND** archive 后 `grep -c "AXI4-Lite slave peripheral MUST 收敛到相位分离" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1

> **反例登记**：上一个已归档 change `2026-10-20-dual-backend-axi4-peripherals`
> 的 spec delta（含 peripheral 列表）**没有**出现在主 spec 中 —— 当时用了 `--skip-specs`，
> Requirement 静默丢失且 `openspec validate --specs` 永远发现不了。本 change 不重复该错误。

## Capabilities

### ADDED Requirements

见 `specs/dual-backend-test-coverage/spec.md`。
