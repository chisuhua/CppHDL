# Tasks: RV32I 字节道写语义（`sb` / `sh`）

## Phase 1: 实现

- [x] T1.1 `DataTCM` 新增 `strbe` 输入端口 + 写路径改读-改-写
      （`aread` → `byte_lane_mask` → `byte_lane_apply` → `write`）
      —— `include/cpu/pipeline/rv32i_tcm.h`
- [x] T1.2 流水线侧 lane 对齐：`data_write_data <<= shl<32>(exmem_rs2_data, lane_shift)`，
      `lane_shift = 8*addr[1:0]` 且**只对 SB/SH** —— `include/cpu/pipeline/rv32i_pipeline.h`
- [x] T1.3 SoC 接线：`dtcm.io().strbe <<= pipeline.io().data_strbe;`
      —— `examples/riscv-mini/src/rv32i_pipeline_top.h`
- [x] T1.4 全部既有 `DataTCM` 实例化点显式驱动 `strbe`
      （`test_pipeline_exec.cpp` ×3、`test_riscv_tests_pipeline.cpp` ×1）

## Phase 2: 测试

- [x] T2.1 固件尾部追加 `sb`（lane 1）/ `sh`（lane 2-3）+ 两条 `lw` 读回整字
      （**尾部追加 ⇒ 既有 word 索引一个都不移动**）
- [x] T2.2 编码器自校验：`SB(2,0,0x101) == 0x102000A3`、`SH(3,0,0x122) == 0x12301123`
- [x] T2.3 golden：store 拍的写数据 = `0x300` / `0xD0000`；读回整字 = `0x030A` / `0xD0000`
- [x] T2.4 反向断言：未对齐的写数据（`3` / `13`）MUST NOT 出现
- [x] T2.5 store 拍计数 3 → 5
- [x] T2.6 写回序列 11 → 13 条（追加 `{12, 0x030A}`、`{13, 0xD0000}`）

## Phase 3: 验收

- [x] T3.1 `ctest -LE perf` 零回归（191/191）
- [x] T3.2 `scripts/verify_test_counts.sh` 绿（测试源数**未变** ⇒ 计数不变）
- [x] T3.3 `openspec validate --strict` 通过
- [x] T3.4 变异验证：去掉 lane 对齐 ⇒ store 拍数据变 `3`/`13` ⇒ 断言转红；
      去掉 `strbe` 接线 ⇒ 整字覆盖 ⇒ 读回值转红
- [ ] T3.5 **对抗审稿**（critic）+ `openspec archive`
      ⇒ 本 change 在归档前 MUST 经对抗审稿（本仓惯例）

## Out of Scope

- 不改 `ch_mem`（不引入字节使能写 API）
- 不做非对齐 `sw` / `sh` 的异常处理
- 不把 SoC 的 dmem 换成 AXI 互连
- 不改 ADR-046 **D10** 的 SoC 32 端口契约（本 change 不新增顶层端口）
