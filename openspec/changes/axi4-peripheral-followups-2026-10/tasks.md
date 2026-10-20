## Tasks

> 前置依赖：`fix-axi4-peripheral-rst-n-port-2026-10` 必须先 ship
> （本 change 假定 3 个 AXI4-Lite slave 已收敛到规范形态）。

- [ ] ### Task 1 (F7): 主 spec 类名修正 —— **验证项，不提交 spec delta**
  - **action**:
    - 确认下游 change `dual-backend-axi4-peripherals-extended-2026-10` 已 archive，
      且其 MODIFIED delta 已把主 spec 的 `AxiLiteDma` 修正为 `AxiDma`
    - 本 change **MUST NOT** 提交同一条 Requirement 的 MODIFIED delta（避免 archive 冲突）
  - **verify**:
    - `grep -c "AxiLiteDma" openspec/specs/dual-backend-test-coverage/spec.md` → 0
    - `grep -c "AxiDma" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1

- [ ] ### Task 2 (F1): `axi_dma.h` 加 `rst_n` + 显式复位值表
  - **action**:
    - `__io` 加 `ch_in<ch_bool> rst_n;`
    - 13 个 `ch_reg` 逐个加 `select(!io().rst_n, <显式 init>, ...)` 复位
    - `state` 复位到 0（idle）
  - **verify**:
    - `grep -c "rst_n" include/axi4/peripherals/axi_dma.h` ≥ 1
    - `cd build && ctest -R test_axi_dma --output-on-failure` → PASS
    - reviewer 复核：13 个 `ch_reg` 每个都有显式复位值（无占位）

- [ ] ### Task 3 (F2): `axi_pwm.h` / `axi_i2c.h` 消除 `bvalid` 双赋值
  - **action**:
    - `axi_pwm.h`：删除 L153 `io().bvalid = w_handshake;`
    - `axi_i2c.h`：`build_register_bank()` L149 与 `describe()` L290 取一
    - 两者改用与规范形态一致的 `b_valid` 寄存器（若 F3 同步进行则一并改）
    - **同时补地址锁存**：AW / AR 握手时锁存 `AWADDR` / `ARADDR`，
      解码 MUST 基于锁存寄存器（MUST NOT 基于活的 `io().awaddr` / `io().araddr`）
  - **verify**:
    - `grep -c "bvalid = " include/axi4/peripherals/axi_pwm.h` → 1
    - `grep -c "bvalid = " include/axi4/peripherals/axi_i2c.h` → 1
    - `cd build && ctest -R "test_axi_(pwm|i2c)" --output-on-failure` → PASS

- [ ] ### Task 4 (F3): `axi_spi.h` 的 `bvalid` 改为 `b_valid` 寄存器
  - **action**:
    - 新增 `b_valid` 寄存器；`bvalid` 由其驱动
    - `aw_done` 改为在 `b_valid && bready` 时清零
    - **同时补地址锁存**：AW / AR 握手时锁存 `AWADDR` / `ARADDR`，
      解码 MUST 基于锁存寄存器
  - **verify**:
    - `grep -c "bvalid = " include/axi4/peripherals/axi_spi.h` → 1
    - `cd build && ctest -R "test_axi_spi" --output-on-failure` → PASS

- [ ] ### Task 5 (F4): UART status / irq 语义
  - **action**:
    - 定义 `tx_empty` / `rx_valid` 的置位 / 清除条件
    - `status_reg` bit0 = TX_EMPTY、bit1 = RX_VALID
    - `irq` 由 `rx_valid` 驱动
  - **verify**:
    - `cd build && ctest -R test_axi_uart --output-on-failure` → PASS（含 status 位断言）

- [ ] ### Task 6 (F5): 孤儿头 `rv32i_soc.h` 处置
  - **action**:
    - 先 `git log --oneline -- examples/riscv-mini/src/rv32i_soc.h` 确认无其它引用
    - 默认选项 A：删除该文件（ZERO-DEBT "no orphan files"）
    - 若选 B：纳入构建 + 补 `AxiLiteUart` 的 `rst_n` 接线 + 补测试
    - ⚠️ **实测约束**（2026-10-20 review）：未驱动的 `ch_in<ch_bool>` 读出为 **0**。
      该文件 L111 实例化了 `AxiLiteUart<16>` 但**未接 `rst_n`** ——
      若纳入构建，UART 会永久处于复位态（所有寄存器恒为 init 值、
      AXI 事务不响应）。选 B 时**必须**同时补 `rst_n` 接线。
  - **verify**:
    - `grep -rn "rv32i_soc" --include=*.cpp --include=*.h . | grep -v "src/rv32i_soc.h:"` → 0 行
    - `cmake --build build -j$(nproc) 2>&1 | grep -cE "error:|Error"` → 0
    - 若选 B：reviewer 复核 `rst_n` 已接线（否则 peripheral 永久复位）

- [ ] ### Task 7 (F6 + F8): `axi_soc_example.cpp` 写路径修复 + `rst_n` 暴露
  - **action**:
    - `AxiSocTop` 加 `ch_in<ch_bool> rst_n` 并透传给 gpio / uart（F8）
    - 定位 `GPIO_OUT` 打印 0x0 的根因（`AxiLiteTestMaster` / `AxiLiteMatrix2x2` 路径）并修复（F6）
    - 补 `REQUIRE(gpio_out == 0xAB)` 断言
    - 复核 `AxiLiteTestMaster` 是否满足 AXI4-Lite "WVALID 保持到 WREADY"
  - **verify**:
    - `./build/examples/axi4/axi_soc_example` → 输出 `GPIO_OUT = 0xab` 且 exit 0
    - `./run_all_ported_tests.sh` → `axi_soc` 项 PASS

- [ ] ### Task 8 (F9): GPIO 写侧 / 读侧地址映射对齐
  - **action**:
    - `axi_gpio.h` 写侧解码改为与读侧 + 头部文档一致：
      `0x00→DATA_OUT`、`0x04→DATA_IN`（只读）、`0x08→DIRECTION`、`0x0C→INTERRUPT`
    - 同步更新 `tests/test_axi_gpio.cpp`：`kRegDirectionWrite` 常量删除，
      DIRECTION 写用例改用 `kRegDirection`（0x08）
  - **verify**:
    - `grep -c "kRegDirectionWrite" tests/test_axi_gpio.cpp` → 0
    - `cd build && ctest -R test_axi_gpio --output-on-failure` → PASS
    - reviewer 复核：写侧与读侧的寄存器偏移一一对应

- [ ] ### Task 9: 全量回归 + 计数同步
  - **verify**:
    - `cd build && ctest -LE perf --output-on-failure` → 全 PASS
    - `cd build && ctest -L verilator_e2e -LE perf --output-on-failure` → 全 PASS
    - `bash scripts/verify_test_counts.sh` → exit 0
    - 根 `AGENTS.md` 的 `CTEST_TOTAL` 与 `ctest -N | tail -1` 一致

- [ ] ### Task 10: openspec archive（**禁用 `--skip-specs`**）
  - **action**:
    - `openspec archive axi4-peripheral-followups-2026-10 --yes`
  - **verify**:
    - `ls openspec/changes/archive/ | grep followups` → 命中
    - `grep -c "AxiLiteDma" openspec/specs/dual-backend-test-coverage/spec.md` → 0
    - `grep -c "AXI4 master peripheral" openspec/specs/dual-backend-test-coverage/spec.md` ≥ 1