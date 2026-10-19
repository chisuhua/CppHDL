# Design: axi-spi-behavior-tests

> 实施级设计。Scope: AxiLiteSpi behavior parity runtime test（interpreter + verilator）。

## 1. 文件结构

```
openspec/changes/axi-spi-behavior-tests/
├── proposal.md
├── design.md
├── tasks.md
└── specs/dual-backend-test-coverage/spec.md
tests/
└── test_axi_spi_behavior.cpp         # 新建: 7 DUAL_BACKEND_TEST_PAIR
tests/CMakeLists.txt                  # 改: add_dual_backend_test 注册
```

## 2. AxiLiteSpi 时序契约

`include/axi4/peripherals/axi_spi.h:113-150` 显示 TransferState 含：
- `ch_reg<ch_bool> busy` / `aw_done` / `r_valid` — AXI handshake
- `ch_reg<ch_uint<32>> tx_data_reg` / `rx_data_reg` / `status_reg` / `ctrl_reg` — register bank
- `ch_reg<ch_uint<BAUD_WIDTH>> baud_reg` (BAUD_WIDTH=16) — baud divider
- `ch_reg<ch_uint<2>> spi_state` — SPI FSM state
- `ch_reg<ch_uint<DATA_WIDTH>> shift_reg` — SPI shift register
- `ch_reg<ch_uint<BAUD_WIDTH>> baud_divider` — baud counter

### 2.1 register 地址

| offset | name | R/W | 描述 |
|--------|------|-----|------|
| 0x00 | TX_DATA | W | 写触发 transfer（per `axi_spi.h:16`） |
| 0x04 | RX_DATA | R | 读 received data |
| 0x08 | STATUS | R | busy / rx_valid bits |
| 0x0C | CTRL | R/W | transfer control |

### 2.2 AXI4-Lite handshake 协议

`axi4/AGENTS.md` 提到：`busy/aw_done/r_valid` trio + `addr >> 2 & mask` decode。

### 2.3 行为契约

| 行为 | expected timing |
|------|-----------------|
| 写 TX_DATA (0x00) | cycle N+1: busy=1, aw_done=1; cycle N+2: bvalid=1, bresp=OKAY (AXI 响应), transfer_active=1 |
| transfer 进行中 | sclk toggles, mosi shifts |
| transfer 完成 | rx_valid=1 (per STATUS), r_valid=1 (AXI 读响应) |
| 读 STATUS (0x08) | rdata = busy/rx_valid bitmask |

## 3. test_axi_spi_behavior.cpp 设计

### 3.1 helper function

```cpp
static inline void axi_write(ch::Simulator& sim, axi4::AxiLiteSpi<8,16>& spi,
                              uint32_t addr, uint32_t data) {
    sim.set_input_value(spi.io().awaddr, addr);
    sim.set_input_value(spi.io().awvalid, 1_d);
    sim.set_input_value(spi.io().wdata, data);
    sim.set_input_value(spi.io().wvalid, 1_d);
    sim.set_input_value(spi.io().bready, 1_d);
    sim.tick();
    // bus must ack awready/wready within 1-2 cycles (per state machine)
    sim.set_input_value(spi.io().awvalid, 0_d);
    sim.set_input_value(spi.io().wvalid, 0_d);
    sim.tick();
}
```

注：handshake 周期数依赖状态机，**首个测试先实测 handshake latency**，再下结论。

### 3.2 macro pattern

```cpp
DUAL_BACKEND_TEST_PAIR(
    axi4::AxiLiteSpi<8, 16>,
    "AxiLiteSpi - AXI write TX_DATA latches value",
    "[axi][spi][behavior][tx-data][dual]",
    [&]() {
        axi_write(sim, dev.instance(), 0x00, 0xAB_h);
        // verify tx_data_reg was latched: read STATUS busy bit
        REQUIRE(static_cast<uint64_t>(
            sim.get_port_value(dev.instance().io().status_reg.bits(0))) == 1);
        // (or use a different observable)
    });
```

注意：STATUS 是 32-bit register 的 ch_out，但 `status_reg` 是不是 ch_out？还是 reg ？需要查 AxiLiteSpi describe 的 io 写入。

### 3.3 port 接口约束

AxiLiteSpi 顶层 ch_out 是 AXI4-Lite response（rdata / bresp / bvalid / rvalid / awready / wready / arready）+ SPI physical（spi_mosi / spi_sclk / spi_cs / irq）+ 内部 state。这些 ch_out 是 observable。

**但 internal register** (tx_data_reg / status_reg) **不是顶层 ch_out** —— 它们是 internal state。要验证"写入 TX_DATA latches"得通过 **status_reg bit 的 ch_out 输出**或者 **axi 响应**间接观测。

### 3.4 7 个 TEST_CASE 设计

| TEST_CASE | 验证 |
|---|---|
| 1. AXI write TX_DATA latches value | awvalid + wdata + wvalid → awready + wready（握手）+ bvalid 后 busy=1 (转 STATUS bit) |
| 2. AXI write CTRL triggers transfer | wvalid 到 0x0C → transfer_active=1 (BUSY bit 翻转) |
| 3. Transfer inactive keeps MOSI low | idle: spi_mosi=0, spi_cs=1 (active low) |
| 4. Read STATUS returns busy + rx_valid bits | arvalid to 0x08 + arready + rvalid + rdata = STATUS |
| 5. AXI write captures BRESP=OKAY | wvalid → bvalid=1, bresp=0 |
| 6. 32-bit variant transfer path | AxiLiteSpi<32, 16> 同样流程 |
| 7. Reset clears busy + SPI idle | rst_n=0 → busy=0, spi_cs=1 |

## 4. spec 增量（详见 specs/dual-backend-test-coverage/spec.md）

```
### Requirement: AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR
- file naming MUST _behavior suffix
- 接口 MUST sim.set/get input value only
```

## 5. 实施 TDD 5 步

1. **新建 test_axi_spi_behavior.cpp + 1 个 DUAL_BACKEND_TEST_PAIR**（interpreter 试通 AXI write BRESP）→ 摸 AxiLiteSpi handshake 时序
2. **加 4 个完成 AXI-Lite 行为的测试**（写 TX_DATA / 读 STATUS / BRESP OKAY / MOSI idle）
4. **加 32-bit variant** + **Reset 行为**
5. **CMakeLists.txt 转换 + 编译 + ctest 验证**

## 6. 风险

| 风险 | 等级 | mitigation |
|------|------|------------|
| AxiLiteSpi handshake 实际 latency 与 plan 不同 | 🟡 | 第一个测试先实测 1-2 cycle 后再下结论 |
| 32-bit variant codegen 路径不稳 | 🟡 | warm cache compile 时间可能 5-15 sec，TIMEOUT 300 |
| STATUS bit 验证需要 ch_out 路径 | 🟡 | status_reg 可能不直出顶层，靠 **rdata** 间接观测 |
| R5 端口 > 64-bit 拒收 | 🟢 | max ch_out = 32-bit，远小于 cap |

## 7. 反向约束

- `verilator-e2e-coverage` spec: test_axi_lite_verilator_e2e.cpp 不动
- root AGENTS.md AXI4 rules: 4-byte aligned register offset 不破
- dual-backend-test-coverage spec: macro R1-R13 + R6 不破

## 8. 验收命令清单

```bash
cmake --build build -j$(nproc) --target test_axi_spi_behavior
ctest -L verilator_e2e -LE perf -R test_axi_spi_behavior --output-on-failure
bash scripts/verify_test_counts.sh
```