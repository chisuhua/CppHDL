/**
 * @file test_verilator_integration.cpp
 * @brief Integration test for the Verilator submodule build (W11 followup).
 *
 * Per perf-report-followup spec v4 (commit 4f9eb16), this test verifies
 * that the Verilator build wired in via ExternalProject_Add actually
 * produces a working binary + wrapper. It invokes VerilatorRunner
 * directly (no subprocess) so it is fast (~1s) and deterministic.
 *
 * Tagged [perf][verilator][integration] — runs under ctest -L perf.
 *
 * Behavior:
 *   - If CPPHDL_VERILATOR_WRAPPER is empty (BUILD_VERILATOR=OFF) or
 *     the wrapper is not actually executable, SKIP the test. This
 *     is the correct behavior on a developer machine that opted out.
 *   - If the wrapper is present and runnable, run a tiny build of a
 *     trivial Verilog module and assert success. This catches the
 *     'I broke the verilator path' regression class.
 */

#include "catch_amalgamated.hpp"
#include "verilator_runner.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

// CATCH_CONFIG_RUNNER (not CATCH_CONFIG_MAIN) so we can detect the
// "wrapper not built yet" case and exit 0. Otherwise Catch2 returns
// exit code 4 ("all tests skipped") which ctest treats as failure.
// Pattern matches tests/test_verilog_external.cpp (ADR-035 / Phase 1.6).
#define CATCH_CONFIG_RUNNER

namespace {

// CPPHDL_VERILATOR_WRAPPER is injected at compile time by CMake via
// target_compile_definitions in tests/CMakeLists.txt.
#ifndef CPPHDL_VERILATOR_WRAPPER
#define CPPHDL_VERILATOR_WRAPPER ""
#endif

#ifndef CPPHDL_VERILATOR_BIN
#define CPPHDL_VERILATOR_BIN ""
#endif

#ifndef CPPHDL_VERILATOR_DIR
#define CPPHDL_VERILATOR_DIR ""
#endif

std::string get_wrapper_path() {
    const char* env = std::getenv("CPPHDL_VERILATOR_WRAPPER");
    if (env && *env) return env;
#ifdef CPPHDL_VERILATOR_WRAPPER
    return CPPHDL_VERILATOR_WRAPPER;
#else
    return "";
#endif
}

bool file_is_executable(const std::string& path) {
    if (path.empty()) return false;
    if (!std::filesystem::exists(path)) return false;
    auto perms = std::filesystem::status(path).permissions();
    return (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none;
}

} // namespace

TEST_CASE("verilator integration: is_available() returns true when wrapper is built",
          "[perf][verilator][integration]") {
    std::string wrapper = get_wrapper_path();
    if (wrapper.empty() || !file_is_executable(wrapper)) {
        SKIP("CPPHDL_VERILATOR_WRAPPER not set or not executable. "
             "Either BUILD_VERILATOR=OFF or the Verilator build has not been "
             "run yet (cmake --build build --target verilator).");
    }
    ch_perf::VerilatorRunner vr("/tmp/cppcphdl_verilator_test_cache", wrapper);
    INFO("wrapper: " << wrapper);
    REQUIRE(vr.is_available());
}

TEST_CASE("verilator integration: wrapper can be invoked for --version",
          "[perf][verilator][integration]") {
    std::string wrapper = get_wrapper_path();
    if (wrapper.empty() || !file_is_executable(wrapper)) {
        SKIP("verilator not available; see preceding test");
    }
    // The wrapper itself returns the version string when invoked as
    // 'wrapper --version' (it locates and execs verilator_bin --version).
    // We can verify the wrapper is callable without going through the
    // VerilatorRunner helper to avoid coupling to its assumptions.
    std::string cmd = wrapper + " --version 2>&1";
    FILE* p = ::popen(cmd.c_str(), "r");
    if (!p) FAIL("popen failed");
    char buf[256];
    std::string out;
    if (fgets(buf, sizeof(buf), p)) out = buf;
    int rc = ::pclose(p);
    INFO("output: " << out);
    REQUIRE(rc == 0);
    // Sanity: version string should look like " rev v5.048" or similar
    // (verilator v5.048 prints " rev vX.YYY"; we accept any "v" + digits
    // pattern as proof the wrapper actually exec'd verilator_bin).
    REQUIRE(out.find(" v") != std::string::npos);  // leading space + 'v'
    // More specific: ensure it looks like a version tag
    REQUIRE(out.find_first_of("0123456789") != std::string::npos);
}

// Per code review C8: the file's header comment promised a "tiny build
// of a trivial Verilog module and assert success" but the two
// TEST_CASEs above only check is_available() and --version. A malicious
// or broken wrapper that prints "v666 fake" and exits 0 would pass
// both. This third TEST_CASE actually exercises the build path
// end-to-end so the "I broke the verilator path" regression class is
// genuinely caught. SKIPs when wrapper is unavailable (same as the
// other two). Build() is the slow path (~1-30s on cold cache).
TEST_CASE("verilator integration: build a trivial and_gate from scratch",
          "[perf][verilator][integration]") {
    std::string wrapper = get_wrapper_path();
    if (wrapper.empty() || !file_is_executable(wrapper)) {
        SKIP("verilator not available; see preceding tests");
    }

    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() /
        "cpphdl_verilator_integration_test";
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);
    std::filesystem::create_directories(tmp, ec);
    REQUIRE(!ec);

    const std::string verilog_path = (tmp / "and_gate.v").string();
    const std::string harness_path = (tmp / "harness.cpp").string();

    {
        std::ofstream f(verilog_path);
        // Module name MUST be `top`: VerilatorRunner hardcodes the
        // expected binary path to <cache>/obj_dir/Vtop (runner.h:217)
        // and the input filenames to top.v + harness.cpp (runner.h:255).
        f << "module top(\n"
             "    input a,\n"
             "    input b,\n"
             "    output y\n"
             ");\n"
             "    assign y = a & b;\n"
             "endmodule\n";
        REQUIRE(f.good());
    }
    {
        std::ofstream f(harness_path);
        f << "#include \"Vtop.h\"\n"
             "#include <cstdlib>\n"
             "int main(int argc, char** argv) {\n"
             "    Vtop* dut = new Vtop;\n"
             "    int ticks = (argc > 1) ? std::atoi(argv[1]) : 1;\n"
             "    for (int i = 0; i < ticks; ++i) dut->eval();\n"
             "    delete dut;\n"
             "    return 0;\n"
             "}\n";
        REQUIRE(f.good());
    }

    const std::string cache_root =
        (std::filesystem::temp_directory_path() /
         "cpphdl_verilator_integration_cache").string();
    ch_perf::VerilatorRunner vr(cache_root, wrapper);

    auto result = vr.build("and_gate_test", verilog_path, harness_path);
    INFO("build error: " << result.error_msg);
    INFO("binary path: " << result.binary_path);
    REQUIRE(result.success);
    REQUIRE(!result.binary_path.empty());
    REQUIRE(std::filesystem::exists(result.binary_path));
}

int main(int argc, char* argv[]) {
    std::string wrapper = get_wrapper_path();
    if (wrapper.empty() || !file_is_executable(wrapper)) {
        std::fprintf(stderr,
                     "test_verilator_integration: CPPHDL_VERILATOR_WRAPPER not "
                     "set or wrapper not executable; skipping.\n");
        return 0;
    }
    Catch::Session session;
    session.applyCommandLine(argc, argv);
    return session.run();
}
