// tests/external_tool_helpers.h
//
// Shared helper functions for invoking external Verilog tools (iverilog,
// verilator --lint-only, etc.) from Catch2 tests. Originally inlined in
// tests/test_verilog_external.cpp; extracted here so that the
// strengthen-verilator-e2e-coverage change (D4 lint upgrade) and the new
// trace-parity / bundle / stream / memory / complex e2e tests can share a
// single implementation. POSIX-only (uses popen / mkstemp); macOS / Linux
// only. Windows MSVC is deferred (see tasks.md "后续 PR").
//
// Usage:
//   #include "external_tool_helpers.h"
//   if (tool_available("verilator")) {
//       auto path = write_to_tempfile(verilog, ".v");
//       auto r = run_tool("verilator --lint-only " + path);
//       REQUIRE(r.exit_code == 0);
//   }
#pragma once

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ch_test {

inline bool tool_available(const char *cmd) {
    std::string which_cmd = std::string("command -v ") + cmd +
                            " >/dev/null 2>&1";
    return std::system(which_cmd.c_str()) == 0;
}

struct ToolResult {
    int exit_code;
    std::string stdout_out;
    std::string stderr_out;
};

inline ToolResult run_tool(const std::string &cmd) {
    ToolResult r{};
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        r.exit_code = -1;
        r.stderr_out = "popen failed";
        return r;
    }
    char buf[4096];
    while (char *line = fgets(buf, sizeof(buf), pipe)) {
        r.stdout_out += line;
    }
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        r.exit_code = WEXITSTATUS(status);
    } else {
        r.exit_code = -1;
    }
    return r;
}

inline std::string write_to_tempfile(const std::string &body,
                                     const std::string &suffix) {
    char tmpl[] = "/tmp/cpphdl_verilog_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) {
        return {};
    }
    close(fd);
    std::string path = std::string(tmpl) + suffix;
    std::ofstream out(path);
    out << body;
    out.close();
    return path;
}

// Make a unique workdir under /tmp suitable for VerilatorBackend's
// work_dir argument. Returns the absolute path with a per-call suffix
// appended (one dir per call), or "" on mkdtemp failure.
inline std::string make_temp_dir(const std::string &suffix) {
    char tmpl[] = "/tmp/cpphdl_vl_XXXXXX";
    char *dir = ::mkdtemp(tmpl);
    if (!dir) return {};
    std::string result(dir);
    result += suffix;
    ::mkdir(result.c_str(), 0755);
    return result;
}

// Convenience: lint a Verilog string with verilator --lint-only. Returns
// the raw ToolResult so the caller can REQUIRE on exit_code. SKIPs in
// TEST_CASE should call this only after `tool_available("verilator")`
// returns true. Default warning suppression matches the project's
// test_verilog_external.cpp baseline.
inline ToolResult lint_verilog(const std::string &verilog) {
    auto path = write_to_tempfile(verilog, ".v");
    std::string cmd = "verilator --lint-only -Wno-fatal -Wno-WIDTH "
                      "-Wno-UNOPTFLAT " + path + " 2>&1";
    return run_tool(cmd);
}

} // namespace ch_test