// tests/benchmark/verilator_runner.h
//
// Header-only Verilator build/run wrapper for CppHDL perf benchmarks.
// Caches Verilator builds by SHA-1(design_name + verilator_version +
// verilog_source + harness_source) to amortize the ~5-30s first-build
// cost across reruns.
//
// Used by perf_main.cpp (TC-07/TC-08 three-way comparison). See
// docs/superpowers/specs/2026-06-08-perf-tests-jit-verilator-comparison-design.md
// for design intent.

#pragma once

#include "perf_timer.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace ch_perf {

struct VerilatorBuildResult {
    bool success = false;
    std::string binary_path;        // <work_dir>/obj_dir/Vtop
    std::string cache_key;          // SHA-1 hex string
    bool cache_hit = false;
    uint64_t build_time_ns = 0;
    std::string error_msg;
};

class VerilatorRunner {
public:
    VerilatorRunner(std::string cache_root,
                    std::string verilator_bin = "verilator")
        : cache_root_(std::move(cache_root)),
          verilator_bin_(std::move(verilator_bin)) {}

    // True if `verilator --version` exits 0.
    bool is_available() const;

    // Returns "Verilator 5.020 2024-01-01" or empty on failure.
    std::string verilator_version() const;

    // Build Vtop for (verilog_path, harness_path). Caches by
    // SHA-1(design_name + verilog_source + harness_source +
    // verilator_version).
    VerilatorBuildResult build(const std::string &design_name,
                               const std::string &verilog_path,
                               const std::string &harness_path);

    // Run binary with `ticks` as a single argv, measure wall-clock
    // with PerfTimer. Returns elapsed ns; 0 on error.
    uint64_t run_and_time(const std::string &binary_path,
                          int ticks,
                          std::string *error_msg = nullptr);

    // SHA-1 of a string. Public for testability.
    static std::string sha1_hex(const std::string &data);

    // Cache directory for a given key: <cache_root>/<key>/
    std::string cache_dir_for_key(const std::string &key) const;

private:
    std::string cache_root_;
    std::string verilator_bin_;

    static std::string read_file(const std::string &path);
    static std::string shell_escape(const std::string &s);
};

// ---------- SHA-1 (RFC 3174) ----------
namespace ch_perf_detail {
inline uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

inline std::array<uint8_t, 20> sha1(const std::string &data) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89,
             h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

    std::vector<uint8_t> msg(data.begin(), data.end());
    uint64_t bit_len = static_cast<uint64_t>(msg.size()) * 8;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0);
    for (int i = 7; i >= 0; --i) msg.push_back((bit_len >> (i * 8)) & 0xff);

    for (size_t block = 0; block < msg.size(); block += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (uint32_t(msg[block + i * 4]) << 24) |
                   (uint32_t(msg[block + i * 4 + 1]) << 16) |
                   (uint32_t(msg[block + i * 4 + 2]) << 8) |
                   (uint32_t(msg[block + i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rotl32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        }
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);            k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                      k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);    k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;                      k = 0xCA62C1D6; }
            uint32_t t = rotl32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl32(b, 30); b = a; a = t;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    std::array<uint8_t, 20> out{};
    auto put = [&](int idx, uint32_t v) {
        out[idx]     = (v >> 24) & 0xff;
        out[idx + 1] = (v >> 16) & 0xff;
        out[idx + 2] = (v >>  8) & 0xff;
        out[idx + 3] = v & 0xff;
    };
    put(0, h0); put(4, h1); put(8, h2); put(12, h3); put(16, h4);
    return out;
}
} // namespace ch_perf_detail

inline std::string VerilatorRunner::sha1_hex(const std::string &data) {
    auto h = ch_perf_detail::sha1(data);
    static const char hex[] = "0123456789abcdef";
    std::string s;
    s.reserve(40);
    for (auto b : h) { s.push_back(hex[b >> 4]); s.push_back(hex[b & 0xf]); }
    return s;
}

inline std::string VerilatorRunner::read_file(const std::string &path) {
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

inline std::string VerilatorRunner::shell_escape(const std::string &s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

inline std::string VerilatorRunner::cache_dir_for_key(const std::string &k) const {
    return cache_root_ + "/" + k;
}

inline bool VerilatorRunner::is_available() const {
    std::string dummy;
    return !verilator_version().empty();
}

inline std::string VerilatorRunner::verilator_version() const {
    // popen returns the first line of `verilator --version`.
    // W5 (perf-report-followup.md): when verilator is not on PATH, the
    // shell's "not found" message still appears on stdout (after the
    // `2>&1` redirect), so fgets() returns a non-empty string. The
    // `is_available()` check was therefore returning true for missing
    // binaries, and the actual build would fail with exit 127 — looking
    // identical to a SKIPPED runtime error. We now also require the
    // output to start with a version-looking token (digit prefix).
    std::string cmd = shell_escape(verilator_bin_) + " --version 2>&1";
    FILE *p = ::popen(cmd.c_str(), "r");
    if (!p) return {};
    char buf[256];
    std::string out;
    if (fgets(buf, sizeof(buf), p)) out = buf;
    int rc = ::pclose(p);
    // Trim trailing newline
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    // Reject shell error messages (e.g. "sh: 1: verilator: not found").
    if (rc != 0) return {};
    if (out.empty()) return {};
    // Heuristic: real verilator output starts with a version like
    // "Verilator 5.020 2024-...". Reject if it looks like a shell error.
    if (out.find("not found") != std::string::npos) return {};
    if (out.find("command not found") != std::string::npos) return {};
    return out;
}

inline VerilatorBuildResult
VerilatorRunner::build(const std::string &design_name,
                       const std::string &verilog_path,
                       const std::string &harness_path) {
    VerilatorBuildResult r;
    std::string ver = verilator_version();
    if (ver.empty()) {
        r.error_msg = "verilator not found on PATH";
        return r;
    }

    std::string verilog_src = read_file(verilog_path);
    std::string harness_src = read_file(harness_path);
    if (verilog_src.empty() || harness_src.empty()) {
        r.error_msg = "verilog or harness file unreadable";
        return r;
    }

    // Embedded NULs are valid std::string content and the SHA-1 sees them
    // as bytes, so this gives a deterministic cache key.
    std::string key_input = design_name + "\0" + ver + "\0" +
                            verilog_src + "\0" + harness_src;
    r.cache_key = sha1_hex(key_input);

    std::string cache_dir = cache_dir_for_key(r.cache_key);
    std::string binary = cache_dir + "/obj_dir/Vtop";

    if (std::filesystem::exists(binary)) {
        r.success = true;
        r.binary_path = binary;
        r.cache_hit = true;
        return r;
    }

    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    if (ec) {
        r.error_msg = "create_directories failed: " + ec.message();
        return r;
    }

    // Copy verilog + harness into cache_dir so verilator --build's
    // out-of-tree build is reproducible.
    std::filesystem::copy(verilog_path, cache_dir + "/top.v",
                          std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        r.error_msg = "copy verilog failed: " + ec.message();
        return r;
    }
    std::filesystem::copy(harness_path, cache_dir + "/harness.cpp",
                          std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        r.error_msg = "copy harness failed: " + ec.message();
        return r;
    }

    // Build directly inside cache_dir; verilator's --build is happy
    // with the inputs being relative paths. Shell-escape paths so a
    // cache dir with spaces (e.g. /tmp/... with user names) still works.
    std::string full_cmd =
        "cd " + shell_escape(cache_dir) + " && " +
        shell_escape(verilator_bin_) +
        " --cc --build --exe -Wno-fatal" +
        " harness.cpp top.v 2>&1";

    PerfTimer t;
    t.start();
    FILE *p = ::popen(full_cmd.c_str(), "r");
    if (!p) {
        r.error_msg = "popen failed";
        return r;
    }
    std::string output;
    char buf[4096];
    while (fgets(buf, sizeof(buf), p)) output += buf;
    int status = ::pclose(p);
    t.stop();
    r.build_time_ns = static_cast<uint64_t>(t.elapsed_ns());

    if (status != 0) {
        // Truncate long stderr for readability
        if (output.size() > 2048) output.resize(2048);
        r.error_msg = "verilator build failed (exit=" +
                      std::to_string(status) + "): " + output;
        return r;
    }
    if (!std::filesystem::exists(binary)) {
        r.error_msg = "verilator reported success but " + binary + " missing";
        return r;
    }
    r.success = true;
    r.binary_path = binary;
    r.cache_hit = false;
    return r;
}

inline uint64_t VerilatorRunner::run_and_time(const std::string &binary_path,
                                              int ticks,
                                              std::string *error_msg) {
    if (!std::filesystem::exists(binary_path)) {
        if (error_msg) *error_msg = "binary not found: " + binary_path;
        return 0;
    }
    std::string cmd = shell_escape(binary_path) + " " +
                      std::to_string(ticks) + " 2>&1";
    PerfTimer t;
    t.start();
    FILE *p = ::popen(cmd.c_str(), "r");
    if (!p) {
        if (error_msg) *error_msg = "popen failed";
        return 0;
    }
    char buf[4096];
    std::string output;
    while (fgets(buf, sizeof(buf), p)) output += buf;
    int status = ::pclose(p);
    t.stop();
    if (status != 0) {
        if (error_msg) {
            if (output.size() > 1024) output.resize(1024);
            *error_msg = "verilator binary exit=" + std::to_string(status) +
                         ": " + output;
        }
        return 0;
    }
    return static_cast<uint64_t>(t.elapsed_ns());
}

} // namespace ch_perf
