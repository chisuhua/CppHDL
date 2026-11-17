// src/rtl_emit/verif_scan.cpp
//
// `.verif` 扫描器 + 生成期表达式求值（`rtl-verification-single-source` · ADR-074 D1/D2）。
//
// ⚠️ **本文件是唯一的语义前段**：所有表达式在这里求值成字面量 ⇒ ⇒ 两个执行后端
//   都不再解释表达式（⇒ 语义只有一份）。
// ⚠️ **fail-closed**：集合外构造 / 文法外 token / 嵌套 REPEAT / 参数个数不对 ⇒ 报错。

#include "rtl_emit/verif.h"

#include <cctype>
#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace ch {
namespace rtl_emit {
namespace {

// ── 表达式词法 ─────────────────────────────────────────────────────────

enum class Tok {
    Num,
    Var,
    Plus,
    Minus,
    Star,
    Amp,
    Pipe,
    Caret,
    Shl,
    Shr,
    Tilde,
    LParen,
    RParen,
    LBracket,
    RBracket,
    Colon,
    End,
};

struct Token {
    Tok kind{Tok::End};
    std::uint64_t value = 0;
};

struct ExprError {
    bool bad = false;
    std::string message;
};

bool tokenize(const std::string &src, std::vector<Token> &out, ExprError &err) {
    std::size_t i = 0;
    const std::size_t n = src.size();
    while (i < n) {
        const char c = src[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::size_t j = i;
            std::uint64_t v = 0;
            int base = 10;
            if (c == '0' && i + 1 < n && (src[i + 1] == 'x' || src[i + 1] == 'X')) {
                base = 16;
                j = i + 2;
                if (j >= n) {
                    err = {true, "0x 后缺数字"};
                    return false;
                }
                std::uint64_t acc = 0;
                while (j < n && std::isxdigit(static_cast<unsigned char>(src[j]))) {
                    const char h = src[j];
                    acc = acc * 16 +
                          static_cast<std::uint64_t>(
                              std::isdigit(static_cast<unsigned char>(h))
                                  ? (h - '0')
                                  : (std::tolower(static_cast<unsigned char>(h)) - 'a' + 10));
                    ++j;
                }
                v = acc;
            } else if (c == '0' && i + 1 < n && (src[i + 1] == 'b' || src[i + 1] == 'B')) {
                base = 2;
                j = i + 2;
                std::uint64_t acc = 0;
                while (j < n && (src[j] == '0' || src[j] == '1')) {
                    acc = acc * 2 + static_cast<std::uint64_t>(src[j] - '0');
                    ++j;
                }
                v = acc;
            } else {
                std::uint64_t acc = 0;
                while (j < n && std::isdigit(static_cast<unsigned char>(src[j]))) {
                    acc = acc * 10 + static_cast<std::uint64_t>(src[j] - '0');
                    ++j;
                }
                v = acc;
            }
            (void)base;
            out.push_back({Tok::Num, v});
            i = j;
            continue;
        }
        if (c == '_') {
            // `_i`（REPEAT 隐式循环变量）
            if (i + 1 < n && src[i + 1] == 'i') {
                out.push_back({Tok::Var, 0});
                i += 2;
                continue;
            }
            err = {true, "未知标识符（表达式内只允许字面量与 `_i`）"};
            return false;
        }
        if (c == '<' && i + 1 < n && src[i + 1] == '<') {
            out.push_back({Tok::Shl, 0});
            i += 2;
            continue;
        }
        if (c == '>' && i + 1 < n && src[i + 1] == '>') {
            out.push_back({Tok::Shr, 0});
            i += 2;
            continue;
        }
        switch (c) {
        case '+': out.push_back({Tok::Plus, 0}); break;
        case '-': out.push_back({Tok::Minus, 0}); break;
        case '*': out.push_back({Tok::Star, 0}); break;
        case '&': out.push_back({Tok::Amp, 0}); break;
        case '|': out.push_back({Tok::Pipe, 0}); break;
        case '^': out.push_back({Tok::Caret, 0}); break;
        case '~': out.push_back({Tok::Tilde, 0}); break;
        case '(': out.push_back({Tok::LParen, 0}); break;
        case ')': out.push_back({Tok::RParen, 0}); break;
        case '[': out.push_back({Tok::LBracket, 0}); break;
        case ']': out.push_back({Tok::RBracket, 0}); break;
        case ':': out.push_back({Tok::Colon, 0}); break;
        default:
            err = {true, std::string("文法外字符 `") + c + "`"};
            return false;
        }
        ++i;
    }
    out.push_back({Tok::End, 0});
    return true;
}

// ── 递归下降求值 ───────────────────────────────────────────────────────

class ExprParser {
public:
    ExprParser(const std::vector<Token> &toks, std::uint64_t loop_var, ExprError &err)
        : toks_(toks), loop_(loop_var), err_(err) {}

    std::uint64_t parse() {
        const auto v = parse_bitor();
        if (!err_.bad && peek() != Tok::End) {
            err_ = {true, "表达式尾部有多余 token"};
        }
        return v;
    }

private:
    Tok peek() const { return toks_[pos_].kind; }
    Token take() { return toks_[pos_++]; }
    bool expect(Tok k, const char *what) {
        if (peek() != k) {
            err_ = {true, std::string("期望 ") + what};
            return false;
        }
        ++pos_;
        return true;
    }

    std::uint64_t parse_bitor() {
        auto v = parse_bitxor();
        while (!err_.bad && peek() == Tok::Pipe) {
            take();
            v |= parse_bitxor();
        }
        return v;
    }
    std::uint64_t parse_bitxor() {
        auto v = parse_bitand();
        while (!err_.bad && peek() == Tok::Caret) {
            take();
            v ^= parse_bitand();
        }
        return v;
    }
    std::uint64_t parse_bitand() {
        auto v = parse_shift();
        while (!err_.bad && peek() == Tok::Amp) {
            take();
            v &= parse_shift();
        }
        return v;
    }
    std::uint64_t parse_shift() {
        auto v = parse_add();
        while (!err_.bad && (peek() == Tok::Shl || peek() == Tok::Shr)) {
            const bool left = peek() == Tok::Shl;
            take();
            const auto s = parse_add();
            v = left ? (s >= 64 ? 0 : (v << s)) : (s >= 64 ? 0 : (v >> s));
        }
        return v;
    }
    std::uint64_t parse_add() {
        auto v = parse_mul();
        while (!err_.bad && (peek() == Tok::Plus || peek() == Tok::Minus)) {
            const bool add = peek() == Tok::Plus;
            take();
            const auto r = parse_mul();
            v = add ? (v + r) : (v - r);
        }
        return v;
    }
    std::uint64_t parse_mul() {
        auto v = parse_unary();
        while (!err_.bad && peek() == Tok::Star) {
            take();
            v *= parse_unary();
        }
        return v;
    }
    std::uint64_t parse_unary() {
        if (peek() == Tok::Tilde) {
            take();
            return ~parse_unary();
        }
        if (peek() == Tok::Minus) {
            take();
            return static_cast<std::uint64_t>(0) - parse_unary();
        }
        return parse_postfix();
    }
    std::uint64_t parse_postfix() {
        auto v = parse_primary();
        while (!err_.bad && peek() == Tok::LBracket) {
            take();
            if (peek() != Tok::Num) {
                err_ = {true, "位选的高位必须是字面量"};
                return v;
            }
            const auto hi = take().value;
            if (!expect(Tok::Colon, "位选 `[hi:lo]`")) return v;
            if (peek() != Tok::Num) {
                err_ = {true, "位选的低位必须是字面量"};
                return v;
            }
            const auto lo = take().value;
            if (!expect(Tok::RBracket, "`]`")) return v;
            if (hi < lo || hi >= 64) {
                err_ = {true, "位选范围非法"};
                return v;
            }
            const std::uint64_t width = hi - lo + 1;
            const std::uint64_t mask = (width >= 64) ? ~static_cast<std::uint64_t>(0)
                                                     : ((static_cast<std::uint64_t>(1) << width) - 1);
            v = (v >> lo) & mask;
        }
        return v;
    }
    std::uint64_t parse_primary() {
        if (peek() == Tok::Num) {
            return take().value;
        }
        if (peek() == Tok::Var) {
            take();
            return loop_;
        }
        if (peek() == Tok::LParen) {
            take();
            const auto v = parse_bitor();
            if (!err_.bad && !expect(Tok::RParen, "`)`")) return v;
            return v;
        }
        err_ = {true, "表达式缺主项（字面量 / `_i` / 括号）"};
        return 0;
    }

    const std::vector<Token> &toks_;
    std::size_t pos_ = 0;
    std::uint64_t loop_ = 0;
    ExprError &err_;
};

bool eval_expr(const std::string &src, std::uint64_t loop_var, std::uint64_t &out,
               std::string &message) {
    std::vector<Token> toks;
    ExprError err;
    if (!tokenize(src, toks, err)) {
        message = err.message;
        return false;
    }
    ExprParser p(toks, loop_var, err);
    out = p.parse();
    if (err.bad) {
        message = err.message;
        return false;
    }
    return true;
}

// ── 行扫描 ─────────────────────────────────────────────────────────────

struct RawOp {
    VerifOpKind kind{VerifOpKind::Tick};
    std::string port;
    std::string expr;        // Set/Expect 的值表达式；Tick/Wait/ExpectStable/Reset 的计数表达式
    std::uint64_t level = 0; // Reset 的电平（1 = high）
};

std::vector<std::string> split_ws(const std::string &line) {
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) {
        out.push_back(tok);
    }
    return out;
}

/// 把一条 raw 语句**求值成字面量**（唯一求值点 ⇒ ⇒ 两个执行后端都不再解释表达式）。
bool materialize(const RawOp &raw, std::uint64_t loop_var, VerifOp &o, std::string &msg) {
    o.kind = raw.kind;
    o.port = raw.port;
    switch (raw.kind) {
    case VerifOpKind::Set:
    case VerifOpKind::Expect:
    case VerifOpKind::Tick:
    case VerifOpKind::Wait:
    case VerifOpKind::ExpectStable: {
        std::uint64_t v = 0;
        if (!eval_expr(raw.expr, loop_var, v, msg)) {
            return false;
        }
        if (raw.kind == VerifOpKind::Set || raw.kind == VerifOpKind::Expect) {
            o.value = v;
            return true;
        }
        if (v > 0xFFFFFFFFull) {
            msg = "计数超出 32 位";
            return false;
        }
        o.count = static_cast<std::uint32_t>(v);
        return true;
    }
    case VerifOpKind::Reset: {
        std::uint64_t v = 0;
        if (!eval_expr(raw.expr, loop_var, v, msg)) {
            return false;
        }
        if (v > 0xFFFFFFFFull) {
            msg = "复位拍数超出 32 位";
            return false;
        }
        o.value = raw.level;
        o.count = static_cast<std::uint32_t>(v);
        return true;
    }
    case VerifOpKind::Settle:
    case VerifOpKind::Observe:
        return true;
    }
    msg = "未知原语";
    return false;
}

void assign_check_ids(VerifScenario &sc) {
    std::size_t c = 0;
    for (auto &op : sc.ops) {
        if (op.kind == VerifOpKind::Expect || op.kind == VerifOpKind::Wait ||
            op.kind == VerifOpKind::ExpectStable) {
            op.check_id = sc.name + "#" + std::to_string(c);
            ++c;
        }
    }
}

/// ⚠️ **时序锚点纪律**（ADR-074 D2）：驱动发生在「沿后采样窗」内 ⇒ 组合输出**尚未结算**
/// ⇒ ⇒ `SET` / `RESET` 之后**紧邻** `EXPECT` 是**未定义观测点** ⇒ ⇒ MUST 先 `SETTLE`
/// （或先 `TICK` / `WAIT`）。⇒ ⇒ 静态强制（而不是靠人记得写）。
bool enforce_settle_after_drive(VerifScenario &sc, std::string &msg) {
    for (std::size_t i = 1; i < sc.ops.size(); ++i) {
        const auto &prev = sc.ops[i - 1];
        const auto &cur = sc.ops[i];
        if ((prev.kind == VerifOpKind::Set || prev.kind == VerifOpKind::Reset) &&
            (cur.kind == VerifOpKind::Expect || cur.kind == VerifOpKind::ExpectStable)) {
            msg = "scenario `" + sc.name + "` 的第 " + std::to_string(i) +
                  " 条语句：`SET`/`RESET` 之后 MUST 先 `SETTLE`（或 `TICK`/`WAIT`）再 "
                  "`EXPECT`（否则观测点落在组合结算之前 ⇒ 未定义）";
            return false;
        }
    }
    return true;
}

} // namespace

bool parse_verif(std::istream &in, VerifProgram &out, VerifParseError &err) {
    out = VerifProgram{};
    err = VerifParseError{};

    std::string line;
    std::size_t lineno = 0;
    VerifScenario *cur = nullptr;

    // REPEAT 单层展开（**扫描期**展开 ⇒ ⇒ IR 里没有循环 ⇒ 后端不解释 `_i`）
    struct RepeatBlock {
        std::string count_expr;
        std::vector<RawOp> body;
    };
    std::vector<RepeatBlock> repeats;

    auto emit_op = [&](const RawOp &raw, std::uint64_t loop_var) -> bool {
        if (!cur) {
            err = {lineno, "语句必须出现在 @scenario 之后"};
            return false;
        }
        VerifOp o;
        std::string msg;
        if (!materialize(raw, loop_var, o, msg)) {
            err = {lineno, msg};
            return false;
        }
        cur->ops.push_back(o);
        return true;
    };

    while (std::getline(in, line)) {
        ++lineno;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // 整行注释（行首 `#`）—— `.verif` 里 `#` 不表达其它含义
        if (!line.empty() && line[0] == '#') {
            continue;
        }
        const auto toks = split_ws(line);
        if (toks.empty()) {
            continue;
        }
        const std::string &head = toks[0];

        if (head == "@expect-src") {
            out.expect_src = line.substr(line.find("@expect-src") + 11);
            while (!out.expect_src.empty() &&
                   std::isspace(static_cast<unsigned char>(out.expect_src.front()))) {
                out.expect_src.erase(out.expect_src.begin());
            }
            continue;
        }
        if (head == "@tb-module") {
            out.tb_module = toks.size() > 1 ? toks[1] : std::string{};
            continue;
        }
        if (head == "@scenario") {
            if (toks.size() < 2) {
                err = {lineno, "@scenario 缺名字"};
                return false;
            }
            if (!repeats.empty()) {
                err = {lineno, "REPEAT 未闭合时不得开始新 @scenario"};
                return false;
            }
            out.scenarios.push_back(VerifScenario{});
            cur = &out.scenarios.back();
            cur->name = toks[1];
            continue;
        }
        if (head == "@anchor") {
            if (!cur) {
                err = {lineno, "@anchor 必须出现在 @scenario 之后"};
                return false;
            }
            cur->anchor = toks.size() > 1 ? toks[1] : std::string{};
            continue;
        }
        if (head == "REPEAT") {
            if (toks.size() < 2) {
                err = {lineno, "REPEAT 缺次数"};
                return false;
            }
            if (!repeats.empty()) {
                err = {lineno, "嵌套 REPEAT 未支持（fail-closed）"};
                return false;
            }
            repeats.push_back(RepeatBlock{toks[1], {}});
            continue;
        }
        if (head == "END") {
            if (repeats.empty()) {
                err = {lineno, "孤立的 END"};
                return false;
            }
            const auto blk = repeats.back();
            repeats.pop_back();
            std::uint64_t n = 0;
            std::string msg;
            if (!eval_expr(blk.count_expr, 0, n, msg)) {
                err = {lineno, msg};
                return false;
            }
            if (n > 65536ull) {
                err = {lineno, "REPEAT 次数过大（> 65536）⇒ 展开后 IR 会爆炸"};
                return false;
            }
            for (std::uint64_t i = 0; i < n; ++i) {
                for (const auto &raw : blk.body) {
                    if (!emit_op(raw, i)) {
                        return false;
                    }
                }
            }
            continue;
        }

        if (!cur) {
            err = {lineno, "语句必须出现在 @scenario 之后"};
            return false;
        }

        RawOp op;
        if (head == "SET" || head == "EXPECT") {
            if (toks.size() < 3) {
                err = {lineno, head + " 需要 <port> <expr>"};
                return false;
            }
            op.kind = (head == "SET") ? VerifOpKind::Set : VerifOpKind::Expect;
            op.port = toks[1];
            // 表达式可含空格（如 `0xA5000000 + _i`）⇒ 取端口名之后的**全部**内容
            op.expr = line.substr(line.find(toks[1]) + toks[1].size());
            while (!op.expr.empty() &&
                   std::isspace(static_cast<unsigned char>(op.expr.front()))) {
                op.expr.erase(op.expr.begin());
            }
            if (op.expr.empty()) {
                err = {lineno, head + " 缺表达式"};
                return false;
            }
        } else if (head == "TICK") {
            if (toks.size() < 2) {
                err = {lineno, "TICK 需要 <n>"};
                return false;
            }
            op.kind = VerifOpKind::Tick;
            op.expr = toks[1];
        } else if (head == "WAIT" || head == "EXPECT_STABLE") {
            if (toks.size() < 3) {
                err = {lineno, head + " 需要 <port> <n>"};
                return false;
            }
            op.kind = (head == "WAIT") ? VerifOpKind::Wait : VerifOpKind::ExpectStable;
            op.port = toks[1];
            op.expr = toks[2];
        } else if (head == "SETTLE") {
            op.kind = VerifOpKind::Settle;
        } else if (head == "OBSERVE") {
            if (toks.size() < 2) {
                err = {lineno, "OBSERVE 需要 <port>"};
                return false;
            }
            op.kind = VerifOpKind::Observe;
            op.port = toks[1];
        } else if (head == "RESET") {
            if (toks.size() < 3 || (toks[2] != "high" && toks[2] != "low")) {
                err = {lineno, "RESET 需要 <port> <high|low> [cycles]"};
                return false;
            }
            op.kind = VerifOpKind::Reset;
            op.port = toks[1];
            op.level = (toks[2] == "high") ? 1u : 0u;
            op.expr = toks.size() > 3 ? toks[3] : std::string("2");
        } else {
            err = {lineno, "未知原语 `" + head + "`（集合外构造 ⇒ fail-closed）"};
            return false;
        }

        if (!repeats.empty()) {
            repeats.back().body.push_back(op);
            continue;
        }
        if (!emit_op(op, 0)) {
            return false;
        }
    }

    if (!repeats.empty()) {
        err = {lineno, "REPEAT 未闭合"};
        return false;
    }
    if (out.tb_module.empty()) {
        err = {lineno, "缺 `@tb-module`（TB 的 module 名无处推导）"};
        return false;
    }
    if (out.expect_src.empty()) {
        err = {lineno, "缺 `@expect-src`（期望值出处 MUST 登记）"};
        return false;
    }
    if (out.scenarios.empty()) {
        err = {lineno, "没有任何 @scenario"};
        return false;
    }
    for (auto &sc : out.scenarios) {
        std::string msg;
        if (!enforce_settle_after_drive(sc, msg)) {
            err = {lineno, msg};
            return false;
        }
        assign_check_ids(sc);
    }
    return true;
}

std::size_t verif_check_count(const VerifProgram &program) {
    std::size_t n = 0;
    for (const auto &sc : program.scenarios) {
        for (const auto &op : sc.ops) {
            if (op.kind == VerifOpKind::Expect || op.kind == VerifOpKind::Wait) {
                ++n;
            } else if (op.kind == VerifOpKind::ExpectStable) {
                // 每拍一条 check（与 IR 解释器一致 ⇒ 三方等式成立）
                n += op.count;
            }
        }
    }
    return n;
}

std::size_t verif_scenario_count(const VerifProgram &program) {
    return program.scenarios.size();
}

} // namespace rtl_emit
} // namespace ch
