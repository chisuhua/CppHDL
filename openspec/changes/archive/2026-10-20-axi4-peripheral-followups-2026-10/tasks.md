## Tasks

> 前置依赖：`fix-axi4-peripheral-rst-n-port-2026-10` 必须先 ship
> （本 change 假定 3 个 AXI4-Lite slave 已收敛到规范形态）。

- [x] ### Task 1 (F7): 主 spec 类名修正 —— **验证项，不提交 spec delta**
  - **action**:
    - ✅ **已验证**：`git log -p --all -- openspec/specs/dual-backend-test-coverage/spec.md | grep -c "AxiLiteDma"` = 0
      → 主 spec 从未出现该错误类名（**空验证项**）
    - 确认下游 change `dual-backend-axi4-peripherals-extended-2026-10` 已 archive，
      且其 MODIFIED delta 已把主 spec 的 `AxiLiteDma` 修正为 `AxiDma`
    - 本 change **MUST NOT** 提交同一条 Requirement 的 MODIFIED delta（避免 archive 冲突）
  - **verify**:
    - `grep -c "AxiLiteDma" openspec/specs/dual-backend-test-coverage/spec.md` → 0
    - `grep -c "AxiDma" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1

- [x] ### Task 2 (F1): `axi_dma.h` 加 `rst_n` + 显式复位值表
  - **action**:
    - `__io` 加 `ch_in<ch_bool> rst_n;`
    - 13 个 `ch_reg` 逐个加 `select(!io().rst_n, <显式 init>, ...)` 复位
    - `state` 复位到 0（idle）
  - **verify**:
    - `grep -c "rst_n" include/axi4/peripherals/axi_dma.h` ≥ 1
    - `cd build && ctest -R test_axi_dma --output-on-failure` → PASS
    - reviewer 复核：13 个 `ch_reg` 每个都有显式复位值（无占位）

- [x] ### Task 3 (F2 + F2'): `axi_pwm.h` / `axi_i2c.h` 回收为规范形态
  - **action**:
    - **现状**（实测）：两者都**已有** `rst_n` / `busy` / `aw_done` / `r_valid` / AW→W 相位分离
      - `axi_pwm.h`：`bvalid` 双赋值（L116 `= aw_done` / L153 `= w_handshake`）→ **L153 覆盖 L116**，
        实际生效组合脉冲（不保持）
      - `axi_i2c.h`：`bvalid` 双赋值（L290 `= aw_done` / L149 `= w_handshake`，
        L149 在 `build_register_bank()` 内、于 `describe()` L310 被调用）→ **L149 覆盖 L290**
    - 新增 `b_valid` 寄存器；`bvalid` 由其驱动，**在 WDATA 被接受后**拉高、保持到 BREADY
    - `aw_done` 清零条件：裸 `bready` → **`b_valid && bready`**
    - 删除多余的那处 `bvalid` 赋值
    - **同时补地址锁存**：新增 `addr_reg` / `ar_addr_reg`，AW / AR 握手时采样地址，
      解码 MUST 基于锁存寄存器（MUST NOT 基于活的 `io().awaddr` / `io().araddr`）
  - **verify**:
    - `grep -c "bvalid = " include/axi4/peripherals/axi_pwm.h` → 1
    - `grep -c "bvalid = " include/axi4/peripherals/axi_i2c.h` → 1
    - `grep -cE 'addr_bits = io\(\)\.aw?r?addr' include/axi4/peripherals/axi_{pwm,i2c}.h` → 0 行
    - `grep -n "aw_done->next" include/axi4/peripherals/axi_{pwm,i2c}.h` → MUST 出现 `b_valid`（非裸 `bready`）
    - `cd build && ctest -R "test_axi_(pwm|i2c)" --output-on-failure` → PASS
      （**防假阳性**：`_behavior` 是 dual-backend 测试，改坏会同时挂两个后端）

- [x] ### Task 4 (F3 + F2'): `axi_spi.h` 回收为规范形态
  - **action**:
    - **现状**（实测）：`bvalid` **只有 1 处**赋值（L350 `= aw_done`），
      但它**早于 WDATA 被接受**就拉高 → 违反"BVALID 在 WDATA 之后"
    - 新增 `b_valid` 寄存器；`bvalid` 由其驱动（WDATA 之后拉高，保持到 BREADY）
    - `aw_done` 改为在 `b_valid && bready` 时清零
    - **同时补地址锁存**：AW / AR 握手时锁存 `AWADDR` / `ARADDR`，
      解码 MUST 基于锁存寄存器
    - **补 `io().bresp = ch_uint<2>(0_d);`**（当前完全未驱动 BRESP → Verilog 里悬空）
  - **verify**:
    - `grep -c "bvalid = " include/axi4/peripherals/axi_spi.h` → 1
    - `grep -c "io().bresp = " include/axi4/peripherals/axi_spi.h` ≥ 1
    - `grep -cE 'addr_bits = io\(\)\.aw?r?addr' include/axi4/peripherals/axi_spi.h` → 0 行
    - `cd build && ctest -R "test_axi_spi" --output-on-failure` → PASS

- [x] ### Task 5 (F4): UART status / irq 语义 —— **定案：最小诚实**
  - **action**:
    - **范围决策（2026-10-20，用户拍板）**：只做 TX 1 深缓冲 + RX 标注未实现；
      **不实现 UART 串行引擎**（RX 引擎 / TX 移位 / 波特率）→ deferred change
    - TX 侧 1 深缓冲：`tx_empty->next = select(!rst_n, true, select(we_tx_data, false, tx_empty))`
      （写 TX_DATA → `tx_empty=0` 一拍 → 回 1）
    - `status_reg` **单一赋值点**（当前只有 RX_VALID 一支，TX_EMPTY 位是死代码）：
      bit0 = TX_EMPTY（跟随 `tx_empty`）、bit1 = RX_VALID（跟随 `rx_valid`）
    - RX 侧：`rx_valid` 恒 false（**无 RX 引擎**）→ `irq` 恒 0 是**设计结果**
      —— MUST 在 `axi_uart.h` 注释中显式写明"RX 引擎未实现，`uart_rx` 未使用"
    - `uart_tx` 保持空闲高电平（无移位引擎，不产生伪波形）
    - 同步更新 `tests/test_axi_uart.cpp` 的 STATUS 用例（现状断言 `== 0`，
      新的 TX_EMPTY 语义下应为 bit0=1）
  - **verify**:
    - `cd build && ctest -R test_axi_uart --output-on-failure` → PASS（含 status 位断言）
    - reviewer 复核：`axi_uart.h` 中 `status_reg->next` **只有 1 处**赋值
    - reviewer 复核：注释里明确写明 RX 引擎未实现（不留"看起来是 bug"的歧义）

- [x] ### Task 6 (F5): 孤儿头 `rv32i_soc.h` 处置 —— **定案：删除（选项 A）**
  - **action**:
    - ✅ 已确认无引用：`grep -rn "rv32i_soc" --include=*.cpp --include=*.h . | grep -v "src/rv32i_soc.h:"` → 0 行
    - `git rm examples/riscv-mini/src/rv32i_soc.h`（ZERO-DEBT "no orphan files"）
    - ⚠️ 归档说明中记一笔：若将来有人要恢复该头，
      **必须**同时补 `AxiLiteUart` 的 `rst_n` 接线 ——
      实测未驱动的 `ch_in<ch_bool>` 读出为 **0** → peripheral **永久复位**
      （寄存器恒为 init 值、AXI 事务不响应）
  - **verify**:
    - `grep -rn "rv32i_soc" --include=*.cpp --include=*.h . | grep -v "src/rv32i_soc.h:"` → 0 行
    - `cmake --build build -j$(nproc) 2>&1 | grep -cE "error:|Error"` → 0

- [x] ### Task 7 (F6 + F8): `axi_soc_example.cpp` 写路径修复 + `rst_n` 暴露
  - **action**:
    - `AxiSocTop` 加 `ch_in<ch_bool> rst_n` 并透传给 gpio / uart（F8）
    - **F6 根因（已实测定位，双重）**：
      1. `src/simulator.cpp:257` `eval_combinational()` 是**单遍、无拓扑序、不迭代**；
         `master`(L154) 先于 `matrix`(L157)/`gpio`(L160) 创建 →
         `master.wvalid`(L89)/`master.bready`(L95) 读到**上一拍**的 `awready`/`bvalid`
      2. `io().wvalid = select(write_start, io().awready, false)`（L88-89）
         用**从设备的 ready** gate 自己的 valid（违 AXI），且只拉 1 拍 →
         新规范形态（`w_handshake` 需 `aw_done`）下必挂
    - **正解**：master 的 `wvalid` / `bready` 改为**状态驱动**
      （`state == WRITE` → `wvalid = 1`，`state` 保持到 `write_done`），
      **不依赖从设备的组合输出**；`awaddr` / `wdata` 全程保持稳定
    - ⚠️ **不要**靠调整模块创建顺序来"碰巧"让单遍求值 work（脆弱）
    - 补 `REQUIRE(gpio_out == 0xAB)` 断言
  - **verify**:
    - `./build/examples/axi4/axi_soc_example` → 输出 `GPIO_OUT = 0xab` 且 exit 0 ✅
    - `./run_all_ported_tests.sh` → `axi_soc` 项 PASS ✅
    - reviewer 复核：`AxiLiteTestMaster` 不再出现 `io().awvalid`/`io().bvalid`
      这类从设备组合输出的**门控依赖** ✅
  - **note**:
    - F8 已完成：`AxiSocTop` 新增 `ch_in<ch_bool> rst_n` 并透传给 gpio / uart，
      `main()` 中 `sim.set_input_value(top.io().rst_n, true)` 释放复位
      （未驱动的 `ch_in` 读出为 0 → 从设备会永久复位）
    - 实测根因有两层（A: master 的 WVALID 生成违 AXI；B: `AxiLiteMatrix2x2` 的 6 类缺陷），
      详见 proposal 的 F6 章节。写路径已打通（`GPIO_OUT = 0xab`）✅
    - **读路径（Test 2）仍未打通** —— 根因是框架层的"子模块输入端口值在 tick 内被覆盖"
      （proposal 的 **F11**）。example 已显式打印该已知限制，不再静默"成功"。
      F11 需另开 change 跟进（框架层 `src/ast/instr_io.cpp` / `src/simulator.cpp`）

- [x] ### Task 8 (F9): GPIO 写侧 / 读侧地址映射对齐
  - **action**:
    - `axi_gpio.h` 写侧解码改为与读侧 + 头部文档一致：
      `0x00→DATA_OUT`、`0x04→DATA_IN`（只读）、`0x08→DIRECTION`、`0x0C→INTERRUPT`
    - 同步更新 `tests/test_axi_gpio.cpp`：`kRegDirectionWrite` 常量删除，
      DIRECTION 写用例改用 `kRegDirection`（0x08）
  - **verify**:
    - `grep -c "kRegDirectionWrite" tests/test_axi_gpio.cpp` → 0
    - `cd build && ctest -R test_axi_gpio --output-on-failure` → PASS
    - reviewer 复核：写侧与读侧的寄存器偏移一一对应

- [x] ### Task 9 (F10): `bresp` 完整性 + 未映射地址 DECERR —— **定案：只修真缺陷**
  - **action**:
    - **范围决策（2026-10-20，用户拍板）**：`wstrb` 字节写**不在本 change**
      （另拆 `axi4-peripheral-deferred-2026-10`）
    - `axi_spi.h` 补 `io().bresp = ch_uint<2>(0_d);`（当前 BRESP 悬空）
    - 6 个 peripheral：地址未命中任何 `sel_*` 时 `bresp` / `rresp` 返回 `DECERR(3)`
      （依赖地址锁存 → 响应正确归属该笔事务）
  - **verify**:
    - 对 6 个 peripheral 执行 `grep -c "io().bresp = " <file>` → MUST ≥ 1
    - `grep -c "io().rresp = " <file>` → MUST ≥ 1
    - 新增用例：写未解码地址 → `bresp == 3`（DECERR）
    - `cd build && ctest -R "test_axi_(pwm|i2c|spi|gpio|uart|timer)" --output-on-failure` → 全 PASS

- [x] ### Task 10: 全量回归 + 计数同步
  - **verify**:
    - `cd build && ctest -LE perf --output-on-failure` → 全 PASS
    - `cd build && ctest -L verilator_e2e -LE perf --output-on-failure` → 全 PASS
    - `bash scripts/verify_test_counts.sh` → exit 0
    - 根 `AGENTS.md` 的 `CTEST_TOTAL` 与 `ctest -N | tail -1` 一致

- [ ] ### Task 11: openspec archive（**禁用 `--skip-specs`**）
  - **action**:
    - `openspec archive axi4-peripheral-followups-2026-10 --yes`
  - **verify**:
    - `ls openspec/changes/archive/ | grep followups` → 命中
    - `grep -c "AxiLiteDma" openspec/specs/dual-backend-test-coverage/spec.md` → 0
    - `grep -c "AXI4 master peripheral" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1