/**
 * @file perf_regression.cpp
 * @brief Compare current perf_test output against a baseline JSON.
 *
 * Per perf-report-followup.md W9: a standalone executable that:
 *   1. Reads a baseline JSON (e.g. tests/benchmark/perf_baseline.json).
 *   2. Reads the current perf_results.json.
 *   3. For each matching row, compares median_us.
 *   4. If any current row's median_us exceeds baseline * 1.2, exit 1.
 *   5. Otherwise exit 0.
 *
 * This is the perf regression gate that runs in CI under ctest -L perf.
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct Row {
    std::string test_name;
    std::string params;
    std::string backend;
    double median_us = 0.0;
    double sim_us = 0.0;
};

namespace {

// Minimal flat-array JSON parser for our schema (no external deps).
// We extract the "results" array (W7 changed the key to "runs"; this
// parser handles both). For each object we look for test_name / params
// / backend / median_us / sim_us.
std::vector<Row> parse_json(const std::string& path) {
    std::vector<Row> out;
    std::ifstream in(path);
    if (!in) {
        std::cerr << "cannot open " << path << "\n";
        std::exit(2);
    }
    std::stringstream ss; ss << in.rdbuf();
    std::string content = ss.str();
    // Find the array key
    std::string key;
    auto arr_start = content.find("\"results\":");
    if (arr_start != std::string::npos) {
        key = "\"results\"";
    } else {
        arr_start = content.find("\"runs\":");
        if (arr_start != std::string::npos) key = "\"runs\"";
    }
    if (arr_start == std::string::npos) {
        std::cerr << "no 'results' or 'runs' array in " << path << "\n";
        return out;
    }
    // Walk top-level braces inside the array.
    size_t i = arr_start;
    while (i < content.size() && content[i] != '[') ++i;
    if (i >= content.size()) return out;
    ++i;  // skip '['
    while (i < content.size()) {
        // Find next '{' at depth 0
        while (i < content.size() && content[i] != '{' && content[i] != ']') ++i;
        if (i >= content.size() || content[i] == ']') break;
        size_t obj_start = i;
        int depth = 0;
        while (i < content.size()) {
            if (content[i] == '{') ++depth;
            else if (content[i] == '}') {
                --depth;
                if (depth == 0) { ++i; break; }
            }
            ++i;
        }
        std::string obj = content.substr(obj_start, i - obj_start);
        // Extract a string-valued field (test_name / params / backend).
        auto extract_str = [&](const std::string& field) -> std::string {
            std::string k = "\"" + field + "\":";
            auto p = obj.find(k);
            if (p == std::string::npos) return "";
            p += k.size();
            while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\n' ||
                   obj[p] == '\r' || obj[p] == '\t')) ++p;
            if (p >= obj.size() || obj[p] != '"') return "";
            ++p;  // skip opening quote
            size_t end = p;
            while (end < obj.size() && obj[end] != '"') {
                if (obj[end] == '\\' && end + 1 < obj.size()) ++end;
                ++end;
            }
            return obj.substr(p, end - p);
        };
        // Extract a numeric field (median_us / sim_us).
        auto extract_num = [&](const std::string& field) -> double {
            std::string k = "\"" + field + "\":";
            auto p = obj.find(k);
            if (p == std::string::npos) return 0.0;
            p += k.size();
            while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\n' ||
                   obj[p] == '\r' || obj[p] == '\t')) ++p;
            size_t end = p;
            while (end < obj.size() && obj[end] != ',' && obj[end] != '}' &&
                   obj[end] != '\n' && obj[end] != ' ') ++end;
            return std::atof(obj.substr(p, end - p).c_str());
        };
        Row r;
        r.test_name = extract_str("test_name");
        r.params = extract_str("params");
        r.backend = extract_str("backend");
        r.median_us = extract_num("median_us");
        r.sim_us = extract_num("sim_us");
        out.push_back(r);
    }
    return out;
}

std::string make_key(const Row& r) {
    return r.test_name + "|" + r.params + "|" + r.backend;
}

} // namespace

int main(int argc, char** argv) {
    std::string baseline_path = "tests/benchmark/perf_baseline.json";
    std::string current_path = "perf_results.json";
    if (argc >= 2) baseline_path = argv[1];
    if (argc >= 3) current_path = argv[2];
    auto baseline = parse_json(baseline_path);
    auto current = parse_json(current_path);
    if (baseline.empty() && current.empty()) {
        std::cerr << "error: no rows parsed from either file\n";
        return 2;
    }
    std::map<std::string, Row> bmap;
    for (const auto& r : baseline) bmap[make_key(r)] = r;
    constexpr double THRESHOLD = 1.2;
    int regressions = 0;
    int compared = 0;
    for (const auto& r : current) {
        auto it = bmap.find(make_key(r));
        if (it == bmap.end()) continue;  // new row, no baseline
        double base = it->second.median_us;
        if (base <= 0.0) continue;       // baseline had no measurement
        ++compared;
        double ratio = r.median_us / base;
        if (ratio > THRESHOLD) {
            std::cerr << "REGRESSION: " << make_key(r)
                      << " baseline=" << base
                      << "us current=" << r.median_us
                      << "us ratio=" << ratio << "\n";
            ++regressions;
        }
    }
    std::cout << "Compared " << compared << " rows against baseline ("
              << baseline_path << "); threshold=" << THRESHOLD << "x\n";
    if (regressions > 0) {
        std::cerr << "\n" << regressions
                  << " regression(s) detected.\n";
        return 1;
    }
    std::cout << "No regressions.\n";
    return 0;
}
