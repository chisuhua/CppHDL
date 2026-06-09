/**
 * @file test_report_consistency.cpp
 * @brief Regression test for W7: md/csv/json reports are byte-equal per row.
 *
 * Per perf-report-followup.md W7, all three output formats must report
 * the same numeric value for the same BenchmarkResult row. We verify
 * that for TC-07 depth=10 interpreter, the sim_us value matches across
 * all three files.
 *
 * Tag: [perf][report][consistency]
 */

#include "catch_amalgamated.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    if (!f.good()) return {};
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

// Pull the sim_us value for TC-07 depth=10 interpreter out of each format.
// Returns -1.0 if not found.
double extract_sim_us_json(const std::string& s) {
    // Look for the row block (objects in "runs" array).
    // For simplicity, find test_name then locate "sim_us" within braces.
    std::regex rx(R"("test_name":\s*"TC-07"\s*,\s*"params":\s*"depth=10"\s*,\s*"backend":\s*"interpreter"[\s\S]*?"sim_us":\s*([0-9.eE+-]+))");
    std::smatch m;
    if (std::regex_search(s, m, rx)) {
        return std::atof(m[1].str().c_str());
    }
    return -1.0;
}

double extract_sim_us_csv(const std::string& s) {
    std::regex rx(R"(^TC-07,depth=10,interpreter,([^,]+),([^,]+),)",
                  std::regex::ECMAScript | std::regex::multiline);
    std::smatch m;
    if (std::regex_search(s, m, rx)) {
        // column 4 (0-based) is sim_us
        return std::atof(m[2].str().c_str());
    }
    return -1.0;
}

double extract_sim_us_md(const std::string& s) {
    // | TC-07 | depth=10 | interpreter | <build> | <sim> | ...
    std::regex rx(R"(\|\s*TC-07\s*\|\s*depth=10\s*\|\s*interpreter\s*\|\s*([^|]+)\s*\|\s*([^|]+)\s*\|)");
    std::smatch m;
    if (std::regex_search(s, m, rx)) {
        return std::atof(m[2].str().c_str());
    }
    return -1.0;
}

} // namespace

TEST_CASE("md/csv/json reports agree on sim_us (W7)",
          "[perf][report][consistency]") {
    // All three files must exist (sibling test must have produced them).
    if (!std::filesystem::exists("perf_results.json") ||
        !std::filesystem::exists("perf_results.csv") ||
        !std::filesystem::exists("perf_results.md")) {
        SKIP("perf_results.{json,csv,md} not present; ensure perf_tests ran first");
    }
    std::string j = slurp("perf_results.json");
    std::string c = slurp("perf_results.csv");
    std::string m = slurp("perf_results.md");
    double js = extract_sim_us_json(j);
    double cs = extract_sim_us_csv(c);
    double ms = extract_sim_us_md(m);
    INFO("sim_us JSON=" << js << " CSV=" << cs << " MD=" << ms);
    REQUIRE(js > 0.0);
    REQUIRE(js == cs);
    REQUIRE(js == ms);
}
