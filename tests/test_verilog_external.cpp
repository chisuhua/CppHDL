// tests/test_verilog_external.cpp
// ADR-035 / Phase 1.6: Validate generated Verilog with external tools
// (iverilog + verilator). This is the end-to-end acceptance gate for
// Phase 1 — the Verilog output must be accepted by real-world Verilog
// tools, not just string-match our own C++ tests.
//
// NOTE: We use CATCH_CONFIG_RUNNER (not CATCH_CONFIG_MAIN) so that we
// can detect the "neither iverilog nor verilator installed" case and
// exit with code 0. Otherwise Catch2 exits with 4 ("all tests were
// skipped"), which ctest reports as a failure. Each TEST_CASE still
// calls SKIP() individually so the executable remains correct on
// hosts that have only one of the two tools.
#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/context.h"
#include "external_tool_helpers.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>

#define CATCH_CONFIG_RUNNER
#include "catch_amalgamated.hpp"

using namespace ch;
using namespace ch::core;

namespace {

using ch_test::tool_available;
using ch_test::run_tool;
using ch_test::write_to_tempfile;
using ch_test::ToolResult;

std::string generate_verilog(context *ctx) {
    std::ostringstream oss;
    verilogwriter writer(ctx);
    writer.print(oss);
    return oss.str();
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

// Custom entry point: when neither iverilog nor verilator is installed,
// every TEST_CASE in this executable calls SKIP(). Catch2 then returns
// exit code 4 ("all tests skipped"), which ctest treats as failure.
// We treat the all-skipped case as a clean pass (exit 0) and surface a
// one-line note on stderr so it's still visible in CI logs.
int main(int argc, char *argv[]) {
    if (!tool_available("iverilog") && !tool_available("verilator")) {
        std::fprintf(stderr,
                     "test_verilog_external: iverilog/verilator not installed; "
                     "skipping all external-tool tests.\n");
        return 0;
    }
    Catch::Session session;
    session.applyCommandLine(argc, argv);
    return session.run();
}
