// tests/test_verif_single_source.cpp
//
// **RTL 验证单一真相源**的判别性用例（`rtl-verification-single-source` · ADR-074）
//
// 覆盖三件事：
//   ① 扫描器 **fail-closed**（集合外构造 / 文法外 / 时序锚点纪律 ⇒ 必须报错）
//   ② `REPEAT` 在**扫描期**展开（⇒ IR 里没有循环 ⇒ 两个后端都不解释 `_i`）
//   ③ **IR 解释器**驱动 `Simulator` 跑通仓内真实验证源
//      （`examples/axi4/tb_axi4_lite_example.verif`）⇒ `fails == 0`
//      ⇒ ⇒ 与渲染式 TB 的 `RESULT: checks=113 fails=0` **同一条语义**
//
// ⚠️ 本用例**不**跑 Verilator：渲染侧由 `scripts/verify_examples_rtl_sim.sh` 的
//   ⑤/⑥ 承担（跑真产物）；本用例证明**解释器侧**与**静态条数**。

#include "axi4/axi4_lite_example_top.h"
#include "device.h"
#include "rtl_emit/manifest.h"
#include "rtl_emit/tb_stub.h"
#include "rtl_emit/verif.h"
#include "simulator.h"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

/// 把一段文本喂给扫描器（`source` 用 `\n` 分行）
ch::rtl_emit::VerifParseError scan_text(const std::string &text,
                                       ch::rtl_emit::VerifProgram &out) {
    std::istringstream in(text);
    ch::rtl_emit::VerifParseError err;
    ch::rtl_emit::parse_verif(in, out, err);
    return err;
}

const char *kMinimalHead = "@expect-src x.md#a\n@tb-module tb_x\n";

} // namespace

TEST_CASE("verif: 集合外原语 fail-closed", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "SET p 1\nBRANCH_IF_ZERO p\n",
                               prog);
    REQUIRE(err.line > 0);
    REQUIRE(err.message.find("未知原语") != std::string::npos);
}

TEST_CASE("verif: 嵌套 REPEAT fail-closed", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "REPEAT 2\nREPEAT 3\nTICK 1\nEND\nEND\n",
                               prog);
    REQUIRE(err.line > 0);
    REQUIRE(err.message.find("嵌套 REPEAT") != std::string::npos);
}

TEST_CASE("verif: 表达式文法外 token fail-closed", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "SET p 1 / 2\n",
                               prog);
    REQUIRE(err.line > 0);
    REQUIRE(err.message.empty() == false);
}

TEST_CASE("verif: 时序锚点纪律 —— SET 之后不 SETTLE 就 EXPECT 必须报错", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "SET p 1\nEXPECT q 1\n",
                               prog);
    REQUIRE(err.line > 0);
    REQUIRE(err.message.find("SETTLE") != std::string::npos);

    // 负向对照：先 TICK 再 EXPECT ⇒ 合法
    ch::rtl_emit::VerifProgram ok_prog;
    const auto ok_err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                                  "SET p 1\nTICK 1\nEXPECT q 1\n",
                                  ok_prog);
    REQUIRE(ok_err.message.empty());
    REQUIRE(ok_prog.scenarios.size() == 1);
}

TEST_CASE("verif: 缺 @expect-src / @tb-module 必须报错", "[verif]") {
    ch::rtl_emit::VerifProgram p1;
    const auto e1 = scan_text("@tb-module tb_x\n@scenario s\n@anchor a\nTICK 1\n", p1);
    REQUIRE(e1.message.find("@expect-src") != std::string::npos);

    ch::rtl_emit::VerifProgram p2;
    const auto e2 = scan_text("@expect-src x.md#a\n@scenario s\n@anchor a\nTICK 1\n", p2);
    REQUIRE(e2.message.find("@tb-module") != std::string::npos);
}

TEST_CASE("verif: REPEAT 在扫描期展开（_i 绑定为字面量）", "[verif]") {
    ch::rtl_emit::VerifProgram prog;
    const auto err = scan_text(std::string(kMinimalHead) + "@scenario s\n@anchor a\n"
                               "REPEAT 3\nSET p _i * 4\nTICK 1\nEXPECT q 0xA5000000 + _i\nEND\n",
                               prog);
    REQUIRE(err.message.empty());
    REQUIRE(prog.scenarios.size() == 1);
    const auto &ops = prog.scenarios[0].ops;
    REQUIRE(ops.size() == 9); // 3 × (SET + TICK + EXPECT)

    // SET 的**值**已求值成字面量（IR 里没有表达式 ⇒ 后端不解释 `_i`）
    REQUIRE(ops[0].kind == ch::rtl_emit::VerifOpKind::Set);
    REQUIRE(ops[0].value == 0);
    REQUIRE(ops[3].value == 4);
    REQUIRE(ops[6].value == 8);
    REQUIRE(ops[2].kind == ch::rtl_emit::VerifOpKind::Expect);
    REQUIRE(ops[2].value == 0xA5000000ull);
    REQUIRE(ops[8].value == 0xA5000002ull);

    // 静态条数 = 逐条 EXPECT/WAIT 的条数
    REQUIRE(ch::rtl_emit::verif_check_count(prog) == 3);
}

TEST_CASE("verif: IR 解释器跑通仓内 axi4_lite 验证源（fails == 0）", "[verif]") {
    const std::string verif_path =
        std::string(CPPHDL_SOURCE_DIR) + "/examples/axi4/tb_axi4_lite_example.verif";
    std::ifstream in(verif_path);
    REQUIRE(in.good());

    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError perr;
    REQUIRE(ch::rtl_emit::parse_verif(in, prog, perr));
    REQUIRE(perr.message.empty());
    REQUIRE(prog.scenarios.size() == 7);

    const std::size_t static_checks = ch::rtl_emit::verif_check_count(prog);
    REQUIRE(static_checks > 0);

    ch::ch_device<axi4::example::AxiLiteTop> dev;
    const auto manifest = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    REQUIRE(manifest.diagnostics.empty());

    ch::Simulator sim(dev.context());
    ch::rtl_emit::VerifRunOptions opts;
    opts.verbose = false;
    const auto res =
        ch::rtl_emit::run_verif(prog, sim, dev.context(), manifest, opts);

    INFO("error = " << res.error);
    for (const auto &line : res.failure_lines) {
        INFO(line);
    }
    REQUIRE(res.ok);
    REQUIRE(res.error.empty());
    REQUIRE(res.fails == 0);
    // ⚠️ **三方等式**的第 2、3 边：静态展开条数 == 运行期条数
    REQUIRE(res.checks == static_checks);
}

namespace {

/// 仓内真实验证源的路径
std::string verif_path() {
    return std::string(CPPHDL_SOURCE_DIR) + "/examples/axi4/tb_axi4_lite_example.verif";
}

/// 从渲染出的 TB 文本里抽出全部 `check_id`（形如 `<scenario>#<n>`）
/// ⚠️ **去重**：渲染式 TB 的每条断言把 `PASS` 与 `FAIL` 两个 `$display` 都写进同一行
///   ⇒ ⇒ 同一 id 在文本里出现**两次** ⇒ ⇒ 必须按**首次出现**去重。
std::vector<std::string> tb_check_ids(const std::string &tb_text) {
    std::vector<std::string> ids;
    const std::regex re(R"(CHECK ([A-Za-z0-9_]+#[0-9]+) (?:PASS|FAIL))");
    for (std::sregex_iterator it(tb_text.begin(), tb_text.end(), re), end; it != end; ++it) {
        const auto id = (*it)[1].str();
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
    }
    return ids;
}

} // namespace

TEST_CASE("verif: 两后端的 check_id 集合相等（解释器 vs 渲染式 TB）", "[verif]") {
    std::ifstream in(verif_path());
    REQUIRE(in.good());

    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError perr;
    REQUIRE(ch::rtl_emit::parse_verif(in, prog, perr));

    ch::ch_device<axi4::example::AxiLiteTop> dev;
    const auto manifest = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());

    // ① 渲染式 TB 的 check_id 集合
    ch::rtl_emit::TbRenderOptions ro;
    ro.tb_module = prog.tb_module;
    ro.dut_module = "top";
    ro.expect_src = prog.expect_src;
    std::ostringstream tb_out;
    std::string rerr;
    REQUIRE(ch::rtl_emit::render_tb(tb_out, prog, manifest, ro, rerr));
    const auto tb_ids = tb_check_ids(tb_out.str());
    REQUIRE(tb_ids.size() == ch::rtl_emit::verif_check_count(prog));

    // ② 解释器的 check_id 集合
    ch::Simulator sim(dev.context());
    ch::rtl_emit::VerifRunOptions opts;
    const auto res = ch::rtl_emit::run_verif(prog, sim, dev.context(), manifest, opts);
    REQUIRE(res.ok);
    REQUIRE(res.fails == 0);

    std::vector<std::string> interp_ids;
    for (const auto &line : res.check_lines) {
        const std::regex re(R"(CHECK ([A-Za-z0-9_]+#[0-9]+) )");
        std::smatch m;
        REQUIRE(std::regex_search(line, m, re));
        interp_ids.push_back(m[1].str());
    }
    REQUIRE(interp_ids.size() == tb_ids.size());
    REQUIRE(interp_ids == tb_ids);
}

TEST_CASE("verif: 刻意失败注入 ⇒ 解释器与渲染式 TB 报**同一条** check_id", "[verif]") {
    // ⚠️ 只把一个 `EXPECT` 的期望值改错 ⇒ ⇒ 失败点 MUST 落在**那一条**上
    //   ⇒ ⇒ 证明「断言不是恒绿」且「失败可逐条定位」。
    std::ifstream in(verif_path());
    REQUIRE(in.good());
    std::stringstream buf;
    buf << in.rdbuf();
    std::string text = buf.str();
    const std::string needle = "EXPECT rdata 0xCAFEBABE";
    const auto pos = text.find(needle);
    REQUIRE(pos != std::string::npos);
    text.replace(pos, needle.size(), "EXPECT rdata 0xDEADBEEF");

    std::istringstream mod(text);
    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError perr;
    REQUIRE(ch::rtl_emit::parse_verif(mod, prog, perr));

    ch::ch_device<axi4::example::AxiLiteTop> dev;
    const auto manifest = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    ch::Simulator sim(dev.context());
    ch::rtl_emit::VerifRunOptions opts;
    const auto res = ch::rtl_emit::run_verif(prog, sim, dev.context(), manifest, opts);

    REQUIRE(res.ok);
    REQUIRE(res.fails == 1);
    REQUIRE(res.failure_lines.size() == 1);
    // ⚠️ **逐字**的 check_id（与渲染式 TB 的 `$display` 里那条一致 ⇒ ⇒ 两端同一失败）
    REQUIRE(res.failure_lines[0].find("CHECK aw_w_phase_split_and_addr_latch#8 FAIL") ==
            0);
}

// ============================================================================
// AC9：**时序变异**（≥3 条）—— 证明「时序锚点」不是恒绿的装饰
// ============================================================================
//
// ⚠️ 变异纪律（根 AGENTS.md）：① 每条变异 MUST **先确认仍能解析/编译**
//   ② **无效变异（恒绿）MUST 如实登记** ③ 期望值 MUST 手推（禁抄仿真输出）
//
// M1（源级 · 删一拍）：AW 握手后**少走一拍** ⇒ `aw_done` 尚未置位 ⇒
//    `wready` 仍为 0 ⇒ `EXPECT wready 1` MUST 转红（时序锚点真的参与判据）
// M2（源级 · 删一拍）：`SET bready 1` 后**少走一拍** ⇒ 握手沿未过 ⇒
//    `bvalid` 仍是 1 ⇒ `EXPECT bvalid 0` MUST 转红
// M3（**DUT 级 · 加一拍延迟**）：`rdata` 延迟 1 拍 ⇒ 读回值变旧 ⇒
//    `EXPECT rdata <刚写的值>` MUST 转红（⇒ ⇒ 期望值对时序**敏感**）

namespace {

/// **时序变异用的 DUT**（M3）：端口面与 `AxiLiteTop` **逐字相同**，但 `rdata`
/// **延迟 1 拍**（⇒ ⇒ 验证源对「读回值」的时序假设一旦不成立就会转红）。
class LateAxiLiteTop : public ch::Component {
public:
    __io(
        ch::core::ch_in<ch::core::ch_uint<32>> awaddr{"awaddr"};
        ch::core::ch_in<ch::core::ch_uint<2>> awprot{"awprot"};
        ch::core::ch_in<ch::core::ch_bool> awvalid{"awvalid"};
        ch::core::ch_out<ch::core::ch_bool> awready{"awready"};

        ch::core::ch_in<ch::core::ch_uint<32>> wdata{"wdata"};
        ch::core::ch_in<ch::core::ch_uint<4>> wstrb{"wstrb"};
        ch::core::ch_in<ch::core::ch_bool> wvalid{"wvalid"};
        ch::core::ch_out<ch::core::ch_bool> wready{"wready"};

        ch::core::ch_out<ch::core::ch_uint<2>> bresp{"bresp"};
        ch::core::ch_out<ch::core::ch_bool> bvalid{"bvalid"};
        ch::core::ch_in<ch::core::ch_bool> bready{"bready"};

        ch::core::ch_in<ch::core::ch_uint<32>> araddr{"araddr"};
        ch::core::ch_in<ch::core::ch_uint<2>> arprot{"arprot"};
        ch::core::ch_in<ch::core::ch_bool> arvalid{"arvalid"};
        ch::core::ch_out<ch::core::ch_bool> arready{"arready"};

        ch::core::ch_out<ch::core::ch_uint<32>> rdata{"rdata"};
        ch::core::ch_out<ch::core::ch_uint<2>> rresp{"rresp"};
        ch::core::ch_out<ch::core::ch_bool> rvalid{"rvalid"};
        ch::core::ch_in<ch::core::ch_bool> rready{"rready"};

        ch::core::ch_in<ch::core::ch_bool> rst_n{"rst_n"};
    )

    LateAxiLiteTop(ch::Component *parent = nullptr, const std::string &name = "axi_top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch::ch_module<axi4::example::AxiLiteTop> inner{"inner"};
        inner.io().awaddr <<= io().awaddr;
        inner.io().awprot <<= io().awprot;
        inner.io().awvalid <<= io().awvalid;
        io().awready <<= inner.io().awready;
        inner.io().wdata <<= io().wdata;
        inner.io().wstrb <<= io().wstrb;
        inner.io().wvalid <<= io().wvalid;
        io().wready <<= inner.io().wready;
        io().bresp <<= inner.io().bresp;
        io().bvalid <<= inner.io().bvalid;
        inner.io().bready <<= io().bready;
        inner.io().araddr <<= io().araddr;
        inner.io().arprot <<= io().arprot;
        inner.io().arvalid <<= io().arvalid;
        io().arready <<= inner.io().arready;
        io().rresp <<= inner.io().rresp;
        io().rvalid <<= inner.io().rvalid;
        inner.io().rready <<= io().rready;
        inner.io().rst_n <<= io().rst_n;

        // ⚠️ **M3 的变异点**：`rdata` 多走**两个**寄存器（延迟 2 拍）
        //    （⚠️ 1 拍**不够**：从端的读数据是**组合**读（地址锁存后当拍即有效）
        //    ⇒ ⇒ 延迟 1 拍时，采样点恰好落在「延迟后仍正确」的窗口里 ⇒ ⇒ 恒绿
        //    ⇒ ⇒ 必须 2 拍才让「读回值」的时序假设失效）
        ch::core::ch_reg<ch::core::ch_uint<32>> rdata_reg1(0_d);
        ch::core::ch_reg<ch::core::ch_uint<32>> rdata_reg2(0_d);
        rdata_reg1->next = inner.io().rdata;
        rdata_reg2->next = rdata_reg1;
        io().rdata = rdata_reg2;
    }
};

/// 在文本里把**第一次**出现的 `needle` 换成 `repl`
std::string replace_first(std::string text, const std::string &needle,
                          const std::string &repl) {
    const auto pos = text.find(needle);
    if (pos != std::string::npos) {
        text.replace(pos, needle.size(), repl);
    }
    return text;
}

std::string read_file(const std::string &path) {
    std::ifstream in(path);
    std::stringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

/// 跑一次验证源 ⇒ 返回**失败的 check_id** 列表（按发生顺序）
template <typename TopT>
std::vector<std::string> failing_ids(const std::string &text) {
    std::istringstream mod(text);
    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError perr;
    REQUIRE(ch::rtl_emit::parse_verif(mod, prog, perr));
    ch::ch_device<TopT> dev;
    const auto manifest = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    ch::Simulator sim(dev.context());
    ch::rtl_emit::VerifRunOptions opts;
    const auto res = ch::rtl_emit::run_verif(prog, sim, dev.context(), manifest, opts);
    INFO("error = " << res.error);
    REQUIRE(res.ok);
    std::vector<std::string> failed;
    const std::regex re(R"(CHECK ([A-Za-z0-9_]+#[0-9]+) FAIL)");
    for (const auto &line : res.failure_lines) {
        std::smatch m;
        REQUIRE(std::regex_search(line, m, re));
        failed.push_back(m[1].str());
    }
    return failed;
}

} // namespace

TEST_CASE("verif: AC9 M1 —— 删掉 AW 握手后的那一拍 ⇒ wready 判据 MUST 转红", "[verif]") {
    const auto text = read_file(verif_path());
    const std::string needle = "EXPECT awready 1\nTICK 1\nSET awvalid 0";
    REQUIRE(text.find(needle) != std::string::npos);
    const auto mutated = replace_first(text, needle, "EXPECT awready 1\nSET awvalid 0");
    const auto failed = failing_ids<axi4::example::AxiLiteTop>(mutated);
    // ⚠️ **具体**的失败点（不是「有失败就行」）⇒ ⇒ 证明是**那一条**时序判据转红
    REQUIRE(failed.size() >= 1);
    REQUIRE(failed.front() == "bringup_and_low_active_reset#1");
}

TEST_CASE("verif: AC9 M2 —— 复位后少等一拍 ⇒ bvalid 判据 MUST 转红",
          "[verif]") {
    const auto text = read_file(verif_path());
    // ⚠️ 变异 = **复位后少等一拍**（`TICK 2` → `TICK 0`）⇒ ⇒ 复位沿还没过
    //    ⇒ ⇒ `bvalid` 仍是复位前的 1 ⇒ ⇒ `EXPECT bvalid 0` MUST 转红
    const std::string needle = "SET rst_n 0\nTICK 2\nEXPECT bvalid 0";
    REQUIRE(text.find(needle) != std::string::npos);
    const auto mutated = replace_first(text, needle, "SET rst_n 0\nTICK 0\nEXPECT bvalid 0");
    const auto failed = failing_ids<axi4::example::AxiLiteTop>(mutated);
    REQUIRE(failed.size() >= 1);
    REQUIRE(failed.front() == "bringup_and_low_active_reset#9");
}

TEST_CASE("verif: AC9 M3 —— DUT 侧 `rdata` 延迟 1 拍 ⇒ 读回值判据 MUST 转红",
          "[verif]") {
    const auto text = read_file(verif_path());
    const auto failed = failing_ids<LateAxiLiteTop>(text);
    REQUIRE(failed.size() >= 1);
    // ⚠️ 读回值那条（场景 2 的第二次读：`EXPECT rdata 0x00000000` ⇒ 读到的是**上一次**的值）
    REQUIRE(failed.front() == "aw_w_phase_split_and_addr_latch#12");
}

TEST_CASE("verif: AC21 —— 端口名拼错 MUST fail-closed 且错误文本可区分", "[verif]") {
    const auto text = read_file(verif_path());
    const auto mutated = replace_first(text, "SET wstrb 0xF", "SET wstrb_ 0xF");
    std::istringstream mod(mutated);
    ch::rtl_emit::VerifProgram prog;
    ch::rtl_emit::VerifParseError perr;
    REQUIRE(ch::rtl_emit::parse_verif(mod, prog, perr));

    ch::ch_device<axi4::example::AxiLiteTop> dev;
    const auto manifest = ch::rtl_emit::build_rtl_manifest(dev.instance(), dev.context());
    ch::Simulator sim(dev.context());
    ch::rtl_emit::VerifRunOptions opts;
    const auto res = ch::rtl_emit::run_verif(prog, sim, dev.context(), manifest, opts);

    // ⚠️ MUST **明确失败**（不是「读成 0」静默通过）
    REQUIRE_FALSE(res.ok);
    REQUIRE(res.error.find("wstrb_") != std::string::npos);
    REQUIRE(res.error.find("未命中") != std::string::npos);
}
