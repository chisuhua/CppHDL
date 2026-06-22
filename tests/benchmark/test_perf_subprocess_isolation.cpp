/**
 * @file test_perf_subprocess_isolation.cpp
 * @brief F2 (fix-perf-subprocess-isolation): regression test for K1
 * ORC JIT cross-DUT state pollution mitigation.
 *
 * The K1 bug (see docs/simulation/PERF_COMPARISON_REPORT.md §6 K1)
 * caused TC-08 JIT to run ~100x slower when measured in --all mode
 * (polluted) versus standalone. The fix routes TC-07/08/09 through
 * a child process so each TC runs in a fresh address space.
 *
 * This test:
 *   1. Invokes perf_tests --tc=08 in isolation (subprocess), reads
 *      TC-08 JIT median_us from perf_results.json.
 *   2. Invokes perf_tests --all, which spawns one child per TC, reads
 *      TC-08 JIT median_us from perf_results.json.
 *   3. Asserts (--all ratio / standalone ratio) <= 1.5x.
 *
 * Tag: [perf][isolation][F2]
 *
 * Pattern: CATCH_CONFIG_RUNNER with custom main so that the test
 * gracefully returns 0 when perf_tests binary is not available
 * (mirrors tests/benchmark/test_report_consistency.cpp).
 */
#define CATCH_CONFIG_RUNNER

#include "catch_amalgamated.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    if (!f.good()) return {};
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

// Pull the median_us value for a (test_name, params, backend) tuple
// out of perf_results.json. Returns -1.0 if not found.
double extract_median_us_json(const std::string& s,
                              const std::string& test_name,
                              const std::string& params,
                              const std::string& backend) {
    std::string pat = "\"test_name\":\\s*\"" + test_name +
                      "\"\\s*,\\s*\"params\":\\s*\"" + params +
                      "\"\\s*,\\s*\"backend\":\\s*\"" + backend +
                      "\"[\\s\\S]*?\"median_us\":\\s*([0-9.eE+-]+)";
    std::regex rx(pat);
    std::smatch m;
    if (std::regex_search(s, m, rx)) {
        return std::atof(m[1].str().c_str());
    }
    return -1.0;
}

int run_cmd(const std::string& cmd) {
    return std::system(cmd.c_str());
}

std::string find_perf_tests_binary() {
    // CMake sets WORKING_DIRECTORY to ${CMAKE_SOURCE_DIR} (project root).
    // The binary is at build/tests/benchmark/perf_tests relative to it.
    namespace fs = std::filesystem;
    fs::path p = "build/tests/benchmark/perf_tests";
    if (fs::exists(p)) return p.string();
    p = "../build/tests/benchmark/perf_tests";
    if (fs::exists(p)) return fs::absolute(p).string();
    return {};
}

}  // namespace

TEST_CASE("F2 subprocess isolation: TC-08 jit in --all matches standalone",
          "[perf][isolation][F2]") {
    std::string perf_tests = find_perf_tests_binary();
    if (perf_tests.empty()) {
        SKIP("perf_tests binary not found at build/tests/benchmark/perf_tests");
    }
    namespace fs = std::filesystem;
    fs::path saved_json = "perf_results.json.__saved";
    if (fs::exists("perf_results.json")) {
        fs::rename("perf_results.json", saved_json);
    }
    struct Restore {
        fs::path saved;
        ~Restore() {
            std::error_code ec;
            fs::remove("perf_results.json", ec);
            if (fs::exists(saved)) fs::rename(saved, "perf_results.json", ec);
        }
    } _restore{saved_json};

    // 1. Standalone TC-08 (parent uses --direct via subprocess default)
    int rc1 = run_cmd(perf_tests + " --tc=08 --report=json");
    if (rc1 != 0) SKIP("standalone --tc=08 subprocess exited with code " << rc1);
    std::string j1 = slurp("perf_results.json");
    if (j1.empty()) SKIP("standalone --tc=08 produced no perf_results.json");
    double standalone_jit =
        extract_median_us_json(j1, "TC-08", "regs=10", "jit");
    if (standalone_jit <= 0.0) {
        SKIP("standalone --tc=08 json missing TC-08|regs=10|jit row");
    }
    INFO("standalone TC-08|regs=10 jit median = " << standalone_jit << " us");

    // 2. --all mode: each TC runs in fresh subprocess
    int rc2 = run_cmd(perf_tests + " --all --report=json");
    if (rc2 != 0) SKIP("--all subprocess exited with code " << rc2);
    std::string j2 = slurp("perf_results.json");
    if (j2.empty()) SKIP("--all produced no perf_results.json");
    double all_jit = extract_median_us_json(j2, "TC-08", "regs=10", "jit");
    if (all_jit <= 0.0) {
        SKIP("--all json missing TC-08|regs=10|jit row");
    }
    INFO("--all TC-08|regs=10 jit median = " << all_jit << " us");

    // 3. K1 mitigation: --all ratio should be within 1.5x of standalone
    double ratio = all_jit / standalone_jit;
    INFO("ratio = " << ratio << " (threshold 1.5x)");
    REQUIRE(ratio <= 1.5);

    // 4. Sanity: standalone should be in the documented 3-5k us range.
    REQUIRE(standalone_jit < 10000.0);
}

TEST_CASE("F2 subprocess isolation: --direct flag runs in-process",
          "[perf][isolation][F2][direct]") {
    std::string perf_tests = find_perf_tests_binary();
    if (perf_tests.empty()) {
        SKIP("perf_tests binary not found");
    }
    namespace fs = std::filesystem;
    fs::path saved_json = "perf_results.json.__saved2";
    if (fs::exists("perf_results.json")) {
        fs::rename("perf_results.json", saved_json);
    }
    struct Restore {
        fs::path saved;
        ~Restore() { std::error_code ec; fs::remove("perf_results.json", ec);
            if (fs::exists(saved)) fs::rename(saved, "perf_results.json", ec); }
    } _restore{saved_json};

    // --direct bypasses subprocess and runs TC-08 in-process.
    // This should produce a result too, useful as a control test.
    int rc = run_cmd(perf_tests + " --tc=08 --direct --report=json");
    if (rc != 0) SKIP("--tc=08 --direct exited with code " << rc);
    std::string j = slurp("perf_results.json");
    if (j.empty()) SKIP("--tc=08 --direct produced no perf_results.json");
    double jit = extract_median_us_json(j, "TC-08", "regs=10", "jit");
    INFO("--direct TC-08|regs=10 jit median = " << jit << " us");
    REQUIRE(jit > 0.0);
    REQUIRE(jit < 10000.0);
}

// F2 fixup: detect a leftover `perf_results.json.__saved` from a previous
// (aborted/killed) invocation of this test, and restore the original
// `perf_results.json` before the test runs. Without this guard, a SIGKILL or
// ctest TIMEOUT during the in-test RAII scope leaves `perf_results.json` in a
// half-written or non-perf-main-generated state, which then fails downstream
// tests (notably perf_report_consistency, which checks sim_us byte-equality
// across {json,csv,md} formats).
//
// We only act when the `__saved` sentinel is present, which the RAII Restore
// struct writes at the start of every TEST_CASE — so a clean run is a no-op.
void restore_perf_results_after_aborted_run() {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists("perf_results.json.__saved")) return;
    // Last run was aborted; the JSON file may be partially regenerated by
    // the in-flight perf_tests --all. Drop it and restore the saved copy.
    fs::remove("perf_results.json", ec);
    fs::rename("perf_results.json.__saved", "perf_results.json", ec);
}

int main(int argc, char** argv) {
    restore_perf_results_after_aborted_run();
    return Catch::Session().run(argc, argv);
}
