// tests/test_verilator_three_way.cpp
//
// verilator-perf-three-way: codegen 字面量 mask 修复测试
//
// Bug: src/codegen_verilog.cpp get_literal_str() 对 ch_uint<N>(literal) 输出的
//      Verilog 字面量未做宽度 mask。当 literal 值 >= 2^N 时,SystemVerilog 字面量
//      'N'h<value> 中的 hex digit 数量超过 N 位容量,触发 Verilator:
//        %Error: Too many digits for N bit number: '8'h<value>'
//      (-Wno-fatal 救不了,这是硬语法错误)
//
// 实测触发: TC-07 depth=1000 XOR chain 在 i=256 时生成 8'h100, 257->8'h101, ...
//           全部被 Verilator 拒绝 (744 处错误)。
//
// Fix: get_literal_str 内部 value &= ((1ULL << width) - 1)。
//      ch_uint<8>(256) -> 8'h0;  ch_uint<8>(999) -> 8'he7 (而非 8'h3e7)。
//
// 这些测试**不依赖 verilator 二进制**,纯 codegen 端到端断言(用 ch::toVerilog() /
// verilogwriter 生成 verilog 字符串后检查字面量形态)。
//
// 注: 使用 int 字面量构造(走 ch_uint(IntT) ctor,强制宽度 N)而非 _d 后缀
//     (走 ch_literal<V,W> 模板,W=bit_width(V),可能 > N 触发 static_assert)。

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace ch;
using namespace ch::core;

namespace {

// Helper: 用一个 context + verilogwriter 生成 verilog 字符串
std::string generate_verilog_within_ctx(
    const std::function<void()> &build) {
    auto ctx = std::make_unique<ch::core::context>(
        "verilator_three_way_test_ctx");
    ch::core::ctx_swap guard(ctx.get());
    build();
    std::ostringstream oss;
    verilogwriter writer(ctx.get());
    writer.print(oss);
    return oss.str();
}

// 检查 verilog 字符串中是否含有 'N'h<value>' 字面量(value 的 hex digit 数量 > ceil(N/4))
// 这是触发 Verilator "Too many digits" 错误的形态。
bool has_overflowing_hex_literal(const std::string &verilog,
                                 uint32_t width) {
    std::regex re("'" + std::to_string(width) + "'h([0-9a-fA-F]+)");
    auto begin = std::sregex_iterator(verilog.begin(), verilog.end(), re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string hex_digits = (*it)[1].str();
        uint32_t max_digits = (width + 3) / 4;
        if (hex_digits.size() > max_digits) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ============================================================================
// 字面量 mask 行为(RED 旧版 -> GREEN mask 后)
// ============================================================================

TEST_CASE("get_literal_str masks ch_uint<8>(256) to 8'h0 (not 8'h100)",
          "[verilog][verilator]") {
    std::string verilog = generate_verilog_within_ctx([] {
        ch_out<ch_uint<8>> out_port("data");
        ch_uint<8> a(static_cast<uint64_t>(256));  // 256 在 8 位内 = 0
        out_port = a;
    });

    // RED 状态: 含 8'h100 (Verilator 拒绝)
    // GREEN 状态: 不含 8'h100, 含 8'h0
    REQUIRE(verilog.find("8'h100") == std::string::npos);
    REQUIRE(verilog.find("8'h0") != std::string::npos);
}

TEST_CASE("get_literal_str masks ch_uint<8>(999) to 8'he7 (not 8'h3e7)",
          "[verilog][verilator]") {
    std::string verilog = generate_verilog_within_ctx([] {
        ch_out<ch_uint<8>> out_port("data");
        ch_uint<8> a(static_cast<uint64_t>(999));  // 999 & 0xFF = 231 = 0xE7
        out_port = a;
    });

    // RED 状态: 含 8'h3e7 (TC-07 depth=1000 触发的关键 case)
    // GREEN 状态: 不含 8'h3e7, 含 8'he7
    REQUIRE(verilog.find("8'h3e7") == std::string::npos);
    REQUIRE(verilog.find("8'he7") != std::string::npos);
}

TEST_CASE("get_literal_str preserves small values (ch_uint<8>(0) and (1))",
          "[verilog][verilator]") {
    std::string verilog = generate_verilog_within_ctx([] {
        ch_out<ch_uint<8>> out_port_a("a");
        ch_out<ch_uint<8>> out_port_b("b");
        ch_uint<8> zero(static_cast<uint64_t>(0));
        ch_uint<8> one(static_cast<uint64_t>(1));
        out_port_a = zero;
        out_port_b = one;
    });

    // mask 不应破坏合法小值
    REQUIRE(verilog.find("8'h0") != std::string::npos);
    REQUIRE(verilog.find("8'h1") != std::string::npos);
    REQUIRE(verilog.find("8'h100") == std::string::npos);
    REQUIRE(verilog.find("8'h10") == std::string::npos);
}

TEST_CASE("get_literal_str handles boundary value ch_uint<8>(255)",
          "[verilog][verilator]") {
    std::string verilog = generate_verilog_within_ctx([] {
        ch_out<ch_uint<8>> out_port("data");
        ch_uint<8> max_8bit(static_cast<uint64_t>(255));  // 边界值, mask 后仍是 8'hff
        out_port = max_8bit;
    });

    REQUIRE(verilog.find("8'hff") != std::string::npos);
    REQUIRE(verilog.find("8'h100") == std::string::npos);
}

// ============================================================================
// 端到端: 模拟 TC-07 depth=N XOR chain,确认无 overflowing literal
// ============================================================================

TEST_CASE("TC-07 XOR chain (ch_uint<8>) produces no overflowing hex literal",
          "[verilog][verilator]") {
    constexpr int kDepth = 1000;
    std::string verilog = generate_verilog_within_ctx([&] {
        ch_reg<ch_uint<8>> acc(0);
        for (int i = 0; i < kDepth; ++i) {
            // i 为 int,通过 ch_uint<8>(i) 走 int ctor,宽度强制 8
            acc = acc ^ ch_uint<8>(static_cast<uint64_t>(i));
        }
        ch_out<ch_uint<8>> out_port("xor_result");
        out_port = acc;
    });

    // RED 状态: 含 8'h<3+ hex digits> 字面量 (744 处, Verilator 拒绝)
    // GREEN 状态: 无 overflowing 8'h 字面量
    bool overflowing = has_overflowing_hex_literal(verilog, 8);
    INFO("Generated verilog snippet:\n" + verilog.substr(0, 2000));
    REQUIRE_FALSE(overflowing);
}

// ============================================================================
// 端到端: ch_uint<16>(65537) 应 mask 到 16'h1 而非 16'h10001
// ============================================================================

TEST_CASE("get_literal_str masks ch_uint<16>(65537) to 16'h1 (not 16'h10001)",
          "[verilog][verilator]") {
    std::string verilog = generate_verilog_within_ctx([] {
        ch_out<ch_uint<16>> out_port("data");
        ch_uint<16> a(static_cast<uint64_t>(65537));  // 0x10001, mask 16 位 = 1
        out_port = a;
    });

    REQUIRE(verilog.find("16'h10001") == std::string::npos);
    REQUIRE(verilog.find("16'h1") != std::string::npos);
}

// ============================================================================
// 端到端: ch_uint<32>(4294967296) 应 mask 到 32'h0 (uint64 范围内)
// ============================================================================

TEST_CASE("get_literal_str masks ch_uint<32>(4294967296) to 32'h0",
          "[verilog][verilator]") {
    std::string verilog = generate_verilog_within_ctx([] {
        ch_out<ch_uint<32>> out_port("data");
        ch_uint<32> a(static_cast<uint64_t>(4294967296ULL));  // 2^32, mask 32 位 = 0
        out_port = a;
    });

    // 32'h<9 hex digits> (32'h100000000) 会触发 overflow
    REQUIRE(verilog.find("32'h100000000") == std::string::npos);
}

// ============================================================================
// Gap C: print_concat 路径 — 大值 lit 在 concat 中也应 mask
// ============================================================================
// 测试 print_concat (src/codegen_verilog.cpp:843-849) 调用 get_literal_str
// 的 LHS/RHS 路径。concat({lit, wire}) 中 lit 必须 mask 到 lit 自己的宽度,
// 而不是 concat 总宽度(否则会与 test_verilog_gen:643 期望的 "4'hf" 冲突)。

TEST_CASE("print_concat masks ch_uint<4>(999) literal in concat RHS",
          "[verilog][verilator][concat]") {
    std::string verilog = generate_verilog_within_ctx([] {
        ch_in<ch_uint<4>> a("a");
        auto result = concat(ch_uint<4>(static_cast<uint64_t>(999)), a);
        ch_out<ch_uint<8>> out_port("io");
        out_port = result;
    });

    // mask 后 999 & 0xF = 7 = 0x7,字面量 MUST 是 4'h7
    REQUIRE(verilog.find("4'h7") != std::string::npos);
    // 不能是 4'h3e7(3 hex digits 超出 4 位)或 8'h3e7(用了总宽度)
    REQUIRE(verilog.find("4'h3e7") == std::string::npos);
    REQUIRE(verilog.find("8'h3e7") == std::string::npos);
    // concat braces 存在证明走了 print_concat
    REQUIRE(verilog.find("{") != std::string::npos);
    REQUIRE(verilog.find("}") != std::string::npos);
}

// ============================================================================
// Gap E: zext/slice 路径 — mask 后不应被错误 zero-extend 到错误宽度
// ============================================================================

TEST_CASE("ch_uint<8>(256) zero-extended to 32-bit produces no 32'h100/8'h100",
          "[verilog][verilator][zext]") {
    std::string verilog = generate_verilog_within_ctx([] {
        ch_out<ch_uint<32>> out_port("wide");
        ch_uint<8> small(static_cast<uint64_t>(256));        // mask 后 8'h0
        auto widened = ch::core::zext<32>(small);              // 32-bit zero-extend
        out_port = widened;
    });

    // 关键:任何含 '32'h100' 或 '8'h100' 都说明 mask 没生效
    REQUIRE(verilog.find("32'h100") == std::string::npos);
    REQUIRE(verilog.find("8'h100") == std::string::npos);
}

// ============================================================================
// Gap D: Random fuzz — 随机 width (1..32) × value (0..2^width*4) 100 次
// ============================================================================

#include <random>

TEST_CASE("get_literal_str fuzz: random (width, value) produces no overflowing hex",
          "[verilog][verilator][fuzz]") {
    std::mt19937_64 rng(0xC0DE'CAFE'BABEULL);
    std::uniform_int_distribution<int> width_dist(1, 32);
    std::uniform_int_distribution<uint64_t> mul_dist(1, 4);

    int total = 0, overflowing_count = 0;
    for (int trial = 0; trial < 100; ++trial) {
        int width = width_dist(rng);
        uint64_t raw_value = (uint64_t{1} << width) * mul_dist(rng);

        std::string verilog = generate_verilog_within_ctx([&] {
            ch_out<ch_uint<32>> out_port("v");
            ch_uint<32> val(raw_value);
            out_port = val;
        });

        std::regex re32("32'h([0-9a-fA-F]+)");
        auto begin = std::sregex_iterator(verilog.begin(), verilog.end(), re32);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            std::string hex = (*it)[1].str();
            // width=32 max digits=8;9+ 必为溢出
            if (hex.size() > 8) {
                ++overflowing_count;
                INFO("Overflow trial=" + std::to_string(trial) +
                     " width=" + std::to_string(width) +
                     " raw_value=" + std::to_string(raw_value) +
                     " verilog_literal=" + hex);
            }
        }
        ++total;
    }
    REQUIRE(overflowing_count == 0);
    REQUIRE(total == 100);
}

// ============================================================================
// Gap A: TC-07 XorChain — codegen + interpreter 验证(真 verilator 仿真在
// test_verilator_e2e_harness.cpp 里覆盖 CounterFixture,这里专攻 XOR 链)
// ============================================================================

namespace {

class Tc07XorFixture : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> result;)

    Tc07XorFixture(ch::Component *p = nullptr,
                   const std::string &n = "top")
        : ch::Component(p, n), depth_(100) {}

    // ch_device ctor 内部立即 build → describe,所以 set_depth
    // 必须在 ch_device 构造**之前**调用。
    void set_depth(int d) { depth_ = d; }

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_uint<8> acc = ch_uint<8>(0);
        for (int i = 0; i < depth_; ++i) {
            acc = acc ^ ch_uint<8>(static_cast<uint64_t>(i));
        }
        io().result = acc;
    }

private:
    int depth_;
};

}  // namespace

TEST_CASE("TC-07 XOR chain depth=10/100/1000 generates valid Verilog "
          "(no 8'h<3-hex-digits>)",
          "[verilog][verilator][tc07][codegen]") {
    // 覆盖 spec Scenario "TC-07 depth=1000 XOR chain verilog passes --lint-only"
    for (int depth : {10, 100, 1000}) {
        std::string verilog = generate_verilog_within_ctx([&] {
            ch_out<ch_uint<8>> out_port("result");
            ch_uint<8> acc = ch_uint<8>(0);
            for (int i = 0; i < depth; ++i) {
                acc = acc ^ ch_uint<8>(static_cast<uint64_t>(i));
            }
            out_port = acc;
        });

        bool overflowing = has_overflowing_hex_literal(verilog, 8);
        INFO("depth=" + std::to_string(depth) +
             " overflowing=" + std::to_string(overflowing));
        REQUIRE_FALSE(overflowing);
    }
}

// ============================================================================
// 综合 e2e: get_literal_str 输出在 verilator --lint-only 下解析 (可选)
// ============================================================================

namespace {

int shell_run(const std::string &cmd) {
    return std::system(cmd.c_str());
}

bool verilator_on_path() {
    return shell_run("command -v verilator >/dev/null 2>&1") == 0;
}

}  // namespace

TEST_CASE("TC-07 depth=1000 XOR chain Verilog passes verilator --lint-only",
          "[verilog][verilator][tc07][slow]") {
    if (!verilator_on_path()) {
        SKIP("verilator not on PATH");
    }

    std::string verilog = generate_verilog_within_ctx([] {
        ch_out<ch_uint<8>> out_port("result");
        ch_uint<8> acc = ch_uint<8>(0);
        for (int i = 0; i < 1000; ++i) {
            acc = acc ^ ch_uint<8>(static_cast<uint64_t>(i));
        }
        out_port = acc;
    });

    char tmpl[] = "/tmp/cpphdl_lint_XXXXXX.v";
    int fd = ::mkstemps(tmpl, 2);
    REQUIRE(fd >= 0);
    ::write(fd, verilog.c_str(), verilog.size());
    ::close(fd);

    std::string cmd = std::string("verilator --lint-only -Wno-fatal ") + tmpl +
                      " >/tmp/lint.out 2>&1";
    int rc = shell_run(cmd);
    std::string lint_output;
    {
        std::ifstream ifs("/tmp/lint.out");
        std::stringstream ss;
        ss << ifs.rdbuf();
        lint_output = ss.str();
    }
    ::unlink(tmpl);

    INFO("verilator --lint-only exit code: " + std::to_string(rc));
    INFO("verilator output:\n" + lint_output);

    REQUIRE(lint_output.find("Too many digits") == std::string::npos);
}

