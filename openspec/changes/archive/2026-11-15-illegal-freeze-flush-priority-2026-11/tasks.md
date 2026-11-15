# Tasks — illegal-freeze-flush-priority-2026-11

- [x] `if_stage` 的 PC 更新门补 `flush_redirect = flush & branch_valid` 优先级
- [x] `if_stage` 的旧互斥论证注释（「flush 与 stall 不会同拍」= 事实错误）重写
- [x] 回归用例（双后端 · 期望值手推）：`Rv32iSoc - flush wins over illegal-freeze stall`
- [x] **变异自证**：反转 `flush_redirect` ⇒ `REQUIRE(saw_x5)` 转红 ⇒ 还原后重建复绿
- [x] ADR-071 修订：D3-b（优先级）/ D3-c（缺陷史）/ D1（假设登记）/ §3-D / §4.3① / §5 V8–V10
- [x] 验收：`ctest -LE perf` 254/254 · 独立 Verilog TB `checks=24 fails=0`
