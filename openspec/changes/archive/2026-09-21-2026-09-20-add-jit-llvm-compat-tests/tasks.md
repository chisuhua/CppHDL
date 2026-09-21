## 1. 预实现调查 — 实证 LLVM API 状态 ✅

- [x] 1.1 `ls /usr/lib/llvm-{17,18,22}/include/llvm/Support/TargetSelect.h` — 确认 LLVM 版本头文件存在
- [x] 1.2 `grep "LLVMInitializeNative" /usr/lib/llvm-22/include/llvm-c/Target.h` — 确认 C 风格 static inline 函数在 `llvm-c/` 子目录仍可用
- [x] 1.3 `grep "InitializeNativeTarget" /usr/lib/llvm-{18,22}/include/llvm/Support/TargetSelect.h` — 确认 C++ 风格 API 在 LLVM 22 完整,在 LLVM 18 缺失
- [x] 1.4 `grep "LLVM_VERSION_MAJOR\|LLVM_VERSION_MINOR" /usr/lib/llvm-22/include/llvm/Config/llvm-config.h` — 确认 `LLVM_VERSION_*` 宏在 LLVM 头中定义,新测试可使用
- [x] 1.5 跑 `ctest --test-dir build -R "^perf_regression$"` 确认当前 perf_regression 通过(green baseline)
- [x] 1.6 跑 `ctest --test-dir build -L base -R "test_jit"` 确认所有 JIT 测试当前通过(green baseline for new tests)

## 2. RED → GREEN — 新测试已存在,验证 GREEN ✅

- [x] 2.1 验证 `tests/jit/test_jit_llvm_version.cpp` 存在
- [x] 2.2 验证 `tests/jit/test_jit_init_succeeds.cpp` 存在
- [x] 2.3 验证 `tests/CMakeLists.txt` 已添加两行 `add_catch_test` 扁平注册
- [x] 2.4 跑 `cmake --build build --target test_jit_llvm_version test_jit_init_succeeds -j$(nproc)` 0 errors 0 warnings
- [x] 2.5 跑 `ctest --test-dir build -R "test_jit_llvm_version|test_jit_init_succeeds" --output-on-failure` **LLVM 22 下 GREEN**

## 3. 验证 — 全量回归 ✅

- [x] 3.1 `cmake --build build -j$(nproc)` 0 errors 0 warnings
- [x] 3.2 `ctest --test-dir build -L base --output-on-failure` 114 测试通过(原 112 + 新 2)
- [x] 3.3 `ctest --test-dir build -R "perf_regression|test_jit_compiler|test_jit_llvm_version|test_jit_init_succeeds|perf_jit_vs_interp_ratio" --output-on-failure` 5/5 通过
- [x] 3.4 `ctest --test-dir build -L spinalhdl-ported` 25/25 通过
- [x] 3.5 CH_JIT_ENABLE=OFF 双配置验证:
  - `cmake -B build-off -DCH_JIT_ENABLE=OFF`
  - `cmake --build build-off -j$(nproc)` 0 errors 0 warnings
  - `ctest --test-dir build-off -L base --output-on-failure` 全部 SUCCEED-skip(不挂)

## 4. 文档同步 ✅

- [x] 4.1 `docs/adr/ADR-027-llvm-version-detection.md` 追加 v2.0 "LLVM 版本兼容性 red-line 测试 (2026-09-20)" 小节:
  - 记录 2026-09-20 调查结论:`LLVMInitializeNative*` 在 LLVM 22 仍作为 `static inline` 在 `llvm-c/Target.h` 行 131-145 可用
  - 记录新测试 `test_jit_llvm_version` 与 `test_jit_init_succeeds` 的目的与覆盖范围 [17, 22]
  - 记录版本兼容矩阵(per design.md §1.2 表)

## 5. 验证与归档 ✅

- [x] 5.1 `openspec validate 2026-09-20-add-jit-llvm-compat-tests --strict` — Change is valid
- [x] 5.2 `openspec validate --specs --strict` — 3 passed (main specs 未被破坏)
- [x] 5.3 `openspec change show 2026-09-20-add-jit-llvm-compat-tests` — 制品齐全
- [x] 5.4 `openspec archive 2026-09-20-add-jit-llvm-compat-tests` — 完成归档

## 6. 应急回滚 ✅ (不需要,正常完成)

- [x] 6.1 `git revert <commit-hash>` — 不需要,正常完成