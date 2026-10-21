## Tasks

> 前置依赖：`axi4-lite-generic-slave-conformance-2026-10`
> （archive `2026-10-21-axi4-lite-generic-slave-conformance-2026-10`）
> —— 本 change 在其确立的「通用 slave 规范形态」之上补齐 AXI4 Full 特有的 burst 语义。
>
> **执行顺序**：Task 0 → Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6 →
> Task 7 → Task 8 → Task 9 → Task 10。
>
> **TDD 纪律**：Task 2 先写会失败的用例（RED），Task 3-7 逐条转绿（GREEN），
> Task 8-9 回归。每个 Task 的 verify 必须是**可执行命令**，
> 不接受「人工检查通过」作为唯一验收。
>
> ⚠️ **verify 必须是本机实测过的命令**（`openspec validate` 只做结构/标题校验，
> **不校验命令可执行性**）。已知踩坑（承自 `axi4-lite-generic-slave-conformance-2026-10`）：
>
> | 踩坑 | 后果 | 正确写法 |
> |---|---|---|
> | `add_dual_backend_test` 写成位置参数 | **全仓 configure FATAL_ERROR**（该函数用 `cmake_parse_arguments`，只认 `SOURCES`） | `add_dual_backend_test(name SOURCES file.cpp TIMEOUT 300)` |
> | `grep -c "addr_reg->next ="` | `addr_reg` 命中 `ar_addr_reg` → 计数为 2 → 验收条件永不可能满足 | `grep -cE "^[[:space:]]*${r}->next ="` |
> | `ctest -R "…slave_byte_lite…"` | 正则笔误 → 目标测试被静默跳过（假绿） | `slave_byte_lane`；并用 `ctest -N -R` 断言命中数 |
> | 新增 `rst_n` 但漏接线 | 读出 0 = **永久复位**（零编译错误的静默破坏），表现为「测试集体红」且错误信息不指向 `rst_n` | Task 1 的 grep 护栏 MUST 覆盖**全仓 3 个实例化点** |
>
> **proposal → task 映射**：D1 → Task 3；D2 → Task 4；D3 → Task 5；D4 → Task 6；
> D5 → Task 7；D6 → Task 6；D7 → Task 2 / Task 8；D8 → Task 10。

- [ ] ### Task 0: spec 冲突消解 + delta 完整性核对
  - **action**:
    - 本 change 的 spec delta MUST 保持 2 个文件齐全：
      - `specs/axi4-full-slave-burst/spec.md`（新 capability，ADDED ×6）
      - `specs/dual-backend-test-coverage/spec.md`（MODIFIED ×1）
    - 确认 MODIFIED 的 requirement 标题与主 spec **逐字一致**：
      `AXI4 peripheral MUST 支持字节写`（OpenSpec 按标题匹配，标题不一致 = 该条 delta 被静默丢弃）
    - 确认主 spec `openspec/specs/axi-slave-conformance/spec.md` 中
      「`Axi4Slave` 属 AXI4 **Full**，其规范形态与 burst 语义由
      `axi4-full-generic-slave-burst-2026-10` 承载」这一指向在本 change 落地后仍然成立
      （该 capability 落地后 MUST NOT 保留「尚未实现」的前瞻措辞）
    - `openspec validate --strict` MUST 通过
  - **verify**: `openspec validate --strict && echo VALIDATE_OK`

- [ ] ### Task 1: 实例化点全量清点（`rst_n` 静默破坏护栏）
  - **action**:
    - 清点全仓 `Axi4Slave` 的实例化点，记录到 proposal 或 design 文档
    - 预期 3 处：`examples/axi4/axi4_full_example.cpp:77`、
      `tests/test_axi_slave_byte_lane.cpp:49-50`（`Full32` / `Full64` 别名）、
      `tests/test_verilator_memory_e2e.cpp:194`
    - MUST NOT 在本 Task 修改代码（只清点），改动在 Task 3 之后统一做
  - **verify**:
    ```bash
    grep -rn "Axi4Slave" --include=*.cpp --include=*.h . | grep -v "include/axi4/axi4_full.h"
    ```
    MUST 命中 3 个 cpp 文件（数量变化时 MUST 在 tasks 里记录原因）

- [ ] ### Task 2: 写会失败的 burst 用例（RED）
  - **action**:
    - 新建 `tests/test_axi4_full_burst.cpp`，用
      `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro
    - 先写下列用例（此时**全部应失败**，证明缺陷真实存在）：
      1. 4-beat INCR 写 → 断言 4 个**不同**寄存器被写入（数据断言，非仅握手断言）
      2. 4-beat INCR 读 → 断言 4 个不同数据被依次读回，`rlast` 只在末拍
      3. FIXED burst 4 beat → 全部写同一寄存器
      4. 逐 beat `wstrb` 变化 → 每拍用当拍掩码
      5. `awlen` 超 `MAX_BURST_LEN` → `bresp == SLVERR`
      6. `awburst == WRAP` → `bresp == SLVERR`
      7. 未映射地址 → `bresp == DECERR`
      8. `rst_n` 复位清零全部状态
    - 在 `tests/CMakeLists.txt` 用 `add_dual_backend_test` 注册
      （**注意**：MUST 用 `SOURCES` 关键字形式）
  - **verify**:
    ```bash
    cmake -B build && cmake --build build -j$(nproc) --target test_axi4_full_burst
    cd build && ctest -N -R test_axi4_full_burst | tail -3
    ```
    MUST 命中 16 个（8 对 × 2 后端）；**此时测试 MUST 是红的**（RED 基线）

- [ ] ### Task 3: `Axi4Slave` 加 `rst_n` + 全部 `ch_reg` 复位 pattern
  - **action**:
    - `include/axi4/axi4_full.h`：`__io` 末尾新增 `ch_in<ch_bool> rst_n;`
    - `SlaveState` 全部 `ch_reg` 的 `next` 表达式 MUST 包上
      `select(!io().rst_n, <init_val>, <expr>)`，`<init_val>` MUST 带显式位宽
    - **接线 3 个实例化点**（Task 1 清点的全部）：testbench MUST 驱动 `rst_n` 为 1
      （未接线 = 永久复位 = 静默破坏）
  - **verify**:
    ```bash
    grep -cE "select\(!io\(\)\.rst_n" include/axi4/axi4_full.h
    grep -rn "Axi4Slave" --include=*.cpp . | wc -l
    ```
    第 1 条 MUST ≥ 1（Task 8 完成后 MUST 与 `ch_reg` 数量一致）；第 2 条 MUST = 3
    且每个文件的 `rst_n` 均已被 testbench 驱动（人工核对 + Task 8 的红绿验证）

- [ ] ### Task 4: 写侧多 beat burst（AW→W 相位分离 + 地址锁存 + 逐 beat 地址）
  - **action**:
    - 写状态机 MUST 在整笔 burst 期间持续拉高 `wready`（直到 `wlast` 或长度超限）
    - AW 握手时把地址 / `awlen` / `awsize` / `awburst` / `awid` 锁存进寄存器
    - 目标地址 MUST 按 burst 类型逐 beat 计算，MUST NOT 恒用同一个 `addr_index`
    - `wlast` 与预期 beat 数不一致 → `bresp = SLVERR`
    - `awlen + 1 > MAX_BURST_LEN` → `bresp = SLVERR`（MUST NOT 静默截断）
  - **verify**:
    ```bash
    cd build && ctest -R test_axi4_full_burst --output-on-failure
    ```
    Task 2 的用例 1 / 3 / 4 / 5 MUST 转绿

- [ ] ### Task 5: 读侧多 beat burst
  - **action**:
    - AR 握手时把地址 / `arlen` / `arsize` / `arburst` / `arid` 锁存进寄存器
    - 每一拍的 `rdata` MUST 取该拍对应地址的寄存器值
    - `rlast` MUST 只在最后一个 beat 拉高
    - 读 beat 计数 MUST 与**锁存的** `arlen` 比较
  - **verify**:
    ```bash
    cd build && ctest -R test_axi4_full_burst --output-on-failure
    ```
    Task 2 的用例 2 MUST 转绿

- [ ] ### Task 6: burst 类型语义 + 响应语义化
  - **action**:
    - `INCR`：地址每 beat 递增 `2^awsize` / `2^arsize` 字节（用端口上的 `awsize` / `arsize`）
    - `FIXED`：地址每 beat 不变
    - `WRAP`：MUST 以 `SLVERR` **明确拒绝**（MUST NOT 静默当作 INCR）
    - `bresp` / `rresp` MUST NOT 恒为 `OKAY`：未映射地址 → `DECERR`；
      长度不符 / 不支持的 burst 类型 → `SLVERR`
    - `bid` / `rid` MUST 是锁存的 ID
  - **verify**:
    ```bash
    cd build && ctest -R test_axi4_full_burst --output-on-failure
    ```
    Task 2 的用例 6 / 7 MUST 转绿

- [ ] ### Task 7: 模板参数 honor 化
  - **action**:
    - `MAX_BURST_LEN` MUST 真正参与长度上限检查
    - `NUM_REGS` MUST 真正决定寄存器数量；
      MUST NOT 用变长数组实现（会触发 CHWARN + O(N) 节点爆炸）
      → 采用「固定 16 个具名寄存器 + `static_assert` 暴露不符」
      或改名为明确「固定 16」语义的名称
    - 任一参数未被 honor → MUST 有 `static_assert`
  - **verify**:
    ```bash
    grep -n "static_assert" include/axi4/axi4_full.h
    ```
    MUST 命中 ≥ 1；且 MUST 有一个**负例编译验证**：
    以 `NUM_REGS = 8` 实例化的 TU MUST 编译失败且诊断指向该 `static_assert`
    （负例 TU 的 include 路径 MUST 用 `CH_INCLUDE_DIRS` 的全部 6 个目录，
    否则会先死在 `ast_nodes.h` 的 `#include "lnodeimpl.h"` → 假负）

- [ ] ### Task 8: 回归 —— 既有测试必须仍然全绿
  - **action**:
    - `tests/test_axi_slave_byte_lane.cpp`（`Axi4Slave` 的 `Full32` / `Full64` 用例）
      MUST 适配新形态：`rst_n` 接线 + 写事务改两拍（AW 拍 / W 拍分离）
    - `tests/test_verilator_memory_e2e.cpp` 的 4-beat 写 MUST **补数据断言**
      （当前只比较握手信号 → B2 的回归护栏不存在）
  - **verify**:
    ```bash
    cd build && ctest -R "axi_slave_byte_lane|verilator_memory_e2e" --output-on-failure
    ```
    MUST 全绿

- [ ] ### Task 9: 双后端一致性回归
  - **action**:
    - `tests/test_axi4_full_burst.cpp` MUST 在 interpreter 与 verilator 两后端下
      给出**逐周期一致**的结果（`DUAL_BACKEND_TEST_PAIR` 的 per-cycle parity）
  - **verify**:
    ```bash
    cd build && ctest -L verilator_e2e -R test_axi4_full_burst --output-on-failure
    ```
    MUST 全绿；MUST NOT 出现「verilator half 被 skip」

- [ ] ### Task 10: 文档同步 + 测试计数更新
  - **action**:
    - `include/axi4/AGENTS.md`：
      - CONVENTIONS 段「通用 AXI slave 规范形态」第 9 条
        （`NUM_REGS` / `MAX_BURST_LEN` 未 honor）MUST 更新为已修复表述
      - 新增「AXI4 Full burst 语义」段（INCR / FIXED / WRAP→SLVERR / 逐 beat wstrb）
      - 字节道语义第 9 条里的「多 beat burst 属待修语义」MUST 移除
    - 根 `AGENTS.md`：
      - KNOWN LIMITATIONS MUST NOT 残留「`Axi4Slave` 仍有 `NUM_REGS` / `MAX_BURST_LEN`
        两个未 honor 的参数」
      - NOTES 的 `CTEST_TOTAL` / non-perf 计数 MUST 与
        `cd build && ctest -N | tail -1` 一致
        （本 change 新增 1 个 Catch2 源文件 + 16 个 TEST_CASE）
    - `scripts/verify_test_counts.sh` MUST 通过
  - **verify**:
    ```bash
    ./scripts/verify_test_counts.sh && cd build && ctest -N | tail -1
    ```
    MUST 全绿且计数一致
