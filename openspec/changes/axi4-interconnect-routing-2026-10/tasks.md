## Tasks

> 前置依赖：无硬依赖。但 MUST 在 `axi4-lite-generic-slave-conformance-2026-10`
> （archive `2026-10-21-axi4-lite-generic-slave-conformance-2026-10`）之后执行 ——
> `AxiToAxiLiteConverter` 的 `rst_n` 透传与相位分离状态机在该 change 中落地，
> `examples/axi4/phase3a_complete.cpp` 的 AXI-Lite 路径只有在其之后才可能真的跑通。
>
> ⚠️ **顺序：已定串行 —— `axi4-full-generic-slave-burst-2026-10` 先，本 change 在后**
> （2026-10-21 决策；理由：两者都改 `tests/CMakeLists.txt` / 根 `AGENTS.md` /
> `include/axi4/AGENTS.md`，并行必然文本冲突；且 `CTEST_TOTAL` 增量可预测
> 178 → 179 → 181）。
> 本 change 的测试 MUST 用 **testbench 扮演 slave**，MUST NOT 实例化 `Axi4Slave`
> —— 后者会隐含依赖 burst change 落地。
>
> **执行顺序**：Task 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9。
>
> **TDD 纪律**：Task 2 先写会失败的用例（RED），Task 3-6 逐条转绿（GREEN），
> Task 7-9 回归。每个 Task 的 verify 必须是**可执行命令**，
> 不接受「人工检查通过」作为唯一验收。
>
> ⚠️ **verify 必须是本机实测过的命令**。已知踩坑：
>
> | 踩坑 | 后果 | 正确写法 |
> |---|---|---|
> | `add_dual_backend_test` 写成位置参数 | **全仓 configure FATAL_ERROR**（该函数用 `cmake_parse_arguments`，只认 `SOURCES`） | `add_dual_backend_test(name SOURCES file.cpp TIMEOUT 600)` |
> | **`ctest -N -R` 数 TEST_CASE** | 每个**文件**只注册 **1 个** ctest 条目（双后端在同一个 Catch2 二进制内跑）→ 期望「命中 16 个」**永假红** | ctest 侧期望恒为 **1**（本 change 共 2 个新文件 → 2 个条目）；TEST_CASE 数用 `./build/tests/<name> --list-tests \| grep -c .` |
> | **`grep -cE "route"` 验证路由锁存** | 文件头注释（`:7` "route to slave 0-3"）就命中 → **永假绿**，与是否实现锁存无关 | MUST 匹配**具体的寄存器名**（如 `write_route` / `read_route`） |
> | verify 只查 `m1_` / `s1_` 前缀 | m2/m3/s2/s3 全漏接也照样通过验收 | MUST 覆盖 `{1,2,3}` 全部前缀 |
> | **给 `ch_in` 端口赋值** | `m*_bready` / `m*_rready` 是 `ch_in`（master 驱动）→ 赋值是运行时 no-op，**编译零报错** | 只对 `ch_out` 赋值；`m*_bready`/`m*_rready` 是**输入**，不由互连驱动 |
> | 用 ctest 粒度验「部分用例转绿」 | ctest 以**整个二进制**为粒度 → 无法观测中间态 → TDD 节奏名存实亡 | 用 Catch2 名称过滤：`./build/tests/test_axi_interconnect_4x4 "<name-glob>"` |
> | **无界握手等待** | C1 类缺陷的表现是握手**永不完成**（hang），不是断言失败 → 挂到 ctest TIMEOUT（600s × N 用例） | 所有握手等待 MUST 是**有界循环**（≤ 64 拍）+ 超时即 FAIL |
> | 多路选择写成「输出读自己」 | 拓扑排序报 `Cycle detected in IR graph` | 每路**单次赋值**（见 `AxiLiteMatrix2x2:226-233` 的修复注释） |
> | `grep ... \| wc -l` 期望「= N 个文件」 | `wc -l` 数**行**不数文件 → 永假红 | `grep -rl ... \| wc -l` |
> | 临时文件写 `/tmp` | 违反仓库约定（root AGENTS.md：临时文件 MUST 用 `/tmp/opencode/<name>`） | 一律用 `/tmp/opencode/` |
> | **`openspec validate --strict` 直接当布尔用** | 本机 CLI **1.14.3** 会对本 change 的 5 条长 Requirement 报 `>500 characters` ⇒ **exit 1 假红**（本仓钉 1.4.1；该规则是 1.14.x 新增，纯净 HEAD 同样红） | 用 Task 0 的**版本容忍**写法：只对 `[ERROR]` / `Unknown item` 判红 |
> | **`grep -c` 数「行」当「个数」用** | `ch_reg<...> a, b, c;` **一行声明多个**寄存器 ⇒ `grep -c "ch_reg<"` = 13 而寄存器数 = 31；文件头**注释**里也含 `select(!io().rst_n` ⇒ 33 | 数**寄存器个数**用 `grep -oE "ch_reg<[^>]*>[^;]*" \| grep -oE "[a-z_][a-z_0-9]*" \| grep -cE "^(aw_ptr\|ar_ptr\|wr_\|rd_\|err_)"`；数**复位 pattern 个数**用 `grep -cE "\->next *= *select\(!io\(\)\.rst_n"`（两者 MUST 相等） |
> | **`ch_reg<T>` 写成裸 `T`** | `struct` 成员写成 `ch_bool x;` 而非 `ch_reg<ch_bool> x;` ⇒ `x->next = ...` 编译报 `base operand of '->' has non-pointer type` | 状态成员 MUST 是 `ch_reg<T>`；`IcState` 里全部 31 个成员均如此 |
> | **`set_input_value()` 后直接 `get_port_value()`** | `set_input_value()` **只写数据槽、不触发重新求值** ⇒ 紧随其后的读拿到的是**上一次 tick** 的组合值 ⇒ 所有握手等待**假超时**（现象：`awready` 恒读出 0，而内部 `active` 已是 1） | 等待循环里 MUST 先 `sim.eval_combinational()` 再读组合值 |
> | **复位窗口内仍挂着 `valid`/`ready`** | 复位释放那一拍 `valid & ready` 同时为高 ⇒ **再握手一次**（在途标志被立刻重新置位）⇒ 复位后「全新事务」用例假红 | 进复位窗口**之前**先撤掉总线上的 VALID/READY |
>
> **proposal → task 映射**：D1 → Task 3；D2 → Task 4；D3 → Task 5；D4 → Task 6；
> D5 → Task 3（裁决）；D6 → Task 6；D7 → Task 7；D8 → Task 2 / Task 3-6 / Task 8；
> D9 → Task 8；D10 → Task 9。

- [x] ### Task 0: spec 冲突消解 + 变更影响面清点
  - **action**:
    - 本 change 的 spec delta 为 **1 个新 capability**：
      `specs/axi-interconnect-routing/spec.md`（ADDED ×6）
    - 确认主 spec 中 **无** 已有 requirement 管辖互连层
      （`axi-slave-conformance` 管从设备侧、`dual-backend-test-coverage` 管双后端一致性）
      → 若发现冲突，MUST 在本 Task 记录并与 `axi-slave-conformance` 的边界写清
    - 清点 `AxiInterconnect4x4` / `AxiLiteMatrix2x2` 的实例化点
      （预期 **2 个** example 文件：`examples/axi4/phase3a_complete.cpp`、
      `examples/axi4/axi_soc_example.cpp`）
    - `openspec validate --strict` MUST 通过（⚠️ **版本容忍**，见下方 verify 的注释）
  - **verify**:
    ```bash
    # ⚠️ CLI 版本容忍（必读）：本仓钉 openspec **1.4.1**（root AGENTS.md §COMMANDS），
    #   而 1.14.x 新增了「Requirement 正文 > 500 字符」「Purpose 仍是占位符」两条 strict 规则
    #   ⇒ 纯净 HEAD 的 15/17 个主 spec 在 1.14.x 同样变红（与本仓改动无关）。
    #   ⇒ 本 verify MUST 只对**结构错误**判红，MUST NOT 对上述版本差异判红。
    out="$(openspec validate axi4-interconnect-routing-2026-10 --strict 2>&1)"; rc=$?
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qE "Unknown item|\[ERROR\]" && { echo "VALIDATE_FAIL：change 不存在或有结构错误"; exit 1; }
    bad="$(printf '%s\n' "$out" | grep "\[WARNING\]" | grep -vE "very long \(>500 characters\)|placeholder")"
    [ -n "$bad" ] && { printf '%s\n' "$bad"; echo "VALIDATE_FAIL：存在非已知版本差异的告警"; exit 1; }
    [ "$rc" -eq 0 ] || echo "NOTE：CLI 退出码 $rc 仅源于 1.14.x 的版本差异规则"
    echo VALIDATE_OK

    grep -rl "AxiInterconnect4x4\|AxiLiteMatrix2x2" --include=*.cpp . | wc -l
    ```
    上一段 MUST 输出 **VALIDATE_OK**；实例化点计数 MUST 为 **2**
    （已实测：`examples/axi4/axi_soc_example.cpp` + `examples/axi4/phase3a_complete.cpp`）

- [x] ### Task 1: 建立「缺陷可观测」基线（不改实现）
  - **action**:
    - 写一个**最小复现**程序（临时文件 MUST 放 `/tmp/opencode/`），
      逐条确认 C1-C4 的症状确实存在：
      1. m1 发起写 → `m1_awready` 有值但 `m1_wready` 恒 0（写不被接受）
      2. m1 发起读 → `m1_arready` 恒 0
      3. slave 1 的 `s1_arvalid` 恒 0（读地址到不了 slave 1）
      4. slave 1 的 `s1_bready` 恒 0（响应永不握手）
    - 同时确认 C11 / C12 / C14：
      5. 地址 ≥ `0xC000_0000` 的写 → `bresp` 为 DECERR（即使 s3 返回 OKAY）
      6. `m0_rresp` 恒 0
      7. 指针指向无请求的 master 时，其他 master 拿不到 grant
    - 把复现步骤记入本 Task 的完成说明（供 Task 2 的 RED 用例对齐）
  - **verify**:
    ```bash
    grep -cE "^[[:space:]]*io\(\)\.m[1-3]_(wready|arready) *=" include/axi4/axi_interconnect_4x4.h
    grep -cE "^[[:space:]]*io\(\)\.s[1-3]_(araddr|arvalid|bready|rready) *=" include/axi4/axi_interconnect_4x4.h
    grep -cE "^[[:space:]]*io\(\)\.m[0-3]_rresp *=" include/axi4/axi_interconnect_4x4.h
    ```
    三条 MUST 均为 **0**（确认缺陷存在，Task 4 之后 MUST 变为非 0）

- [x] ### Task 2: 写会失败的路由用例（RED）
  - **action**:
    - 新建 `tests/test_axi_interconnect_4x4.cpp` 与
      `tests/test_axi_lite_matrix_2x2.cpp`，用
      `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro
    - ⚠️ **DUT 形态**：互连测试 MUST 用 **testbench 扮演 slave**
      （驱动 4×~13 个 slave 侧输入），MUST NOT 实例化 `Axi4Slave` 当 fixture
    - ⚠️ **master stimulus MUST 复用/仿照 `tests/axi_lite_slave_fixture.h`**
      （仓库已有的协议合规 master fixture）→ MUST NOT 手写违规 stimulus
    - ⚠️ **所有握手等待 MUST 是有界循环**（≤ 64 拍）+ 超时即 FAIL
    - 先写下列用例（此时**全部应红**）：
      1. 4 个 master 各自到达全部 4 个 slave（16 对全扇出）
      2. 地址映射正确（每个 slave 拿到自己窗口内的地址）
      3. 写事务 AW → W → B 全链路，**数据断言**
      4. 读事务 AR → R 全链路，**数据断言**
      5. 未映射地址 → 写 `DECERR`（`bvalid` 保持到 `bready`）
      6. 未映射地址 → 读 `arlen + 1` 个 `DECERR` beat，末拍 `rlast`
      7. 写/读并发（不同 master 同时发起）不串扰
      8. 路由锁存：AW 握手后改写 AWADDR，数据仍落到原 slave
      9. W 通道不被仲裁轮转改投
      10. `WSTRB` 完整转发
      11. 读事务不被写请求阻塞
      12. 仲裁不饿死（指针指向无请求的 master 时其他 master 仍能拿到 grant）
      13. 在途期间 `awready` MUST 为 0
      14. **中途复位逃生路径**：事务在途时拉低 `rst_n` ≥1 拍 →
          全部 `active` 清零 → 复位后可接受全新事务
          （⚠️ MUST 双后端一致 —— 两后端的中途复位语义只由组件级 `rst_n` 保证一致；
          `Simulator::reset()` 不委派 backend，interpreter 清 0 而 verilator 加载 INIT）
    - ⚠️ 用例 5 / 6 需要**非默认地址映射配置**（默认映射覆盖全空间 → DECERR 不可测）
      → 该配置 MUST 在测试里显式实例化并在 design 文档记录
    - 在 `tests/CMakeLists.txt` 用 `add_dual_backend_test` 注册
      （**MUST 用 `SOURCES` 关键字形式**，`TIMEOUT 600`）
  - **verify**:
    ```bash
    cmake -B build && cmake --build build -j$(nproc) --target test_axi_interconnect_4x4
    ./build/tests/test_axi_interconnect_4x4 --list-tests | grep -cE "^  [A-Za-z]"
    cd build && ctest -N -R "test_axi_interconnect_4x4" | tail -1
    ```
    第 2 条 MUST 命中 **28**（14 对 × 2 后端；Task 2 的 RED 基线规模 ——
    后续 critic 修复批扩到 23 对 / 46 TEST_CASE，见文末记录）
    （⚠️ as-built 订正：原判据 `grep -c .` 数的是**行**（含 tag 行）⇒ 实测 **68** ⇒ 永假红；
    真判据是 `grep -cE "^  [A-Za-z]"` = **28**）
    第 3 条 MUST 命中 **1**（1 个文件 = 1 个 ctest 条目）
    （⚠️ as-built **偏差（已记录，非疏漏）**：原计划本 Task 同时建
    `test_axi_lite_matrix_2x2.cpp`，但它的 DUT（`AxiLiteMatrix2x2`）在本 change 里
    **要到 Task 7 才修** ⇒ 本 Task 无法写出有判别力的 RED（2×2 的期望语义需与
    实现同读）⇒ **2×2 的测试文件 + CMake 注册随 Task 7 一并落地**，
    见 Task 7 的 verify。）
    **此时测试 MUST 是红的**（RED 基线）

- [x] ### Task 3: 引入 `rst_n` + 路由锁存
  - **action**:
    - **复位契约已裁决**（2026-10-21，proposal D5）：互连层**暴露** `rst_n`
      → 本 Task MUST 落地，不存在「选不选」的分支
    - `include/axi4/axi_interconnect_4x4.h`：`__io` 末尾新增 `ch_in<ch_bool> rst_n;`
      （仓库惯例：`AxiLiteSlave` 在 `include/axi4/axi4_lite_slave.h:71`，
      注释标明「必需端口，实例化点 MUST 显式接线」）
    - 全部 `ch_reg`（含既有 `IcState::arb_state`）的 `->next` MUST 包上
      `select(!io().rst_n, <init_val>, ...)`，`<init_val>` MUST 带**显式位宽**
      （`ch_bool` → `ch_bool(false)`；`ch_uint<N>` → `ch_uint<N>(0_d)`）
    - **接线 2 个实例化点**（顶层 `rst_n` 已存在且已驱动下游，接线是纯增量）：
      - `examples/axi4/phase3a_complete.cpp`：
        `interconnect.io().rst_n <<= io().rst_n;`
        （顶层 `rst_n` 在 `:60`，已透传到 converter `:126` / slave `:127`）
      - `examples/axi4/axi_soc_example.cpp`：
        `matrix.io().rst_n <<= io().rst_n;`
        （顶层 `rst_n` 在 `:47`，已透传到 gpio/uart `:263-264`）
      - ⚠️ MUST NOT tie 常量（`<<= ch_bool(true)`）—— 那会永久掩盖复位路径
    - 每个 master 引入：`write_route`（2 bit）+ `write_active`、
      `read_route`（2 bit）+ `read_active`
    - 路由 MUST 在 AW / AR 握手当拍由**该通道自己的地址**解码并锁存
    - 写侧与读侧 MUST 各自独立锁存
    - 形态参照 `AxiLiteMatrix2x2` 已验证的 `m0_route_s1` / `m1_route_s1`
      （`include/axi4/axi4_lite_matrix.h:146-149`、`:212-213`），但 MUST 覆盖写读两个方向
  - **verify**:
    ```bash
    # ⚠️ as-built 订正（2026-10-26，builder）：原判据**两处失真** ——
    #   ① `ch_reg<...> a, b, c;` **一行声明多个**寄存器 ⇒ `grep -c "ch_reg<"` 数的是
    #      **行**（13）而非寄存器数（31）；
    #   ② 文件头与 `IcState` 的**注释**里也写着 `select(!io().rst_n, ...)`
    #      ⇒ `grep -cE "select\(!io\(\)"` 多命中 2 次（33 vs 31）。
    #   ⇒ 唯一有判别力的判据 = 「`->next` 赋值次数 == 寄存器个数」
    #      （漏一个复位 pattern = 静默破坏 ⇒ 两者 MUST 相等）。
    grep -cE "\->next *= *select\(!io\(\)\.rst_n" include/axi4/axi_interconnect_4x4.h
    grep -oE "ch_reg<[^>]*>[^;]*" include/axi4/axi_interconnect_4x4.h | grep -oE "[a-z_][a-z_0-9]*" | grep -cE "^(aw_ptr|ar_ptr|wr_|rd_|err_)"
    grep -rn "interconnect.io().rst_n <<=" examples/ | wc -l
    grep -cE "wr_route|rd_route" include/axi4/axi_interconnect_4x4.h
    ```
    第 1 与第 2 条 MUST 相等（实测 **31 == 31**）：每个 `ch_reg` 都有复位 pattern；
    第 3 条 MUST = 1（4×4 接线护栏）；
    第 4 条 MUST ≥ 4（4 个 master 各一组，实测 **60**）
    （⚠️ MUST NOT 用 `grep -cE "route"` —— 文件头注释就命中，**永假绿**）
    （⚠️ 原判据里的 `grep -rn "matrix.io().rst_n <<=" ... MUST = 1` **移入 Task 7** ——
    2×2 的 `rst_n` 端口本身在 Task 7 才存在 ⇒ Task 3 无法接线
    ⇒ **计划排序缺陷**，as-built 修正为「4×4 在 Task 3 接线 / 2×2 在 Task 7 接线」）
  - **action（续）**:
    - 路由锁存用例 MUST 由红转绿
  - **verify（续）**:
    ```bash
    ./build/tests/test_axi_interconnect_4x4 "*latch*"
    ./build/tests/test_axi_interconnect_4x4 "*route latch*"
    ```
    MUST 转绿（用 Catch2 名称过滤，**不要**用 ctest 粒度）
    （⚠️ as-built 订正：原过滤串 `*lock*` **匹配不到任何用例**（用例名是
    `... route latch: changing AWADDR after handshake`，无 `lock` 子串）
    ⇒ 空过滤 ⇒ 0 用例 ⇒ **恒过**（永假绿）⇒ 改用 `*latch*`）

- [x] ### Task 4: 16 对全扇出（AW / W / AR 正向 + B / R 反向）
  - **action**:
    - 补齐 C1-C4 / C12 的全部缺失赋值（**只对 `ch_out` 赋值**）：
      - `m1/m2/m3` 的 `wready` / `arready`（**输出**）
      - `s1/s2/s3` 的 `araddr` / `arid` / `arlen` / `arsize` / `arburst` / `arvalid`（**输出**）
      - `s1/s2/s3` 的 `bready` / `rready`（**输出**）
      - `m0/m1/m2/m3` 的 `rresp`（**输出**）
      - ⚠️ `m*_bready` / `m*_rready` 是 **`ch_in`**（master 驱动）→ **MUST NOT 赋值**
    - AW / W / AR 三个正向通道 MUST 覆盖全部 16 对
    - B / R 反向通道 MUST 按**写/读路由锁存**反查来源
    - 每次 mux MUST **单次赋值**（MUST NOT 出现输出读自己的自环）
    - `WSTRB` MUST 随 W 通道一起转发
    - ⚠️ **在途门控**：`m*_awready` MUST 被 `!write_active[m]` 门控，
      `m*_arready` MUST 被 `!read_active[m]` 门控（修 C13）
  - **verify**:
    ```bash
    for p in m1 m2 m3; do grep -cE "^[[:space:]]*io\(\)\.${p}_(wready|arready) *=" include/axi4/axi_interconnect_4x4.h; done
    for s in s1 s2 s3; do grep -cE "^[[:space:]]*io\(\)\.${s}_(araddr|arvalid|bready|rready) *=" include/axi4/axi_interconnect_4x4.h; done
    grep -cE "^[[:space:]]*io\(\)\.m[0-3]_rresp *=" include/axi4/axi_interconnect_4x4.h
    ```
    7 条 MUST **全部**由 0 变为非 0
    （⚠️ MUST 覆盖 `{1,2,3}` 全部前缀 —— 只查 `m1_`/`s1_` 会漏接 m2/m3/s2/s3）
  - **action（续）**:
    - Task 2 的用例 1 / 2 / 3 / 4 / 7 / 10 / 13 MUST 转绿
  - **verify（续）**:
    ```bash
    ./build/tests/test_axi_interconnect_4x4 "*fanout*" && ./build/tests/test_axi_interconnect_4x4 "*write*" && ./build/tests/test_axi_interconnect_4x4 "*read*"
    ```
    MUST 逐项转绿

- [x] ### Task 5: 独立仲裁器（AW / AR 各一套）
  - **action**:
    - 拆成 2 个 `ch_reg<ch_uint<2>>` 指针 + 各自的 grant：
      写仲裁用 `awvalid`，读仲裁用 `arvalid`
    - 轮转条件 MUST 是**事务真正完成**：
      写 = **当前 grant 指向的**那个 slave 的 `bvalid && bready`；
      读 = **当前 grant 指向的**那个 slave 的 `rvalid && rready`
      ⚠️ MUST NOT 写成「任一 slave 的 `bvalid && bready`」（那是 C7 的换皮）
    - 在途事务的 master MUST 保持授权（`active` 优先于轮询）
    - **让位逻辑**（修 C14）：指针指向无请求的 master 时 MUST 让位给有请求的 master
      （形态参照 `AxiLiteMatrix2x2:181-193` 的 `m0_only` / `m1_only`）
  - **verify**:
    ```bash
    ./build/tests/test_axi_interconnect_4x4 "*arbit*" && ./build/tests/test_axi_interconnect_4x4 "*starv*"
    ```
    Task 2 的用例 9 / 11 / 12 MUST 转绿

- [x] ### Task 6: 地址映射参数化 + error responder + 死代码清理
  - **action**:
    - 把硬编码的 `awaddr >> 30` 改为**编译期可配置**的窗口映射
      （base / size 或等价形式）
    - 默认值 MUST 与当前 `>>30` 行为**等价**（避免破坏
      `examples/axi4/phase3a_complete.cpp`）
      ⚠️ 默认映射覆盖**全部** 32 位地址空间 → DECERR 在默认配置下不可测
      → MUST 在文档中记录该事实
    - **新增 error responder**（未映射地址由互连层应答）：
      - 写：`awready` 拉高 + `bvalid`（**寄存器**，保持到 `bready`）+ `bresp = DECERR`
      - 读：`arready` 拉高 + 返回 **`arlen + 1` 个 beat**，每拍 `rresp = DECERR`，
        末拍 `rlast = 1`
      - ⚠️ MUST NOT 用组合单拍脉冲（合规 master 看不到 BVALID → 永久死锁）
    - 读通道 MUST 用**读地址**解码
    - 删除 `build_decoder()` 空壳死代码（`include/axi4/axi_interconnect_4x4.h:210-221`）
    - 删除本 change 中被替代的任何旧路径（MUST NOT 留死代码 —— ZERO-DEBT POLICY）
  - **verify**:
    ```bash
    grep -c "build_decoder" include/axi4/axi_interconnect_4x4.h
    ./build/tests/test_axi_interconnect_4x4 "*decerr*"
    ```
    第 1 条 MUST 为 0；第 2 条 Task 2 的用例 5 / 6 MUST 转绿

### Tasks 0-6 完成记录（2026-10-26，builder）

| Task | 交付 | 实测验收 |
|---|---|---|
| 0 | spec 冲突消解 + 实例化点清点 | `VALIDATE_OK`；实例化点 = **2**（`phase3a_complete.cpp` + `axi_soc_example.cpp`） |
| 1 | 缺陷面基线（15 项逐条确认） | `m1-3 wready/arready`=0、`s1-3 AR/B/R`=0、`m0-3 rresp`=0、`build_decoder` 死码、两组件 `rst_n`=0 —— 全部确认缺陷**仍在** |
| 2 | RED 测试骨架（`tests/test_axi_interconnect_4x4.cpp`） | 14 对 × 2 后端 = **28** 个 TEST_CASE |
| 3 | `rst_n` + 路由锁存 | **31 个寄存器 / 31 个复位 pattern**（1:1）；4×4 接线 1 处 |
| 4 | 16 对全扇出（AW/W/B/AR/R 五通道）+ 在途门控 | `m1-3 wready/arready`=**6**、`s1-3 AR/B/R`=**9**、`m0-3 rresp`=**4**；120 个 `ch_out` **各赋值恰好 1 次** |
| 5 | AW/AR **独立**仲裁 + 让位 + 在途保持 + 仅事务完成才轮转 | `*arbit*` / `*inflight*` / `*concurrency*` 用例转绿 |
| 6 | 编译期可配置窗口映射 + error responder + 删死码 | `build_decoder`=**0**；`*decerr*` 用例转绿（写 = bvalid 保持；读 = `arlen+1` 个 DECERR beat + 末拍 rlast） |

**核心改动**：`include/axi4/axi_interconnect_4x4.h` 从 520 行 → **923 行**（接近重写：
C1-C14 横跨全部 5 个通道 + 读侧错误解码）。新增模板参数 `Map`（尾部默认参数，
向后兼容 `AxiInterconnect4x4<32,32,4>`）+ `AxiAddrWindow` / `AxiDefaultInterconnectMap`
（4×1GiB 窗口，与旧 `awaddr >> 30` **逐位等价** ⇒ 默认配置下 DECERR 不可触发；
测试用 `SparseTestMap`（4×4KiB 窗口）⇒ DECERR 可测）。

**验收实测**：
- `test_axi_interconnect_4x4` interpreter 半边：**14/14 通过，354 断言**（critic 修复**前**的基线）
- 同上 verilator 半边：**14/14 通过，354 断言**（同上）
- ⚠️ 最终计数见文末「critic 对抗审稿 第 1 轮」——修复后为 **23 对 / 461 断言 × 2 后端**
- `ctest -LE perf` 全量：**190/190 通过**（基线 189 + 本 change 的 1 个新文件 ⇒ 无回归）
- `ctest -N`：**201**（原 200）

### Tasks 7-9 完成记录（2026-10-26，builder）

| Task | 交付 | 实测验收 |
|---|---|---|
| 7 | `AxiLiteMatrix2x2` 活地址解码修正 | 读侧 `rd_route` 锁存（此前**完全没有**）；`s*_rready` 改用**读**路由（此前用**写**通道 grant）；`m*_awready`/`m*_arready` 加 `!active` 门控 + **当拍**地址解码；AW/AR **独立**仲裁；新增 `rst_n` + **18 个寄存器 / 18 个复位 pattern**（1:1）；`axi_soc_example.cpp` 接线 1 处 |
| 8 | 回归 + `phase3a_complete` 真跑 | `run_all_ported_tests.sh` **28/28**、`ctest -LE perf` **191/191**、`run_all_samples.sh` **35/35**、7 个护栏脚本全 exit 0 |
| 9 | 文档同步 + 计数 | `CTEST_TOTAL` **200 → 202**、non-perf **189 → 191**、`tests/test_*.cpp` **115 → 117**；`include/axi4/AGENTS.md` 新增「互连层」段（7 条契约）+ STRUCTURE/KEY FILES/TESTS 表 + 2 条 ANTI-PATTERN；根 `AGENTS.md` NOTES 新增条目 |

**Task 7 顺带修掉的额外缺陷**（超出 Task 7 的 5 条 action，但**由 spec 契约要求**）：
- **响应载荷按在途标志门控**：`m*_bresp`/`m*_bid`/`m*_rdata`/`m*_rresp` 在没有在途事务时
  MUST 为 0 —— 否则本 master 会镜像某个从设备的当前值（可能是**另一个 master 正在用**的
  那个）⇒ 表面上像「响应串到了别人身上」。**4×4 同步修正**（同一缺陷）。
- **2×2 也实现了可配置地址映射 + DECERR error responder + 独立仲裁** ——
  `axi-interconnect-routing` 的 5 条 Requirement 是**泛化**表述（同时管辖两个组件），
  只做 Task 7 列的 5 条 ⇒ 归档后主 spec 会含 2×2 **不满足**的条款 ⇒ spec/实现漂移。
- **新增 `include/axi4/interconnect_common.h`**：两个互连组件共用的 `AxiResp4x4` +
  `AxiAddrWindow` + 两个默认映射（避免 2×2 反向依赖 4×4 的头）。

**Task 8 顺带修掉的 example 缺陷**（`examples/axi4/phase3a_complete.cpp`）：
- `io().done` 与 `io().rdata` 此前被 `<<=` **双驱动**（`m0_bvalid` + `m0_rvalid` /
  常量 0 + `m0_rdata`）⇒ 行为未定义 ⇒ 改为各由**一个**寄存器驱动。
- `awvalid` / `wvalid` 此前**同拍**拉高，而下游 converter 与 slave 都是**相位分离**的
  ⇒ 同拍 AW+W 的 W **永不被接受** ⇒ 改为 AWVALID / WVALID **各自保持到对应握手**。
- 此前**只构建设计、不跑仿真** ⇒ AXI-Lite 链路从未被验证过 ⇒ 现补上
  **协议合规的内部 AXI4 Full master** + 写/读回**数据断言**（写 0xAB → 读回 0xAB）。

**新测试文件**（最终规模，含 critic 修复批的补强）：
`tests/test_axi_interconnect_4x4.cpp`（**46** TEST_CASE / **474** 断言 × 2 后端）、
`tests/test_axi_lite_matrix_2x2.cpp`（**34** TEST_CASE / **170** 断言 × 2 后端）。

- [x] ### Task 7: `AxiLiteMatrix2x2` 的活地址解码修正
  - **action**:
    - 读侧引入 `ar_route` 锁存（2x2 当前完全没有）；
      `m*_rdata` / `m*_rresp` / `m*_rvalid` / `s*_rready` MUST 基于该锁存
    - `s*_rready` MUST NOT 用**写**通道的 grant（`include/axi4/axi4_lite_matrix.h:326-329`）
    - `m0_awready` / `m1_awready`（`:251-252`）MUST 改为
      「**`!m*_active` 门控 + 按当拍地址解码的目标 slave 的 `awready`**」
      ⚠️ MUST NOT 写成「按锁存的写路由」—— 反例：锁存残留 `route = s1` 时，
      一笔新的到 s0 的 AW 会拿到 `s1_awready` → **AW 半丢失**
    - **新增 `rst_n` 端口 + 全部 5 个既有 `ch_reg`（`arb_state` / `m0_active` /
      `m0_route_s1` / `m1_active` / `m1_route_s1`）加复位 pattern**
      （2x2 当前 `rst_n` 命中 0 —— proposal D5）
    - `examples/axi4/axi_soc_example.cpp` MUST 接线
      `matrix.io().rst_n <<= io().rst_n;`
  - **verify**:
    ```bash
    # ⚠️ as-built 订正（2026-10-26，builder）：原判据**两处失真**（与 Task 3 同型）——
    #   ① `grep -c "ch_reg<"` 数的是**行**（12）而非寄存器数（18）；
    #   ② 注释里也含 `select(!io().rst_n` ⇒ 多命中 2 次（20 vs 18）。
    #   ⇒ 唯一有判别力的判据 = 「`->next` 赋值次数 == 寄存器个数」。
    grep -cE "\->next *= *select\(!io\(\)\.rst_n" include/axi4/axi4_lite_matrix.h
    grep -oE "ch_reg<[^>]*>[^;]*" include/axi4/axi4_lite_matrix.h | grep -oE "[a-z_][a-z_0-9]*" | grep -cE "^(aw_ptr|ar_ptr|wr_|rd_|err_)"
    grep -rn "matrix.io().rst_n <<=" examples/ | wc -l
    grep -cE "rd_route|wr_route" include/axi4/axi4_lite_matrix.h
    cd build && ctest -R test_axi_lite_matrix_2x2 --output-on-failure
    ```
    第 1 与第 2 条 MUST 相等（实测 **18 == 18**）；第 3 条 MUST = 1；
    第 4 条 MUST ≥ 2（读侧 + 写侧各一组路由锁存，实测 **32**）；
    第 5 条 2x2 用例 MUST 全绿
    （Task 7 当时实测 **28/28** = 14 对 × 2 后端 / 126 断言 × 2；
      critic 修复批补强后为 **34/34** = 17 对 × 2 后端 / 170 断言 × 2）

- [x] ### Task 8: 回归 —— 既有测试与 examples 必须仍然全绿
  - **action**:
    - `examples/axi4/phase3a_complete.cpp`（4×4）与
      `examples/axi4/axi_soc_example.cpp`（2×2）MUST 仍可编译
    - `phase3a_complete` 的 `main()` 当前**只构建不仿真** ——
      本 Task MUST 让它的 AXI-Lite 路径**真的跑起来**并断言结果
      - ⚠️ 该 example 有 2 个**已确认的现存缺陷**，MUST 一并处理：
        1. `io().done` 被 `<<=` 驱动**两次**（`:165` = `m0_bvalid`，
           `:187` = `m0_rvalid`）→ 同一个 `ch_out` 双 driver
        2. `done` 组合直出 + `awvalid` / `wvalid` 同拍驱动
           → 同拍 AW+W 会被相位分离的 `AxiToAxiLiteConverter` 拒收
      - ⚠️ **工作量校准**：该 example 没有 `Simulator`、没有 master stimulus、
        s1-3 被 tie-off → 「跑起来」≈ 写一个 AXI4 Full master testbench
        + 驱动顶层 `rst_n` + 数据断言，且只能验证 s0 一路
        → **16 对路由的验收由 Task 2-6 的单元测试承担，MUST NOT 把 example
        当作 16 对路由的验收手段**
    - `run_all_ported_tests.sh` 跟踪的 examples MUST 全绿
  - **verify**:
    ```bash
    cmake --build build -j$(nproc) && ./run_all_ported_tests.sh
    cd build && ctest -L verilator_e2e -R "interconnect|matrix" --output-on-failure
    ```
    MUST 全绿
    （⚠️ as-built：`run_all_ported_tests.sh` 要求**先**构建被跟踪的 examples
    ⇒ `cmake --build build --target all-examples`；否则「缺失二进制 = 失败」）
    **实测（2026-10-26）**：`run_all_ported_tests.sh` **28/28**；
    `ctest -L verilator_e2e -R "interconnect|matrix"` **2/2**；
    `ctest -LE perf` **191/191**；`run_all_samples.sh` **35/35**；
    `scripts/verify_test_counts.sh` exit 0；`scripts/verify_rst_n_wiring.sh` exit 0

- [x] ### Task 9: 文档同步 + 测试计数更新
  - **action**:
    - `include/axi4/AGENTS.md`：
      - 新增「互连层」段（地址映射、路由锁存、仲裁、error responder、复位契约）
      - STRUCTURE 表中 `axi_interconnect_4x4.h` / `axi4_lite_matrix.h` 的行注 MUST 更新
      - TESTS 表 MUST 列出两个新测试文件
    - 根 `AGENTS.md`：
      - NOTES 中「`axi4-interconnect-routing-2026-10` —— 4×4 全路由」MUST 改写为已实施
      - `CTEST_TOTAL` MUST 更新：**178 → 180**（本 change 独立落地）
        或 **179 → 181**（若 burst change 已先落地）→ 以实际 `ctest -N` 为准
        （**2 个新文件 = 2 个 ctest 条目**，双后端不增加条目数）
    - `scripts/verify_test_counts.sh` MUST 通过
  - **verify**:
    ```bash
    ./scripts/verify_test_counts.sh && cd build && ctest -N | tail -1
    ```
    MUST 全绿且计数与实际一致
    （⚠️ as-built 计数口径：基线是 **200**（`rv32i-pipeline-commit-path-2026-10` 之后），
    本 change 独立落地 **+2**（2 个新测试文件 = 2 个 ctest 条目；双后端在**同一个**
    Catch2 二进制内跑 ⇒ 每文件只占 1 个条目）⇒ **202**；
    non-perf **189 → 191**；perf 恒 **11**；
    `tests/test_*.cpp` **115 → 117**（2 个新文件都在根 `tests/` 下））

### critic 对抗审稿 第 1 轮（2026-10-26）—— REJECT ⇒ 已修复

**结论**：**2 BLOCKER + 4 SHOULD_FIX + 5 NIT**，全部用**独立复现程序**实测确认
（源码与二进制留在 `/tmp/opencode/critic-ic/`）。

#### BLOCKER（2 项，均已修）

| # | 缺陷 | 根因 | 修法 |
|---|---|---|---|
| 1 | **未映射地址的多 beat 写永久死锁**（`axi_interconnect_4x4.h`） | `err_w_hs` 在**第一个** W beat 握手时就置位 `err_w_valid` ⇒ ① `m*_wready` 随之恒 0 ⇒ 后续 beat **永不被接受** ⇒ master 持 `wvalid` 永久挂死；② B 响应在 `wlast` 之前发出，违反 AXI | `err_w_remaining`（**「当前拍之后」还需的 beat 数**，4 bit，初值 = `awlen`）逐拍递减（带 `!= 0` 守卫 ⇒ 无下溢）；`err_w_valid` **只在最后一拍被接受后**置位 |
| 2 | **幻影 AW / AR**（两个组件同型） | 正向转发（`aw_sel` / `ar_sel`）只吃**当拍** grant，**没吃** `!active` 门控 ⇒ master 在途期间合法举起的下一笔 `awvalid` 被**转发**给 slave 并被接受，而互连侧 `awready=0` ⇒ slave 等不到 W、master 等不到 `awready` ⇒ **双向永久死锁** | 正向转发加 `!write_active[m]` / `!read_active[m]` 门控（4×4 + 2×2）；**spec 第 4 条同步补该条款**（此前 spec 只说「`m*_awready` 吃门控」⇒ 漏了正向转发这一面） |

#### SHOULD_FIX（4 项，均已修）

1. `m*_rid` / `m*_rlast` **未按在途标志门控** ⇒ 空闲时 `rlast` 恒 1、`rid` 镜像某个从设备的值（可能是**另一个 master 的在途读**在用的）。
2. DECERR 响应**不回显事务 ID**（与 `Axi4Slave` 的 `write_id_reg` / `read_id_reg` 形态不一致）⇒ 加 `err_w_id` / `err_r_id` 锁存 + 回显。
3. `decode_addr` 的 `static_assert` **只校验了窗口 [0]**（4×4 一个都不校验）⇒ 改为**全窗口** `axi_window_is_valid`（`size` 为 2 的幂 + `base` 按 `size` 对齐）。
4. `window_hit` 用 64 位掩码构造 32 bit 逻辑值 ⇒ **调试构建**（assert 打开）触发 `CH_CHECK`「value out of range」；NDEBUG 下静默截断（行为仍正确）⇒ 此前被 NDEBUG 掩盖。修法：先截到 `ADDR_WIDTH` 位。

#### NIT（5 项，均已处理）

`err_r_busy` 与 `err_r_valid` 转移完全相同（删除冗余）；`wr_done` / `rd_done` 的 `& active` 冗余（`b_hs` / `r_hs` 已蕴含）；`AxiResp4x4` 与 `AxiResp` 逐值重复且 guard 宏不同（**死 guard**）⇒ 统一到新头 `include/axi4/axi_resp.h`；`AGENTS.md` 的「5 条 Requirement」→ **6 条**；`include/axi4/AGENTS.md` 的 spec 路径加「archive 前主 spec 不存在」说明。

#### 测试补强（critic 指出的盲区）

`write_txn` / `read_txn` 此前**恒 `len = 0`** ⇒ 多 beat 路径与 `wlast` **零覆盖**。

- 4×4：**14 → 23 对**（46 TEST_CASE，**474 断言** × 2 后端）
- 2×2：**14 → 17 对**（34 TEST_CASE，**170 断言** × 2 后端）

新增用例：多 beat mapped 写（逐 beat 数据 + `wlast`）/ **多 beat unmapped 写**（逐 beat 接受 + B 只在末拍后出现 + ID 回显）/ 多 beat mapped 读（`rlast` 只在末拍）/ **幻影 AW** / **幻影 AR** / 4×4「**恰好一个 grant**」（旋转编码判别力）/ 4×4 + 2×2 **复位清 error responder** / DECERR 读的 ID 回显。

#### 变异验证 9/9 全部符合预期

脚本：`/tmp/opencode/mutation_check.sh`。
⚠️ **脚本 MUST 校验编译成功** —— 第 1 轮过早的版本有 3 个变异**编译失败却跑旧二进制** ⇒ 假绿（已修）。

| # | 变异 | 预期 | 实测 |
|---|---|---|---|
| 1 | 删 `aw_sel` 在途门控 | 幻影 AW 用例转红 | ✅ fail |
| 2 | `err_w_valid` 改任意 beat 即置位 | 多 beat DECERR 写转红 | ✅ fail |
| 3 | 仲裁 `g3 = q3`（少一级优先编码） | 恰好一个 grant 转红 | ✅ fail |
| 4 | `rd_active` 清零去掉 `& m2_rlast` | 多 beat 读转红 | ✅ fail |
| 5a / 5b | DECERR 写 / 读不锁存 ID | ID 回显用例转红 | ✅ fail ×2 |
| 6 | 2×2 删 `ar_sel` 在途门控 | 幻影 AR 转红 | ✅ fail |
| 7 / 8 | 2×2 / 4×4 的 `err_w_busy` 复位 pattern 去掉 | 复位用例转红 | ✅ fail ×2 |
| 9 | `window_hit` 去掉位宽截断 | （仅调试构建受影响） | ✅ pass |

**4 个 critic 复现程序全部转为修复后行为**：`repro`（多 beat DECERR 写 ⇒ 第 2 拍 `wready=1`）、`repro2` / `repro3`（幻影 AW ⇒ `s*_awvalid=0`）、`repro4`（空闲 ⇒ `rid=0` / `rlast=0`）。

#### 修复后回归（2026-10-26 实测）

```
cmake --build build -j$(nproc)                  # 0 error / 改动文件 0 warning
cd build && ctest -LE perf                      # 191/191 通过（800 s）
./run_all_ported_tests.sh                       # 28/28
bash scripts/run_all_samples.sh                 # 35/35
scripts/verify_{example_assertion_quality,example_test_wiring,
  no_duplicate_rv32i_headers,openspec_delta,rst_n_wiring,test_counts}.sh   # 全 exit 0
cd build && ctest -N                            # 202（未变：新增的是用例不是文件）
```


### critic 对抗审稿 第 2 轮（2026-10-26）—— APPROVE_WITH_CONDITIONS ⇒ 条件项已全部处理

**结论**：第 1 轮的 **11 项（2 BLOCKER + 4 SHOULD_FIX + 5 NIT）已全部正确修复**，
修复本身**未引入新 BLOCKER**。critic 逐条核对了实现并**实际运行**了两个测试二进制 +
4 个复现程序。剩 **2 SHOULD_FIX + 4 NIT**（含一条「注释与代码矛盾」，矛盾的恰好是
BLOCKER #1 修复所依赖的那条不变量）。

#### SHOULD_FIX（2 项，均已修）

1. **新增注释与紧邻代码直接矛盾**（`axi_interconnect_4x4.h`）——
   struct 声明处写的是正确语义「当前拍**之后**还需的 beat 数」，而实现段注释
   写的是**相反的**「含当前这一拍在内 … 末拍 ⟺ 剩余 == 1」（代码实际是 `== 0`），
   且写着「`err_w_remaining` 用 **5 bit** —— 4 bit 会溢出」而声明就是
   `ch_reg<ch_uint<4>>`。**这不是无害笔误**：它精确反转了 BLOCKER #1 修复所依赖的
   不变量 ⇒ 按注释改代码就会重新引入「首拍即置位 / 下溢回绕」。
   ⇒ 全部注释改为与代码一致，并把「MUST NOT 改成 5 bit（`ch_uint<5>` 无法从
   `ch_in<ch_uint<4>>` 构造，实测编译失败）」写进注释。
2. **`axi_window_is_valid` 不校验 `base` 相对 `ADDR_WIDTH` 的上界** ——
   本批新增的 `base & addr_bits_mask()` 截断使 `base >= 2^ADDR_WIDTH` 的窗口
   **静默别名**到低地址（与低窗口双命中 ⇒ 按优先级静默错路由），而截断**消除**了
   原本唯一的配置错误检测（改动前调试构建会 `CH_CHECK`）。
   ⇒ 新增 `constexpr bool axi_window_fits(w, addr_width)`（用 `base | (size-1)`
   避免 64 位溢出）+ 两个组件的 `decode_addr` 对**每个**窗口 `static_assert`。
   **验证**：探针 `Map` 的第 4 个窗口 `base = 0x1_0000_0000` ⇒ 编译失败且错误信息
   命中「地址窗口 3 超出 ADDR_WIDTH 位地址空间」；对照探针（合法窗口）⇒ 编译通过。

#### NIT（4 项，均已处理）

1. `axi_resp.h` 的 `#pragma once` 之内又套 `#ifndef AXI4_AXIRESP_DEFINED`
   ⇒ 删掉（该头存在的理由正是消灭一个永不可能碰撞的死 guard 宏）。
2. 文档 stale：`include/axi4/AGENTS.md` 的 STRUCTURE 树**漏 `axi_resp.h`**、
   行数 stale（949 / 525）⇒ 补入并刷新为 **1094 / 544**；
   根 `AGENTS.md` 的「变更面」同步（并补 `axi_resp.h` 的来龙去脉）。
3. 幻影 AW/AR 用例**只有负向半面**（4 拍内不转发），没有验证排队那笔**最终被接受**
   —— 一个把请求**永久丢弃**的实现同样能通过纯负向断言。
   ⇒ 4 个用例（4×4 AW / 4×4 AR / 2×2 AW / 2×2 AR）各补 **liveness 半面**：
   完成在途事务后断言排队那笔 `awready`/`arready` 拉高、且转发到**正确**的 slave。
4. `addr_bits_mask()` 对 `ADDR_WIDTH == 0` / `> 64` 无约束 ⇒ 加 `static_assert`
   （0 会让掩码恒 0 ⇒ 全地址命中；> 64 会静默丢掉地址高位）。

#### 变异验证 12/12 全部符合预期

第 1 轮的 9 项 + 本轮新增 2 项（`wr_active` 永不清零 ⇒ liveness 半面转红；
窗口 base 越界 ⇒ 探针编译失败 + 对照探针编译通过）。脚本同时校验**编译成功**
（编译失败的变异会跑旧二进制 ⇒ 假绿）。

#### 最终计数

- `test_axi_interconnect_4x4`：**23 对 = 46 TEST_CASE / 474 断言 × 2 后端**
- `test_axi_lite_matrix_2x2`：**17 对 = 34 TEST_CASE / 170 断言 × 2 后端**
- `ctest -N` = **202**（未变 —— 新增的是用例，不是文件）

