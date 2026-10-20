# Change: axi4-peripheral-followups-2026-10 — AXI4 peripheral 收敛后的遗留项

## Why

`fix-axi4-peripheral-rst-n-port-2026-10` 把 `AxiLiteUart` / `AxiLiteTimer` / `AxiLiteGpio`
三个 AXI4-Lite slave 收敛到规范形态（`rst_n` + `busy`/`aw_done`/`b_valid`/`r_valid` +
AW→W 相位分离 + 响应保持）。

收敛过程中暴露出一批**不属于该 change 范围**的遗留项。它们分散在不同子系统中，
混进上一个 change 会让 diff 失焦、回归定位困难，因此单独开本 change 收口。

### 遗留项清单

| # | 项 | 位置 | 性质 |
|---|---|---|---|
| F1 | `AxiDma` 缺 `rst_n` | `include/axi4/peripherals/axi_dma.h` | AXI4 **master**（非 `AxiLite*` slave），复位语义与 slave 不同，13 个 `ch_reg`（7 个声明行 / 13 个变量）的复位值需单独定义 |
| F2 | `bvalid` 退化为组合脉冲（不保持） | `include/axi4/peripherals/axi_pwm.h:116,153`、`include/axi4/peripherals/axi_i2c.h:149,290` | 两处赋值，**后执行者覆盖先执行者** → 实际生效 `bvalid = w_handshake`（组合、仅 1 周期），违反 AXI4-Lite 响应保持。`axi_i2c.h` 的覆盖顺序：`build_register_bank()`（内含 L149）在 `describe()` L310 被调用 → **L149 覆盖 L290** |
| F3 | `bvalid = aw_done`（BVALID 早于 WDATA） | `include/axi4/peripherals/axi_spi.h:350` | **只有 1 处赋值**（非双赋值），但 BVALID 在 AW 完成、**WDATA 尚未被接受**时就拉高 → 违反"BVALID 在 WDATA 之后" |
| F2' | 三者缺 `b_valid` 寄存器、`aw_done` 在裸 `bready` 上清零、缺地址锁存 | `axi_pwm.h:245` / `axi_i2c.h:172` / `axi_spi.h:316`（`aw_done->next`） | `aw_done->next = ... select(io().bready, false, aw_done)` → master 常态拉高 BREADY 时 `aw_done` 被提前清零 → W 永不被接受。**这与主 spec 规范形态 Requirement 第 326 行直接冲突**。三者均无 `addr_reg`/`ar_addr_reg` → 与主 spec 第 7 条（地址锁存）冲突 |
| F4 | UART status / irq 语义缺失 | `include/axi4/peripherals/axi_uart.h` | TX_EMPTY status 位是死代码；`tx_empty`/`rx_valid` 无 next 逻辑；`irq` 恒 0 |
| F5 | 孤儿头 `rv32i_soc.h` | `examples/riscv-mini/src/rv32i_soc.h` | 全仓库无任何 TU include（`grep -rn rv32i_soc` 除自身外零命中）→ 改动不会被编译；其中还实例化了 `AxiLiteUart`（缺 `rst_n` 接线） |
| F6 | `AxiSocTop` example 的 `GPIO_OUT` 打印 0x0 | `examples/axi4/axi_soc_example.cpp` | 写 0xAB 未落地。**实测根因（见下方 F6 章节）**：`AxiLiteTestMaster` 的 `wvalid = write_start && awready` 依赖跨模块组合路径，而 simulator 的组合求值是**单遍无拓扑序** → master 读到上一拍的 `awready` → WVALID 恒 0。与 peripheral 无关；无 assert → CI 察觉不到 |
| F7 | 主 spec 的 `AxiLiteDma` 类名不存在 | `openspec/specs/dual-backend-test-coverage/spec.md` | **已验证满足**：`git log -p -- openspec/specs/dual-backend-test-coverage/spec.md \| grep -c "AxiLiteDma"` = 0 → 主 spec 从未出现过该错误类名。**注**：该 Requirement 的 MODIFIED 由下游 change `dual-backend-axi4-peripherals-extended-2026-10` 统一负责（它重写整个 peripheral 列表），本 change 只做**验证**，不重复提交 MODIFIED delta（避免两个 change 同时 MODIFIED 同一 Requirement 造成 archive 冲突） |
| F8 | 顶层 SoC 未暴露 `rst_n` | `examples/axi4/axi_soc_example.cpp` 的 `AxiSocTop` | peripheral 有复位口但 SoC 顶层没有；复位逃生路径无法从 SoC 外部触达 |
| F9 | GPIO 写侧/读侧地址映射不一致 | `include/axi4/peripherals/axi_gpio.h` | 写侧：`0x00→DATA_OUT`、`0x04→DIRECTION`、`0x08→INTERRUPT`；读侧：`0x00→DATA_OUT`、`0x04→DATA_IN`、`0x08→DIRECTION`、`0x0C→INTERRUPT`。按头部文档的映射写 DIRECTION(0x08) 会落到 INTERRUPT 寄存器；写侧比读侧少了 DATA_IN 一档，整体错位一档。**影响面已核实**：唯一写 0x04 的是 `tests/test_axi_gpio.cpp`（`kRegDirectionWrite`）；example 只**读** 0x04 → 低风险 |
| F10 | `wstrb` 被忽略 + `spi` 不驱动 `bresp` + 未映射地址返回 OKAY | 全部 6 个 peripheral | ① `wstrb` 只在端口声明出现，**无任何一处使用** → 写始终整字覆盖（违 AXI4-Lite 字节写语义）；② `axi_spi.h` **完全没有 `io().bresp = ...` 赋值** → BRESP 悬空（Verilog 里是未驱动 wire）；③ 全部 peripheral 对未解码地址返回 `bresp=OKAY`（`include/axi4/AGENTS.md` 已列为 ANTI-PATTERN，但主 spec 规范形态 Requirement 未收录） |

## 目标

把上述遗留项逐条收口，使 `include/axi4/peripherals/` 全体 peripheral 满足同一套规范形态，
并让主 spec / 示例 / 文档与实际代码一致。

## What Changes

### F1: `axi_dma.h` 加 `rst_n`

- `__io` 加 `ch_in<ch_bool> rst_n;`
- 13 个 `ch_reg` 逐个定义复位值（必须给出显式表，不允许"init_val"占位）：
  - `state` → 0（与 `is_idle` 耦合，`axi_dma.h:48`）
  - `src_addr` / `dst_addr` / `curr_raddr` / `curr_waddr` / `length_reg` / `bytes_rem` → 0
  - `beat_cnt` → 0、`rdata_buf` → 0
  - `data_vld` / `err_flag` → false、`irq_en` → false
  - `id_reg` → 0（ID 语义需明示：每次 `reg_start` 自增，故复位归 0）
- `state->next` 复位到 0（idle）
- **注意**：`AxiDma` 是 AXI4 master，`rst_n` 的复位对象是 master 自身的 FSM 状态，
  与 slave 的 "复位寄存器值" 语义不同 —— spec delta 需单列 Requirement，不复用
  `fix-axi4-peripheral-rst-n-port-2026-10` 的 slave Requirement

### F2 + F2' + F3: 参考实现回收到规范形态

**现状实测**（三者都**已有** `rst_n` / `busy` / `aw_done` / `r_valid` / AW→W 相位分离，
差距比"bvalid 双赋值"更大）：

| 文件 | `bvalid` 赋值 | 实际生效 | `aw_done` 清零条件 | `b_valid` | 地址锁存 |
|---|---|---|---|---|---|
| `axi_pwm.h` | L116 `= aw_done`、L153 `= w_handshake` | L153（组合脉冲，不保持） | L245-246 裸 `bready` ❌ | 无 ❌ | 无 ❌ |
| `axi_i2c.h` | L149 `= w_handshake`、L290 `= aw_done` | L149（组合脉冲，不保持） | L172 裸 `bready` ❌ | 无 ❌ | 无 ❌ |
| `axi_spi.h` | L350 `= aw_done`（**仅 1 处**） | L350（**早于 WDATA** 拉高） | L316-317 裸 `bready` ❌ | 无 ❌ | 无 ❌ |

> ⚠️ **更正**：先前把 `axi_spi.h:350` 记作"唯一 `bvalid` 正确保持的参考实现"是**错的** ——
> `bvalid = aw_done` 在 AW 完成时即拉高，**早于 WDATA 被接受**，恰恰违反规范形态。
> 正确的参考实现是刚落地的 `axi_{gpio,uart,timer}.h`。

**改法**（三者统一）：

1. 新增 `b_valid` 寄存器；`bvalid` 由其驱动，**在 WDATA 被接受后**才拉高，保持到 BREADY
2. `aw_done` 的清零条件由裸 `bready` 改为 `b_valid && bready`
   （否则 master 常态拉高 BREADY 时 `aw_done` 被提前清零 → W 永不被接受）
3. 新增 `addr_reg` / `ar_addr_reg`，AW / AR 握手时锁存地址；解码基于锁存地址
4. `axi_pwm.h` / `axi_i2c.h`：删除多余的那处 `bvalid` 赋值
5. `axi_spi.h`：`bvalid` 从 `aw_done` 改为 `b_valid`（修早拉高）

**破坏面已核实**：`tests/test_axi_{pwm,i2c,spi}_behavior.cpp` 用的是旧式 master
（AW+W+BREADY 同周期脉冲），其断言宽松（`REQUIRE((is_latched || is_default))`），
且 `spi_behavior` 的等待循环能等到 `bvalid` → **新旧形态都能通过**，
但改完后 MUST 重跑确认（`ctest -R "test_axi_(pwm|i2c|spi)"`）。

### F4: UART status / irq 语义

- 明确 `tx_empty` / `rx_valid` 的状态机（何时置位 / 清除）
- `status_reg` bit0 = TX_EMPTY、bit1 = RX_VALID（与寄存器映射表 L9 对齐）
- `irq` 由 `rx_valid` 驱动（现状 L137 已是，但因 `rx_valid` 恒 false 而恒 0）
- 需要决定 TX 侧的"发送完成"事件来源（当前 UART TX 是简化实现：写 TX_DATA 即发送）

### F5: 孤儿头 `rv32i_soc.h` 处置 —— **定案：选项 A（删除）**

- **证据**：`grep -rn "rv32i_soc" --include=*.cpp --include=*.h . | grep -v "src/rv32i_soc.h:"` → 0 行
  （无任何 TU include → 不会被编译 → 改动它对构建零影响）
- **依据**：ZERO-DEBT 政策"No orphan files"（每个头文件必须被 include somewhere）
- **附带风险提示**（若将来有人要保留它）：该文件 L111 实例化了 `AxiLiteUart<16>` 但
  **未接 `rst_n`** → 实测未驱动的 `ch_in<ch_bool>` 读出为 **0** → peripheral **永久复位**
  （所有寄存器恒为 init 值、AXI 事务不响应）
- 选项 B（纳入构建 + 补 `rst_n` 接线 + 补测试）成本显著更高，**不推荐**

### F6: `AxiSocTop` example 的写路径 —— **根因已实测定位**

**症状**：`GPIO_OUT = 0x0`（写 0xAB 未落地），且**新旧 peripheral 形态下都是 0x0**
（已用 `git show 15545d3^:` 回退验证 → **不是**本轮改动引入的回归）。

**根因（双重，均在 example 侧，与 peripheral 无关）**：

1. **组合求值是单遍的，跨模块组合路径读到陈旧值**
   - `src/simulator.cpp:257` `eval_combinational()` 对 `combinational_instr_list_`
     **单遍遍历，无拓扑序、不迭代到不动点**
   - 节点按创建顺序入列；`AxiSocTop::describe()` 的创建顺序是
     `master`(L154) → `matrix`(L157) → `gpio`(L160) → `uart`(L163)
   - `master` 的组合节点（`wvalid` L89 / `bready` L95）**先于** `gpio`/`matrix`
     的组合输出被求值 → master 读到的是**上一拍**的 `awready` / `bvalid`
2. **master 的 WVALID 生成方式违 AXI 且不满足新规范形态**
   - `io().wvalid = select(write_start, io().awready, false)`（L88-89）
     —— 用**从设备的 ready** gate 自己的 valid（AXI 禁止），
     且只在 `write_start` 那一拍可能为 1
   - 旧形态下 `w_handshake = wvalid && aw_handshake`（同拍即可接受）→ 侥幸能写
   - **新规范形态下 `w_handshake = wvalid && aw_done`（需 AW 握手后 1 拍）→ 必挂**

**正解**：master 的 `wvalid` / `bready` 改为**状态驱动**（`state == WRITE` → `wvalid = 1`，
`state` 保持到 `write_done`），**不依赖从设备的组合输出**；`awaddr` / `wdata` 在整个
写事务期间保持稳定。**不要**靠调整模块创建顺序来"碰巧"让单遍求值 work —— 那是脆弱的。

**附带产出（架构约束，应写入 `include/axi4/AGENTS.md`）**：
CppHDL 的组合求值是单遍无拓扑序 → **跨模块组合路径要求"生产方先于消费方"创建**，
否则同一拍内读到上一拍的陈旧值。规范形态（`w_handshake` 只依赖输入与寄存器）天然免疫；
任何"用从设备 ready gate 自己 valid"的 master 都会踩坑。

### F7: 主 spec 类名修正 —— **已验证满足（空验证项）**

- **实测**：`git log -p --all -- openspec/specs/dual-backend-test-coverage/spec.md | grep -c "AxiLiteDma"` = **0**
  → 主 spec **从未**出现过 `AxiLiteDma`；当前唯一相关行是 `AxiDma`（L313）
- 归档前仍**验证**主 spec 中不再出现 `AxiLiteDma`（防回归）
- **不得**在本 change 重复提交同一条 Requirement 的 MODIFIED delta
  （两个 change 同时 MODIFIED 同一 Requirement 会在 archive 时产生冲突）
  —— 该修正由下游 change `dual-backend-axi4-peripherals-extended-2026-10` 统一负责

### F8: `AxiSocTop` 暴露 `rst_n`

- `AxiSocTop` 加 `ch_in<ch_bool> rst_n`，透传给 gpio / uart
- `main()` 中 `sim.set_input_value(top.io().rst_n, 1u);`

### F9: GPIO 写侧 / 读侧地址映射对齐

- `axi_gpio.h` 写侧解码（`sel_data_out` / `sel_direction` / `sel_interrupt`）与读侧解码
  （`read_sel_data_out` / `read_sel_data_in` / `read_sel_direction` / `read_sel_interrupt`）
  MUST 指向同一组寄存器
- 目标映射（与头部文档一致）：
  `0x00→DATA_OUT`、`0x04→DATA_IN`、`0x08→DIRECTION`、`0x0C→INTERRUPT`
  （写侧 `0x04` 的 DATA_IN 为只读，写入被忽略）
- 同步更新 `tests/test_axi_gpio.cpp` 中依赖写侧旧映射的用例
  （当前用 `kRegDirectionWrite = 0x04` 锁定旧行为）

### F10: `wstrb` 字节写语义 + `bresp` 完整性

- **`wstrb` 被忽略**：6 个 peripheral 的 `wstrb` 只出现在端口声明，写路径
  （`select(we_xxx, io().wdata, xxx_reg)`）**整字覆盖**，违 AXI4-Lite 字节写语义
  - 改法：写掩码合并（`new = (old & ~mask) | (wdata & mask)`），`mask` 由 `wstrb` 逐字节展开
  - 影响面：6 个 peripheral 的所有 `ch_reg` 写路径
- **`axi_spi.h` 不驱动 `bresp`**：`grep -c "io().bresp = " include/axi4/peripherals/axi_spi.h` = 0
  → BRESP 悬空（Verilog 里是未驱动 wire，interpreter 里靠默认值 0 侥幸为 OKAY）
  - 改法：与其它 peripheral 一致 `io().bresp = ch_uint<2>(0_d);`
- **未映射地址返回 OKAY**：全部 peripheral 对未解码地址返回 `bresp=OKAY`，
  与 `include/axi4/AGENTS.md` 已有的 ANTI-PATTERN"Unmapped address DECERR"冲突
  - 改法：地址未命中任何 `sel_*` 时返回 `DECERR(3)`

## 非目标（Out of Scope）

- ❌ 改 `include/axi4/peripherals/axi_{uart,timer,gpio}.h`
  （已由 `fix-axi4-peripheral-rst-n-port-2026-10` 收敛）
- ❌ 改 `tests/test_axi_{uart,timer,gpio}.cpp`（同上）
- ❌ 改 `tests/test_axi_{pwm,i2c,spi}_behavior.cpp`（已归档）
- ❌ 改 `tests/test_verilator_*.cpp`（spec 锁定的 macro reference impl）
- ❌ 顶层 interconnect（`axi_interconnect_4x4.h`）的复位策略

## Acceptance

### Scenario: 编译与回归不退化

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -cE "error:|Error"`
- **THEN** `0`
- **AND** `cd build && ctest -LE perf --output-on-failure` → 全 PASS（数字按实际 `ctest -N` 校准）

### Scenario: F1 `AxiDma` 复位语义有显式表

- **WHEN** reviewer 阅读 `axi_dma.h` 的复位实现
- **THEN** MUST 能找到 13 个 `ch_reg` 各自的显式复位值
- **AND** `state` 的复位值 MUST 为 0（与 `is_idle` 一致）
- **AND** `AxiDma` MUST 有独立 Requirement（不得复用 slave 的 Requirement）

### Scenario: F2/F2'/F3 参考实现与新规范一致

- **WHEN** 对 `axi_pwm.h` / `axi_i2c.h` / `axi_spi.h` 执行
  `grep -c "bvalid = " <file>`
- **THEN** 每个文件 MUST 只有 1 处 `bvalid` 赋值（消除双赋值）
- **AND** `bvalid` MUST 由 `b_valid` 寄存器驱动（保持语义）
- **AND** `bvalid` MUST 在 **WDATA 被接受之后**才拉高
  （MUST NOT 是 `bvalid = aw_done` —— 那会早于 WDATA）
- **AND** `aw_done->next` 的清零条件 MUST 是 `b_valid && bready`
  （MUST NOT 是裸 `bready`）
- **AND** MUST 存在 `addr_reg` / `ar_addr_reg`，解码 MUST 基于锁存地址
  （`grep -cE 'addr_bits = io\(\)\.aw?r?addr' <file>` → 0）
- **AND** `cd build && ctest -R "test_axi_(pwm|i2c|spi)" --output-on-failure` → 全 PASS
  （**防假阳性**：`_behavior` 测试是 dual-backend 的，改坏会同时挂两个后端）

### Scenario: F10 字节写与 bresp 完整性

- **WHEN** 对 6 个 peripheral 统计 `io().wstrb` 的**使用**（非声明）次数
- **THEN** MUST ≥ 1（写路径 MUST 消费字节掩码）
- **AND** `grep -c "io().bresp = " include/axi4/peripherals/axi_spi.h` ≥ 1（BRESP MUST 被驱动）
- **AND** 未解码地址 MUST 返回 `DECERR`（而非 OKAY）

### Scenario: F5 孤儿头处置完成

- **WHEN** `grep -rn "rv32i_soc" --include=*.cpp --include=*.h . | grep -v "src/rv32i_soc.h:"`
- **THEN** 命中 0 行（已删除 —— 定案选项 A）
- **AND** 不得保持"存在但不被编译"的中间态

### Scenario: F6 example 有行为断言且写路径真实可用

- **WHEN** `./build/examples/axi4/axi_soc_example`
- **THEN** 输出 `GPIO_OUT = 0xab`
- **AND** exit 0
- **AND** example 源码含对 `gpio_out` 的断言（CI 可捕获回归）
- **AND** `AxiLiteTestMaster` 的 `wvalid` / `bready` MUST 由状态驱动
  （MUST NOT 依赖 `io().awready` / `io().bvalid` 这类从设备的组合输出）

### Scenario: F7 主 spec 类名一致（验证项）

- **WHEN** `grep -c "AxiLiteDma" openspec/specs/dual-backend-test-coverage/spec.md`
- **THEN** `0`
- **AND** `grep -c "AxiDma" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1
- **AND** 本 change **MUST NOT** 提交 "AXI4 peripheral behavior test SHALL 走
  DUAL_BACKEND_TEST_PAIR" 的 MODIFIED delta（由下游 change 统一负责）

### Scenario: openspec 合规

- **WHEN** `openspec validate axi4-peripheral-followups-2026-10 --strict`
- **THEN** valid
- **AND** archive 时**不得**使用 `--skip-specs`（本 change 有 spec delta）

### Scenario: F8/F9 文档与测试同步

- **WHEN** `grep -c "kRegDirectionWrite" tests/test_axi_gpio.cpp`
- **THEN** 修复后应为 `0`（写侧与读侧映射统一后不再需要写侧专用偏移常量）
- **AND** `grep -c "rst_n" examples/axi4/axi_soc_example.cpp` ≥ 3
  （顶层 `rst_n` 端口 + gpio + uart 的连线）

## Capabilities

### ADDED Requirements

见 `specs/dual-backend-test-coverage/spec.md`（2 个 ADDED Requirement：
AXI4 master 复位契约 + 参考实现与规范形态一致）。
