## Tasks

> **执行顺序**：Task 0 → 1 → 2 → 3 → 4 → 5 → 6。
>
> **TDD 纪律**：Task 2 的护栏**先写脚本、先看它红**（对当前的 3 个遗留文件应当报红），
> Task 3 再让它们转绿。

- [x] ### Task 0: spec 解锁核对 + RED 基线
  - **action**:
    - 确认本 change 的 spec delta 齐全（`dual-backend-test-coverage` MODIFIED ×1）
    - 确认 MODIFIED 的 requirement 标题与主 spec **逐字一致**
      （OpenSpec 按标题匹配，标题不一致 = 该条 delta 被静默丢弃）
    - 记录 RED 基线：当前 HEAD 的非 perf ctest 必须全绿
  - **verify**:
    ```bash
    openspec validate legacy-axi-peripheral-test-cleanup-2026-10 --strict && echo VALIDATE_OK
    cd build && ctest -LE perf -j4 --output-on-failure
    ```

- [x] ### Task 1: 死 helper 的零调用静态核实
  - **action**:
    - 在删之前**静态核实** `axi_lite_write` / `axi_lite_read` 确实是零调用
      （删错会直接编译失败，但先核实可避免误判）
  - **verify**:
    ```bash
    grep -n "axi_lite_write(\|axi_lite_read(" tests/test_axi_spi.cpp
    grep -n "axi_lite_write(\|axi_lite_read(" tests/test_axi_pwm.cpp
    ```
    MUST **只命中定义行**（无调用点）

- [x] ### Task 2: 静态护栏脚本（先红）
  - **action**:
    - 新建 `scripts/verify_rst_n_wiring.sh`：
      扫描全仓（排除 `include/axi4/` 组件定义目录、`build/`、`third_party/`）
      实例化了「带必需 `rst_n` 端口的组件」的文件，
      要求同一文件出现 `rst_n`；否则打印文件 + 组件名并退出 1
    - ⚠️ 组件清单 MUST 与「带必需 `rst_n` 端口」的组件清单同步维护
  - **verify**:
    ```bash
    bash scripts/verify_rst_n_wiring.sh; echo "exit=$?"
    ```
    **此时 MUST 报红**（当前 3 个遗留文件未接线 `rst_n`）→ RED 基线

- [x] ### Task 3: 清理 3 个遗留文件（转绿）
  - **action**:
    - `tests/test_axi_spi.cpp`：删除 2 个零调用 helper；所有实例化 device 的用例驱动 `rst_n = 1`
    - `tests/test_axi_pwm.cpp`：同上
    - `tests/test_axi_i2c.cpp`：实例化 device 的用例驱动 `rst_n = 1`
    - ⚠️ **驱动 `rst_n` 可能暴露既有隐藏失败**（DUT 从「永久复位」变成「正常运行」）
      → 每个文件单独跑，若转红 MUST 判定是「暴露了真问题」还是「用例依赖了复位态」
      （后者改用例，前者另开 change，**MUST NOT** 在本 change 里顺手改实现）
  - **verify**:
    ```bash
    bash scripts/verify_rst_n_wiring.sh; echo "exit=$?"
    ./build/tests/test_axi_spi && ./build/tests/test_axi_pwm && ./build/tests/test_axi_i2c
    ```
    第 1 条 MUST 退出 0；第 2 条 MUST 全绿

- [x] ### Task 4: 运行时护栏（双后端）
  - **action**:
    - 新建 `tests/test_axi_rst_n_guard.cpp`：用 `DUAL_BACKEND_TEST_PAIR`
      （interpreter + verilator），**故意不驱动 `rst_n`**，断言：
      写事务无 B 响应（`bvalid == 0`）+ 目标寄存器读回 0 +
      读事务无 R 响应（`rvalid == 0`，`rdata == 0`）
    - ⚠️ 握手等待 MUST 是**有界**循环（≤ 64 拍）——永久复位下
      `bvalid` / `rvalid` 永不出现，无界等待会 hang 到 ctest TIMEOUT
    - 在 `tests/CMakeLists.txt` 用 `add_dual_backend_test` 注册
      （**MUST 用 `SOURCES` 关键字形式**，`TIMEOUT 300`）
  - **verify**:
    ```bash
    cmake -B build && cmake --build build -j$(nproc) --target test_axi_rst_n_guard
    ./build/tests/test_axi_rst_n_guard --list-tests | tail -1
    cd build && ctest -N -R test_axi_rst_n_guard | tail -1
    ```
    第 2 条 MUST 输出 **4 test cases**（2 个 DUT × 2 后端）；
    ⚠️ **不要用 `--list-tests | grep -c .`** —— 它把标题行与空行也数进去（实测 11），
    正确读法是末行的 `N test cases`；
    第 3 条 MUST 命中 **1**（1 个文件 = 1 个 ctest 条目）

- [x] ### Task 5: CI 接线
  - **action**:
    - `.github/workflows/ci.yml`：与 `scripts/verify_test_counts.sh` 并列，
      新增 `scripts/verify_rst_n_wiring.sh` 的 step（同样在多 OS 矩阵里跑，
      因为脚本是纯 bash + grep）
  - **verify**:
    ```bash
    grep -n "verify_rst_n_wiring" .github/workflows/ci.yml
    ```
    MUST 命中 ≥ 1

- [x] ### Task 6: 文档同步 + 回归
  - **action**:
    - 根 `AGENTS.md` NOTES：记录本 change（清理内容 + 两道护栏）
    - `include/axi4/AGENTS.md` CONVENTIONS → 复位段：
      登记「接线护栏脚本」的存在与用法（`scripts/verify_rst_n_wiring.sh`）
    - `scripts/verify_test_counts.sh` MUST 仍然通过
  - **verify**:
    ```bash
    ./scripts/verify_test_counts.sh
    bash scripts/verify_rst_n_wiring.sh
    cd build && ctest -LE perf -j4 --output-on-failure
    ./run_all_ported_tests.sh
    ```
    MUST 全绿

---

## 实施记录（2026-10-21）

- **Task 1**：`axi_lite_write` / `axi_lite_read` 在 `test_axi_spi.cpp` /
  `test_axi_pwm.cpp` 各 2 处，**只命中定义行**（零调用）→ 核实通过。
- **Task 2**：`scripts/verify_rst_n_wiring.sh` 落地。初版只做**文件级**检查，
  实测**被自己骗过 3 次**：
  1. 「注释里提到 `rst_n`」就算通过 → 改为只认**真实驱动**；
  2. 「同一文件里另一个 DUT 驱动了 `rst_n`」就算通过 → 改为
     **per-test-case** 粒度；
  3. `DUAL_BACKEND_TEST_PAIR` 宏内部才构造 `ch_device`，源文件里根本没有
     `ch_device` 字样 → 扫描过滤条件与「是否仿真」的判据都 MUST 认识该宏
     （并解析 `using X = axi4::Axi4Slave<...>;` 这类**类型别名**）。
  → 最终实现 **R1（per test case）+ R2（wrapper 透传）+ R3（文件级兜底）**，
  只用 bash + awk（CI 矩阵是 ubuntu-22.04 / macos-14，不引入 python 依赖）。
- **Task 3**：`test_axi_spi.cpp` 删除 73 行死 helper、`test_axi_pwm.cpp` 删除 77 行
  （含全部 5 处无界握手等待）；`test_axi_spi.cpp` 的 6 个 device 用例驱动 `rst_n = 1`。
  `test_axi_i2c.cpp`（唯一 device 只用于 codegen）与 `test_axi_dma.cpp`
  （只创建 standalone Component）**不是**违规点 → 不改。
- **护栏自检**：3 个人为注入的漏接（burst 测试全量、byte_lane 的 3 个 Full 用例、
  example 的 wrapper 透传）**全部被抓到**（10 / 3 / 1 处）。
  ⚠️ 护栏还**当场抓到本会话自己**在备份/恢复实验中弄丢的 3 行 `rst_n` 驱动
  （`cp` 还原了一个「已被改动」的备份）→ 补回后转绿。
- **Task 4**：`tests/test_axi_rst_n_guard.cpp` 落地（2 个 DUT × 2 后端 = 4 个
  TEST_CASE，12 个断言）。用**固定拍数**推进（不是无界等待）：复位态下
  `bvalid` / `rvalid` 永不出现，无界等待会 hang 到 ctest TIMEOUT。
- **Task 5**：CI 的 2 个 job（主矩阵 + `pr-verilator-fast`）各加 1 个 step。
- **Task 6**：根 `AGENTS.md` NOTES + `include/axi4/AGENTS.md` CONVENTIONS → 复位段
  登记两道护栏；`CTEST_TOTAL` 179→180。
