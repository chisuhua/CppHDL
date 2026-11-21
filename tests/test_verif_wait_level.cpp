// tests/test_verif_wait_level.cpp
//
// `WAIT` 的**电平形参**（`WAIT <port> [<high|low>] <max>`）—— ADR-074 §7.2 **D2′**
// + 2026-11-20 dated 订正（`high` ≡ **非零** ≡ 缺省，**不是** N 位全 1）。
//
// ⚠️ **本文件是「运行期同结论」的载体**（ADR-074 §7.6 的 AC-D8′-1）：
//   同一份 `.verif` 在 ① **IR 解释器**（`run_verif` + `Simulator`）与
//   ② **渲染出的 Verilog TB**（真 verilator 编译 + 运行）各跑一次
//   ⇒ ⇒ 逐条 `(check_id, PASS/FAIL)` **对**比对相等。
//   ⇒ ⇒ **不是**静态 `check_id` 集合比对（那样对时序敏感原语**结构性失明**：
//   16 拍窗口会吸收 1 拍偏移 ⇒ 两端各自全绿）。
//
// ⚠️ **期望值 MUST 手推**（禁抄仿真输出）—— 且**两端一致不能当正确性证据**
//   （两侧由同一份 C++ 语义派生 ⇒ 同构缺陷会一致地错）⇒ ⇒ 每条构例都另带
//   **手推期望**（`want`）。
//
// ⚠️ 夹具的时序（**手推**）：`cnt` 复位后 0，每拍 +1 ⇒ 第 k 拍 `cnt == k`
//   · `sig = (cnt >= 8) & (cnt < 16)` ⇒ 第 1..7 拍 0 · 第 8..15 拍 1 · 第 16 拍起 0
//   · `val = cnt`（8 位）⇒ 第 1 拍 = 1（**非零但非全 1** ⇒ 用于判别 `high` 的定义）
//   · `p3 = (cnt == 3)`（1 位）⇒ **只有第 3 拍为高** ⇒ 用于钉死「命中拍 = 第 k 拍」
//     （AC9：若命中被推迟到第 4 拍，`WAIT p3 high 3` 必 FAIL ⇒ 该构例有判别力）

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

class WaitLevelDut : public ch::Component {
public:
    __io(ch_in<ch_bool> rst_n{"rst_n"}; ch_out<ch_bool> sig{"sig"}; ch_out<ch_bool> p3{"p3"};
         ch_out<ch_uint<8>> val{"val"};)

    ch_reg<ch_uint<8>> cnt{0_d, "cnt"};

    WaitLevelDut(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // `rst_n` 低有效 ⇒ 计数器**清零**（`next = 0`，不是「保持」—— 保持会把前置序列的
        // 1 拍偏移带进场景）⇒ 场景用 `SET` + `TICK` 显式对齐起点
        cnt->next = select(io().rst_n, cnt + 1_d, ch_uint<8>(0_d));
        io().sig = (cnt >= 8_d) & (cnt < 16_d);
        io().p3 = (cnt == 3_d);
        io().val = cnt;
    }
};

/// 每个场景的**对齐前缀**：显式把计数器拉到已知起点。
///
/// ⚠️ **为什么必需**（2026-11-20 实测，两条**前置**差异，非 `WAIT` 引入）：
///   ① ~~渲染式 TB 的 `write_tb_reset_pulse` 末尾多一个 `@(posedge clk)` 而解释器的
///      `apply_frame_reset` 只做 `sim.reset()` ⇒ 依赖「复位后**绝对**拍数」的场景差 1 拍~~
///      ⇒ ⇒ **2026-11-21 已修**（`src/rtl_emit/verif_run.cpp` 的 `apply_frame_reset`
///      末尾补 `sim.tick()` ⇒ 两侧相位一致）⇒ ⇒ 本夹具**仍**保留显式前缀
///      （不依赖隐式框架复位的拍数 ⇒ 场景自足）。
///   ② `RESET` 原语的**去断言与最后一个 `@(posedge)` 同拍**
///      （`src/rtl_emit/verif_render.cpp` 的 `repeat (n) @(posedge); name = <deassert>;`
///      **无 `#1`**）⇒ 与 DUT 的 `always_ff` 竞态 ⇒ Verilator 实测「该沿已采样到去断言」
///      ⇒ 与解释器（`set → tick n → set`，无竞态）**差 1 拍**。
///   ⇒ ⇒ 故本夹具用 **`SET` + `TICK`**（`TICK` 的发射末尾带 `#1` ⇒ 无竞态）显式对齐。
///   ⇒ ⇒ 本文件判的是 **`WAIT` 的语义**，不是前置序列的对齐
///   （后者已作为**独立发现**登记，见 `tasks.md` 的 B0）。
constexpr const char *kPrelude = "SET rst_n 0\nTICK 2\nSET rst_n 1\n";

constexpr const char *kHead = "@expect-src x.md#a\n@tb-module tb_wait_level\n";

std::string scenario_text(const std::string &body) {
    return std::string(kHead) + "@scenario s\n@anchor a\n" + body;
}

// 一条结论 = (check_id, PASS|FAIL)
using Pairs = std::vector<std::pair<std::string, std::string>>;

/// 从 `CHECK <id> PASS|FAIL ...` 行抽 `(id, verdict)`
/// （⚠️ 比对粒度 = **对**，不是原始文本 —— 两端的 FAIL 文案本就不同：
/// 解释器的 `failure_lines` 带 `max=`，渲染侧带端口名）
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
    const fs::path d = repo_root() / "build" / "tests" / "verif_wait_level_tmp" / name;
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

/// ① IR 解释器侧：同一份 `.verif` 文本 ⇒ 结论对
Pairs run_interp(const std::string &text, std::string &error) {
    ch::ch_device<WaitLevelDut> dev;
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
    ch::ch_device<WaitLevelDut> dev;
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
    ro.tb_module = "tb_wait_level";
    ro.dut_module = "top";
    ro.example_hint = "verif_wait_level";
    std::ostringstream tb;
    std::string rerr;
    if (!ch::rtl_emit::render_tb(tb, prog, manifest, ro, rerr)) {
        error = "渲染失败：" + rerr;
        return {};
    }
    write_file(d / "tb_wait_level.sv", tb.str());

    const fs::path obj = d / "obj";
    std::error_code ec;
    fs::create_directories(obj, ec);
    // ⚠️ `--x-assign unique`：Verilator 默认把未驱动值当 0 ⇒ 「未驱动」与「驱动成 0」
    //   不可区分（与 `tests/test_rtl_emit_contract.cpp` 同一理由）
    // ⚠️ **禁 `-Wno-fatal`**（与护栏 ④ 同一纪律）：warning 必须**影响退出码**
    //   ⇒ ⇒ 否则「渲染出宽度扩展赋值」这类缺陷会被 warning 静默吞掉
    //   （实测 2026-11-21：`hold_val = <1 位端口>` 触发 `WIDTHEXPAND` ⇒ 产品产物编译失败）
    const std::string cmd =
        vlt + " --binary --timing -CFLAGS -fcoroutines --x-assign unique" +
        " --top-module tb_wait_level -Mdir " + obj.string() + " " +
        (d / "tb_wait_level.sv").string() + " " + (d / "dut.v").string() + " -o sim >" +
        (d / "compile.log").string() + " 2>&1";
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
    const char *body;   // `.verif` 场景体
    const char *want;   // **手推**期望结论（按顺序空格分隔：`PASS` / `FAIL`）
};

// 7 条构例 —— 覆盖 AC5/AC9/AC10/AC11/AC13 与向后兼容
// ⚠️ 每条都带 `kPrelude`（显式对齐起点）⇒ 期望拍数从**释放复位后**起算（手推）：
//   第 k 次 `WAIT` 迭代后 `cnt == k`
const Case kCases[] = {
    // AC11 ①：命中**恰在第 max 拍** ⇒ PASS
    {"high_hit_at_max", "SET rst_n 0\nTICK 2\nSET rst_n 1\nWAIT sig high 8\n", "PASS"},
    // AC11 ②：命中在第 max+1 拍 ⇒ FAIL（sig 第 8 拍才为高，max=7 ⇒ 窗口内从未成立）
    {"high_miss_max_minus_1", "SET rst_n 0\nTICK 2\nSET rst_n 1\nWAIT sig high 7\n", "FAIL"},
    // AC5 ① / AC13：入口即低 ⇒ 第 1 次迭代即命中；**紧跟 `EXPECT val 1`** 证明
    //   「命中后仍推进**恰好 1 拍**」（不是 0 拍、也不是 2 拍）
    {"low_entry_already_low",
     "SET rst_n 0\nTICK 2\nSET rst_n 1\nWAIT sig low 8\nEXPECT val 1\n", "PASS PASS"},
    // AC10：8 位端口 val=1（**非零但非全 1**）⇒ `high` 命中（若 high = 全 1（255）必 FAIL）
    {"high_nonzero_not_all_ones", "SET rst_n 0\nTICK 2\nSET rst_n 1\nWAIT val high 1\n", "PASS"},
    {"low_nonzero_miss", "SET rst_n 0\nTICK 2\nSET rst_n 1\nWAIT val low 1\n", "FAIL"},
    // 向后兼容：缺省 level ≡ 非零
    {"default_level_backcompat", "SET rst_n 0\nTICK 2\nSET rst_n 1\nWAIT sig 8\n", "PASS"},
    // AC9：1 位信号 `p3` **只在第 3 拍**为高 ⇒ `WAIT p3 high 3` 恰在 max 拍命中 ⇒ PASS
    //   ⚠️ 若命中被推迟到第 4 拍 ⇒ 该构例 FAIL ⇒ 判别「命中拍 = 第 k 拍」
    {"high_hits_at_third_tick", "SET rst_n 0\nTICK 2\nSET rst_n 1\nWAIT p3 high 3\n", "PASS"},
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

} // namespace

// ============================================================================
// 扫描器：`level` 只接受 `high` / `low` 关键字（fail-closed · AC2 / AC12）
// ============================================================================
TEST_CASE("verif-wait-level: 扫描器负向 —— level 只接受 high/low 关键字",
          "[verif][wait-level]") {
    struct Bad {
        const char *body;
        const char *needle; // 错误消息里 MUST 出现的词
    };
    const Bad bads[] = {
        {"WAIT sig 1 8\n", "level"},   // 数字 ⇒ 非法（且**不是**「非法数字」这种泛化文案）
        {"WAIT sig HIGH 8\n", "level"}, // 大小写变体 ⇒ 非法
        {"WAIT sig High 8\n", "level"},
        {"WAIT sig high 8 9\n", "参数过多"}, // 多余实参 ⇒ 非法
    };
    for (const auto &b : bads) {
        std::istringstream in(scenario_text(b.body));
        ch::rtl_emit::VerifProgram prog;
        ch::rtl_emit::VerifParseError err;
        const bool ok = ch::rtl_emit::parse_verif(in, prog, err);
        INFO("body: " << b.body);
        REQUIRE_FALSE(ok);
        REQUIRE(err.line > 0);
        REQUIRE(err.message.find(b.needle) != std::string::npos);
    }
}

// ============================================================================
// 合法形态：三种写法都能解析，且 IR 的 `level` 正确（AC1）
// ============================================================================
TEST_CASE("verif-wait-level: `high` / `low` / 缺省 都能解析且 level 正确",
          "[verif][wait-level]") {
    const std::pair<const char *, ch::rtl_emit::WaitLevel> good[] = {
        {"WAIT sig 8\n", ch::rtl_emit::WaitLevel::High},        // 缺省 = 非零
        {"WAIT sig high 8\n", ch::rtl_emit::WaitLevel::High},
        {"WAIT sig low 8\n", ch::rtl_emit::WaitLevel::Low},
    };
    for (const auto &g : good) {
        std::istringstream in(scenario_text(g.first));
        ch::rtl_emit::VerifProgram prog;
        ch::rtl_emit::VerifParseError err;
        INFO("body: " << g.first);
        REQUIRE(ch::rtl_emit::parse_verif(in, prog, err));
        REQUIRE(prog.scenarios.size() == 1);
        REQUIRE(prog.scenarios[0].ops.size() == 1);
        const auto &op = prog.scenarios[0].ops[0];
        REQUIRE(op.kind == ch::rtl_emit::VerifOpKind::Wait);
        REQUIRE(op.level == g.second);
        REQUIRE(op.count == 8u);
    }
}

// ============================================================================
// **运行期同结论**（AC4 · 本文件的核心判据）
// ============================================================================
TEST_CASE("verif-wait-level: 两端（IR 解释器 / 渲染 TB）逐条结论相等",
          "[verif][wait-level][verilator_e2e]") {
    if (verilator_path().empty()) {
        SKIP("verilator 不可用（本用例的渲染侧需要真编译）");
    }
    std::size_t idx = 0;
    for (const auto &c : kCases) {
        const std::string text = scenario_text(c.body);
        std::string ierr, rerr;
        const Pairs ip = run_interp(text, ierr);
        const Pairs rp = run_rendered(text, std::string(c.name) + "_" + std::to_string(idx++), rerr);
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
            // check_id MUST **按序命名**（`s#0` / `s#1` …）⇒ ⇒ FAIL 行能**点名**是哪条断言
            // （AC5 ③ / AC11 ② 的「点名 check_id」）
            REQUIRE(ip[i].first == "s#" + std::to_string(i));
            REQUIRE(ip[i].second == want[i]);
        }
        // ② 两端**逐条**（id, 结论）相等
        REQUIRE(rp == ip);
    }
}

// ============================================================================
// **`EXPECT_STABLE` 的两端结论相等**（窄端口 / 宽端口 · 稳定 / 不稳定）
// ============================================================================
//
// ⚠️ **为什么单列一个用例**（2026-11-21）：
//   ① `EXPECT_STABLE` 此前**没有任何单元消费者**（只有 pwm 迁移后才有）⇒ ⇒
//      渲染侧「把窄端口赋给 64 位载体」的 `WIDTHEXPAND` **结构性测不出**
//      （旧 harness 带 `-Wno-fatal` ⇒ warning 不影响退出码）
//   ② 本用例的期望值 **手推**（cnt 复位后 0，每拍 +1）：
//      · `p3 = (cnt == 3)`（1 位）· `val = cnt`（8 位）· `sig = (8 <= cnt < 16)`
//      ⇒ `TICK 1` 后 cnt=1；`EXPECT_STABLE p3 4` ⇒ 观测 cnt 2,3,4,5 ⇒ `0,1,0,0`
//        ⇒ **PASS FAIL PASS PASS**
//      ⇒ `EXPECT_STABLE val 3` ⇒ 观测 cnt 6,7,8 ⇒ `6,7,8` ≠ 5 ⇒ **FAIL ×3**
//      ⇒ `EXPECT_STABLE sig 2` ⇒ 观测 cnt 9,10 ⇒ `1,1` == 1 ⇒ **PASS ×2**
//   ③ harness **禁 `-Wno-fatal`** ⇒ ⇒ 渲染出的宽度扩展赋值会让**编译**失败 ⇒
//      本用例在缺陷回归时**转红**（不是「warning 静默通过」）。
TEST_CASE("verif-stable: 两端逐条结论相等（窄 / 宽端口 · 稳定 / 不稳定）",
          "[verif][wait-level][verilator_e2e]") {
    if (verilator_path().empty()) {
        SKIP("verilator 不可用（本用例的渲染侧需要真编译）");
    }
    const std::string body = std::string(kPrelude) + "TICK 1\n" +
                             "EXPECT_STABLE p3 4\n" +  // cnt 2,3,4,5 ⇒ 0,1,0,0
                             "EXPECT_STABLE val 3\n" + // cnt 6,7,8   ⇒ 6,7,8 (≠5)
                             "EXPECT_STABLE sig 2\n";  // cnt 9,10    ⇒ 1,1 (==1)
    const std::string text = scenario_text(body);
    std::string ierr, rerr;
    const Pairs ip = run_interp(text, ierr);
    const Pairs rp = run_rendered(text, "stable_wide_narrow", rerr);
    INFO("interp error: " << ierr);
    INFO("render error: " << rerr);
    REQUIRE(ierr.empty());
    REQUIRE(rerr.empty());

    const std::vector<std::string> want = {"PASS", "FAIL", "PASS", "PASS", "FAIL",
                                           "FAIL", "FAIL", "PASS", "PASS"};
    REQUIRE(ip.size() == want.size());
    for (std::size_t i = 0; i < want.size(); ++i) {
        INFO("check index: " << i);
        // ⚠️ `EXPECT_STABLE` 的 **N 条**结论共用**同一个** `check_id`（一个原语 = 一个 id）
        //   ⇒ ⇒ 只判前缀，不判 `s#i`（与 `EXPECT` 的逐条命名不同）
        REQUIRE(ip[i].first.rfind("s#", 0) == 0);
        REQUIRE(ip[i].second == want[i]);
    }
    REQUIRE(rp == ip);
}

// ============================================================================
// **变异自证**：只改夹具 `.verif` 的 `max`（8 → 7）⇒ 边界构例必红（AC5 · B4）
// ============================================================================
TEST_CASE("verif-wait-level: 变异（max 8→7）⇒ 边界构例转红（两端同时）",
          "[verif][wait-level][verilator_e2e]") {
    if (verilator_path().empty()) {
        SKIP("verilator 不可用（本用例的渲染侧需要真编译）");
    }
    const std::string base = scenario_text(std::string(kPrelude) + "WAIT sig high 8\n");
    const std::string mutated = scenario_text(std::string(kPrelude) + "WAIT sig high 7\n");
    std::string e1, e2, e3, e4;
    const Pairs i_base = run_interp(base, e1);
    const Pairs i_mut = run_interp(mutated, e2);
    const Pairs r_base = run_rendered(base, "mut_base", e3);
    const Pairs r_mut = run_rendered(mutated, "mut_max7", e4);
    REQUIRE(e1.empty());
    REQUIRE(e2.empty());
    REQUIRE(e3.empty());
    REQUIRE(e4.empty());

    REQUIRE(i_base.size() == 1);
    REQUIRE(i_mut.size() == 1);
    REQUIRE(i_base[0].first == "s#0");
    REQUIRE(i_mut[0].first == "s#0");
    REQUIRE(i_base[0].second == "PASS"); // 第 8 拍恰好命中
    REQUIRE(i_mut[0].second == "FAIL");  // 第 7 拍还没到
    REQUIRE(r_base == i_base);
    REQUIRE(r_mut == i_mut);
    // 变异**真的**改变了结论（否则本构例对 1 拍偏移无判别力）
    REQUIRE(i_base[0].second != i_mut[0].second);
}
