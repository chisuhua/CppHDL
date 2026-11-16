# Tasks — csr-mcause-write-2026-11

- [x] 1 `ExceptionUnit`：CSR 写改**跨两拍**（拍 1 写 mepc + 锁存 cause · 拍 2 写 mcause）
- [x] 2 新增**复合 DUT**（`ExceptionUnit` → `CsrBank`）⇒ ⇒ 判据落在 `CsrBank` 的**读回**值
- [x] 3 既有 case 6 的写口断言**改观察点** + 写明理由（旧断言是缺陷的产物 ⇒ 零判别力）
- [x] 4 新增「`exception_valid` 是电平」的契约用例（持续拉高 ⇒ 值仍正确 ⇒ 无数据丢失）
- [x] 5 **变异自证**：还原旧形态 ⇒ ⇒ 复合 DUT 用例转红 ⇒ ⇒ 还原 + **重建** ⇒ 复绿
- [x] 6 spec delta（新 capability `rv32i-privilege` 的种子 Requirement）
- [x] 7 文档：`docs/CHANGELOG.md` · `include/cpu/AGENTS.md`（陷阱入口契约）·
      improvement `rv32i-isa-gap-closure-2026-11`（G5 闭口）· 路线图 §7
- [x] 8 验收：`ctest -LE perf` 全绿 · 全部 `scripts/verify_*.sh` PASS · `openspec validate --specs` 绿
