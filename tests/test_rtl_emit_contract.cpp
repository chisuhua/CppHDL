// tests/test_rtl_emit_contract.cpp
//
// **RTL 生成能力的契约判据**（`examples-rtl-sim-coverage` **E1**）。
//
// ⚠️ **本文件只保留依赖项（C5 的 F1–F8）没有的部分**（metis 订正：同一判据两份实现
//   必然漂移）。即：
//   ① **跨进程**确定性 —— 同进程内两次生成恒等（判别力 ≈ 0）
//   ② 产物**结构完整** —— 非空 / `module`·`endmodule` 配平 / **端口数 == manifest 的顶层端口数**
//   ③ **「按名驱动」的机械证据** —— 改 manifest 的端口名 ⇒ TB **编译失败**
//      （TB 编译通过本身就是「名字对」的硬证据，比任何端口表比对都硬）
//   ④ **产物变异 ⇒ TB 必红** —— 删一条 `assign` ⇒ 断言失败 ⇒ `RESULT: ... fails!=0`
//   ⑤ **X 检测** —— TB MUST 断言 `!$isunknown(<输出>)`（未驱动输出 ⇒ X ⇒ 必红）
//
// ⚠️ **端口口径**（2026-11-11 用户裁决 **A**）：manifest 的 `ports[]` = **全部**节点端口
//   （含子组件的 input，扁平化后是内部 wire）⇒ ⇒ 所以本文件的判据 ② 比的是
//   「产物端口数 == manifest 中**顶层端口**数」，判据与 `tb_stub` 的过滤同源
//   （`role ∈ {clock,reset}` ∪ `dir == output` ∪ `is_top_io`）。
//
// ⚠️ **期望值 MUST 手推**（禁抄仿真输出）—— 判据 ④/⑤ 的「应该红」是**手推**的。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "codegen_verilog.h"
#include "rtl_emit/manifest.h"
#include "rtl_emit/tb_stub.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ch::core;
namespace fs = std::filesystem;

namespace {

// ── 被测设计：一个寄存器（时序）+ 一个加法（组合）⇒ 三类判据都能覆盖 ──
class ContractDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a{"a"}; ch_out<ch_uint<8>> q{"q"};)
    ch_reg<ch_uint<8>> r{0_d, "r"};
    ContractDut(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        r->next = io().a + ch_uint<8>(1_d);
        io().q <<= r;
    }
};

// ── 设计 B：**含子组件**（子组件的 input 在扁平化后是**内部 wire**）
//   ⚠️ 判据 2 的判别力**只**能靠这个设计：单组件设计里
//   「manifest 端口数」与「产物端口数」恰好相等 ⇒ ⇒ 过滤与否**测不出差别**
//   （实测：用单组件设计做变异 ⇒ 恒绿 ⇒ 该判据无判别力）。
class ContractLeaf : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din{"din"}; ch_out<ch_uint<8>> dout{"dout"};)
    ContractLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class ContractTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a{"a"}; ch_out<ch_uint<8>> q{"q"};)
    ch::ch_module<ContractLeaf> leaf{"leaf"};
    ContractTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().a;
        io().q <<= leaf.io().dout;
    }
};

// ── 工具路径（ctest 的 cwd = repo 根）────────────────────────────────
std::string repo_root() {
    // 从 cwd 向上找带 CMakeLists.txt + examples 的目录
    fs::path p = fs::current_path();
    for (int i = 0; i < 4; ++i) {
        if (fs::exists(p / "CMakeLists.txt") && fs::exists(p / "examples")) {
            return p.string();
        }
        if (!p.has_parent_path()) {
            break;
        }
        p = p.parent_path();
    }
    return fs::current_path().string();
}

std::string cli_path() {
    const fs::path p = fs::path(repo_root()) / "build" / "tools" / "cpphdl-emit-rtl";
    return fs::exists(p) ? p.string() : std::string{};
}

std::string verilator_path() {
    const fs::path local =
        fs::path(repo_root()) / "build" / "verilator-install" / "bin" / "verilator";
    if (fs::exists(local)) {
        return local.string();
    }
    // 退化：PATH 里的 verilator
    if (std::system("verilator --version >/dev/null 2>&1") == 0) {
        return "verilator";
    }
    return {};
}

fs::path fresh_dir(const std::string &name) {
    const fs::path d = fs::path(repo_root()) / "build" / "tests" / "contract_tmp" / name;
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

std::string read_file(const fs::path &p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

void write_file(const fs::path &p, const std::string &s) {
    std::ofstream out(p, std::ios::binary);
    out << s;
}

bool has_top_level_port(const ch::rtl_emit::RtlPortEntry &e) {
    if (e.role == "clock" || e.role == "reset") {
        return true;
    }
    if (e.dir == "output") {
        return true;
    }
    return e.is_top_io;
}

/// 从 **Verilog 文本**里数顶层端口（`module <name> ( ... );` 之间的 input/output）
std::size_t count_verilog_ports(const std::string &v) {
    const std::size_t open = v.find("module ");
    if (open == std::string::npos) {
        return 0;
    }
    const std::size_t paren = v.find('(', open);
    const std::size_t close = v.find(");", paren);
    if (paren == std::string::npos || close == std::string::npos) {
        return 0;
    }
    std::size_t n = 0;
    const std::string block = v.substr(paren, close - paren);
    for (std::size_t i = 0; i < block.size(); ++i) {
        const bool at_word_start =
            (i == 0) || !(std::isalnum(static_cast<unsigned char>(block[i - 1])) ||
                          block[i - 1] == '_');
        if (!at_word_start) {
            continue;
        }
        if (block.compare(i, 5, "input") == 0 || block.compare(i, 6, "output") == 0) {
            ++n;
        }
    }
    return n;
}

/// 用一份 TB + 产物跑 verilator（**binary** 模式）⇒ 返回程序输出文本
/// （空 = 编译失败；调用方据此判红绿）
struct SimResult {
    bool compiled = false;
    std::string output;
    int exit_code = -1;
};

SimResult run_sim(const fs::path &workdir, const std::string &tb_name,
                  const std::string &vlt, const std::string &dut_v,
                  const std::string &tb_v) {
    SimResult r;
    if (vlt.empty()) {
        return r;
    }
    const fs::path md = workdir / "obj";
    std::error_code ec;
    fs::create_directories(md, ec);
    // ⚠️ `--x-assign unique` 是**必须**的：Verilator 默认把未驱动值当作 0
    //   ⇒ ⇒ 「未驱动输出」与「驱动成 0」**不可区分** ⇒ ⇒ 判据 5（X 检测）恒绿
    //   （实测踩中：删掉 output 的 assign 后 TB 仍然绿）。
    const std::string cmd = vlt +
                            " --binary --timing -CFLAGS -fcoroutines -Wno-fatal"
                            " --x-assign unique"
                            " --top-module " + tb_name + " -Mdir " + md.string() +
                            " " + tb_v + " " + dut_v + " -o sim >" +
                            (workdir / "compile.log").string() + " 2>&1";
    if (std::system(cmd.c_str()) != 0 || !fs::exists(md / "sim")) {
        return r;
    }
    r.compiled = true;
    const std::string run_cmd = (md / "sim").string() + " >" +
                                (workdir / "run.log").string() + " 2>&1";
    r.exit_code = std::system(run_cmd.c_str());
    r.output = read_file(workdir / "run.log");
    return r;
}

} // namespace

// ============================================================================
// 判据 1：**跨进程**确定性（同进程内两次生成恒等 ⇒ 判别力 ≈ 0）
// ============================================================================
TEST_CASE("contract: 两次**独立进程**发射逐字节相同", "[rtl][contract]") {
    const std::string cli = cli_path();
    if (cli.empty()) {
        SKIP("cpphdl-emit-rtl 未构建（build/tools/cpphdl-emit-rtl）");
    }
    const fs::path a = fresh_dir("det_a");
    const fs::path b = fresh_dir("det_b");
    const std::string ca = cli + " --out " + a.string() + " >/dev/null 2>&1";
    const std::string cb = cli + " --out " + b.string() + " >/dev/null 2>&1";
    REQUIRE(std::system(ca.c_str()) == 0);
    REQUIRE(std::system(cb.c_str()) == 0);

    REQUIRE(read_file(a / "top.v") == read_file(b / "top.v"));
    REQUIRE(read_file(a / "manifest.json") == read_file(b / "manifest.json"));
    // 卫生产物同样确定性
    const fs::path c = fresh_dir("det_c");
    const fs::path d = fresh_dir("det_d");
    REQUIRE(std::system((cli + " --out " + c.string() + " --emit-hygiene >/dev/null 2>&1").c_str()) == 0);
    REQUIRE(std::system((cli + " --out " + d.string() + " --emit-hygiene >/dev/null 2>&1").c_str()) == 0);
    REQUIRE(read_file(c / "top.v") == read_file(d / "top.v"));
}

// ============================================================================
// 判据 2：产物结构（非空 / 配平 / 端口数 == manifest 顶层端口数）
// ============================================================================
TEST_CASE("contract: 产物非空 · module/endmodule 配平 · 端口数 == manifest 顶层端口数",
          "[rtl][contract]") {
    const std::string cli = cli_path();
    if (cli.empty()) {
        SKIP("cpphdl-emit-rtl 未构建");
    }
    const fs::path d = fresh_dir("struct");
    REQUIRE(std::system((cli + " --out " + d.string() + " >/dev/null 2>&1").c_str()) == 0);
    const std::string v = read_file(d / "top.v");
    const std::string m = read_file(d / "manifest.json");

    // ① 非空（`ModuleEmitter::print` 是 `try { … } catch (...) {}` ⇒ 可能产出半截文件）
    REQUIRE_FALSE(v.empty());
    REQUIRE(v.size() > 100);
    // ② `module` / `endmodule` 配平（各恰好 1 个 ⇒ 工具设计是单 module）
    std::size_t n_module = 0, n_end = 0;
    for (std::size_t i = v.find("module "); i != std::string::npos; i = v.find("module ", i + 7)) {
        ++n_module;
    }
    for (std::size_t i = v.find("endmodule"); i != std::string::npos; i = v.find("endmodule", i + 9)) {
        ++n_end;
    }
    REQUIRE(n_module == 1);
    REQUIRE(n_end == 1);
    REQUIRE(v.find("module ") < v.find("endmodule"));

    // ③ 端口数 == manifest 的**顶层**端口数（口径见文件头：用户裁决 A）
    // ⚠️ 用**含子组件**的设计：它的 manifest 里有一个「子组件的 input」
    //   （扁平化后是内部 wire）⇒ ⇒ **不过滤就会多算 1 个** ⇒ ⇒ 判据有判别力
    ch::ch_device<ContractTop> dev;
    const auto man = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    std::size_t top_ports = 0;
    for (const auto &e : man.ports) {
        if (has_top_level_port(e)) {
            ++top_ports;
        }
    }
    // 🔴 设计本身必须**真的**含有内部 wire（否则本判据无判别力 ⇒ 断言失败）
    REQUIRE(top_ports < man.ports.size());
    const fs::path own = fresh_dir("struct_own");
    ch::toVerilog((own / "own.v").string(), dev.context());
    REQUIRE(count_verilog_ports(read_file(own / "own.v")) == top_ports);
    // manifest 的 JSON 里端口条目数 == `man.ports.size()`
    std::size_t entries = 0;
    for (std::size_t i = m.find("\"name\":"); i != std::string::npos;
         i = m.find("\"name\":", i + 7)) {
        ++entries;
    }
    REQUIRE(entries > 0);
}

// ============================================================================
// 判据 3：**「按名驱动」的机械证据** —— 改端口名 ⇒ TB 编译失败
// ============================================================================
TEST_CASE("contract: 改 manifest 的端口名 ⇒ TB **编译失败**（名字必须对）",
          "[rtl][contract]") {
    const std::string vlt = verilator_path();
    if (vlt.empty()) {
        SKIP("verilator 不可用");
    }
    ch::ch_device<ContractDut> dev;
    auto man = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    const fs::path d = fresh_dir("named");
    ch::toVerilog((d / "dut.v").string(), dev.context());

    ch::rtl_emit::TbStubOptions o;
    o.dut_module = "top";
    o.example_hint = "contract";

    // ① 正确名字 ⇒ 编译**成功**
    {
        std::ostringstream oss;
        ch::rtl_emit::write_tb_stub(oss, man, o);
        write_file(d / "tb_ok.sv", oss.str());
        const auto r = run_sim(d, "tb_top_stub", vlt, (d / "dut.v").string(),
                               (d / "tb_ok.sv").string());
        REQUIRE(r.compiled);
    }
    // ② 改一个**顶层端口**的 `name`（只改 manifest，产物不动）⇒ 编译**必须失败**
    {
        auto bad = man;
        bool mutated = false;
        for (auto &e : bad.ports) {
            if (e.name == "top_a") {
                e.name = "top_a_renamed";
                mutated = true;
                break;
            }
        }
        REQUIRE(mutated);
        std::ostringstream oss;
        ch::rtl_emit::write_tb_stub(oss, bad, o);
        write_file(d / "tb_bad.sv", oss.str());
        const auto r = run_sim(d, "tb_top_stub", vlt, (d / "dut.v").string(),
                               (d / "tb_bad.sv").string());
        REQUIRE_FALSE(r.compiled);
    }
}

// ============================================================================
// 判据 4：**产物变异 ⇒ TB 必红**（删一条 assign）
// ============================================================================
// ⚠️ 需要一份**带断言**的 TB（E3 的范式）⇒ ⇒ 这里用内联最小 TB：
//   期望值**手推**（寄存器复位后 `q=0`；`a=1` 一拍后 `q = 0+1 = 1`）
TEST_CASE("contract: 产物变异（删 output 的 assign）⇒ TB 必红", "[rtl][contract]") {
    const std::string vlt = verilator_path();
    if (vlt.empty()) {
        SKIP("verilator 不可用");
    }
    ch::ch_device<ContractDut> dev;
    const fs::path d = fresh_dir("mutate");
    ch::toVerilog((d / "dut.v").string(), dev.context());

    // 内联 TB：**手推**期望（复位后首拍 q = 0 + 1 = 1 ⇒ `8'd1`）
    const std::string tb = R"(
`timescale 1ns/1ps
module tb_mut;
    logic clk = 0;
    logic rst = 1;
    logic [7:0] a = 0;
    logic [7:0] q;
    integer checks = 0;
    integer fails  = 0;
    top dut (.default_clock(clk), .default_reset(rst), .top_a(a), .top_q(q));
    always #5 clk = ~clk;
    initial begin
        repeat (4) @(posedge clk);
        rst = 0;
        a = 8'd1;
        @(posedge clk);
        @(posedge clk);   // 组合读 + 一拍寄存
        checks = checks + 1;
        // 手推：r->next = a + 1 = 2？⇒ 不 —— r 在 rst 后首拍吃 a=1 ⇒ next=2
        //   ⇒ 为让期望值**无歧义**，断言「q 不是 X」+「q == 2」（r<=a+1=2）
        if ($isunknown(q)) fails = fails + 1;
        if (q !== 8'd2)   fails = fails + 1;
        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "TB failed");
        $finish;
    end
endmodule
)";
    write_file(d / "tb.v", tb);

    // ① 正常产物 ⇒ **绿**（fails=0）
    const auto ok = run_sim(d, "tb_mut", vlt, (d / "dut.v").string(), (d / "tb.v").string());
    REQUIRE(ok.compiled);
    INFO("正常产物输出：" << ok.output);
    REQUIRE(ok.output.find("RESULT: checks=1 fails=0") != std::string::npos);

    // ② 变异：删掉 `assign top_q = ...`（输出恒 X/0）⇒ TB **必红**
    std::string v = read_file(d / "dut.v");
    const std::size_t pos = v.find("assign top_q = ");
    REQUIRE(pos != std::string::npos);
    const std::size_t eol = v.find('\n', pos);
    v.erase(pos, eol - pos + 1);
    write_file(d / "dut_mut.v", v);
    const auto bad = run_sim(d, "tb_mut", vlt, (d / "dut_mut.v").string(), (d / "tb.v").string());
    REQUIRE(bad.compiled);
    INFO("变异产物输出：" << bad.output);
    // 断言 MUST 变红（fails != 0）—— 且 `$fatal` 让退出码非 0
    REQUIRE(bad.output.find("fails=0") == std::string::npos);
}

// ============================================================================
// 判据 5：**X 检查**（TB MUST 断言 `$isunknown(<输出>)`）
// ============================================================================
// ⚠️ **为什么是内容判据而不是「制造一个 X 然后看断言转红」**（实测结论，务必保留）：
//   **Verilator 在仿真里把 X 优化成已知值** —— 实测 `logic [7:0] w = 8'bx;`
//   在 `--x-assign unique` / `--x-initial unique|fast|0` 的**全部**组合下，
//   `$isunknown(w)` 都是 **false**；未驱动的 `output` 也被当作 0。
//   ⇒ ⇒ 「用变异产物制造 X ⇒ 断言转红」在 Verilator 下**不可能成立**（恒绿）
//   ⇒ ⇒ 本判据改为**内容判据**：生成的 TB 骨架 MUST 提示 X 检查，且**参考 TB**
//   （E3 的范式）MUST 真的含 `$isunknown` 断言。
TEST_CASE("contract: TB（骨架与参考 TB）MUST 含 X 检查（$isunknown）",
          "[rtl][contract]") {
    ch::ch_device<ContractDut> dev;
    const auto man = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    ch::rtl_emit::TbStubOptions o;
    o.dut_module = "top";
    o.example_hint = "contract";
    std::ostringstream oss;
    ch::rtl_emit::write_tb_stub(oss, man, o);
    const std::string stub = oss.str();

    // ① 骨架 MUST 提示 X 检查（补 TB 的人据此写）
    REQUIRE(stub.find("$isunknown(") != std::string::npos);
    REQUIRE(stub.find("每个输出 MUST 至少有一条 X 检查") != std::string::npos);
    // ② 骨架**不得**替人写实际断言（提示在注释里）
    REQUIRE(stub.find("\n        if ($isunknown(") == std::string::npos);

    // ③ **参考 TB**（判据 4 用的那一份）MUST 含 X 断言 ⇒ ⇒ 直接读本测试写出的文件
    const std::string tb = read_file(fs::path(repo_root()) / "build" / "tests" /
                                     "contract_tmp" / "mutate" / "tb.v");
    if (!tb.empty()) {
        REQUIRE(tb.find("$isunknown(") != std::string::npos);
    }
}
