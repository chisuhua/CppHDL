## Tasks

> 顺序原则：**先建立可见性（RED）→ 再改 design（GREEN）→ 最后回归 + spec 合规**。
> 每条 task 的 verify 判据都必须**能真正拦住问题**（不允许假阳性）。

- [x] ### Task 0: 建立 RED 可见性 —— 3 个新测试文件 + 潜伏硬错误暴露
  - **action**:
    - 新建 `tests/test_axi_gpio.cpp` / `tests/test_axi_uart.cpp` / `tests/test_axi_timer.cpp`
      （interpreter-only 回归测试；`tests/CMakeLists.txt` 用 `add_catch_test` 注册）
    - 测试用例至少覆盖 proposal §Why 的 2 个 RED 场景：
      1. 协议合规 master（AW/W 同周期握手后撤 valid，转等 BVALID）→ 断言 BVALID 可被观察到
      2. AW 与 W 分离两周期 → 断言写数据落地
      3. `rst_n` 拉低 → 断言寄存器回到 init 值
  - **verify**:
    - `cmake --build build -j$(nproc) 2>&1 | grep -E "axi_timer.h.*error"` → **必须报出**
      `ambiguous overload for 'operator||'`（预期 RED，证明 `AxiLiteTimer` 首次被真正编译）
    - `cd build && ctest -R "test_axi_(uart|timer|gpio)" --output-on-failure` → **预期 FAIL**
      （RED 基线；失败点必须是行为断言而非编译错误）
  - **note**: `tests/test_axi_{uart,timer,gpio}.cpp` 是**新建**（原本不存在），
    不违反 spec R"原 test_axi_*.cpp 不动"（该约束针对已存在的 baseline 文件）

- [x] ### Task 1: `axi_timer.h` —— 先修硬错误 + 8 处逻辑 overload，再收敛形态
  - **action**:
    - 按 proposal §1 规范形态重写 AW/W/B/AR/R 通道 + 状态寄存器组
    - `__io` 加 `ch_in<ch_bool> rst_n;`
    - 4 个寄存器（`load_reg` / `count_reg` / `ctrl_reg` / `status_reg`）加 `select(!rst_n, init, ...)` 复位
    - 修掉现状 L93 `arready` 误用 `awvalid` 的 bug（规范形态下由 `ar_handshake` 驱动）
    - 全部 8 处 `&&` / `||` 消除（含 L123 的 ambiguous overload 硬错误）
  - **verify**:
    - `grep -nE 'io\(\)\.[a-z_]+ *(&&|\|\|)' include/axi4/peripherals/axi_timer.h` → 0 行
    - `cmake --build build -j$(nproc) 2>&1 | grep "axi_timer.h" | grep -c deprecated` → 0
    - `cd build && ctest -R test_axi_timer --output-on-failure` → PASS

- [x] ### Task 2: `axi_uart.h` —— 加 rst_n + 规范形态 + status_reg 单处赋值
  - **action**:
    - 按 proposal §1 规范形态重写通道 + 状态寄存器组
    - `__io` 加 `ch_in<ch_bool> rst_n;`
    - 4 个数据寄存器加 `select(!rst_n, ch_uint<32>(0_d), ...)` 复位
    - `tx_empty` / `rx_valid` 加显式保持 + 复位赋值
    - `status_reg->next` 合并为**唯一一处**赋值（保留当前实际生效的 rx_valid 分支语义；
      TX_EMPTY status 位登记到 follow-up）
  - **verify**:
    - `grep -c "status_reg->next" include/axi4/peripherals/axi_uart.h` → 1
    - `grep -cE 'io\(\)\.[a-z_]+ *(&&|\|\|)' include/axi4/peripherals/axi_uart.h` → 0
    - `cd build && ctest -R test_axi_uart --output-on-failure` → PASS

- [x] ### Task 3: `axi_gpio.h` —— 加 rst_n + 规范形态
  - **action**:
    - 按 proposal §1 规范形态重写通道 + 状态寄存器组
    - `__io` 加 `ch_in<ch_bool> rst_n;`
    - 3 个寄存器加 `select(!rst_n, ch_uint<32>(0_d), ...)` 复位
    - `gpio_out` / `gpio_dir` / `irq` 语义保持不变
  - **verify**:
    - `grep -cE 'io\(\)\.[a-z_]+ *(&&|\|\|)' include/axi4/peripherals/axi_gpio.h` → 0
    - `cd build && ctest -R test_axi_gpio --output-on-failure` → PASS

- [x] ### Task 3.5: 地址锁存（归档后 review 的实测发现）
  - **action**:
    - 3 个 peripheral 各加 `addr_reg` / `ar_addr_reg`（AW / AR 握手时采样地址）
    - 写侧 / 读侧地址解码改为基于**锁存**的地址（MUST NOT 读活的 `io().awaddr`）
    - 3 个测试文件各加 1 条"AW 握手后改写 awaddr 写目标不变"回归用例
    - `include/axi4/AGENTS.md` CONVENTIONS 增补「地址锁存」约定 + ANTI-PATTERN 锚点
  - **verify**:
    - `grep -nE 'addr_bits = io\(\)\.aw?r?addr' include/axi4/peripherals/*.h` → 0 行
    - `cd build && ctest -R "test_axi_(uart|timer|gpio)" --output-on-failure` → PASS
    - **防假阳性**：临时移除 `addr_reg` 锁存后重跑，`test_axi_gpio` MUST 变红（实测确认）
  - **note**:
    - 根因：相位分离把 W 的接受窗口从"与 AW 同周期"拉长到"AW 之后任意周期"，
      而地址解码仍读活的 `io().awaddr` —— master 在 AW 握手后改写 AWADDR
      会让写静默落到错误寄存器。旧实现因 W 必须与 AW 同周期而侥幸安全。
    - spec delta 因此从 `## ADDED Requirements` 改为 `## MODIFIED Requirements`
      （主 spec 已有该 Requirement，re-archive 报 `~ 1 modified`，无重复 Requirement）

- [x] ### Task 4: `examples/axi4/axi_soc_example.cpp` —— rst_n 接线
  - **action**:
    - `AxiSocTop::describe()` 内把 `gpio.io().rst_n` 与 `uart.io().rst_n` 接到常量 `ch_bool(true)`
  - **verify**:
    - `grep -c "rst_n" examples/axi4/axi_soc_example.cpp` → 2
    - `./build/examples/axi4/axi_soc_example` → exit 0
    - `./run_all_ported_tests.sh` 中 `axi_soc` 项 PASS

- [x] ### Task 5: 文档同步
  - **action**:
    - `include/axi4/AGENTS.md`：CONVENTIONS 记录规范形态；
      ANTI-PATTERN 的 `Found at axi_timer.h:77-81` 行号锚点改为 grep 模式
    - 根 `AGENTS.md`：`CTEST_TOTAL: 169` → `172`，并同步 NOTES 的测试计数段落
  - **verify**:
    - `bash scripts/verify_test_counts.sh` → exit 0
    - `cd build && ctest -N | tail -1` 与根 `AGENTS.md` 数字一致

- [x] ### Task 6: 全量回归
  - **verify**:
    - `cd build && ctest -LE perf --output-on-failure` → 161/161 PASS
    - `cd build && ctest -L verilator_e2e -LE perf --output-on-failure` → 23/23 PASS
    - `touch tests/test_axi_{uart,timer,gpio}.cpp && cmake --build build -j$(nproc) --target test_axi_uart test_axi_timer test_axi_gpio 2>&1 | tee /tmp/opencode/build.log | grep -cE "error:"` → 0
    - `grep -c "test_axi_timer.cpp" /tmp/opencode/build.log` ≥ 1
      （**防假阳性**：证明实例化 `AxiLiteTimer` 的 TU 真的被编译过 ——
      该头文件原本零实例化，其 hard error 在未实例化时根本不进编译。
      `touch` 是必需的：增量构建时目标已是最新，log 里不会有该文件名）
    - `grep -cE "axi_(uart|timer|gpio)\.h.*-Wdeprecated-declarations" /tmp/opencode/build.log` → 0

- [x] ### Task 7: openspec archive（**禁用 `--skip-specs`**）
  - **action**:
    - `openspec archive fix-axi4-peripheral-rst-n-port-2026-10 --yes`
      （**不得**加 `--skip-specs` —— 本 change 有 spec delta，必须合并进主 spec）
  - **verify**:
    - `ls openspec/changes/archive/ | grep rst-n-port` → 命中
    - `grep -c "AXI4-Lite slave peripheral MUST 收敛到相位分离" openspec/specs/dual-backend-test-coverage/spec.md` → ≥ 1
      （证明 Requirement 已合并，未被 `--skip-specs` 静默丢弃）
