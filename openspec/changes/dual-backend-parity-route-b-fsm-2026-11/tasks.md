# Tasks：路线 B 第 1 批 —— FSM 运行期转移双后端

## C1 —— 新增 dual 文件（**原文件 MUST NOT 修改**）

- [x] `tests/test_state_machine_dual.cpp`（**2** pair）
      ⇒ DUT 1 无条件链 A→B→C（3 拍 × 2 断言）· DUT 2 条件转移（6 拍 × 2 断言）
- [ ] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`

## C2 —— 判据（N2′：断言数 == 2 ×）

- [ ] `test_state_machine_dual`：**36 == 2 × 18**（有/无 verilator）
- [x] 期望值**全部手推**：文件头逐拍列出，并注明**互证来源**
      （`tests/test_state_machine.cpp` 的 `StateMachineDut` / `DefaultTailDut`
      两条已知良好的 interpreter 断言）

## C3 —— 变异证明判别力

- [ ] M1：删掉 `sm.state(A).on_active(...)` 里的 `transition_when`
      ⇒ 状态停在 A ⇒ `kChainState[0]==1` 失败 ⇒ **红**
- [ ] M2：把 DUT 2 的 `cyc == 2` 改成 `cyc == 3`
      ⇒ 状态序列整体后移一拍 ⇒ **红**
- [ ] ⚠️ 每条变异 MUST **先确认仍可编译**（否则跑的是旧二进制 ⇒ 假绿）

## C4 —— 计数与验收

- [ ] 锚点：`CTEST_TOTAL` **215** · `VERILATOR_E2E_TOTAL` **47** ·
      `DUAL_BACKEND_PARITY_TOTAL` **38** · `PAIRS` **212** · non-perf **204** ·
      `tests/test_*.cpp` **126**
- [ ] `ctest -LE perf` 零回归
- [ ] `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 通过
- [ ] 本批冷缓存墙钟实测 ⇒ 回填 plan §5
- [ ] 归档：**用** `--skip-specs`（`.openspec.yaml` 已显式声明 no spec changes）
