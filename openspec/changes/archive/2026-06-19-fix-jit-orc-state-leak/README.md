# fix-jit-orc-state-leak

> **⚠️ SUPERSEDED by `fix-perf-subprocess-isolation` (2026-06-19)**
>
> **Reason for supersession**: Phase 1 实施调查发现 F3 设计前提错误。
> `src/jit/jit_compiler.cpp:179` 的 `clear()` 已正确 `delete static_cast<LLJIT*>(jit_session_)`,
> `LLJIT` 析构自动清理 ExecutionSession + JITDylib。ORC 资源**已正确释放**。
>
> K1 真实存在(standalone TC-08 jit=4.5-6k us vs polluted=641-711k us,~142× 慢),
> 但**真因不是 ORC 资源未释放**。Phase 1 调查无法在不运行 valgrind/strace/git bisect
> 的情况下定位真因(可能候选:LLVM 22 JITLink 段累积、LLVM 类型 uniquing 表膨胀、
> mmap 累积、`LLVMInitialize*` 副作用、interpreter sequential eval 累积)。
>
> 决策:不再追 F3(治根需 runtime profiling,超出当前对话能力)。
> 转 F2(子进程隔离,治标不治本,但保证 perf_regression 阈值可恢复 1.6×)。
>
> 保留此 change 文件作为 K1 调查记录与 F2 设计前的过程参考。
> 正式实施见 [`../fix-perf-subprocess-isolation/`](../fix-perf-subprocess-isolation/)。

Root-cause fix for K1 ORC JIT cross-DUT state pollution. Clean up LLVM ORC ExecutionSession and JITDylib in jit_compiler destructor and reset jit_compiler_ singleton in Simulator destructor. Eliminates the 2-3x performance inflation that perf_main currently sees when running TC-07 -> TC-08 -> TC-09 sequentially in one process. After this fix, perf_regression JIT threshold can be restored from 4.0x to 1.6x.
