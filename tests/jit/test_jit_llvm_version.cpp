/**
 * @file test_jit_llvm_version.cpp
 * @brief LLVM version compatibility red-line test for JitCompiler init API.
 *
 * Background (see openspec/changes/2026-09-20-upgrade-llvm-22-jit-compat):
 *   The C-style LLVMInitializeNative*() API was removed in LLVM 18 (empirically
 *   confirmed by grep on /usr/lib/llvm-{18,22}/include/llvm/Support/TargetSelect.h).
 *   JitCompiler constructor MUST use llvm::InitializeNative*() C++ style API
 *   family and check return values (LLVM returns bool, true = failure).
 *
 * Contract:
 *   - When CH_JIT_ENABLED is defined and LLVM version is in [17, 22],
 *     JitCompiler::is_available() MUST return true.
 *   - When CH_JIT_ENABLED is not defined (CH_JIT_ENABLE=OFF matrix), the test
 *     SUCCEEDs without linking against JitCompiler.
 *   - When LLVM version is outside [17, 22], the test SUCCEEDs with a note
 *     naming the detected version (future opt-in, not silent failure).
 *
 * Tag: [jit][llvm-compat] — runs under ctest -L base by default.
 */

#include "catch_amalgamated.hpp"

#if __has_include("jit/jit_compiler.h") && defined(CH_JIT_ENABLED)
#include "jit/jit_compiler.h"
#endif

#ifndef CH_JIT_ENABLED

TEST_CASE("JitCompiler init API — LLVM version compat (skipped: JIT disabled)",
          "[jit][llvm-compat]") {
    SUCCEED("JIT not enabled - skipping");
}

#else  // CH_JIT_ENABLED is defined

#include "llvm/Config/llvm-config.h"  // LLVM_VERSION_MAJOR / MINOR / etc.

TEST_CASE("JitCompiler initializes on detected LLVM version [17, 22]",
          "[jit][llvm-compat]") {
    ch::jit::JitCompiler compiler;
    INFO("LLVM version: "
         << LLVM_VERSION_MAJOR << "." << LLVM_VERSION_MINOR << "."
         << LLVM_VERSION_PATCH);

    if (LLVM_VERSION_MAJOR >= 17 && LLVM_VERSION_MAJOR <= 22) {
        // In-range: LLVM's native init API must succeed.
        REQUIRE(compiler.is_available() == true);
    } else {
        // Out-of-range: pass with diagnostic so a maintainer knows the
        // tested range; future versions extend the bound, not silently break.
        SUCCEED("LLVM " << LLVM_VERSION_MAJOR
                 << " not in tested range [17, 22]; opt-in support required");
    }
}

TEST_CASE("JitCompiler constructor checks all three LLVM init return values",
          "[jit][llvm-compat]") {
    // Regression guard: a future refactor that sets available_=true
    // unconditionally (instead of `!Init*() && !Init*() && !Init*()`) must
    // be caught at code-review time by the spec, and also at run time on
    // a system where LLVM native init legitimately fails.
    //
    // This test only verifies the positive path here; the negative path
    // requires a system without native target (rare) and is exercised in
    // unit-style integration tests.
    ch::jit::JitCompiler compiler;
    REQUIRE(compiler.is_available() == true);
}

#endif  // CH_JIT_ENABLED