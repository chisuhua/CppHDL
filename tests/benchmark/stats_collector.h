#ifndef CPPCPPHDL_STATS_COLLECTOR_H
#define CPPCPPHDL_STATS_COLLECTOR_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <string>
#include <fstream>

class StatsCollector {
public:
    void add(double value) { data_.push_back(value); }

    double mean() const {
        if (data_.empty()) return 0.0;
        return std::accumulate(data_.begin(), data_.end(), 0.0) / data_.size();
    }

    double stddev() const {
        if (data_.size() < 2) return 0.0;
        double m = mean();
        double sq_sum = 0.0;
        for (auto v : data_) sq_sum += (v - m) * (v - m);
        return std::sqrt(sq_sum / (data_.size() - 1));
    }

    double min() const {
        return data_.empty() ? 0.0 : *std::min_element(data_.begin(), data_.end());
    }

    double max() const {
        return data_.empty() ? 0.0 : *std::max_element(data_.begin(), data_.end());
    }

    double median() const {
        if (data_.empty()) return 0.0;
        auto sorted = data_;
        std::sort(sorted.begin(), sorted.end());
        size_t n = sorted.size();
        return (n % 2 == 0) ? (sorted[n/2-1] + sorted[n/2]) / 2 : sorted[n/2];
    }

    size_t count() const { return data_.size(); }

    void report_csv(const std::string& filename) const {
        std::ofstream f(filename);
        f << "metric,value\n";
        f << "mean," << mean() << "\n";
        f << "stddev," << stddev() << "\n";
        f << "min," << min() << "\n";
        f << "max," << max() << "\n";
        f << "median," << median() << "\n";
        f << "count," << count() << "\n";
    }

    void clear() { data_.clear(); }

private:
    std::vector<double> data_;
};

#endif  // CPPCPPHDL_STATS_COLLECTOR_H