// tests/test_verif_history_primitives.cpp
//
// `.verif` 的**历史/序列判据原语**（ADR-076）—— `RECORD` + `EXPECT_SEQ_SELF` /
// `EXPECT_SEQ_STEP` / `EXPECT_SEQ_COUNT`。
//
// ⚠️ **本文件是「运行期同结论」的载体**（ADR-076 AC4）：
//   同一份 `.verif` 在 ① **IR 解释器**（`run_verif` + `Simulator`）与
//   ② **渲染出的 Verilog TB**（真 verilator 编译 + 运行）各跑一次
//   ⇒ ⇒ 逐条 `(check_id, PASS/FAIL)` **对**比对相等。
//   ⇒ ⇒ **不是**静态 `check_id` 集合比对（对序列敏感原语**结构性失明**）。
//
// ⚠️ **期望值 MUST 手推**（禁抄仿真输出）—— 且**两端一致不能当正确性证据**
//   （两侧由同一份 C++ 语义派生 ⇒ 同构缺陷会一致地错）⇒ ⇒ 每条构例都另带
//   **手推期望**（`want`）。
//
// ⚠️ **夹具的时序（手推）**：
//   · `cnt` 复位后 0，每拍 +1（`rst_n` 低 ⇒ 清零 · 高 ⇒ +1）
//   · `flag` = `(cnt & 1) == 0` ⇒ **周期 2**（1,0,1,0…）⇒ ⇒ **相位无关**：
//     「任意 4 个连续项里有 2 个 1」·「相隔 2 项相等」·「相隔 3 项不等」
//   · `konst` = 常量 3 ⇒ ⇒ 恒等 ⇒ ⇒ 「相隔任意项都相等」
//   ⇒ ⇒ 全部构例的期望**与起始相位无关** ⇒ ⇒ 判据稳。

#include "catch_amalgamated.hpp"

#include "codegen_verilog.h"
#include "device.h"
#include "external_tool_helpers.h"
#include "rtl_emit/manifest.h"
#include "rtl_emit/verif.h"
#include "simulator.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace ch::core;

namespace {

/// 被测设计（**最小** · 只提供判据需要的可观测面）
class HistDut : public ch::Component {
  public:
    __io(ch_in<ch_bool> rst_n{"rst_n"}; ch_in<ch_bool> en{"en"};
         ch_out<ch_uint<8>> cnt{"cnt"}; ch_out<ch_bool> flag{"flag"};
         ch_out<ch_uint<8>> konst{"konst"}; ch_out<ch_uint<128>> wide{"wide"};)

    ch_reg<ch_uint<8>> c{0_d, "c"};
    ch_reg<ch_uint<128>> w{0_d, "w"};

    HistDut(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        c->next = select(io().rst_n, c + ch_uint<8>(1_d), ch_uint<8>(0_d));
        io().cnt = c;
        const auto lowbit = c & ch_uint<8>(1_d);
        io().flag = (lowbit == ch_uint<8>(0_d));
        io().konst = ch_uint<8>(3_d);
        io().wide = w;
    }
};

/// 每个场景的**对齐前缀**：显式把计数器拉到已知起点。
///
/// ⚠️ 末尾的 `TICK 1` **必需**：`SET` 之后紧邻 `RECORD`/`EXPECT` 会被扫描器
///   拒绝（时序锚点纪律）⇒ ⇒ 前缀必须以一个 `TICK` 收尾。
constexpr const char *kPrelude = "SET rst_n 0\nTICK 2\nSET rst_n 1\nTICK 1\n";

constexpr const char *kHead = "@expect-src x.md#a\n@tb-module tb_hist\n";

std::string scenario_text(const std::string &body) {
    return std::string(kHead) + "@scenario s\n@anchor a\n" + body;
}

/// 一条结论 = (check_id, PASS|FAIL)
using Pairs = std::vector<std::pair<std::string, std::string>>;

/// 从 `CHECK <id> PASS|FAIL ...` 行抽 `(id, verdict)`
///
/// ⚠️ 比对粒度 = **对**，不是原始文本 —— 两端的 FAIL 文案本就不同。
Pairs parse_checks(const std::vector<std::string> &lines) {
    Pairs out;
    for (const auto &ln : lines) {
        if (ln.rfind("CHECK ", 0) != 0) {
            continue;
        }
        std::istringstream ss(ln);
        std::string tag, id, verdict;
        ss >> tag >> id >> verdict;
        if (id.empty() || verdict.empty()) {
            continue;
        }
        out.emplace_back(id, verdict.rfind("PASS", 0) == 0 ? "PASS" : "FAIL");
    }
    return out;
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

fs::path repo_root() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 4; ++i) {
        if (fs::exists(p / "CMakeLists.txt") && fs::exists(p / "examples")) {
            return p;
        }
        if (!p.has_parent_path()) {
            break;
        }
        p = p.parent_path();
    }
    return fs::current_path();
}

std::string verilator_path() {
    const fs::path local = repo_root() / "build" / "verilator-install" / "bin" / "verilator";
    if (fs::exists(local)) {
        return local.string();
    }
    if (ch_test::tool_available("verilator")) {
        return "verilator";
    }
    return {};
}

fs::path fresh_dir(const std::string &name) {
    const fs::path d = repo_root() / "build" / "tests" / "verif_history_tmp" / name;
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

/// ① IR 解释器侧：同一份 `.verif` 文本 ⇒ 结论对
Pairs run_interp(const std::string &text, std::string &error) {
    ch::ch_device<HistDut> dev;
    std::istringstream in(text);
    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError perr;
    if (!ch::rtl_emit::parse_verif(in, prog, perr)) {
        error = "解析失败（line " + std::to_string(perr.line) + "）：" + perr.message;
        return {};
    }
    const auto manifest = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    ch::Simulator sim(dev.context());
    const auto res = ch::rtl_emit::run_verif(prog, sim, dev.context(), manifest);
    if (!res.ok) {
        error = "解释器失败：" + res.error;
        return {};
    }
    return parse_checks(res.check_lines);
}

/// ② 渲染侧：同一份 `.verif` 文本 ⇒ 渲染 TB + 真 verilator 编译运行 ⇒ 结论对
Pairs run_rendered(const std::string &text, const std::string &tag, std::string &error) {
    const std::string vlt = verilator_path();
    if (vlt.empty()) {
        error = "verilator 不可用";
        return {};
    }
    ch::ch_device<HistDut> dev;
    std::istringstream in(text);
    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError perr;
    if (!ch::rtl_emit::parse_verif(in, prog, perr)) {
        error = "解析失败（line " + std::to_string(perr.line) + "）：" + perr.message;
        return {};
    }
    const auto manifest = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());

    const fs::path d = fresh_dir(tag);
    ch::toVerilog((d / "dut.v").string(), dev.instance(), ch::EmitOptions{});

    ch::rtl_emit::TbRenderOptions ro;
    ro.tb_module = "tb_hist";
    ro.dut_module = "top";
    ro.example_hint = "verif_history";
    std::ostringstream tb;
    std::string rerr;
    if (!ch::rtl_emit::render_tb(tb, prog, manifest, ro, rerr)) {
        error = "渲染失败：" + rerr;
        return {};
    }
    write_file(d / "tb_hist.sv", tb.str());

    const fs::path obj = d / "obj";
    std::error_code ec;
    fs::create_directories(obj, ec);
    const std::string cmd =
        vlt + " --binary --timing -CFLAGS -fcoroutines -Wno-fatal --x-assign unique" +
        " --top-module tb_hist -Mdir " + obj.string() + " " + (d / "tb_hist.sv").string() + " " +
        (d / "dut.v").string() + " -o sim >" + (d / "compile.log").string() + " 2>&1";
    if (ch_test::run_tool(cmd).exit_code != 0 || !fs::exists(obj / "sim")) {
        error = "verilator 编译失败（见 " + (d / "compile.log").string() + "）";
        return {};
    }
    const std::string run = (obj / "sim").string() + " 2>&1";
    const auto rr = ch_test::run_tool(run);
    std::istringstream out(rr.stdout_out);
    std::vector<std::string> lines;
    std::string ln;
    while (std::getline(out, ln)) {
        lines.push_back(ln);
    }
    return parse_checks(lines);
}

struct Case {
    const char *name;   // 构例名（同时是工作目录名）
    std::string body;   // `.verif` 场景体
    const char *want;   // **手推**期望结论（按顺序空格分隔：`PASS` / `FAIL`）
    const char *id0;    // **首条**断言的 `check_id`（= `<scenario>#0`）
};

// 记录 `konst` 8 次（常量 ⇒ 恒等）⇒ ⇒ 「相隔 lag 项相等」恒成立
constexpr const char *kRecKonst = "REPEAT 8\nTICK 1\nRECORD konst\nEND\n";
// 记录 `cnt` 8 次（每拍 +1 ⇒ 递增）⇒ ⇒ 「相隔 lag 项相等」不成立
constexpr const char *kRecCnt = "REPEAT 8\nTICK 1\nRECORD cnt\nEND\n";
// 记录 `flag` 8 次（周期 2 ⇒ 奇偶交替）⇒ ⇒ 相隔 2 项相等 · 相隔 3 项不等
constexpr const char *kRecFlag = "REPEAT 8\nTICK 1\nRECORD flag\nEND\n";
// 同时记录 `cnt` 与 `en`（`en` 恒为 1 ⇒ ⇒ Δcnt == 1 == en）
constexpr const char *kRecCntEn =
    "SET en 1\nREPEAT 6\nTICK 1\nRECORD cnt\nRECORD en\nEND\n";

const Case kCases[] = {
    // ① `EXPECT_SEQ_SELF` PASS：常量 ⇒ 相隔 4 项仍相等
    {"self_pass_konst", std::string(kPrelude) + kRecKonst + "EXPECT_SEQ_SELF konst 4 4\n",
     "PASS", "s#0"},
    // ② `EXPECT_SEQ_SELF` FAIL：递增量 ⇒ 相隔 4 项不等
    {"self_fail_cnt", std::string(kPrelude) + kRecCnt + "EXPECT_SEQ_SELF cnt 4 4\n", "FAIL", "s#0"},
    // ③ `EXPECT_SEQ_SELF` PASS（周期 2 ⇒ 相隔 2 项相等）—— **M1 的基线**
    {"self_pass_flag_lag2", std::string(kPrelude) + kRecFlag + "EXPECT_SEQ_SELF flag 4 2\n",
     "PASS", "s#0"},
    // ④ `EXPECT_SEQ_STEP` PASS：Δcnt == en（en 恒 1）
    {"step_pass", std::string(kPrelude) + kRecCntEn + "EXPECT_SEQ_STEP cnt en 4\n", "PASS", "s#0"},
    // ⑤ `EXPECT_SEQ_STEP` FAIL：Δcnt（=1）与 `flag`（周期 2 ⇒ 含 0）不符
    {"step_fail", std::string(kPrelude) + "SET en 1\nREPEAT 6\nTICK 1\nRECORD cnt\nRECORD flag\nEND\n"
                                         "EXPECT_SEQ_STEP cnt flag 4\n",
     "FAIL", "s#0"},
    // ⑥ `EXPECT_SEQ_COUNT` PASS：4 个连续 `flag` 里恰有 2 个 1
    {"count_pass", std::string(kPrelude) + kRecFlag + "EXPECT_SEQ_COUNT flag 1 4 2\n", "PASS", "s#0"},
    // ⑦ `EXPECT_SEQ_COUNT` FAIL：改成 3 ⇒ 不符
    {"count_fail", std::string(kPrelude) + kRecFlag + "EXPECT_SEQ_COUNT flag 1 4 3\n", "FAIL", "s#0"},
    // ⑧ **跨 scenario 不清空**：A 记录 8 次 · B 比较（⇒ 证明历史是**全局**的）
    {"cross_scenario",
     std::string(kHead) + "@scenario a\n@anchor a\n" + kPrelude + kRecKonst +
         "@scenario b\n@anchor a\nEXPECT_SEQ_SELF konst 4 4\n",
     "PASS", "b#0"},
    // ⑨ `EXPECT_SEQ_*` **不受** `SETTLE` 纪律约束（`SET` 之后**直接**断言）
    {"seq_ignores_settle",
     std::string(kHead) + "@scenario a\n@anchor a\n" + kPrelude + kRecFlag +
         "@scenario b\n@anchor a\nSET en 1\nEXPECT_SEQ_COUNT flag 1 4 2\n",
     "PASS", "b#0"},
};

std::vector<std::string> split_ws(const char *s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string tok;
    while (ss >> tok) {
        out.push_back(tok);
    }
    return out;
}

/// 扫描器负向：构造一段 `.verif` ⇒ 期望**扫描失败**且消息命中关键词
bool scan_fails(const std::string &text, const std::string &needle) {
    std::istringstream in(text);
    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError err;
    if (ch::rtl_emit::parse_verif(in, prog, err)) {
        return false;
    }
    return err.message.find(needle) != std::string::npos;
}

} // namespace

// ============================================================================
// 扫描器负向（AC2 · AC11 · AC13 · AC14 · AC15 · AC16 · AC18）
// ============================================================================
TEST_CASE("verif-history: 扫描器 fail-closed —— 参数 / 取值域 / 历史充足性 / 深度 / 纪律",
          "[verif][history]") {
    // 参数个数
    REQUIRE(scan_fails(scenario_text("RECORD\n"), "RECORD"));
    REQUIRE(scan_fails(scenario_text("RECORD a b\n"), "RECORD"));
    REQUIRE(scan_fails(scenario_text("EXPECT_SEQ_SELF a 4\n"), "EXPECT_SEQ_SELF"));
    REQUIRE(scan_fails(scenario_text("EXPECT_SEQ_STEP a b\n"), "EXPECT_SEQ_STEP"));
    REQUIRE(scan_fails(scenario_text("EXPECT_SEQ_COUNT a 1 4\n"), "EXPECT_SEQ_COUNT"));

    // 取值域：`n == 0` / `lag == 0` / `count > n`
    REQUIRE(scan_fails(scenario_text("REPEAT 4\nTICK 1\nRECORD konst\nEND\n"
                                     "EXPECT_SEQ_SELF konst 0 4\n"),
                       "n"));
    REQUIRE(scan_fails(scenario_text("REPEAT 4\nTICK 1\nRECORD konst\nEND\n"
                                     "EXPECT_SEQ_SELF konst 4 0\n"),
                       "lag"));
    REQUIRE(scan_fails(scenario_text("REPEAT 4\nTICK 1\nRECORD konst\nEND\n"
                                     "EXPECT_SEQ_COUNT konst 3 4 5\n"),
                       "count"));

    // 历史不足
    REQUIRE(scan_fails(scenario_text("REPEAT 4\nTICK 1\nRECORD konst\nEND\n"
                                     "EXPECT_SEQ_SELF konst 4 4\n"),
                       "历史不足"));
    // `EXPECT_SEQ_STEP` 的**两个端口各判**：`cnt` 够 · `en` 不够
    REQUIRE(scan_fails(scenario_text("REPEAT 6\nTICK 1\nRECORD cnt\nEND\n"
                                     "EXPECT_SEQ_STEP cnt en 4\n"),
                       "历史不足"));

    // 时序锚点纪律：`SET` 之后紧邻 `RECORD` ⇒ 拒绝
    REQUIRE(scan_fails(scenario_text("SET en 1\nRECORD en\n"), "SETTLE"));
    // ⚠️ **反面对照**：`SET` 之后**直接** `EXPECT_SEQ_COUNT` ⇒ **不**受该纪律约束
    {
        std::istringstream in(scenario_text("SET en 1\nEXPECT_SEQ_COUNT flag 1 4 2\n"));
        ch::rtl_emit::VerifProgram prog;
        ch::rtl_emit::VerifParseError err;
        // ⚠️ 这条会因为「历史不足」而失败 ⇒ ⇒ 但**消息**不是时序纪律 ⇒ ⇒ 用来区分
        REQUIRE_FALSE(ch::rtl_emit::parse_verif(in, prog, err));
        REQUIRE(err.message.find("SETTLE") == std::string::npos);
    }
}

TEST_CASE("verif-history: 深度上限 1024 —— 单 scenario 与**跨 scenario** 两形态",
          "[verif][history]") {
    // ① 单 scenario：1025 次 `RECORD` ⇒ 扫描期拒绝
    {
        std::string body;
        for (int i = 0; i < 1025; ++i) {
            body += "TICK 1\nRECORD konst\n";
        }
        REQUIRE(scan_fails(scenario_text(body), "上限"));
    }
    // ② **跨 scenario**：600 + 425 = 1025 ⇒ 仍拒绝（⇒ ⇒ 计数器**不是** per-scenario 重置）
    {
        std::string a;
        for (int i = 0; i < 600; ++i) {
            a += "TICK 1\nRECORD konst\n";
        }
        std::string b;
        for (int i = 0; i < 425; ++i) {
            b += "TICK 1\nRECORD konst\n";
        }
        const std::string text = std::string(kHead) + "@scenario a\n@anchor a\n" + a +
                                 "@scenario b\n@anchor a\n" + b;
        REQUIRE(scan_fails(text, "上限"));
    }
    // ③ 恰好 1024 ⇒ **通过**（上界是闭区间）
    {
        std::string body;
        for (int i = 0; i < 1024; ++i) {
            body += "TICK 1\nRECORD konst\n";
        }
        std::istringstream in(scenario_text(body));
        ch::rtl_emit::VerifProgram prog;
        ch::rtl_emit::VerifParseError err;
        REQUIRE(ch::rtl_emit::parse_verif(in, prog, err));
    }
}

TEST_CASE("verif-history: 宽端口（>64 位）⇒ 两端同源 fail-closed",
          "[verif][history]") {
    // 解释器侧：从同一个 manifest 查 width ⇒ ⇒ `wide`（128 位）⇒ 拒绝
    const std::string text = scenario_text("TICK 1\nRECORD wide\n");
    std::istringstream in(text);
    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError perr;
    REQUIRE(ch::rtl_emit::parse_verif(in, prog, perr));

    ch::ch_device<HistDut> dev;
    const auto manifest = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    ch::Simulator sim(dev.context());
    const auto res = ch::rtl_emit::run_verif(prog, sim, dev.context(), manifest);
    REQUIRE_FALSE(res.ok);
    REQUIRE(res.error.find("64") != std::string::npos);
}

TEST_CASE("verif-history: `RECORD` 不计入断言数 · `EXPECT_SEQ_*` 各计 1 条",
          "[verif][history]") {
    std::istringstream in(scenario_text(std::string(kPrelude) + kRecKonst +
                                        "EXPECT_SEQ_SELF konst 4 4\n"
                                        "EXPECT_SEQ_COUNT konst 3 4 2\n"));
    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError err;
    REQUIRE(ch::rtl_emit::parse_verif(in, prog, err));
    // 8 条 `RECORD`（0 条断言）+ 2 条 `EXPECT_SEQ_*`（各 1 条）
    REQUIRE(ch::rtl_emit::verif_check_count(prog) == 2);
    // `check_id` 按序编号（`s#0` / `s#1`）⇒ ⇒ `RECORD` 不占号
    const auto &ops = prog.scenarios[0].ops;
    std::vector<std::string> ids;
    for (const auto &op : ops) {
        if (!op.check_id.empty()) {
            ids.push_back(op.check_id);
        }
    }
    REQUIRE(ids.size() == 2);
    REQUIRE(ids[0] == "s#0");
    REQUIRE(ids[1] == "s#1");
}

// ============================================================================
// **运行期同结论**（AC4 · 本文件的核心判据）
// ============================================================================
TEST_CASE("verif-history: 两端（IR 解释器 / 渲染 TB）逐条结论相等",
          "[verif][history][verilator_e2e]") {
    if (verilator_path().empty()) {
        SKIP("verilator 不可用（本用例的渲染侧需要真编译）");
    }
    std::size_t idx = 0;
    for (const auto &c : kCases) {
        const std::string text = scenario_text(c.body);
        std::string ierr, rerr;
        const Pairs ip = run_interp(text, ierr);
        const Pairs rp = run_rendered(text, std::string(c.name) + "_" + std::to_string(idx++),
                                       rerr);
        INFO("case: " << c.name << " | body: " << c.body);
        INFO("interp error: " << ierr);
        INFO("render error: " << rerr);
        REQUIRE(ierr.empty());
        REQUIRE(rerr.empty());

        // ① **手推**期望（两端一致**不是**正确性证据 ⇒ 必须另有独立期望）
        const std::vector<std::string> want = split_ws(c.want);
        REQUIRE(ip.size() == want.size());
        for (std::size_t i = 0; i < want.size(); ++i) {
            INFO("check index: " << i);
            REQUIRE(ip[i].first == std::string(c.id0).substr(0, std::string(c.id0).find('#')) +
                                  "#" + std::to_string(i));
            REQUIRE(ip[i].second == want[i]);
        }
        // ② 两端**逐条**（id, 结论）相等
        REQUIRE(rp == ip);
    }
}

// ============================================================================
// **变异自证**（AC6）—— 变异投在夹具的 `.verif` 输入上（不动实现代码）
// ============================================================================
TEST_CASE("verif-history: 变异自证 —— 5 条变异各自必红",
          "[verif][history][verilator_e2e]") {
    if (verilator_path().empty()) {
        SKIP("verilator 不可用（本用例的渲染侧需要真编译）");
    }
    struct Mut {
        const char *name;
        std::string base;
        std::string mutated;
        const char *needle; // 变异体的**预期**（PASS/FAIL）或扫描期错误关键词
    };
    const Mut muts[] = {
        // M1：`EXPECT_SEQ_SELF` 的 lag 2 → 3（周期 2 ⇒ 相隔 3 项**不等** ⇒ 转红）
        {"M1_self_lag", std::string(kPrelude) + kRecFlag + "EXPECT_SEQ_SELF flag 4 2\n",
         std::string(kPrelude) + kRecFlag + "EXPECT_SEQ_SELF flag 4 3\n", "FAIL"},
        // M2：`EXPECT_SEQ_COUNT` 的 count 2 → 3
        {"M2_count", std::string(kPrelude) + kRecFlag + "EXPECT_SEQ_COUNT flag 1 4 2\n",
         std::string(kPrelude) + kRecFlag + "EXPECT_SEQ_COUNT flag 1 4 3\n", "FAIL"},
        // M3：`EXPECT_SEQ_STEP` 的两个端口**对调**（Δen == cnt ⇒ 不成立）
        {"M3_step_swap", std::string(kPrelude) + kRecCntEn + "EXPECT_SEQ_STEP cnt en 4\n",
         std::string(kPrelude) + kRecCntEn + "EXPECT_SEQ_STEP en cnt 4\n", "FAIL"},
        // M5：`EXPECT_SEQ_COUNT` 的 value 1 → 2（`flag` 取不到 2 ⇒ 计数为 0）
        {"M5_count_value", std::string(kPrelude) + kRecFlag + "EXPECT_SEQ_COUNT flag 1 4 2\n",
         std::string(kPrelude) + kRecFlag + "EXPECT_SEQ_COUNT flag 2 4 2\n", "FAIL"},
    };
    std::size_t idx = 0;
    for (const auto &m : muts) {
        const std::string b = scenario_text(m.base);
        const std::string mu = scenario_text(m.mutated);
        std::string e1, e2, e3, e4;
        const Pairs ib = run_interp(b, e1);
        const Pairs im = run_interp(mu, e2);
        const Pairs rb = run_rendered(b, std::string(m.name) + "_b_" + std::to_string(idx), e3);
        const Pairs rm = run_rendered(mu, std::string(m.name) + "_m_" + std::to_string(idx), e4);
        INFO("mutation: " << m.name);
        REQUIRE(e1.empty());
        REQUIRE(e2.empty());
        REQUIRE(e3.empty());
        REQUIRE(e4.empty());
        REQUIRE(ib.size() == 1);
        REQUIRE(im.size() == 1);
        REQUIRE(ib[0].second == "PASS"); // 基线是绿的
        REQUIRE(im[0].second == m.needle);
        REQUIRE(rb == ib);
        REQUIRE(rm == im);
        // 变异**真的**改变了结论（否则本构例对该变异无判别力）
        REQUIRE(ib[0].second != im[0].second);
        ++idx;
    }

    // M4：`SET` 之后紧邻 `RECORD` ⇒ **扫描期**转红（⇒ 时序锚点纪律**有判别力**）
    {
        const std::string mu = scenario_text("SET en 1\nRECORD en\n");
        std::istringstream in(mu);
        ch::rtl_emit::VerifProgram prog;
        ch::rtl_emit::VerifParseError err;
        REQUIRE_FALSE(ch::rtl_emit::parse_verif(in, prog, err));
        REQUIRE(err.message.find("SETTLE") != std::string::npos);
    }
}
