/**
 * @file test_jit_init_succeeds.cpp
 * @brief Initialization smoke test for JitCompiler.
 *
 * Background (see openspec/changes/2026-09-20-upgrade-llvm-22-jit-compat):
 *   Catches any future regression in the JitCompiler constructor's LLVM
 *   initialization path (e.g., reverting to C-style LLVMInitializeNative*
 *   that was removed in LLVM 18+). When the constructor succeeds, this
 *   test passes; when it fails (e.g., build-time undeclared-identifier error
 *   on a new LLVM version), the test target fails to compile.
 *
 * Tag: [jit][init] — runs under ctest -L base by default.
 */

#include "catch_amalgamated.hpp"

#if __has_include("jit/jit_compiler.h") && defined(CH_JIT_ENABLED)
#include "jit/jit_compiler.h"
#endif

#ifndef CH_JIT_ENABLED

TEST_CASE("JitCompiler init succeeds (skipped: JIT disabled)",
          "[jit][init]") {
    SUCCEED("JIT not enabled - skipping");
}

#else  // CH_JIT_ENABLED is defined

TEST_CASE("JitCompiler constructs without crash and reports availability",
          "[jit][init]") {
    ch::jit::JitCompiler compiler;

    // is_available() must be consistent with the LLVM-version guard tested
    // in test_jit_llvm_version.cpp; here we just assert non-crash + bool
    // sanity.
    INFO("JitCompiler::is_available() = "
         << (compiler.is_available() ? "true" : "false"));
    CHECK(compiler.is_available() == true);  // CHECK, not REQUIRE: a future
                                            // compiler without native target
                                            // should still compile & report
                                            // false, not crash.
}

#endif  // CH_JIT_ENABLED