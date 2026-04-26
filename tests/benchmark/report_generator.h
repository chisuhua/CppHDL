#ifndef CPPCPPHDL_REPORT_GENERATOR_H
#define CPPCPPHDL_REPORT_GENERATOR_H

#include <string>
#include <vector>
#include <fstream>

struct BenchmarkResult {
    std::string test_name;
    std::string params;
    double ticks_per_sec;
    double ns_per_tick;
    double ns_per_node_tick;
    double overhead_percent;
};

class ReportGenerator {
public:
    void add_result(const BenchmarkResult& r) { results_.push_back(r); }

    void export_csv(const std::string& filename) const {
        std::ofstream f(filename);
        f << "test,params,ticks_per_sec,ns_per_tick,ns_per_node_tick,overhead_pct\n";
        for (auto& r : results_) {
            f << r.test_name << ","
              << r.params << ","
              << r.ticks_per_sec << ","
              << r.ns_per_tick << ","
              << r.ns_per_node_tick << ","
              << r.overhead_percent << "\n";
        }
    }

    void export_json(const std::string& filename) const {
        std::ofstream f(filename);
        f << "{\n";
        f << "  \"benchmarks\": [\n";
        for (size_t i = 0; i < results_.size(); ++i) {
            auto& r = results_[i];
            f << "    {\n";
            f << "      \"name\": \"" << r.test_name << "\",\n";
            f << "      \"params\": \"" << r.params << "\",\n";
            f << "      \"ticks_per_sec\": " << r.ticks_per_sec << ",\n";
            f << "      \"ns_per_tick\": " << r.ns_per_tick << ",\n";
            f << "      \"ns_per_node_tick\": " << r.ns_per_node_tick << ",\n";
            f << "      \"overhead_percent\": " << r.overhead_percent << "\n";
            f << "    }" << (i + 1 < results_.size() ? "," : "") << "\n";
        }
        f << "  ]\n";
        f << "}\n";
    }

    void print_summary() const {
        for (auto& r : results_) {
            printf("%-20s | %-15s | %12.2f ticks/sec | %8.2f ns/tick\n",
                   r.test_name.c_str(), r.params.c_str(),
                   r.ticks_per_sec, r.ns_per_tick);
        }
    }

    void clear() { results_.clear(); }

private:
    std::vector<BenchmarkResult> results_;
};

#endif  // CPPCPPHDL_REPORT_GENERATOR_H