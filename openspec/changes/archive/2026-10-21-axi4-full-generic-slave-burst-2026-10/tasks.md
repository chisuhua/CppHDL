## Tasks

> 前置依赖：`axi4-lite-generic-slave-conformance-2026-10`
> （archive `2026-10-21-axi4-lite-generic-slave-conformance-2026-10`）
> —— 本 change 在其确立的「通用 slave 规范形态」之上补齐 AXI4 Full 特有的 burst 语义。
>
> ⚠️ **顺序：已定串行 —— 本 change 先，`axi4-interconnect-routing-2026-10` 在后**
> （2026-10-21 决策；理由：两者共享 `tests/CMakeLists.txt` / 根 `AGENTS.md` /
> `include/axi4/AGENTS.md`，并行必然文本冲突；且 `CTEST_TOTAL` 增量可预测
> 178 → 179 → 181）。本 change 落地后 `Axi4Slave` 才成为可用的 fixture slave，
> 供后继的互连 change 使用。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11。
>
> **TDD 纪律**：Task 3 先写会失败的用例（RED），Task 4-8 逐条转绿（GREEN），
> Task 9-10 回归。每个 Task 的 verify 必须是**可执行命令**，
> 不接受「人工检查通过」作为唯一验收。
>
> ⚠️ **verify 必须是本机实测过的命令**（`openspec validate` 只做结构/标题校验，
> **不校验命令可执行性**）。已知踩坑：
>
> | 踩坑 | 后果 | 正确写法 |
> |---|---|---|
> | `add_dual_backend_test` 写成位置参数 | **全仓 configure FATAL_ERROR**（该函数用 `cmake_parse_arguments`，只认 `SOURCES`） | `add_dual_backend_test(name SOURCES file.cpp TIMEOUT 600)` |
> | **`ctest -N -R` 数 TEST_CASE** | `add_catch_test` / `add_dual_backend_test` 每个**文件**只注册 **1 个** ctest 条目（双后端在同一个 Catch2 二进制内跑）→ 期望「命中 16 个」**永假红** | ctest 侧期望恒为 **1**；TEST_CASE 数用 `./build/tests/<name> --list-tests \| grep -c .` |
> | **Task 顺序：先写用例后加端口** | 用例引用尚未存在的 `io().rst_n` → **整个测试目标编译失败** → RED 基线拿不到（不是断言红，是编译红） | Task 2 先只加 `rst_n` **端口声明 + 接线**（行为不变），Task 3 才写用例 |
> | `grep ... \| wc -l` 期望「= 3 个文件」 | `wc -l` 数**行**不数文件（`test_axi_slave_byte_lane.cpp` 一个文件就有 ~12 行命中）→ 永假红 | `grep -rl ... \| wc -l`（数文件）或 `sort -u \| cut -d: -f1` |
> | 用 ctest 粒度验「部分用例转绿」 | ctest 以**整个二进制**为粒度 → 「用例 1/3/4/5 绿、2/6/7 红」**无法观测** → TDD 节奏名存实亡 | 用 Catch2 名称过滤：`./build/tests/test_axi4_full_burst "<name-glob>"` |
> | `grep -cE "select\(!io\(\)\.rst_n"` 期望 ≥ 1 | `SlaveState` 有 24 个 `ch_reg`，漏一个就是静默破坏；「≥1」过弱 | 期望 **= 24**（与 `ch_reg` 数量一致） |
> | 负例编译 TU 只给 `-I include` | 先死在 `ast_nodes.h` 的 `#include "lnodeimpl.h"` → 诊断不含 `static_assert` → **假负** | 用 `CH_INCLUDE_DIRS` 的全部 6 个目录（见 Task 8 给出的完整命令） |
> | 「verilator half 被 skip」用 ctest 判 | `dual_backend_fixture.h` 的 WARN-skip **返回 exit 0** → ctest 照常全绿 | 必须断言**输出**里 skip 计数 = 0（见 Task 10） |
> | `rst_n` 未接线 | 读出 0 = **永久复位**（零编译错误的静默破坏）→ 表现为「测试集体红」且错误信息不指向 `rst_n` | Task 1 的 grep 护栏 MUST 覆盖**全仓 3 个实例化点** |
>
> **proposal → task 映射**：D1 → Task 2 + Task 4；D2 → Task 5；D3 → Task 6；
> D4 → Task 7；D5 → Task 8；D6 → Task 7；D7 → Task 3 / Task 5-7 / Task 10；
> D8 → Task 11。

- [x] ### Task 0: spec 冲突消解 + delta 完整性核对
  - **action**:
    - 本 change 的 spec delta MUST 保持 **3 个文件**齐全：
      - `specs/axi4-full-slave-burst/spec.md`（新 capability，ADDED ×7）
      - `specs/dual-backend-test-coverage/spec.md`（MODIFIED ×3）
      - `specs/axi-slave-conformance/spec.md`（MODIFIED ×1）
    - 确认 MODIFIED 的 requirement 标题与主 spec **逐字一致**（OpenSpec 按标题匹配，
      标题不一致 = 该条 delta 被静默丢弃）：
      - `现有 9 个 verilator 测试 MUST 保留原状`
      - `AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR`
      - `AXI4 peripheral MUST 支持字节写`
      - `通用 AXI4-Lite slave MUST 提供低有效复位`
    - ⚠️ 两条 spec 锁 MUST 显式解锁（否则 Task 9 改测试 = spec 违规）：
      - `tests/test_verilator_memory_e2e.cpp` 内嵌 `Axi4Slave` → 需接 `rst_n`
        + 补数据断言
      - `dual-backend-test-coverage` 的「不破既有 spec 锁」Scenario 需追加该文件的例外
    - `openspec validate --strict` MUST 通过
  - **verify**: `openspec validate axi4-full-generic-slave-burst-2026-10 --strict && echo VALIDATE_OK`

- [x] ### Task 1: 实例化点全量清点（`rst_n` 静默破坏护栏）
  - **action**:
    - 清点全仓 `Axi4Slave` 的实例化点，记入 proposal 或 design 文档
    - 预期 3 个文件：`examples/axi4/axi4_full_example.cpp`、
      `tests/test_axi_slave_byte_lane.cpp`（`Full32` / `Full64` 别名，**双后端**测试）、
      `tests/test_verilator_memory_e2e.cpp`
    - MUST NOT 在本 Task 修改代码（只清点），改动在 Task 2 统一做
  - **verify**:
    ```bash
    grep -rl "Axi4Slave" --include=*.cpp . | sort
    ```
    MUST 命中 3 个 `.cpp` 文件（数量变化时 MUST 在 tasks 里记录原因）

- [x] ### Task 2: 先加 `rst_n` 端口声明 + 接线（行为不变）
  - **action**:
    - `include/axi4/axi4_full.h`：`__io` 末尾新增 `ch_in<ch_bool> rst_n;`
      （**只加端口声明**，本 Task 不加复位 pattern、不改状态机）
    - **接线 3 个实例化点**（Task 1 清点的全部）：
      - `tests/test_axi_slave_byte_lane.cpp`：fixture 暴露 `rst_n` 并
        `sim.set_input_value(io().rst_n, 1)`
      - `tests/test_verilator_memory_e2e.cpp`：同上
      - `examples/axi4/axi4_full_example.cpp`：该 example 的 `main()` **不仿真**
        （只构建 + 打印）→ 无 testbench 可放；MUST 用
        「wrapper 透传 `rst_n` 端口」或「tie `ch_bool(true)`」的方式处理，
        并在 design 文档记录选择
        - ✅ **已裁决（2026-10-21）**：选「**wrapper 透传 `rst_n` 端口**」
          （`Axi4Top::__io` 末尾新增 `ch_in<ch_bool> rst_n;` +
          `axi_slave.io().rst_n <<= io().rst_n;`）。
          理由：与 `examples/axi4/axi4_lite_example.cpp`（Lite 版本）同源；
          tie `true` 会让 wrapper 无法仿真复位场景
    - ⚠️ 加了端口但**未加复位 pattern** 时，`rst_n` 当前**无效果** →
      既有行为 MUST NOT 变化 → **全部既有测试 MUST 仍然全绿**
      （这是本 Task 的验收判据）
  - **verify**:
    ```bash
    grep -n "rst_n" include/axi4/axi4_full.h
    cd build && ctest -R "axi_slave_byte_lane|verilator_memory_e2e" --output-on-failure
    ```
    第 1 条 MUST 命中 ≥ 1；第 2 条 MUST **全绿**（行为未变）

- [x] ### Task 3: 写会失败的 burst 用例（RED）
  - **action**:
    - 新建 `tests/test_axi4_full_burst.cpp`，用
      `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro
    - ⚠️ **master stimulus MUST 复用/仿照 `tests/axi_lite_slave_fixture.h`**
      （仓库已有的协议合规 master fixture，专治「master 用从设备的 ready
      gate 自己的 valid」这一 ANTI-PATTERN）→ MUST NOT 手写违规 stimulus
    - ⚠️ **所有握手等待 MUST 是有界循环**（如 ≤ 64 拍），超时即 FAIL ——
      「握手永不完成」类缺陷的表现是 hang，不是断言失败；
      无界 `while(!awready) tick();` 会挂到 ctest TIMEOUT（300s × N 用例）
    - 写下列用例（此时**全部应红**，证明缺陷真实存在）：
      1. 4-beat INCR 写 → 断言 4 个**不同**寄存器被写入（数据断言，非仅握手断言）
      2. 4-beat INCR 读 → 断言 4 个不同数据被依次读回，`rlast` 只在末拍
      3. FIXED burst 4 beat → 全部写同一寄存器
      4. 逐 beat `wstrb` 变化 → 每拍用当拍掩码
      5. `awlen` 超 `MAX_BURST_LEN` → `bresp == SLVERR`
      6. `awburst == WRAP` → `bresp == SLVERR`
      7. 未映射地址 → `bresp == DECERR`
      8. `rst_n` 复位清零全部状态
      9. 64 bit 总线（`DATA_WIDTH = 64`）的 4-beat INCR 写
         （补既有 `test_axi_slave_byte_lane.cpp` 的 `Full64` 覆盖面）
    - 在 `tests/CMakeLists.txt` 用 `add_dual_backend_test` 注册
      （**MUST 用 `SOURCES` 关键字形式**，`TIMEOUT 600`）
  - **verify**:
    ```bash
    cmake -B build && cmake --build build -j$(nproc) --target test_axi4_full_burst
    ./build/tests/test_axi4_full_burst --list-tests | grep -c .
    cd build && ctest -N -R test_axi4_full_burst | tail -1
    ```
    第 2 条 MUST 命中 **18**（9 对 × 2 后端）；
    第 3 条 MUST 命中 **1**（1 个文件 = 1 个 ctest 条目）；
    **此时测试 MUST 是红的**（RED 基线）

- [x] ### Task 4: 全部 `ch_reg` 加复位 pattern
  - **action**:
    - `SlaveState` 全部 `ch_reg`（24 个：2 个状态机 + 16 个数据寄存器 + 6 个计数/锁存）
      的 `next` 表达式 MUST 包上 `select(!io().rst_n, <init_val>, <expr>)`，
      `<init_val>` MUST 带该寄存器的**显式位宽**（如 `ch_uint<32>(0_d)`）
    - ⚠️ 漏一个就是静默破坏（该寄存器永远停在初值）→ verify 期望数量 MUST 精确
  - **verify**:
    ```bash
    grep -cE "^[[:space:]]*s\.[a-z0-9_]+->next = select\(!io\(\)\.rst_n" include/axi4/axi4_full.h
    grep -cE "ch_reg<" include/axi4/axi4_full.h
    ```
    两条 MUST 相等（= 24）；MUST NOT 只满足「≥ 1」
  - **action（续）**:
    - Task 3 的用例 8 MUST 转绿
  - **verify（续）**:
    ```bash
    ./build/tests/test_axi4_full_burst "*rst_n*" 2>/dev/null || ./build/tests/test_axi4_full_burst --list-tests | grep -i reset
    ```
    MUST 观察到复位用例由红转绿（用 Catch2 名称过滤，**不要**用 ctest 粒度）

- [x] ### Task 5: 写侧多 beat burst（AW→W 相位分离 + 地址锁存 + 逐 beat 地址）
  - **action**:
    - 写状态机 MUST 在整笔 burst 期间持续拉高 `wready`（直到 `wlast` 或长度超限）
    - AW 握手时把地址 / `awlen` / `awsize` / `awburst` / `awid` 锁存进寄存器
    - 目标地址 MUST 按 burst 类型逐 beat 计算，MUST NOT 恒用同一个 `addr_index`
    - ⚠️ **「结束/完成」判定 MUST 用「下一个值」**（`next_beat_cnt == awlen`），
      MUST NOT 用当前寄存器值比较 —— 仓库已登记该坑
      （`include/axi4/AGENTS.md` ANTI-PATTERNS：UART RX 引擎因用当前值判定结束
      而只采到 7 位）
    - `wlast` 与预期 beat 数不一致 → `bresp = SLVERR`
    - `awlen + 1 > MAX_BURST_LEN` → `bresp = SLVERR`（MUST NOT 静默截断）
  - **verify**:
    ```bash
    ./build/tests/test_axi4_full_burst "*write*"
    ```
    Task 3 的用例 1 / 3 / 4 / 5 MUST 转绿（用 Catch2 名称过滤逐项确认）
  - ⚠️ **实施时补强（2026-10-21）**：
    1. **「wlast 缺失」必须检出**：原实现只检出了「wlast 提前」，
       「预期末拍到了但 wlast=0」会漏检 → 事务错误地返回 `OKAY`。
       改为**异或**判定（`wlast != expected_last`），
       并新增回归用例「wlast mismatch ends with SLVERR」（双后端）。
    2. **burst 结束后 `wready` MUST 撤销**（新增 `write_done` 寄存器）：
       否则 master 多发的 beat 会被静默写入寄存器（越界写入）。
       → 同步补进 spec delta（写 burst Requirement 第 1 条）。
    3. **被拒绝的事务（WRAP / 超 `MAX_BURST_LEN`）不写入任何 beat**：
       事务整体被拒绝（响应 `SLVERR`），与「长度不符」的情形
       （已写入的 beat **不**回滚，AXI 允许部分写入 + 错误响应）区分开。
       → 同步补进 spec delta。

- [x] ### Task 6: 读侧多 beat burst
  - **action**:
    - AR 握手时把地址 / `arlen` / `arsize` / `arburst` / `arid` 锁存进寄存器
    - 每一拍的 `rdata` MUST 取该拍对应地址的寄存器值
    - `rlast` MUST 只在最后一个 beat 拉高，
      且**判定 MUST 用「下一个值」**（同 Task 5 的坑）
    - 读 beat 计数 MUST 与**锁存的** `arlen` 比较
    - ⚠️ **实施时裁决（2026-10-21）**：读侧 `rlast` 的「下一个值」写法与本 change
      原 spec 的 Requirement 6 **字面冲突**，且**字面写法是错的**：
      流式形态下末拍在 `rready` 拉低期间**仍停留在总线上**，
      `rlast = (next_beat_cnt == arlen)` 会在该周期给出错误的 `rlast = 0`
      （master 认为 burst 未结束 → 末拍被重复接受 / 事务挂起）。
      **裁决**：读侧 `rlast` MUST 与「正在呈现的 beat」同源
      → `rlast = (beat_idx == arlen)`（`beat_idx` = 已被 `rready` 接受的 beat 数，
      **当前值**）；「下一个值」规则只约束**写侧**的「本拍是否为末拍」判定
      与「burst 结束判定基于 `w_handshake` 的下一状态」。
      → 已同步修正本 change 的 spec delta
      （`specs/axi4-full-slave-burst/spec.md` 的该 Requirement 第 6/7 条）
      + 新增回归用例「rlast is asserted while the last beat is held on the bus」
      （`tests/test_axi4_full_burst.cpp`，双后端）
  - **verify**:
    ```bash
    ./build/tests/test_axi4_full_burst "*read*"
    ```
    Task 3 的用例 2 MUST 转绿

- [x] ### Task 7: burst 类型语义 + 响应语义化
  - **action**:
    - `INCR`：地址每 beat 递增 `2^awsize` / `2^arsize` 字节
      - ⚠️ `2^awsize` **MUST NOT 用运行期移位** ——
        `ch_uint::operator<<` 按右操作数类型宽度推算结果宽度
        → MUST 用 `select` 链查表（`awsize` 0..7 → 1/2/4/8/16/32/64/128 字节）
    - `FIXED`：地址每 beat 不变
    - `WRAP`：MUST 以 `SLVERR` **明确拒绝**（MUST NOT 静默当作 INCR）
    - `bresp` / `rresp` MUST NOT 恒为 `OKAY`：未映射地址 → `DECERR`；
      长度不符 / 不支持的 burst 类型 → `SLVERR`
    - INCR 越界（某 beat 跨出窗口）→ 该 beat 抑制 + 事务以 `DECERR` 结束
  - **verify**:
    ```bash
    ./build/tests/test_axi4_full_burst "*burst*" && ./build/tests/test_axi4_full_burst "*decerr*"
    ```
    Task 3 的用例 6 / 7 MUST 转绿

- [x] ### Task 8: 模板参数 honor 化（含决策点裁决）
  - **action**:
    - ✅ **已裁决（2026-10-21，实施时记录）**：走「**固定 16 + `static_assert`**」，
      **不改名**。
      理由：① 与 `AxiLiteSlave`（`include/axi4/axi4_lite_slave.h:41-43`）
      **同源**（同一仓库先例：`static_assert(NUM_REGS == 4, ...)`）；
      ② `NUM_REGS` 这个名字在「未 honor」的状态下本就是**接口谎言**，
      改名成 `NUM_REGS_FIXED` 会把谎言换成「名字正确但语义仍不生效」，
      比 `static_assert` 更危险（用户会以为传 8 就能拿 8 个寄存器）；
      ③ 保持参数名 → 3 个位置实参实例化点源码兼容，零 API 破坏面。
      实现：`static_assert(NUM_REGS == NUM_IMPL_REGS /* 16 */, ...)`
      （`include/axi4/axi4_full.h:92`）
    - 补充事实：现有 3 个实例化点全部用**位置参数**
      （`Axi4Slave<32,32,4,16,16>`）→ 改名对它们源码兼容
    - `MAX_BURST_LEN` MUST 真正参与长度上限检查
    - MUST 加 `static_assert(MAX_BURST_LEN <= 16)`（`awlen` 是 4 bit，物理上限 16 beat）
    - MUST 加 `static_assert(DATA_WIDTH <= 64)`
      （`byte_lane.h` 已封顶 64 + Verilator 后端拒 >64 bit 端口 ADR-035 R3）
    - 修正 `include/axi4/axi4_full.h:9` 的头注释 —— 当前宣称支持 128–1024 位数据宽度
    - 任一参数未被 honor → MUST 有 `static_assert`
  - **verify**:
    ```bash
    grep -n "static_assert" include/axi4/axi4_full.h
    ```
    MUST 命中 ≥ 3（`NUM_REGS` / `MAX_BURST_LEN` / `DATA_WIDTH`）；
    且 MUST 有一个**负例编译验证**（完整命令）：
    ```bash
    cat > /tmp/opencode/neg_tu.cpp <<'EOF'
    #include "axi4/axi4_full.h"
    using Bad = axi4::Axi4Slave<32, 32, 4, 8, 16>;   // NUM_REGS = 8 与实现不符
    int main() { return 0; }
    EOF
    g++ -std=c++20 -fsyntax-only /tmp/opencode/neg_tu.cpp \
      -I include -I include/core -I include/ast -I include/lnode \
      -I include/bv -I include/utils
    ```
    MUST **编译失败**且诊断中 MUST 含 `static_assert` 相关文本
    （若先死在 `ast_nodes.h` 的 `#include "lnodeimpl.h"` → 补齐缺失的 include 目录，
    **假负不算通过**）
    - ⚠️ **实测修正（2026-10-21）**：原命令用 `using Bad = axi4::Axi4Slave<...>;`
      **不触发类模板实例化** → `static_assert` 根本不求值 → 编译**通过** = **假负**。
      MUST 改为**真实实例化**：
      ```bash
      cat > /tmp/opencode/neg_tu.cpp <<'EOF'
      #include "axi4/axi4_full.h"
      // 真实实例化（`using` 别名不会触发类模板实例化 → static_assert 不会求值）
      axi4::Axi4Slave<32, 32, 4, 8, 16> bad;   // NUM_REGS = 8 与实现不符
      int main() { return 0; }
      EOF
      g++ -std=c++20 -fsyntax-only /tmp/opencode/neg_tu.cpp \
        -I include -I include/core -I include/ast -I include/lnode \
        -I include/bv -I include/utils
      ```
      实测输出：
      `include/axi4/axi4_full.h:92:28: error: static assertion failed: Axi4Slave: NUM_REGS is not honoured ...`
      另两条亦实测触发：`MAX_BURST_LEN=32` → line 97；`DATA_WIDTH=128` → line 102

- [x] ### Task 9: 回归 —— 既有测试必须仍然全绿
  - **action**:
    - `tests/test_axi_slave_byte_lane.cpp`（`Axi4Slave` 的 `Full32` / `Full64` 用例）
      MUST 适配：`rst_n` 接线（Task 2 已做）+ 写事务**是否需要改两拍**
      - ⚠️ 复核结论：`Axi4Slave` **今天就已经不接受同拍 AW+W**
        （`wready` 由 `write_state == 1` 门控，state 1 只能在 AW 握手后一拍进入）
        → **不需要**改时序，只需接线 `rst_n`
    - `tests/test_verilator_memory_e2e.cpp` 的 4-beat 写 MUST **补数据断言**
      （当前只比较握手信号 → 多 beat 被静默丢弃时测试仍绿）
    - `examples/axi4/axi4_full_example.cpp` MUST 仍可编译
  - **verify**:
    ```bash
    cd build && ctest -R "axi_slave_byte_lane|verilator_memory_e2e" --output-on-failure
    ./run_all_ported_tests.sh
    ```
    MUST 全绿

- [x] ### Task 10: 双后端一致性回归
  - **action**:
    - `tests/test_axi4_full_burst.cpp` MUST 在 interpreter 与 verilator 两后端下
      给出**逐周期一致**的结果（`DUAL_BACKEND_TEST_PAIR` 的 per-cycle parity）
    - ⚠️ **verilator skip 的检测 MUST 断言输出** ——
      `dual_backend_fixture.h` 的 WARN-skip **返回 exit 0**，
      ctest 照常全绿 → 单纯 `ctest` 无法判定
  - **verify**:
    ```bash
    cd build && ctest -L verilator_e2e -R test_axi4_full_burst --output-on-failure
    ./build/tests/test_axi4_full_burst -s 2>&1 | grep -ciE "skipped|skip" || true
    ```
    第 1 条 MUST 全绿；第 2 条 MUST 输出 **0**（无 skip）

- [x] ### Task 11: 文档同步 + 测试计数更新
  - **action**:
    - `include/axi4/AGENTS.md`：
      - CONVENTIONS 段「通用 AXI slave 规范形态」第 9 条
        （`NUM_REGS` / `MAX_BURST_LEN` 未 honor）MUST 更新为已修复表述
      - 新增「AXI4 Full burst 语义」段（INCR / FIXED / WRAP→SLVERR / 逐 beat wstrb /
        读写方向各自独立 outstanding / 64 位步长限制）
      - 字节道语义第 9 条里的「多 beat burst 属待修语义」MUST 移除
    - 根 `AGENTS.md`：
      - KNOWN LIMITATIONS MUST NOT 残留「`Axi4Slave` 仍有 `NUM_REGS` / `MAX_BURST_LEN`
        两个未 honor 的参数」
      - NOTES 的 `CTEST_TOTAL` MUST 更新：**178 → 179**
        （新增 `tests/test_axi4_full_burst.cpp` = **1 个 ctest 条目**；
        双后端在同一 Catch2 二进制内跑，**不**增加 ctest 条目数）
    - `scripts/verify_test_counts.sh` MUST 通过
  - **verify**:
    ```bash
    ./scripts/verify_test_counts.sh && cd build && ctest -N | tail -1
    ```
    MUST 全绿且 `ctest -N` 总数 = 179
