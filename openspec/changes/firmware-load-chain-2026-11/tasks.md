# Tasks: 固件装载链路（显式映射 + 统一加载端口 + 转储器 / ELF 装载）

> **实施顺序固定**（见 `design.md` §1）⇒ ⇒ MUST 按序执行。
> ⚠️ ~~**L1 与 L2 MUST 同批**（二者都改 SoC 顶层端口契约 ⇒ ⇒ 公开面登记只写一次）~~
> ⇒ ⇒ **2026-11-14 用户裁决**：L2 改走 **路线 A（内存初值）** ⇒ ⇒ **零端口变更**
> ⇒ ⇒ 该「同批」约束**随之失效**（理由成立的前提对象不存在了）。
> ⚠️ **与 `memory-timing-contract` 同批实施** ⇒ ⇒ **先做那一个**（D-TCM 读时序先定）。

## S0 — 开工前基线（**零改动**）

- [x] 记录基线：`ctest -N` 总数 · 现有 `prog_*` 语义（字索引）· 编码器副本数（**3 处**）
      ⇒ ⇒ **2026-11-14 实测订正**：编码器**只剩 1 处**（`examples/riscv-mini/tests/
      test_rv32i_soc_dual_parity.cpp`）⇒ ⇒ 「3 处副本」是**陈旧记录**（另两处已在先前的
      change 里删除）⇒ ⇒ L3 的真实工作 = **把它上移到库**（否则工具无法复用）
- [x] 确认工作区干净（`git status --short` 为空）⇒ ⇒ **禁** `git add -A`
      ⇒ ⇒ **2026-11-14 已完成**（提交 `2e8b529` / `a2f5075` 后工作区干净）

```bash
cd /workspace/project/CppHDL
ctest --test-dir build -N | tail -1
grep -rn 'has_store(t, 0x' tests/ | wc -l
bash scripts/verify_test_counts.sh
```

## S1 — L1：显式映射表 + 越界诊断（**先红后绿**）

- [ ] 🔴 **写**越界投毒判据**：地址设到区域外 ⇒ ⇒ **MUST 有诊断** ⇒ ⇒ **先红**
      ⇒ ⇒ **2026-11-14 复核：真未做** ⇒ ⇒ **归 T4 批次**（`memory-timing-contract`）
      ⇒ ⇒ ⚠️ **为什么必须做**：`address_decoder` 现有用例只测 `0x12345678`
      ⇒ ⇒ **旧 64 KB 声明下也红** ⇒ ⇒ **零判别力**
      ⇒ ⇒ **有判别力的形态**：`0x8000_4000`（**旧 64 KB ITCM 内 · 新 16 KB 外**）
      ⇒ ⇒ ⇒ ⇒ **MUST** `error_cs == 1` **且** `i_tcm_cs == 0`（旧声明下会红 ⇒ 才算投毒有效）
      ⇒ ⇒ 载体 = `examples/riscv-mini/tests/test_riscv_tests_soc.cpp`（既有 decoder 用例旁）
- [x] `examples/riscv-mini/src/rv32i_pipeline_top.h`：显式 `{base, size}` 表
      ⇒ ⇒ **2026-11-14 已实施**：表**上移到库** = `include/cpu/riscv/rv32i_mem_map.h`
      （`kItcm` / `kDtcm` / `kTcmBytes` / `kPcReset`）⇒ ⇒ SoC 顶层与 `address_decoder.h`
      都改为**引用**它 ⇒ ⇒ 「声明 64 KB · 实现 16 KB」那段不一致**已消**（size 派生 = 16 KB）
  ⇒ ⇒ `size` **由容量派生**（禁手写）· `dtcm.base == 0x8001_0000`
- [x] **PC 复位值 MUST 由 `itcm.base` 派生**（禁第二个字面量）
      ⇒ ⇒ **2026-11-14 已实施**：`if_stage.h` 的 `pc_reg` 复位值改为 `riscv::kPcReset`
      （= `kItcm.base`）⇒ ⇒ **值不变**（仍 `0x80000000`）⇒ ⇒ **既有期望值零漂移** ✓
      ⇒ ⇒ ⚠️ 按 metis 订正：**不改复位值本身**（改了只会 churn 全部 `dbg_id_pc` 期望）
- [x] ⚠️ **对齐要求是 16 KB 而非 4 KB** ⇒ ⇒ 「截断等价的最小判据」按 16 KB 取
      ⇒ ⇒ **2026-11-14 已落地**：`kTcmBytes = 1 << (kTcmAddrWidth + 2)` **派生** = 16 KB
      ⇒ ⇒ 证据：转储器 manifest 的 `size` 断言 = **16384**（护栏 D2）
- [x] ⚠️ **受影响地址字面量清单**逐条登记（`grep -n 'has_store(t, 0x'` ⇒ ⇒ **禁**只报计数增量）
      ⇒ ⇒ **2026-11-14 复核：前提未触发** ⇒ ⇒ L1 **没有**改任何绝对地址
      ⇒ ⇒ （地址仍是**区域内偏移** `0x100` / `0x104` ⇒ ⇒ golden 的地址字面量**零条目**需重推）
      ⇒ ⇒ ⚠️ 若将来改成**绝对地址**（如 `0x8001_0100`）⇒ ⇒ 这条**重新生效** ⇒ ⇒ 必须逐条登记

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
bash scripts/verify_port_naming.sh
```

## S2 — L2：**内存初值装载**（路线 A · 2026-11-14 用户裁决）

> 原设计（统一加载端口 `(we, addr[字节], data[size])`）**已移出** ⇒ ⇒ 见 proposal 的 Out of Scope。
> 理由：装载是**发射期**的事 ⇒ ⇒ 产物自足 ⇒ ⇒ **零端口变更**；而路线 B 要付
> 「D-TCM 写端口 MULTIDRIVEN」「装载期读地址共用」两项代价。

- [x] `InstrTCM` / `DataTCM` 增加**可选** `init_words` 构造参数（默认空 ⇒ **零行为变化**）
      ⇒ ⇒ **2026-11-14 已实施**（`include/cpu/pipeline/rv32i_tcm.h`）⇒ ⇒ `ch_mem(init_words_, …)`
- [x] `Rv32iPipelineTop` 增加 `itcm_init` / `dtcm_init` 并**透传**到 TCM 构造
      ⇒ ⇒ **2026-11-14 已实施**（`ch::ch_module<T, Args…>` 已支持参数透传 ⇒ 零新机制）
- [x] 带 `.data` 的示例固件 + 带初值的发射器
      ⇒ ⇒ **2026-11-14 已实施**（`examples/riscv-mini/src/rv32i_fw_demo.h` ·
      `examples/riscv-mini/emit_rtl_fw_init.cpp`）
- [x] ⚠️ **零端口变更**有**产物级判据**（不是口头承诺）
      ⇒ ⇒ `scripts/verify_rv32i_fw_init_rtl.sh` **F2**（两次发射的顶层端口表逐字 diff）
- [x] 公开面登记：`docs/CHANGELOG.md`（**新增**可选构造参数 ⇒ 非破坏性）

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc) --target emit_rtl_rv32i_soc emit_rtl_rv32i_soc_fw_init
bash scripts/verify_rv32i_fw_init_rtl.sh
bash scripts/verify_rv32i_fw_init_rtl.sh --self-test
bash scripts/verify_port_naming.sh
```

## S3 — L3：编码器库化

- [x] 新增 `include/cpu/riscv/rv32i_asm_encode.h`（header-only · `constexpr`）
      ⇒ ⇒ **2026-11-14 已实施**（`NOP` / `enc_{r,i,s,b,j,u}` / 17 个便捷别名）
- [x] 既有 **3 处副本**归一为 **1 份**（`enc_r` / `enc_i` 等定义**唯一**）
      ⇒ ⇒ **2026-11-14 已实施**：测试的本地副本**删除** ⇒ ⇒ 改为 `using riscv::…` 引用库
- [x] ⚠️ **`FirmwareEncoderCrossCheck` MUST 保留**（手算常量 vs helper）⇒ ⇒ **禁**顺手删
      ⇒ ⇒ **2026-11-14 复核：仍在**（`test_rv32i_soc_dual_parity.cpp` 的
      `Rv32iSoc - FirmwareEncoderCrossCheck` ⇒ ⇒ 校验的是**库** `riscv::enc_*`）
- [x] 新头文件有**仓内消费者**（既有测试 + 新工具）⇒ ⇒ 不违反 ZERO-DEBT
      ⇒ ⇒ **2026-11-14 已满足**：`rv32i_asm_encode.h` ← 测试 + `tools/rv32i-fw-dump`；
      ⇒ ⇒ `rv32i_mem_map.h` ← SoC 顶层 + `address_decoder.h` + `if_stage.h` + 转储器工具

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -R 'rv32i|firmware|encoder'
bash scripts/verify_header_self_contained.sh
```

## S4 — L4：转储器工具

- [x] `tools/rv32i-fw-dump` 可独立运行 · 三种输出（`vh` / `mem` / `manifest`）
      ⇒ ⇒ **2026-11-14 已实施**（`tools/rv32i-fw-dump/main.cpp` · 退出码 `0`/`2`/`3` fail-closed）
- [x] `manifest` MUST 携带**基址** + **入口** + 「字索引 → 地址」映射
      ⇒ ⇒ **2026-11-14 已实施**（schema `rv32i-fw-manifest-v1` · 逐字给**字节地址**）
- [x] ⚠️ `tools/` 下的二进制 **MUST NOT 链 `examples/**`**（独立自测载体）
      ⇒ ⇒ **2026-11-14 已实施** + **有护栏**（`scripts/verify_rv32i_fw_dump.sh` 的 **D6**，`ldd` 实测）

```bash
cd /workspace/project/CppHDL
cmake --build build --target rv32i-fw-dump -j$(nproc)
bash scripts/verify_example_test_wiring.sh
```

## S5 — L5：ELF 装载

- [x] 解析 ELF32 header + `PT_LOAD` ⇒ ⇒ `p_paddr` / `p_filesz` / `p_memsz`
      ⇒ ⇒ **2026-11-14 已实施**（校验 EI_CLASS/EI_DATA/e_machine=243/ET_EXEC）
- [x] `.bss`（`memsz > filesz`）**补零** ⇒ ⇒ 旧内容不留
      ⇒ ⇒ **2026-11-14 已实施** + 判据 = 护栏 **D3**（19 字 = 3 代码 + 16 零）
- [x] ⚠️ **非零重定位 ⇒ 报错**（不是静默忽略）⇒ ⇒ **改判为「已知边界 · 不做」**
      ⇒ ⇒ **2026-11-14 复核**：本工具**不解析符号表** ⇒ ⇒ `.rela` 在 section 里
      ⇒ ⇒ ⇒ ⇒ **program header 看不到** ⇒ ⇒ **静态检查不完整**（不是「漏做」而是「做不到」）
      ⇒ ⇒ 已登记于 `design.md` §5 的「已知边界」⇒ ⇒ 真要闭环 = **独立立项**（解析 section）
- [x] ⚠️ **不解析符号表** ⇒ ⇒ 符号留给未来工具
      ⇒ ⇒ **2026-11-14 已实施**（**不**读 section header）⇒ ⇒ ⚠️ 由此带来的**边界**已登记：
      「非零重定位」的静态检查**不完整**（`.rela` 在 section 里 ⇒ program header 看不到）
      ⇒ ⇒ 落点 = 本 change 的「已知边界」+ 未来工具

```bash
cd /workspace/project/CppHDL
cmake --build build --target rv32i-fw-dump -j$(nproc)
ctest --test-dir build --output-on-failure -R 'firmware|elf'
```

## S6 — L6：装载可观测性 + 收口

- [x] 装载后**走真实读通路**读回 ⇒ ⇒ 断言「装进去的值 == 期望」
  ⇒ ⇒ ⚠️ **不是**「loader 返回成功」
  ⇒ ⇒ **2026-11-14 已实施**：`Rv32iSoc - FirmwareInitDataLoadedFromConstruction`
  （`examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`）⇒ ⇒ **零 `prog_*` 写**
  ⇒ ⇒ 断言来自 **WB 写回**（`dbg_wb_rd_addr` / `dbg_wb_write_data`）而不是直读内存模型
  ⇒ ⇒ 双后端逐拍全等（interpreter + verilator ⇒ verilator 侧 = 真 Verilog 的 `initial` 生效）
- [x] 一个带 `.data` 的固件可装并读回 ⇒ ⇒ **D-TCM 加载通路非零覆盖**
  ⇒ ⇒ **2026-11-14 已实施**：`x1 = 0x5A`（`lw x1, 0x100(x0)` ⇒ 来自 `.data`）· `x2 = 0x5B`
  ⇒ ⇒ **变异自证**：把 `dtcm_init_` 换成 `itcm_init_`（丢掉 `.data`）⇒ ⇒ 重建后 **红**
  （`x1 = 0` ≠ `0x5A`）⇒ ⇒ 还原 + **重建**后复绿 ✅
- [x] 计数锚点同步（用 `scripts/update_test_count_anchors.sh` ⇒ ⇒ **禁**手写数字）
      ⇒ ⇒ **2026-11-14**：`CTEST_TOTAL` 263 → **264**（+1 = `test_hazard_unit_stall`）
- [x] 全护栏复跑 + 验收报告落 `.rddf/reviews/`
      ⇒ ⇒ **2026-11-14 已完成**：`.rddf/reviews/memory-timing-contract-t1-t2-verify-2026-11.md`
      ⇒ ⇒ + `.rddf/reviews/firmware-load-chain-l2-a-verify-2026-11.md`（L2 路线 A / L6）

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
bash scripts/verify_openspec_delta.sh
```

## Out of Scope（**本 change 不做**）

- ❌ **运行期装载口**（不重发射即可灌固件）⇒ ⇒ 原 L2 的「统一加载端口」**已移出**
  （2026-11-14 用户裁决）⇒ ⇒ 它本质是「**可装载性接口**」（真机 bring-up / 调试口）
  ⇒ ⇒ 独立立项，且 MUST 先解决「D-TCM 不能加第二个 `ch_mem` 写端口」（Verilog **MULTIDRIVEN**）
  与「D-TCM 的 `addr` 被读路径共用 ⇒ 装载期流水线 MUST 在复位」两条约束

- ❌ 符号表解析 / 重定位实现（**非零重定位报错**即可）
- ❌ MMIO 设备接入（**只留位**）
- ❌ CSR / ECALL / FENCE —— 属 `rv32i-isa-gap-closure`
- ❌ cache 接入 —— 独立立项
