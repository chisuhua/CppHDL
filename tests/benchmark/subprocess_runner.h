// SPDX-License-Identifier: MIT
// CppHDL — perf benchmark subprocess isolation helper
//
// F2 (fix-perf-subprocess-isolation): run each TC in a fresh child process
// via std::system() + chdir to a unique temp dir. The child writes
// perf_results.json into its own CWD; the parent reads it back, parses
// the rows, and merges them into the parent's reporter.
//
// Design rationale (see openspec/changes/fix-perf-subprocess-isolation/design.md
// Decision 1): std::system() + temp dir is the simplest reliable approach
// for one-shot perf measurement where the child runs to completion before
// the parent needs the result. No fork+execve+pipes complexity needed.
//
// JSON parsing: no external deps (nlohmann::json not available in this
// project). We use a minimal flat-array parser identical in shape to the
// one in tests/benchmark/perf_regression.cpp:39-120, restricted to the
// "results" or "runs" array schema produced by ReportGenerator.

#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace ch::perf_test {

struct SubprocessRow {
    std::string test_name;
    std::string params;
    std::string backend;
    double median_us = 0.0;
    double sim_us = 0.0;
    double build_us = 0.0;
    double ticks_per_sec = 0.0;
    double ns_per_tick = 0.0;
    double ns_per_node_tick = 0.0;
    double overhead_percent = 0.0;
    double total_us = 0.0;
    int iterations = 0;
    std::string status = "UNKNOWN";
    std::string skip_reason;
};

struct SubprocessResult {
    int exit_code = 0;
    bool timed_out = false;
    std::filesystem::path output_file;
    std::filesystem::path temp_dir;
    std::string error_msg;
    std::vector<SubprocessRow> rows;
};

// 生成唯一临时目录:`<temp>/perf_main_<pid>_<tag>`
inline std::filesystem::path make_temp_dir(const std::string& tag) {
    auto base = std::filesystem::temp_directory_path();
    auto dir = base / ("perf_main_" + std::to_string(::getpid()) + "_" + tag);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);  // 清理上次残留
    std::filesystem::create_directories(dir, ec);
    return dir;
}

// 最小 JSON 解析器:仅识别 "results" / "runs" 数组下的扁平对象
// 字段:test_name / params / backend / median_us / sim_us
inline std::vector<SubprocessRow> parse_subprocess_json(
    const std::filesystem::path& path) {
    std::vector<SubprocessRow> out;
    std::ifstream in(path);
    if (!in) return out;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    std::string key;
    auto arr_start = content.find("\"results\":");
    if (arr_start != std::string::npos) {
        key = "\"results\"";
    } else {
        arr_start = content.find("\"runs\":");
        if (arr_start != std::string::npos) key = "\"runs\"";
    }
    if (arr_start == std::string::npos) return out;

    size_t i = arr_start;
    while (i < content.size() && content[i] != '[') ++i;
    if (i >= content.size()) return out;
    ++i;  // skip '['

    while (i < content.size()) {
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
        auto extract_str = [&](const std::string& field) -> std::string {
            std::string k = "\"" + field + "\":";
            auto p = obj.find(k);
            if (p == std::string::npos) return "";
            p += k.size();
            while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\n' ||
                   obj[p] == '\r' || obj[p] == '\t')) ++p;
            if (p >= obj.size() || obj[p] != '"') return "";
            ++p;
            size_t end = p;
            while (end < obj.size() && obj[end] != '"') {
                if (obj[end] == '\\' && end + 1 < obj.size()) ++end;
                ++end;
            }
            return obj.substr(p, end - p);
        };
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
        SubprocessRow r;
        r.test_name = extract_str("test_name");
        r.params = extract_str("params");
        r.backend = extract_str("backend");
        r.median_us = extract_num("median_us");
        r.sim_us = extract_num("sim_us");
        r.build_us = extract_num("build_us");
        r.ticks_per_sec = extract_num("ticks_per_sec");
        r.ns_per_tick = extract_num("ns_per_tick");
        r.ns_per_node_tick = extract_num("ns_per_node_tick");
        r.overhead_percent = extract_num("overhead_percent");
        r.total_us = extract_num("total_us");
        r.iterations = static_cast<int>(extract_num("iterations"));
        r.status = extract_str("status");
        r.skip_reason = extract_str("skip_reason");
        out.push_back(r);
    }
    return out;
}

// 启动子进程跑 perf_tests 单 TC,捕获 JSON 结果
// exe_path: perf_tests 二进制绝对路径
// tc: "07" / "08" / "09"
// extra_args: 透传给子进程的额外参数(verilator_bin / cache_root / workdir / ticks / warmup / measured)
// timeout_sec: wall-clock 超时(秒),超时发送 SIGTERM
inline SubprocessResult run_perf_subprocess(
    const std::filesystem::path& exe_path,
    const std::string& tc,
    const std::vector<std::string>& extra_args,
    int timeout_sec = 180) {
    SubprocessResult result;
    result.temp_dir = make_temp_dir("tc" + tc);

    // 构造命令:`<exe> --tc=NN <extra...> --report=json --workdir=<temp_dir> 2>&1`
    std::ostringstream cmd;
    cmd << "\"" << exe_path.string() << "\"";
    cmd << " --tc=" << tc;
    for (const auto& a : extra_args) {
        cmd << " " << a;
    }
    cmd << " --report=json";
    cmd << " --workdir=\"" << result.temp_dir.string() << "\"";
    cmd << " 2>&1";
    // Materialize the command before fork() so the child does not
    // depend on std::ostringstream's internal buffer (libc++/libstdc++
    // are not async-signal-safe in the child between fork and exec).
    std::string cmd_str = cmd.str();

    // Subprocess dispatch no longer mutates the parent's cwd.
    // Previously run_perf_subprocess() called current_path(temp_dir)
    // before fork(), then restored it after waitpid(). That pattern
    // was fragile in any multi-threaded parent (current_path() is
    // process-wide; another thread could observe the temp-dir cwd),
    // and it forced callers to remember to absolute-ize relative paths
    // in the parent before they were forwarded to the child.
    //
    // Now the child does its own chdir after fork and before exec, so:
    //   - parent's cwd stays at the project root (ctest sets it via
    //     WORKING_DIRECTORY=${CMAKE_SOURCE_DIR}) and any relative
    //     std::filesystem operation the parent does between fork and
    //     waitpid resolves correctly.
    //   - child's cwd is still the per-TC temp dir, so the relative
    //     "perf_results.json" written by the child lands under
    //     result.temp_dir as before.
    // The caller must still pass absolute paths (or absolute-ize them
    // before dispatch) for harness_path / cache_root / workdir, since
    // the child's cwd is the temp dir — see openspec/specs/perf-test-
    // isolation/spec.md for the contract.
    std::error_code ec;

    pid_t pid = fork();
    if (pid == 0) {
        // 子进程:先 chdir 到 temp_dir,再 exec。这样父进程的 cwd 不动。
        if (::chdir(result.temp_dir.c_str()) != 0) {
            // chdir 失败:用 _exit(126) 标记,父进程通过 exit_code 区分
            // (sh 也用 126 表示"found but not executable",本场景下区分不
            // 重要——父进程只检查 result.output_file 是否存在)。
            ::_exit(126);
        }
        ::execl("/bin/sh", "sh", "-c", cmd_str.c_str(), (char*)nullptr);
        ::_exit(127);
    } else if (pid < 0) {
        result.error_msg = "fork failed";
        // 清理 fork 失败留下的 temp_dir,避免泄漏 perf_main_<pid>_tcNN
        std::filesystem::remove_all(result.temp_dir, ec);
        return result;
    }

    int status = 0;
    int waited_sec = 0;
    while (waited_sec < timeout_sec) {
        pid_t r = ::waitpid(pid, &status, WNOHANG);
        if (r == pid) break;  // 已退出
        ::sleep(1);
        ++waited_sec;
    }
    if (waited_sec >= timeout_sec) {
        ::kill(pid, SIGTERM);
        ::waitpid(pid, &status, 0);
        result.timed_out = true;
        result.error_msg = "subprocess timeout after " + std::to_string(timeout_sec) + "s";
    } else {
        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result.exit_code = 128 + WTERMSIG(status);
            result.error_msg = "subprocess killed by signal " +
                std::to_string(WTERMSIG(status));
        }
    }

    // 父进程的 cwd 不再需要 saved_cwd / restore——这是本 change 的核心收益。

    // 读取 perf_results.json
    result.output_file = result.temp_dir / "perf_results.json";
    if (std::filesystem::exists(result.output_file)) {
        result.rows = parse_subprocess_json(result.output_file);
    } else if (!result.timed_out) {
        result.error_msg = "perf_results.json not produced (exit " +
            std::to_string(result.exit_code) + ")";
    }

    // 清理临时目录(包含 perf_results.json 与子进程日志)
    std::filesystem::remove_all(result.temp_dir, ec);
    return result;
}

}  // namespace ch::perf_test
