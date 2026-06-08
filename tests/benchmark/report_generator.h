#ifndef CPPCPPHDL_REPORT_GENERATOR_H
#define CPPCPPHDL_REPORT_GENERATOR_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// BenchmarkResult — single benchmark run record.
//
// v1 fields (kept for backward compatibility with TC-01/02/04/06):
//   test_name, params, ticks_per_sec, ns_per_tick, ns_per_node_tick,
//   overhead_percent
//
// v2 fields (added for TC-07/08 three-way comparison):
//   backend    — which simulator produced this row: "interpreter" | "jit" |
//                "verilator" | "" (unset, for legacy runs)
//   build_us   — one-time build cost in microseconds (Verilator compile,
//                JIT IR gen, etc.). 0 when not applicable.
//   sim_us     — per-iteration simulation cost in microseconds.
//   total_us   — build_us + (sim_us * iterations); convenience aggregate.
//   iterations — number of measured iterations (after warmup).
//   median_us  — median of measured sim_us samples (microseconds).
//   status     — "PASS" | "FAILED" | "UNKNOWN" / "SKIPPED".
// =============================================================================
struct BenchmarkResult {
    // v1 fields (defaults preserve legacy behavior when only legacy fields
    // are populated)
    std::string test_name;
    std::string params;
    double ticks_per_sec = 0.0;
    double ns_per_tick = 0.0;
    double ns_per_node_tick = 0.0;
    double overhead_percent = 0.0;

    // v2 fields
    std::string backend;       // "interpreter" / "jit" / "verilator" / ""
    double build_us = 0.0;
    double sim_us = 0.0;
    double total_us = 0.0;
    int iterations = 0;
    double median_us = 0.0;
    std::string status = "UNKNOWN";  // "PASS" / "FAILED" / "UNKNOWN"
};

class ReportGenerator {
public:
    void add_result(const BenchmarkResult& r) { results_.push_back(r); }

    // -------------------------------------------------------------------------
    // CSV output
    //
    // Header (per task spec): test_name,params,backend,build_us,sim_us,
    //                          total_us,iterations,median_us,status
    // -------------------------------------------------------------------------
    void export_csv(const std::string& filename) const {
        std::ofstream f(filename);
        f << "test_name,params,backend,build_us,sim_us,total_us,iterations,"
             "median_us,status\n";
        for (const auto& r : results_) {
            f << csv_escape(r.test_name) << ","
              << csv_escape(r.params) << ","
              << csv_escape(r.backend) << ","
              << format_double(r.build_us) << ","
              << format_double(r.sim_us) << ","
              << format_double(r.total_us) << ","
              << r.iterations << ","
              << format_double(r.median_us) << ","
              << csv_escape(r.status) << "\n";
        }
    }

    // -------------------------------------------------------------------------
    // JSON output
    //
    // Top-level structure:
    //   {
    //     "version": "1.0",
    //     "timestamp": "2026-06-08T12:34:56Z",
    //     "runs": [
    //       { ...all BenchmarkResult fields... },
    //       ...
    //     ]
    //   }
    // -------------------------------------------------------------------------
    void export_json(const std::string& filename) const {
        std::ofstream f(filename);
        f << "{\n";
        f << "  \"version\": \"1.0\",\n";
        f << "  \"timestamp\": \"" << iso8601_now() << "\",\n";
        f << "  \"runs\": [\n";
        for (size_t i = 0; i < results_.size(); ++i) {
            const auto& r = results_[i];
            f << "    {\n";
            f << "      \"test_name\": \"" << json_escape(r.test_name) << "\",\n";
            f << "      \"params\": \"" << json_escape(r.params) << "\",\n";
            f << "      \"backend\": \"" << json_escape(r.backend) << "\",\n";
            f << "      \"ticks_per_sec\": " << format_double(r.ticks_per_sec) << ",\n";
            f << "      \"ns_per_tick\": " << format_double(r.ns_per_tick) << ",\n";
            f << "      \"ns_per_node_tick\": " << format_double(r.ns_per_node_tick) << ",\n";
            f << "      \"overhead_percent\": " << format_double(r.overhead_percent) << ",\n";
            f << "      \"build_us\": " << format_double(r.build_us) << ",\n";
            f << "      \"sim_us\": " << format_double(r.sim_us) << ",\n";
            f << "      \"total_us\": " << format_double(r.total_us) << ",\n";
            f << "      \"iterations\": " << r.iterations << ",\n";
            f << "      \"median_us\": " << format_double(r.median_us) << ",\n";
            f << "      \"status\": \"" << json_escape(r.status) << "\"\n";
            f << "    }" << (i + 1 < results_.size() ? "," : "") << "\n";
        }
        f << "  ]\n";
        f << "}\n";
    }

    // -------------------------------------------------------------------------
    // Markdown output (GitHub-flavored Markdown table)
    //
    // Header: | Design | Backend | Build (μs) | Sim (μs) | Total (μs) |
    //         | Median (μs) | Iterations | Status |
    //
    // One separator row + one row per BenchmarkResult.
    // -------------------------------------------------------------------------
    void export_markdown(const std::string& filename) const {
        std::ofstream f(filename);
        f << "# Performance Comparison Report\n\n";
        f << "| Design | Backend | Build (μs) | Sim (μs) | Total (μs) | "
             "Median (μs) | Iterations | Status |\n";
        f << "|---|---|---:|---:|---:|---:|---:|---|\n";
        for (const auto& r : results_) {
            f << "| " << md_cell(r.test_name) << " "
              << "| " << md_cell(r.params) << " "
              << "| " << md_cell(r.backend.empty() ? "(legacy)" : r.backend) << " "
              << "| " << format_double(r.build_us) << " "
              << "| " << format_double(r.sim_us) << " "
              << "| " << format_double(r.total_us) << " "
              << "| " << format_double(r.median_us) << " "
              << "| " << r.iterations << " "
              << "| " << md_cell(r.status) << " |\n";
        }
    }

    // -------------------------------------------------------------------------
    // Console summary — extends v1 format with backend column.
    // -------------------------------------------------------------------------
    void print_summary() const {
        printf("%-20s | %-12s | %-12s | %12s | %10s | %10s\n",
               "test", "params", "backend", "ticks/sec", "ns/tick", "us/iter");
        for (const auto& r : results_) {
            printf("%-20s | %-12s | %-12s | %12.2f ticks/sec | %10.2f ns/tick | %10.2f us/iter\n",
                   r.test_name.c_str(),
                   r.params.c_str(),
                   (r.backend.empty() ? "(legacy)" : r.backend).c_str(),
                   r.ticks_per_sec, r.ns_per_tick, r.median_us);
        }
    }

    void clear() { results_.clear(); }

    // Read-only access (for downstream tools / tests).
    const std::vector<BenchmarkResult>& results() const { return results_; }

private:
    std::vector<BenchmarkResult> results_;

    // ----- helpers -----------------------------------------------------------

    // Quote-escape a string for CSV. If it contains comma, quote, or newline,
    // wrap in double quotes and escape internal quotes by doubling them.
    static std::string csv_escape(const std::string& s) {
        bool needs_quote = s.find_first_of(",\"\n\r") != std::string::npos;
        if (!needs_quote) return s;
        std::string out;
        out.reserve(s.size() + 2);
        out.push_back('"');
        for (char c : s) {
            if (c == '"') out.push_back('"');
            out.push_back(c);
        }
        out.push_back('"');
        return out;
    }

    // Escape a string for embedding inside a JSON double-quoted value.
    // Handles ", \, control chars (per RFC 8259 minimal subset).
    static std::string json_escape(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 2);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                                      static_cast<unsigned char>(c));
                        out += buf;
                    } else {
                        out.push_back(c);
                    }
            }
        }
        return out;
    }

    // Escape a string for embedding in a Markdown table cell.
    // GFM tables use | as separator; replace with \| and collapse pipes.
    static std::string md_cell(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '|': out += "\\|"; break;
                case '\n': out += " "; break;
                case '\r': break;
                default: out.push_back(c);
            }
        }
        return out;
    }

    // Format a double with fixed precision, dropping trailing zeros.
    // Use 6 significant fractional digits — enough for sub-microsecond sim
    // times without spamming CSV columns.
    static std::string format_double(double v) {
        if (std::isnan(v)) return "nan";
        if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << v;
        return oss.str();
    }

    // Current UTC time in ISO 8601 format: 2026-06-08T12:34:56Z
    static std::string iso8601_now() {
        using namespace std::chrono;
        const auto now = system_clock::now();
        const auto t = system_clock::to_time_t(now);
        std::tm tm_buf{};
        gmtime_r(&t, &tm_buf);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
        return std::string(buf);
    }
};

#endif  // CPPCPPHDL_REPORT_GENERATOR_H
