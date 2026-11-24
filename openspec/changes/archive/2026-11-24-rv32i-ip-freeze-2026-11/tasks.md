# tasks · `riscv-mini` IP 冻结（判据完备）

> **纪律**：TDD（先见红）· 每步跑验证 · 提交单独成段 · **不改端口面 / 不改行为**

## P1 · IP 级 MMIO 判据（T1 / AC1–AC5）

- [x] **T1.1** ✅ 新建 `examples/riscv-mini/tests/test_rv32i_core_mmio.cpp`：`ch_device<Rv32iCoreTop>` + 响应式 stub
  ⇒ ⇒ **7 组判据 / 50 断言**（AC1 读值 / AC2 计数 / AC3 **反证档**（从设备不应答 ⇒ 程序永不完成） / AC4 DECERR / AC5 非 MMIO 零变化 / AC6 AXI-Lite 握手协议 / AC7 **慢从设备**（`ready_lat=3` ⇒ VALID 窗口跨多拍））
  ⇒ ⚠️ **AC3 已 dated 订正**（design §1.3）：初稿的「事务拍数上界」把实现时序写进判据 ⇒ 改为**两侧夹逼**
- [x] **T1.2** ✅ `examples/riscv-mini/CMakeLists.txt` 注册 target（`riscv_mini_catch_test` ⇒ 进 CTest · 标签 `base`）
- [x] **T1.3** ✅ 先见红（实测记录）：**M8** = 读数据 mux 退回 S4a 缺陷形态 ⇒ **3 用例转红**；**M9** = 分流方向反转 ⇒ **6 用例全红**；均还原复绿（`MUTANT` 残留 = False）

## P2 · 层次化等价纳入新端口（T2 / AC6）

- [x] **T2.1** ✅ `test_rv32i_hier_rtl.cpp` 追加第 4 个 TEST_CASE：8 根 `axil_*` 输入受驱动 + 11 根输出被观测 + 4 根值锚 + MMIO 程序 ⇒ `compare` **equal**（4 用例 / 186 断言 · 27s）
- [x] **T2.2** ✅ 变异自证（**并入 T2.1 的同一 TEST_CASE** ⇒ 省一次 Verilator 构建）：篡改 `assign top_axil_awaddr = …` ⇒ `equal == false` ∧ `mismatch_count > 0`。另加**观测面非常量**自证（`axil_awvalid` / `axil_araddr` 随拍变化）

## P3 · 文档与载体（T3 / T4）

- [x] **T3.1** ✅ `examples/riscv-mini/AGENTS.md` 新增「IP 集成契约」段（7 条 + 复用方两个高频坑）+ 按实测订正 target/source 计数（19 / 20）
- [x] **T4.1** ✅ `CTEST_TOTAL` **276 → 277**（实测 `ctest -N` = 277）· `EXAMPLES_TEST_SOURCES` **21 → 22** · CH4 新增一行（下限 **34** · 实测 35）+ hier 行描述补 19 根 axil 面
- [x] **T4.2** ✅ `docs/CHANGELOG.md` 一条（4 小节）

## P4 · 验证（T5 / AC7）

- [x] **T5.1** ✅ 新 target 7 用例 / 50 断言 · hier 4 用例 / 186 断言 · SoC `checks=10 fails=0` · 核级 `checks=24 fails=0`（零变化）
- [x] **T5.2** ✅ 全量 `ctest` **277/277 通过 0 失败**（verilator_e2e 81/81 · 总时 1450s）· `run_all_ported_tests` **PASS 29/29** · `rv32i_rtl_sim_env` **PASS**（flat/hier 各 `checks=24 fails=0`）· 全部 `verify_*.sh` 一轮绿（`verify_riscv_soc_core_ports` 曾因本批新增的契约表被误解析而红 ⇒ 已把该表改成非数字首列；其 `--self-test` 7 条投毒全红）

## P5 · 收口（T6）

- [x] **T6.1** ✅ `openspec validate rv32i-ip-freeze-2026-11 --strict` = valid（`skip_specs` 显式接受）· `verify_openspec_delta` R1–R4 全绿
- [x] **T6.2** ✅ 归档为 `openspec/changes/archive/2026-11-24-rv32i-ip-freeze-2026-11/`（`.openspec.yaml` 声明 `skip_specs: true` ⇒ 无 spec delta）· 母 improvement 记 IP 冻结（含 S4b 待办）· `examples/riscv-soc/AGENTS.md` 登记「消费 IP v1」（并顺手订正该节标题里 S4a 残留的 34 → 53）