## Tasks

> 前置依赖：`axi4-peripheral-followups-2026-10` 必须先 ship
> （本 change 假定全体 peripheral 已收敛到规范形态，且 `bresp` / `rresp` 已完整驱动）。

- [ ] ### Task 1 (D1-a): 定义并登记"字节道"语义
  - **action**:
    - 决定哪些寄存器字节可访问（32 位 / 亚 32 位）
    - 决定亚 32 位寄存器的 `wstrb` 高位处理方式
    - 决定只读寄存器（`DATA_IN` / `RX_DATA` / `COUNT`）被写时的处理
    - 在 `include/axi4/AGENTS.md` CONVENTIONS 登记该语义
  - **verify**:
    - reviewer 复核：语义表覆盖 6 个 peripheral 的全部可写寄存器

- [ ] ### Task 2 (D1-b): 6 个 peripheral 的写路径消费 `wstrb`
  - **action**:
    - 写路径改为 `new = (old & ~mask) | (wdata & mask)`
    - `mask` 由 `wstrb` 逐字节展开
  - **verify**:
    - 对 6 个 peripheral 统计 `io().wstrb` 的**使用**次数（排除端口声明）→ MUST ≥ 1
    - 新增用例：部分字节掩码写 → 未选中字节保持原值
    - `cd build && ctest -R "test_axi_(pwm|i2c|spi|gpio|uart|timer)" --output-on-failure` → 全 PASS

- [ ] ### Task 3 (D2-a): UART TX 串行引擎（波特率 + 移位 + 帧）
  - **action**:
    - 波特率分频器 + 8 位移位寄存器 + 起始位/停止位
    - `uart_tx` 输出真实串行波形；`tx_empty` 反映移位寄存器空
  - **verify**:
    - 新增串行波形测试（采样 `uart_tx` 还原字节）
    - `cd build && ctest -R test_axi_uart --output-on-failure` → PASS

- [ ] ### Task 4 (D2-b): UART RX 接收引擎（起始位 + 8 位采样）
  - **action**:
    - 起始位检测 + 8 位采样 + 帧错误标志
    - `rx_data_reg` / `rx_valid` / `irq` 获得真实语义
  - **verify**:
    - 新增 RX 测试（模拟串行发送 → 读 `RX_DATA` → `rx_valid` 清除）
    - `cd build && ctest -R test_axi_uart --output-on-failure` → PASS

- [ ] ### Task 5 (D3): 子模块输入端口的值在 tick 内被覆盖（框架层）
  - **action**:
    - 写最小复现：子模块输入端口 `set_input_value(port, 0)` → `tick()` → 值应仍为 0
    - 定位写入者（`instr_input::eval` / `eval_combinational` / JIT 的 input 处理）
    - 修复后回归 `examples/axi4/axi_soc_example.cpp` 的**读事务**（Test 2）
  - **verify**:
    - 最小复现用例通过
    - `./build/examples/axi4/axi_soc_example` → 输出 `Read complete! DATA_IN = 0x...`
      （不再打印 known limitation）
    - `cd build && ctest -LE perf` → 全 PASS

- [ ] ### Task 6: 全量回归 + 计数同步
  - **verify**:
    - `cd build && ctest -LE perf --output-on-failure` → 全 PASS
    - `cd build && ctest -L verilator_e2e -LE perf --output-on-failure` → 全 PASS
    - `bash scripts/verify_test_counts.sh` → exit 0

- [ ] ### Task 7: openspec archive（**禁用 `--skip-specs`**）
  - **action**:
    - `openspec archive axi4-peripheral-deferred-2026-10 --yes`
  - **verify**:
    - `ls openspec/changes/archive/ | grep deferred` → 命中
