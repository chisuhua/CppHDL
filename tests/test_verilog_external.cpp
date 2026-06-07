// tests/test_verilog_external.cpp
// ADR-035 / Phase 1.6: Validate generated Verilog with external tools
// (iverilog + verilator). This is the end-to-end acceptance gate for
// Phase 1 — the Verilog output must be accepted by real-world Verilog
// tools, not just string-match our own C++ tests.
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/context.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace ch;
using namespace ch::core;

namespace {

bool tool_available(const char *cmd) {
    std::string which_cmd = std::string("which ") + cmd +
                             " >/dev/null 2>&1";
    return std::system(which_cmd.c_str()) == 0;
}

std::string generate_verilog(context *ctx) {
    std::ostringstream oss;
    verilogwriter writer(ctx);
    writer.print(oss);
    return oss.str();
}

struct ToolResult {
    int exit_code;
    std::string stdout_out;
    std::string stderr_out;
};

ToolResult run_tool(const std::string &cmd) {
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

std::string write_to_tempfile(const std::string &body,
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

} // namespace

TEST_CASE("VerilogExternal - iverilogCompilesCounter",
          "[verilog][external][iverilog]") {
    if (!tool_available("iverilog")) {
        SKIP("iverilog not installed");
    }

    auto ctx = std::make_unique<context>("ext_counter_test");
    ctx_swap guard(ctx.get());

    ch_out<ch_uint<4>> out_port("io");
    ch_reg<ch_uint<4>> reg_c(0_d, "counter");
    reg_c->next = reg_c + 1_d;
    out_port <<= reg_c;

    std::string verilog = generate_verilog(ctx.get());
    std::string path = write_to_tempfile(verilog, ".v");
    REQUIRE(!path.empty());

    ToolResult r = run_tool("iverilog -g2012 -Wall -o /dev/null " + path +
                            " 2>&1");
    INFO("Generated verilog:\n" + verilog);
    INFO("iverilog output:\n" + r.stdout_out + r.stderr_out);
    REQUIRE(r.exit_code == 0);
}

TEST_CASE("VerilogExternal - iverilogCompilesBundle",
          "[verilog][external][iverilog][bundle]") {
    if (!tool_available("iverilog")) {
        SKIP("iverilog not installed");
    }

    auto ctx = std::make_unique<context>("ext_bundle_test");
    ctx_swap guard(ctx.get());

    ch_in<ch_uint<8>> a("awaddr");
    ch_in<ch_uint<1>> b("awvalid");
    ch_out<ch_uint<8>> c("rdata");
    ch_uint<8> zero8(0_d);
    ch_uint<1> zero1(0_d);
    a = zero8;
    b = zero1;
    c = zero8;

    std::string verilog = generate_verilog(ctx.get());
    std::string path = write_to_tempfile(verilog, ".v");
    REQUIRE(!path.empty());

    ToolResult r = run_tool("iverilog -g2012 -Wall -o /dev/null " + path +
                            " 2>&1");
    INFO("Generated verilog:\n" + verilog);
    INFO("iverilog output:\n" + r.stdout_out + r.stderr_out);
    REQUIRE(r.exit_code == 0);
}

TEST_CASE("VerilogExternal - iverilogCompilesMultiReg",
          "[verilog][external][iverilog]") {
    if (!tool_available("iverilog")) {
        SKIP("iverilog not installed");
    }

    auto ctx = std::make_unique<context>("ext_multireg_test");
    ctx_swap guard(ctx.get());

    ch_reg<ch_uint<4>> reg_a(0_d, "reg_a");
    ch_reg<ch_uint<4>> reg_b(0_d, "reg_b");
    reg_a->next = reg_a + 1_d;
    reg_b->next = reg_b + 2_d;

    ch_out<ch_uint<4>> io("io");
    ch_out<ch_uint<4>> aux_out("aux_out");
    io <<= reg_a;
    aux_out <<= reg_b;

    std::string verilog = generate_verilog(ctx.get());
    std::string path = write_to_tempfile(verilog, ".v");
    REQUIRE(!path.empty());

    ToolResult r = run_tool("iverilog -g2012 -Wall -o /dev/null " + path +
                            " 2>&1");
    INFO("Generated verilog:\n" + verilog);
    INFO("iverilog output:\n" + r.stdout_out + r.stderr_out);
    REQUIRE(r.exit_code == 0);
}

TEST_CASE("VerilogExternal - verilatorLintsCounter",
          "[verilog][external][verilator]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not installed");
    }

    auto ctx = std::make_unique<context>("ext_vl_counter_test");
    ctx_swap guard(ctx.get());

    ch_out<ch_uint<4>> out_port("io");
    ch_reg<ch_uint<4>> reg_c(0_d, "counter");
    reg_c->next = reg_c + 1_d;
    out_port <<= reg_c;

    std::string verilog = generate_verilog(ctx.get());
    std::string path = write_to_tempfile(verilog, ".v");
    REQUIRE(!path.empty());

    ToolResult r = run_tool("verilator --lint-only -Wno-fatal -Wno-WIDTH "
                            "-Wno-UNOPTFLAT " +
                            path + " 2>&1");
    INFO("Generated verilog:\n" + verilog);
    INFO("verilator output:\n" + r.stdout_out + r.stderr_out);
    REQUIRE(r.exit_code == 0);
}

TEST_CASE("VerilogExternal - verilatorLintsBundle",
          "[verilog][external][verilator][bundle]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not installed");
    }

    auto ctx = std::make_unique<context>("ext_vl_bundle_test");
    ctx_swap guard(ctx.get());

    ch_in<ch_uint<8>> a("awaddr");
    ch_in<ch_uint<1>> b("awvalid");
    ch_out<ch_uint<8>> c("rdata");
    ch_uint<8> zero8(0_d);
    ch_uint<1> zero1(0_d);
    a = zero8;
    b = zero1;
    c = zero8;

    std::string verilog = generate_verilog(ctx.get());
    std::string path = write_to_tempfile(verilog, ".v");
    REQUIRE(!path.empty());

    ToolResult r = run_tool("verilator --lint-only -Wno-fatal -Wno-WIDTH "
                            "-Wno-UNOPTFLAT " +
                            path + " 2>&1");
    INFO("Generated verilog:\n" + verilog);
    INFO("verilator output:\n" + r.stdout_out + r.stderr_out);
    REQUIRE(r.exit_code == 0);
}

TEST_CASE("VerilogExternal - verilatorLintsMultiReg",
          "[verilog][external][verilator]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not installed");
    }

    auto ctx = std::make_unique<context>("ext_vl_multireg_test");
    ctx_swap guard(ctx.get());

    ch_reg<ch_uint<4>> reg_a(0_d, "reg_a");
    ch_reg<ch_uint<4>> reg_b(0_d, "reg_b");
    reg_a->next = reg_a + 1_d;
    reg_b->next = reg_b + 2_d;

    ch_out<ch_uint<4>> io("io");
    ch_out<ch_uint<4>> aux_out("aux_out");
    io <<= reg_a;
    aux_out <<= reg_b;

    std::string verilog = generate_verilog(ctx.get());
    std::string path = write_to_tempfile(verilog, ".v");
    REQUIRE(!path.empty());

    ToolResult r = run_tool("verilator --lint-only -Wno-fatal -Wno-WIDTH "
                            "-Wno-UNOPTFLAT " +
                            path + " 2>&1");
    INFO("Generated verilog:\n" + verilog);
    INFO("verilator output:\n" + r.stdout_out + r.stderr_out);
    REQUIRE(r.exit_code == 0);
}
