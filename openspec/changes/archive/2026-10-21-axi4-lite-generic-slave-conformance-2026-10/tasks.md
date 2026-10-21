## Tasks

> 前置依赖：`axi4-slave-byte-lane-2026-10`（archive `2026-10-20-axi4-slave-byte-lane-2026-10`）
> —— 本 change 在其产出的字节道语义之上继续收敛协议规范形态。
>
> **执行顺序**：Task 0 → Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6 →
> Task 7 → Task 8 → Task 9。
>
> **TDD 纪律**：Task 2 先写会失败的用例（RED），Task 3-6 逐条转绿（GREEN），
> Task 7 回归。每个 Task 的 verify 必须是**可执行命令**，
> 不接受「人工检查通过」作为唯一验收。
>
> ⚠️ **verify 必须是本机实测过的命令**（`openspec validate` 只做结构/标题校验，
> **不校验命令可执行性**）。已知踩坑（复审实测）：
>
> | 踩坑 | 后果 | 正确写法 |
> |---|---|---|
> | `add_dual_backend_test` 写成位置参数 | **全仓 configure FATAL_ERROR**（该函数用 `cmake_parse_arguments`，只认 `SOURCES`） | `add_dual_backend_test(name SOURCES file.cpp TIMEOUT 300)` |
> | `grep -c "addr_reg->next ="` | `addr_reg` 命中 `ar_addr_reg` → 计数为 2 → 验收条件永不可能满足 | `grep -cE "^[[:space:]]*${r}->next ="` |
> | 负例 TU 只给 `-I include` | 先死在 `ast_nodes.h` 的 `#include "lnodeimpl.h"` → 错误信息无 `NUM_REGS` → 假负 | 补齐 `CH_INCLUDE_DIRS` 的 6 个目录 |
> | `ctest -R "…slave_byte_lite…"` | 正则笔误 → 目标测试被静默跳过（假绿） | `slave_byte_lane`；并用 `ctest -N -R` 断言命中数 |
>
> **proposal → task 映射**：D1 → Task 3；D2 → Task 4；D3 → Task 5；
> D4 → Task 6；D5 → Task 2 / Task 7 / Task 8。

- [x] ### Task 0: spec 冲突消解 + delta 完整性核对
  - **action**:
    - 本 change 的 spec delta 由重写阶段产出，MUST 保持 3 个文件齐全：
      - `specs/axi-slave-conformance/spec.md`（新 capability，ADDED ×9）
      - `specs/dual-backend-test-coverage/spec.md`（MODIFIED ×3）
      - `specs/verilator-backend-e2e/spec.md`（MODIFIED ×1）
    - 确认 MODIFIED 的 requirement 标题与主 spec **逐字一致**（OpenSpec 按标题匹配，
      标题不一致 = 该条 delta 被静默丢弃）：
      - `现有 9 个 verilator 测试 MUST 保留原状`
      - `AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR`
      - `AXI4-Lite slave peripheral MUST 收敛到相位分离 + 响应保持的规范形态`
        （本 change 修正其第 1 条的复位措辞：「低有效**异步**复位」→「低有效，
        **在时钟沿生效**」，与新 capability 措辞统一）
      - `VerilatorBackend MUST 正确传播 Vtop 输出到 Simulator 的 data_map`
    - ⚠️ **commit 前 MUST `git add`** 本 change 的 `specs/` 目录
      （2 个 delta 目录当前 untracked，`git commit -a` 会静默丢掉 3 条 MODIFIED）
  - **verify**:
    - `openspec validate axi4-lite-generic-slave-conformance-2026-10 --strict` → PASS
    - `openspec validate --specs --strict` → PASS
    - MODIFIED 标题逐字核对（期望：两侧标题完全一致）：
      ```bash
      for r in "现有 9 个 verilator 测试 MUST 保留原状" \
               "AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR" \
               "AXI4-Lite slave peripheral MUST 收敛到相位分离 + 响应保持的规范形态"; do
        printf "%s: main=%s delta=%s\n" "$r" \
          "$(grep -c "### Requirement: $r" openspec/specs/dual-backend-test-coverage/spec.md)" \
          "$(grep -c "### Requirement: $r" openspec/changes/axi4-lite-generic-slave-conformance-2026-10/specs/dual-backend-test-coverage/spec.md)"
      done
      grep -c "### Requirement: VerilatorBackend MUST 正确传播 Vtop 输出到 Simulator 的 data_map" \
        openspec/specs/verilator-backend-e2e/spec.md \
        openspec/changes/axi4-lite-generic-slave-conformance-2026-10/specs/verilator-backend-e2e/spec.md
      ```
      → 每个数字 MUST ≥ 1（两侧都有）
    - MODIFIED 不得漏抄 Scenario（delta 侧 Scenario 集合 MUST ⊇ 主 spec 侧）：
      ```bash
      diff <(grep -c "^#### Scenario:" openspec/specs/dual-backend-test-coverage/spec.md) \
           <(grep -c "^#### Scenario:" openspec/changes/axi4-lite-generic-slave-conformance-2026-10/specs/dual-backend-test-coverage/spec.md)
      ```
      → 人工确认 delta 侧逐条覆盖主 spec 侧对应 Requirement 的全部 Scenario
    - `git status --porcelain openspec/changes/axi4-lite-generic-slave-conformance-2026-10/`
      → commit 前 MUST 无 `??` 条目

- [x] ### Task 1: 实例化点盘点 + `rst_n` 接线范式落定
  - **action**:
    - 盘点 `AxiLiteSlave` 的**全部**实例化点（已 grep 确认 5 处，MUST 复核无新增）：
      | 位置 | 形态 |
      |---|---|
      | `examples/axi4/axi4_lite_example.cpp:53` | `ch_module<AxiLiteSlave<32,32,4>>` |
      | `examples/axi4/phase3a_complete.cpp:84` | `ch_module<AxiLiteSlave<32,32,4>>`（经 converter） |
      | `tests/test_axi_lite_verilator_e2e.cpp:111` | `ch_module<AxiLiteSlave<32,32,4>>`（`AxiLiteTop`） |
      | `tests/test_verilator_trace_parity.cpp:165` | `ch_module<AxiLiteSlave<32,32,4>>` |
      | `tests/test_axi_slave_byte_lane.cpp:41-42` | `typedef`：`Lite32 = <32,32,4>` / `Lite64 = <32,64,4>` |
    - 落定**接线范式**（MUST 写进 `include/axi4/AGENTS.md`）：
      1. 顶层组件 MUST 暴露 `ch_in<ch_bool> rst_n` 并透传给子 slave；
      2. testbench MUST `sim.set_input_value(io().rst_n, 1)`；
      3. **MUST NOT** 把 `rst_n` tie 成常量 `1`（绕过复位语义 + 掩盖未接线）；
      4. 新 fixture MUST 在第一个动作处加守卫断言
         `REQUIRE(sim.get_port_value(io().rst_n) == 1)`（防「未接线 → 全 0 → 假绿」）；
      5. **层级复位传递无需额外约束** —— `context::topological_sort_visit`
         （`src/core/context.cpp:273`）沿 `src` 边递归，父的 `ch_out` 排在
         子组件的 `ch_in` 之前 → 同一拍内传递生效。
         `operator<<=` 只做 `set_driver` / `set_src`（**不创建节点**），
         故接线语句在 `describe()` 中的位置**不影响**求值顺序
         （MUST NOT 把位置当作顺序保证）
  - **verify**:
    - `grep -rn "AxiLiteSlave<" tests/ examples/ include/` → 上述 5 处全部命中
    - `grep -rn "rst_n" examples/axi4/axi4_lite_example.cpp examples/axi4/phase3a_complete.cpp
      tests/test_axi_lite_verilator_e2e.cpp tests/test_verilator_trace_parity.cpp
      tests/test_axi_slave_byte_lane.cpp` → 每处 MUST 命中
    - `grep -rnE "rst_n[^;]*= *ch_bool\(true\)|rst_n[^;]*<<= *ch_bool\(true\)" tests/ examples/`
      → MUST 命中 0 行（禁止常量 tie）

- [x] ### Task 2: RED — 新建 `tests/test_axi_lite_slave_conformance.cpp`（双后端）
  - **action**:
    - **2a（先做，本 Task 内）**：
      1. 在 `tests/CMakeLists.txt` 注册（⚠️ `add_dual_backend_test` 用
         `cmake_parse_arguments` 且只认 `SOURCES` 关键字，位置参数会被丢弃 →
         写成位置参数会让**全仓 configure 直接 FATAL_ERROR**）：
         ```cmake
         add_dual_backend_test(test_axi_lite_slave_conformance
             SOURCES test_axi_lite_slave_conformance.cpp
             TIMEOUT 300
         )
         ```
         （`TIMEOUT 300` 与兄弟测试 `test_axi_slave_byte_lane` 一致；
         默认 60s 在冷 verilator 编译下会超时）
      2. 新建测试文件，用 `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR`
         （interpreter + verilator 双跑）
      3. DUT 用 `ch_device<AxiLiteSlave<32,32,4>>`
      4. **只写不依赖 `rst_n` 的用例**（`rst_n` 端口在 Task 3 才存在，
         此刻引用它会编译失败 → RED 必须是**断言失败**而非编译错误）
    - **2b（Task 3 完成后补）**：把 `rst_n` 相关用例追加进同一文件
      （用例 #1 / #2，见 Task 3 的 verify）
    - MUST 覆盖的用例（每个都是 spec 的可测断言；**「前置断言」列是必需的** ——
      缺了它，用例可能在当前实现上假绿）：

      | # | 用例 | 前置断言（必须有） | 对应 spec Scenario |
      |---|---|---|---|
      | 1 | `rst_n` 拉低 + tick ≥1 → `busy`/4 个寄存器读回初值 | 先写入非 0 值再拉低 `rst_n` | 复位清零全部状态寄存器（Task 3 后补） |
      | 2 | `rst_n` 拉低 + tick ≥1 → `busy` 归 0，可接下一笔 | 复位前 `busy == 1` | 复位提供逃生路径（Task 3 后补） |
      | 3 | AW 握手后改写 `awaddr` → 写目标不变（仍写 reg0） | **先给 reg0 与 reg1 写入互不相同的值**（否则「目标不变」可空过） | 握手后改写写地址不改变写目标 |
      | 4 | AR 握手后改写 `araddr` → 读回原目标寄存器 | **先给 reg0 与 reg1 写入互不相同的值** | 握手后改写读地址不改变读目标 |
      | 5 | AW 一拍 → W 延后 3 拍 → 写仍被接受 | AW 拍后 `awready == 1` | W 可延后多个周期被接受 |
      | 6 | 同拍 AW+W → 该拍 `wready == 0`（W 不被接受）；保持 `wvalid` 后下一拍被接受 | — | 同拍 AW 与 W 时 W 不被接受 |
      | 7 | 写数据被接受后不拉 `bready`，连续 tick ≥3 → `bvalid` 恒 1、`awready` 恒 0 | W 拍后先 `REQUIRE(bvalid == 1)` | master 不拉 BREADY 时 bvalid 保持 |
      | 8 | `bready` 拉高 → 下一拍 `bvalid` 清 0，`awready` 恢复 1 | **先 `REQUIRE(bvalid == 1)`**（否则当前实现的 `busy` 会在 `bready` 高时提前释放 → 本用例假绿） | BREADY 握手后 bvalid 清除 |
      | 9 | `bready` 常态拉高时，**旧 B 未被取走前 MUST NOT 接受新 AW** | 前一笔写已完成、B 未被取走 | 不得在裸 bready 上释放 busy |
      | 10 | AR 被接受后不拉 `rready`，连续 tick ≥3 → `rvalid` 恒 1、`rdata` 稳定 | AR 拍后先 `REQUIRE(rvalid == 1)` | master 不拉 RREADY 时 rvalid 保持 |
      | 11 | `rready` 拉高 → 下一拍 `rvalid` 清 0 | 先 `REQUIRE(rvalid == 1)` | RREADY 握手后 rvalid 清除 |
      | 12 | 写未映射地址 `0x10` → `bresp == 0b11`，4 个寄存器全不变 | 先给 4 个寄存器写入互不相同的值 | 未映射地址写被拒绝 |
      | 13 | 读未映射地址 `0x10` → `rresp == 0b11`，`rdata == 0` | 先给 4 个寄存器写入互不相同的值 | 未映射地址读被拒绝 |
      | 14 | 写别名地址 `0x100` → `bresp == 0b11`，reg0 不变（**不得别名**） | 先给 reg0 写入非 0 值 | 别名地址不被别名到已实现寄存器 |
      | 15 | 写已映射地址但 `wstrb == 0` → `bresp == 0b00`，寄存器不变 | 先给目标寄存器写入非 0 值 | 空写的响应仍为 OKAY |

    - 用例 12-15 的**写法风格**参考同文件的 `full_write`（`tests/test_axi_slave_byte_lane.cpp:92-119`，
      本就是相位分离的三拍写法）；⚠️ **MUST NOT** 照抄同文件的 `lite_write`
      （`:57-71`，当前是同拍 AW+W，与新规范形态相反）
  - **verify**:
    - `cd build && cmake -B . .. >/dev/null && cmake --build . -j$(nproc) --target test_axi_lite_slave_conformance`
      → 编译通过（**RED 阶段也必须编译通过**）
    - `cd build && ctest -N -R test_axi_lite_slave_conformance | tail -1` → MUST 显示 1 个 test
    - `cd build && ctest -R test_axi_lite_slave_conformance --output-on-failure`
      → 此刻 MUST **FAIL**，且失败用例 MUST ≥ 3
      （至少覆盖：地址锁存 / 未映射 `DECERR` / 响应保持 三类）
    - 若 verilator 不在 PATH，verilator 半边 MUST 是 `WARN` 而非整条 SKIP
      （`DUAL_BACKEND_TEST_PAIR` R4/R5）

- [x] ### Task 3: GREEN — `AxiLiteSlave` 收敛到规范形态
  - **action**:
    - 参照实现：`include/axi4/peripherals/axi_gpio.h:96-196`（已双后端验证，**照抄形态**）
    - `__io` **末尾**新增 `ch_in<ch_bool> rst_n;`（仓库惯例：`axi_gpio.h:66` 等 6 处）
    - `describe()` 内的状态寄存器（每个 MUST 恰好一处 `->next =`，复位 pattern 裹在最外层）：

      | 寄存器 | 类型 | 初值 | 置 1 条件 | 清 0 条件 | 输出 |
      |---|---|---|---|---|---|
      | `busy` | `ch_bool` | false | `aw_handshake` 或 `ar_handshake` | `b_valid && bready` 或 `r_valid && rready` | — |
      | `aw_done` | `ch_bool` | false | `aw_handshake` | `b_valid && bready` | — |
      | `b_valid` | `ch_bool` | false | `w_handshake` | `bready` | `bvalid = b_valid` |
      | `r_valid` | `ch_bool` | false | `ar_handshake` | `rready` | `rvalid = r_valid` |
      | `addr_reg` | `ch_uint<ADDR_WIDTH>` | 0 | AW 握手时锁存 `io().awaddr` | — | — |
      | `ar_addr_reg` | `ch_uint<ADDR_WIDTH>` | 0 | AR 握手时锁存 `io().araddr` | — | — |
      | `reg0..reg3` | `ch_uint<DATA_WIDTH>` | 0 | 写路径（保留现有 `byte_lane_apply` 写法） | — | — |

      ⚠️ **置 1 条件 MUST 优先于清 0 条件**（写成清 0 优先会在 `bready` 常态拉高时
      让 `b_valid` 永远为 0 → `bvalid` 永不拉高 → 死锁，且 `aw_done` 永不清零
      → 同一笔 W 被反复接受）
    - **握手形式（与 `axi_gpio.h` 逐字同形）**：
      ```cpp
      auto aw_handshake = select(io().awvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
      io().awready = aw_handshake;
      auto w_handshake = select(io().wvalid, aw_done, ch_bool(false));
      io().wready   = w_handshake;
      ```
      （`aw_done` 是寄存器 → AW 握手当拍为 0 → 同拍 W 不被接受，属规范形态固有性质）
    - **地址解码**：MUST 基于 `addr_reg`（写）/ `ar_addr_reg`（读），
      **MUST NOT** 再用 `& 3` 掩码（那正是别名 bug 的来源）：
      `auto addr_index = addr_reg >> ch_uint<ADDR_WIDTH>(2_d);`
      步长**固定 4 字节**（与 `DATA_WIDTH` 无关）；窗口 = 字节地址 `0x00..0x0C`；
      窗口外 → `DECERR`
    - **响应**：`bresp` = 已解码 ? `0b00`(OKAY) : `0b11`(DECERR)；`rresp` 同理；
      未映射读时 `rdata` MUST 为 0；空写（`wstrb == 0` 且已映射）响应仍为 `OKAY`
    - **复位 pattern 形式**：`select(!io().rst_n, <init>, <正常逻辑>)`，
      `<init>` MUST 带显式位宽（如 `ch_uint<32>(0_d)`、`ch_bool(false)`）
    - 追加 Task 2b 的 `rst_n` 用例（#1 / #2）到测试文件
  - **verify**:
    - `grep -nE 'addr_bits = io\(\)\.aw?r?addr' include/axi4/axi4_lite_slave.h` → MUST 0 行
    - `grep -n "io().bvalid = w_handshake\|io().rvalid = ar_handshake" include/axi4/axi4_lite_slave.h`
      → MUST 0 行
    - `grep -c "rst_n" include/axi4/axi4_lite_slave.h` → MUST ≥ 11
      （1 端口 + 10 个寄存器的复位 pattern：busy / aw_done / b_valid / r_valid /
      addr_reg / ar_addr_reg / reg0..reg3）
    - `grep -cE 'io\(\)\.[a-z_]+ *(&&|\|\|)' include/axi4/axi4_lite_slave.h` → MUST 0 行
    - 单处 `next` 赋值（每个寄存器恰好 1 处；⚠️ 必须加行首锚点，
      否则 `addr_reg` 会命中 `ar_addr_reg`）：
      ```bash
      for r in busy aw_done b_valid r_valid addr_reg ar_addr_reg reg0 reg1 reg2 reg3; do
        printf "%s=%s\n" "$r" "$(grep -cE "^[[:space:]]*${r}->next =" include/axi4/axi4_lite_slave.h)"
      done
      ```
      → 每个数字 MUST 为 1
    - `cd build && ctest -R test_axi_lite_slave_conformance --output-on-failure` → 全 PASS

- [x] ### Task 4: `AxiToAxiLiteConverter` 加 `rst_n` 透传 + 状态寄存器复位
  - **action**:
    - `__io` 新增 `ch_in<ch_bool> rst_n;` 与 `ch_out<ch_bool> axil_rst_n;`
      （防御性顺序：`rst_n`（in）写在 `axil_rst_n`（out）之前；
      实际求值顺序由 `context` 的拓扑排序保证，与声明顺序无关）
    - `describe()`：`io().axil_rst_n = io().rst_n;`
    - `write_state` / `read_state` 两个 `ch_reg` 加复位 pattern
      （当前**无任何复位 pattern** → 复位后 converter 可能停在非 IDLE 态）
    - 实例化点 `examples/axi4/phase3a_complete.cpp` MUST 把顶层 `rst_n` 接到 converter
  - **verify**:
    - `grep -n "rst_n" include/axi4/axi_to_axilite.h` → 端口 + 透传 + 2 处复位 pattern 均命中
    - `grep -n "rst_n" examples/axi4/phase3a_complete.cpp` → MUST 命中
    - 复位链路接线（converter → slave）：
      `grep -n "axil_slave.io().rst_n <<= converter.io().axil_rst_n" examples/axi4/phase3a_complete.cpp`
      → MUST 命中 1 行
    - `cd build && cmake --build . -j$(nproc)` → 0 error 0 warning

- [x] ### Task 5: `NUM_REGS` 改为编译期诊断
  - **action**:
    - 在 `AxiLiteSlave` 类体内加 `static_assert`（仓库先例：
      `include/axi4/axi_to_axilite.h:35`、`include/axi4/byte_lane.h:42-43`）：
      `static_assert(NUM_REGS == 4, "AxiLiteSlave: NUM_REGS != 4 — NUM_REGS is not honoured ...")`
      （错误信息 MUST 含 `NUM_REGS` 字样）
    - **MUST NOT** 删除模板参数（会破坏 5 处 `AxiLiteSlave<32,32,4>` / `<32,64,4>` 调用点）
    - **MUST NOT** 让 `NUM_REGS` 真正生效（需本仓库不存在的寄存器数组原语，
      且触发 2 条 `CHWARN`，违反 ZERO-DEBT）
  - **verify**:
    - `grep -n "static_assert" include/axi4/axi4_lite_slave.h` → MUST 命中 ≥1 且含 `NUM_REGS`
    - **编译期负例**（用 scratch TU，**MUST NOT 改动任何仓库文件**；
      ⚠️ include 路径必须与 `CMakeLists.txt:115-122` 的 `CH_INCLUDE_DIRS` 一致 ——
      只写 `-I include` 会先死在 `ast_nodes.h:3` 的 `#include "lnodeimpl.h"` 上，
      错误信息里**没有** `NUM_REGS` → verify 假负）：
      ```bash
      mkdir -p /tmp/opencode/numregs
      cat > /tmp/opencode/numregs/neg.cpp <<'EOF'
      #include "axi4/axi4_lite_slave.h"
      template class axi4::AxiLiteSlave<32, 32, 8>;
      EOF
      g++ -std=c++23 -fsyntax-only \
          -I include -I include/core -I include/ast -I include/abstract \
          -I include/utils -I external/inipp \
          /tmp/opencode/numregs/neg.cpp 2>&1 | head -5
      ```
      → 期望：编译失败，错误信息含 `NUM_REGS`
      （`template class ...` 显式实例化会实例化类体 → 触发类体内的 `static_assert`）
    - 正例：`AxiLiteSlave<32,32,4>` MUST 编译通过
      （`cd build && cmake --build . -j$(nproc)` → 0 error）

- [x] ### Task 6: 死代码清理
  - **action**:
    - 删除 `include/axi4/byte_lane.h` 的 `byte_lane_write()`
      （全仓库零调用点，违反 ZERO-DEBT 的「无死代码」）
  - **verify**:
    - `grep -rn "byte_lane_write" include/ tests/ examples/` → MUST 0 处
    - `cd build && cmake --build . -j$(nproc)` → 0 error 0 warning

- [x] ### Task 7: 既有测试与 example 同步（4 个文件 + 2 个 example）
  - **action**:
    - ⚠️ **写事务时序变化**（新形态下同拍 AW+W **不被接受**，master 必须保持 `wvalid`
      直到 `wready` 拉高）：
      - `tests/test_axi_slave_byte_lane.cpp`：`lite_write` 助手当前同拍驱动 AW+W
        （`:60-68`）→ MUST 改为「AW 一拍（`awvalid=1, wvalid=0`）→ W 一拍
        （`wvalid=1`）→ B 拍（`bready=1`）」三拍形式（与同文件 `full_write` 对称）
      - `tests/test_axi_lite_verilator_e2e.cpp`：写事务同拍 AW+W → 改两拍；
        `UNSCOPED_INFO` 的写拍观测 MUST 升级为断言
        （W 拍后 `bvalid == 1`、`awready == 0` —— 对应 `verilator-backend-e2e`
        的 MODIFIED delta）
      - `tests/test_verilator_trace_parity.cpp`：写事务同拍 AW+W → 改两拍
    - `rst_n` 接线：`tests/test_axi_lite_verilator_e2e.cpp`（`AxiLiteTop`）、
      `tests/test_verilator_trace_parity.cpp`（`TraceParityAxiLite`）
      MUST 暴露并透传 `rst_n`；
      `tests/test_axi_slave_byte_lane.cpp`（无 fixture class，DUT 是 `ch_device<Lite32>` 直连）
      MUST 由 TB **直接驱动 DUT 顶层 `rst_n` 端口**
    - `examples/axi4/axi4_lite_example.cpp`：顶层暴露 `rst_n` + 透传 + TB 驱动
      （写事务**已是**相位分离：AW cycle 0 / W cycle 1 → 时序无需改）
    - `examples/axi4/phase3a_complete.cpp`：顶层暴露 `rst_n` → converter → slave 全链路透传
    - ⚠️ **`examples/axi4/axi4_lite_example.cpp` 的数据校验分支 MUST 非零退出**：
      现状 `[DATA] FAIL` 只打印不退出 → 该 example 写不进去也照样 exit 0
      （`run_all_ported_tests.sh` 照样绿）→ 本 change 修好了它，必须加回归护栏
    - `tests/test_axi_slave_byte_lane.cpp` 的**文件头注释**（`:19-21` 与 `:52-56` 描述
      「AW 与 W 必须同拍握手」「bvalid 是组合单拍」「两个通用 slave 都没有 `rst_n`」）
      MUST 一并更新 → 否则变成假陈述；更新后 MUST 写明
      「**`Axi4Slave` 仍无 `rst_n`**（属 `axi4-full-generic-slave-burst-2026-10`）」
    - ⚠️ 这 3 个测试文件被 `dual-backend-test-coverage` 锁定 → 本 change 的 MODIFIED delta
      已解锁「fixture 暴露/透传 `rst_n` + TB 驱动 + 写事务改两拍 + 断言更新」这一受限例外，
      变更 MUST 严格落在此范围内
  - **verify**:
    - `git diff --stat tests/test_verilator_trace_parity.cpp tests/test_axi_lite_verilator_e2e.cpp`
      → MUST 只触及 fixture / testbench / 断言，MUST NOT 改 TEST_CASE 名与 SECTION 结构
    - `cd build && ctest -R "test_axi_(slave_byte_lane|lite_verilator_e2e)|test_verilator_trace_parity" --output-on-failure`
      → 全 PASS（⚠️ 正则 MUST 是 `slave_byte_lane`，不是 `slave_byte_lite`）
    - `cd build && ctest -N -R "test_axi_(slave_byte_lane|lite_verilator_e2e)|test_verilator_trace_parity" | tail -1`
      → MUST 显示 3 个 test（若少于 3，说明正则写错）
    - `./run_all_ported_tests.sh` → 28/28 PASS

- [x] ### Task 8: 文档同步
  - **action**:
    - `include/axi4/AGENTS.md`：
      - CONVENTIONS 新增「通用 AXI slave 规范形态」小节
        （`rst_n` / AW→W 相位分离 / 同拍 AW+W 不被接受 / 地址锁存 + 4 字节步长 /
        写读响应保持 / 置 1 优先于清 0 / `busy` 释放条件 / 未映射 `DECERR` /
        空写仍 `OKAY`）
      - ANTI-PATTERNS 新增 verify 命令：
        - `grep -nE 'addr_bits = io\(\)\.aw?r?addr' include/axi4/axi4_lite_slave.h` → MUST 0 行
        - `grep -n "io().bvalid = w_handshake" include/axi4/axi4_lite_slave.h` → MUST 0 行
      - CONVENTIONS → **复位**：把「低有效**异步**复位」改为「低有效，**在时钟沿生效**」
        （与新 spec 一致；`select(!rst_n, init, ...)` 落在 `next` 表达式上，由时钟沿采样）
      - **6 个 peripheral 头注释的「异步复位」措辞** MUST 同步改写
        （`include/axi4/peripherals/axi_gpio.h:65` / `axi_pwm.h:70,153,239` /
        `axi_timer.h:62` / `axi_uart.h:83` / `axi_dma.h:41` 共 7 处）
        —— 新 spec 的 Requirement 1 第 3 点**点名**了这些措辞不准确，
        只改 AGENTS.md 会留下 7 处错措辞
      - 修正已失效陈述：「Response: All peripherals return `bresp=OKAY(0)`」→
        通用 slave 对未映射地址返回 `DECERR`
      - 字节道语义段第 9 条关于 `Axi4Slave` burst 的描述 MUST NOT 与新小节冲突
    - 根 `AGENTS.md`：测试计数同步（`CTEST_TOTAL` / non-perf / `tests/test_*.cpp` /
      `ctest -L verilator_e2e` 计数）+ NOTES 新增条目
  - **verify**:
    - `bash scripts/verify_test_counts.sh` → exit 0
    - `grep -rn "异步复位" include/axi4/AGENTS.md include/axi4/peripherals/*.h`
      → MUST 0 行（全部改为「在时钟沿生效」）
    - reviewer 复核：文档条目与 spec 的 Scenario 一一对应

- [x] ### Task 9: 全量回归 + 计数同步 + archive + 补写主 spec Purpose
  - **action**:
    - 计数同步（新增 1 个测试文件）：`CTEST_TOTAL` / non-perf /
      `tests/test_*.cpp` 数量与根 `AGENTS.md` NOTES §Test counts 一致
    - `openspec archive axi4-lite-generic-slave-conformance-2026-10 --yes`
      （**禁用 `--skip-specs`** —— 本 change 含 ADDED Requirements，
      必须合并进主 spec `openspec/specs/axi-slave-conformance/spec.md`；
      3 条 MODIFIED 必须合并进各自主 spec）
    - ⚠️ **archive 后补写主 spec 的 `## Purpose`**：OpenSpec 不合并 delta 的
      `## Purpose`，新 capability 的主 spec 会是
      `TBD - created by archiving change ...` 骨架
      （已核实 `@fission-ai/openspec@1.4.1` `dist/core/specs-apply.js:289`）
      → MUST 手工写入主 spec 的 Purpose，且 MUST 包含以下 **4 项**：
      1. 适用范围（`AxiLiteSlave` + `AxiToAxiLiteConverter`；`Axi4Slave` 归
         `axi4-full-generic-slave-burst-2026-10`）
      2. 与 `dual-backend-test-coverage` 的分工（**按适用对象**分界，非按「轴」分界）
      3. 字节道语义以 `dual-backend-test-coverage` 为准
      4. 复位措辞为「低有效，在时钟沿生效」
      （⚠️ **MUST NOT** 把 delta 里「archive 注意」这类过程性说明写进主 spec）
  - **verify**:
    - `cd build && ctest -LE perf --output-on-failure` → 全 PASS
    - `cd build && ctest -L verilator_e2e -LE perf --output-on-failure` → 全 PASS
    - `./run_all_ported_tests.sh` → 28/28 PASS
    - `bash scripts/verify_test_counts.sh` → exit 0
    - `openspec validate --specs --strict` → 全 PASS
    - `ls openspec/changes/archive/ | grep lite-generic-slave-conformance` → 命中
    - `ls openspec/specs/axi-slave-conformance/spec.md` → 命中（ADDED 已合并）
    - `head -6 openspec/specs/axi-slave-conformance/spec.md` → MUST NOT 含
      `TBD - created by archiving change`（Purpose 已补写）
    - `grep -c "TBD - created by archiving change" openspec/specs/axi-slave-conformance/spec.md`
      → MUST 为 0
    - `grep -c "rst_n" openspec/specs/dual-backend-test-coverage/spec.md` → 命中（MODIFIED 已合并）
    - `grep -c "在时钟沿生效" openspec/specs/dual-backend-test-coverage/spec.md` → ≥ 1（第 4 条 MODIFIED 已合并）
    - `grep -c "W-beat tick" openspec/specs/verilator-backend-e2e/spec.md` → 命中（MODIFIED 已合并）
