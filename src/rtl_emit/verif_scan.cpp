// src/rtl_emit/verif_scan.cpp
//
// `.verif` 扫描器 + 生成期表达式求值（`rtl-verification-single-source` · ADR-074 D1/D2）。
//
// ⚠️ **本文件是唯一的语义前段**：所有表达式在这里求值成字面量 ⇒ ⇒ 两个执行后端
//   都不再解释表达式（⇒ 语义只有一份）。
// ⚠️ **fail-closed**：集合外构造 / 文法外 token / 嵌套 REPEAT / 参数个数不对 ⇒ 报错。

#include "rtl_emit/verif.h"

#include "verif_internal.h"

#include <cctype>
#include <istream>
#include <sstream>
#include <string>
#include <unordered_map>
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
    std::string port2;  // ExpectSeqStep 的第二端口
    std::string expr;   // Set/Expect 的值表达式；Tick/Wait/ExpectStable/Reset 的计数表达式；
                        // ExpectSeqSelf = n · ExpectSeqStep = n · ExpectSeqCount = value（64 位）
    std::string expr2;  // ExpectSeqSelf = lag · ExpectSeqCount = n
    std::string expr3;  // ExpectSeqCount = count
    std::uint64_t level = 0; // Reset 的电平（1 = high）
    WaitLevel wait_level{WaitLevel::High}; // Wait 的目标电平（缺省 = 非零）
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
    o.port2 = raw.port2;
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
        if (raw.kind == VerifOpKind::Wait) {
            o.level = raw.wait_level;
        }
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
    case VerifOpKind::Record:
        return true;
    // ── 历史/序列判据（ADR-076 D1）────────────────────────────────────
    case VerifOpKind::ExpectSeqSelf: {
        std::uint64_t n = 0;
        std::uint64_t lag = 0;
        if (!eval_expr(raw.expr, loop_var, n, msg)) {
            return false;
        }
        if (!eval_expr(raw.expr2, loop_var, lag, msg)) {
            return false;
        }
        if (n > 0xFFFFFFFFull || lag > 0xFFFFFFFFull) {
            msg = "序列窗口长度超出 32 位";
            return false;
        }
        o.seq_n = static_cast<std::uint32_t>(n);
        o.seq_lag = static_cast<std::uint32_t>(lag);
        return true;
    }
    case VerifOpKind::ExpectSeqStep: {
        std::uint64_t n = 0;
        if (!eval_expr(raw.expr, loop_var, n, msg)) {
            return false;
        }
        if (n > 0xFFFFFFFFull) {
            msg = "序列窗口长度超出 32 位";
            return false;
        }
        o.seq_n = static_cast<std::uint32_t>(n);
        return true;
    }
    case VerifOpKind::ExpectSeqCount: {
        std::uint64_t value = 0;
        std::uint64_t n = 0;
        std::uint64_t cnt = 0;
        if (!eval_expr(raw.expr, loop_var, value, msg)) {
            return false;
        }
        if (!eval_expr(raw.expr2, loop_var, n, msg)) {
            return false;
        }
        if (!eval_expr(raw.expr3, loop_var, cnt, msg)) {
            return false;
        }
        if (n > 0xFFFFFFFFull || cnt > 0xFFFFFFFFull) {
            msg = "序列窗口长度超出 32 位";
            return false;
        }
        o.seq_value = value;
        o.seq_n = static_cast<std::uint32_t>(n);
        o.seq_count = static_cast<std::uint32_t>(cnt);
        return true;
    }
    }
    msg = "未知原语";
    return false;
}

/// 该原语**产生一条断言**（⇒ 分配 `check_id` · 计入 `verif_check_count`）。
///
/// ⚠️ `Record` **不在**此列 ⇒ ⇒ 它只记录、不判断 ⇒ ⇒ 不计入断言数（ADR-076 D1）。
bool is_assert_op(VerifOpKind k) {
    switch (k) {
    case VerifOpKind::Expect:
    case VerifOpKind::Wait:
    case VerifOpKind::ExpectStable:
    case VerifOpKind::ExpectSeqSelf:
    case VerifOpKind::ExpectSeqStep:
    case VerifOpKind::ExpectSeqCount:
        return true;
    // ⚠️ 下面这些**不**产生断言 ⇒ ⇒ **必须逐个列出**（仓内开 `-Wswitch-enum` ⇒
    //   即使有 `default:` 也会对「未列出的枚举值」告警 ⇒ ⇒ 漏列即编译红）。
    case VerifOpKind::Set:
    case VerifOpKind::Tick:
    case VerifOpKind::Settle:
    case VerifOpKind::Observe:
    case VerifOpKind::Reset:
    case VerifOpKind::Record:
        return false;
    }
    return false;
}

void assign_check_ids(VerifScenario &sc) {
    std::size_t c = 0;
    for (auto &op : sc.ops) {
        if (is_assert_op(op.kind)) {
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
        // ⚠️ `Record` **与 `Expect` 同型**（它读的是**当前**端口值 ⇒ ⇒ 依赖结算点）
        //   ⇒ ⇒ `SET`/`RESET` 之后紧邻 `RECORD` 同样 MUST 先 `SETTLE`（ADR-076 D4）。
        // ⚠️ `EXPECT_SEQ_*` **不在**此列（它们只读**历史** ⇒ 与当前结算状态无关）。
        if ((prev.kind == VerifOpKind::Set || prev.kind == VerifOpKind::Reset) &&
            (cur.kind == VerifOpKind::Expect || cur.kind == VerifOpKind::ExpectStable ||
             cur.kind == VerifOpKind::Record)) {
            msg = "scenario `" + sc.name + "` 的第 " + std::to_string(i) +
                  " 条语句：`SET`/`RESET` 之后 MUST 先 `SETTLE`（或 `TICK`/`WAIT`）再 "
                  "`EXPECT`/`RECORD`（否则观测点落在组合结算之前 ⇒ 未定义）";
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

    // ── 历史/序列判据的**静态**状态（ADR-076 D3）────────────────────────
    //
    // ⚠️ **一份**计数器 · **跨 scenario 不清空** · 按**展开后**的程序顺序累计
    //   ⇒ ⇒ **运行期历史长度恒等于静态计数** ⇒ ⇒ 「深度超限」与「历史不足」**都不可达**
    //   ⇒ ⇒ 这正是「扫描期消灭运行期分叉」的充要前提。
    // ⚠️ **不要**把它们声明成 per-scenario 局部（那会重置深度计数 ⇒ 跨 scenario 越界；
    //   且会误拒「scenario B 引用 scenario A 记录」的合法用法）。
    std::unordered_map<std::string, std::uint32_t> record_counts;
    // sanitized 名字 → 原标识符（⇒ ⇒ 撞名检测；sanitize 本身是**单射**的 ⇒
    //   ⇒ 理论上永不撞 ⇒ ⇒ 但**仍要检测** ⇒ ⇒ 一旦实现改了转义规则 ⇒ ⇒ 这里先红）
    std::unordered_map<std::string, std::string> hist_ident_owner;

    // 注册一个历史端口标识符 ⇒ ⇒ 撞名 ⇒ ⇒ fail-closed（ADR-076 D8）
    auto register_hist_ident = [&](const std::string &ident) -> bool {
        const auto sanitized = verif_hist_ident(ident);
        auto it = hist_ident_owner.find(sanitized);
        if (it != hist_ident_owner.end() && it->second != ident) {
            err = {lineno, "端口标识符 `" + ident + "` 与 `" + it->second +
                           "` 映射到同一个历史数组名（sanitize 撞名 ⇒ fail-closed）"};
            return false;
        }
        hist_ident_owner[sanitized] = ident;
        return true;
    };

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
        // ── 历史/序列判据的**静态良构性**（ADR-076 D4）──────────────────
        switch (o.kind) {
        case VerifOpKind::Record: {
            if (!register_hist_ident(o.port)) {
                return false;
            }
            auto &c = record_counts[o.port];
            if (c >= kVerifHistoryDepth) {
                err = {lineno, "端口 `" + o.port + "` 的历史记录数超过上限 " +
                               std::to_string(kVerifHistoryDepth) +
                               " ⇒ fail-closed（跨 scenario 累计）"};
                return false;
            }
            ++c;
            break;
        }
        case VerifOpKind::ExpectSeqSelf: {
            if (!register_hist_ident(o.port)) {
                return false;
            }
            if (o.seq_n == 0) {
                err = {lineno, "EXPECT_SEQ_SELF 的窗口长度 n 至少为 1"};
                return false;
            }
            if (o.seq_lag == 0) {
                err = {lineno, "EXPECT_SEQ_SELF 的 lag 至少为 1（lag = 0 ⇒ 恒真 ⇒ 禁）"};
                return false;
            }
            const auto have = record_counts[o.port];
            const auto need = static_cast<std::uint64_t>(o.seq_n) + o.seq_lag;
            if (have < need) {
                err = {lineno, "端口 `" + o.port + "` 的历史不足：EXPECT_SEQ_SELF 需要 " +
                               std::to_string(need) + " 条（n=" + std::to_string(o.seq_n) +
                               " + lag=" + std::to_string(o.seq_lag) + "），当前只有 " +
                               std::to_string(have) + " 条 ⇒ fail-closed"};
                return false;
            }
            break;
        }
        case VerifOpKind::ExpectSeqStep: {
            if (!register_hist_ident(o.port) || !register_hist_ident(o.port2)) {
                return false;
            }
            if (o.seq_n == 0) {
                err = {lineno, "EXPECT_SEQ_STEP 的窗口长度 n 至少为 1"};
                return false;
            }
            const auto have_v = record_counts[o.port];
            const auto need_v = static_cast<std::uint64_t>(o.seq_n) + 1u;
            if (have_v < need_v) {
                err = {lineno, "端口 `" + o.port + "` 的历史不足：EXPECT_SEQ_STEP 需要 " +
                               std::to_string(need_v) + " 条，当前只有 " +
                               std::to_string(have_v) + " 条 ⇒ fail-closed"};
                return false;
            }
            const auto have_e = record_counts[o.port2];
            if (have_e < need_v) {
                err = {lineno, "端口 `" + o.port2 + "` 的历史不足：EXPECT_SEQ_STEP 需要 " +
                               std::to_string(need_v) + " 条，当前只有 " +
                               std::to_string(have_e) + " 条 ⇒ fail-closed"};
                return false;
            }
            break;
        }
        case VerifOpKind::ExpectSeqCount: {
            if (!register_hist_ident(o.port)) {
                return false;
            }
            if (o.seq_n == 0) {
                err = {lineno, "EXPECT_SEQ_COUNT 的窗口长度 n 至少为 1"};
                return false;
            }
            const auto have = record_counts[o.port];
            if (have < o.seq_n) {
                err = {lineno, "端口 `" + o.port + "` 的历史不足：EXPECT_SEQ_COUNT 需要 " +
                               std::to_string(o.seq_n) + " 条，当前只有 " +
                               std::to_string(have) + " 条 ⇒ fail-closed"};
                return false;
            }
            if (o.seq_count > o.seq_n) {
                err = {lineno, "EXPECT_SEQ_COUNT 的 count=" + std::to_string(o.seq_count) +
                               " 超过窗口长度 n=" + std::to_string(o.seq_n) +
                               " ⇒ fail-closed"};
                return false;
            }
            break;
        }
        // ⚠️ 下面这些原语**不涉及**历史 ⇒ ⇒ **必须逐个列出**（仓内开 `-Wswitch-enum`
        //   ⇒ 即使有 `default:` 也会对「未列出的枚举值」告警 ⇒ ⇒ 漏列即编译红）。
        case VerifOpKind::Set:
        case VerifOpKind::Expect:
        case VerifOpKind::Tick:
        case VerifOpKind::Settle:
        case VerifOpKind::Wait:
        case VerifOpKind::ExpectStable:
        case VerifOpKind::Observe:
        case VerifOpKind::Reset:
            break;
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
        } else if (head == "WAIT") {
            // `WAIT <port> [<high|low>] <max>`（ADR-074 §7.2 D2′）⇒ 2 或 3 个实参
            // ⚠️ 与 `EXPECT_STABLE` **不共分支**：后者的参数个数/语法不得被本原语的放宽连带。
            if (toks.size() < 3) {
                err = {lineno, "WAIT 需要 <port> [<high|low>] <max>"};
                return false;
            }
            if (toks.size() > 4) {
                err = {lineno, "WAIT 参数过多（MUST 是 <port> [<high|low>] <max>）"};
                return false;
            }
            op.kind = VerifOpKind::Wait;
            op.port = toks[1];
            if (toks.size() == 4) {
                if (toks[2] != "high" && toks[2] != "low") {
                    err = {lineno, "WAIT 的 level 形参 MUST 是 `high` / `low` 关键字（收到 `" +
                                       toks[2] + "`）⇒ 数字与大小写变体均非法（fail-closed）"};
                    return false;
                }
                op.wait_level = (toks[2] == "high") ? WaitLevel::High : WaitLevel::Low;
                op.expr = toks[3];
            } else {
                op.expr = toks[2]; // 缺省 level = 非零（High）
            }
        } else if (head == "EXPECT_STABLE") {
            if (toks.size() < 3) {
                err = {lineno, head + " 需要 <port> <n>"};
                return false;
            }
            op.kind = VerifOpKind::ExpectStable;
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
        } else if (head == "RECORD") {
            // `RECORD <port>`（ADR-076 D1）⇒ ⇒ 追加**当前采样点**的值到该端口的历史序列
            if (toks.size() != 2) {
                err = {lineno, "RECORD 需要 <port>"};
                return false;
            }
            op.kind = VerifOpKind::Record;
            op.port = toks[1];
        } else if (head == "EXPECT_SEQ_SELF") {
            // `EXPECT_SEQ_SELF <port> <n> <lag>`
            if (toks.size() != 4) {
                err = {lineno, "EXPECT_SEQ_SELF 需要 <port> <n> <lag>"};
                return false;
            }
            op.kind = VerifOpKind::ExpectSeqSelf;
            op.port = toks[1];
            op.expr = toks[2];
            op.expr2 = toks[3];
        } else if (head == "EXPECT_SEQ_STEP") {
            // `EXPECT_SEQ_STEP <portV> <portE> <n>`
            if (toks.size() != 4) {
                err = {lineno, "EXPECT_SEQ_STEP 需要 <portV> <portE> <n>"};
                return false;
            }
            op.kind = VerifOpKind::ExpectSeqStep;
            op.port = toks[1];
            op.port2 = toks[2];
            op.expr = toks[3];
        } else if (head == "EXPECT_SEQ_COUNT") {
            // `EXPECT_SEQ_COUNT <port> <value> <n> <count>`
            if (toks.size() != 5) {
                err = {lineno, "EXPECT_SEQ_COUNT 需要 <port> <value> <n> <count>"};
                return false;
            }
            op.kind = VerifOpKind::ExpectSeqCount;
            op.port = toks[1];
            op.expr = toks[2];
            op.expr2 = toks[3];
            op.expr3 = toks[4];
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
            if (op.kind == VerifOpKind::ExpectStable) {
                // 每拍一条 check（与 IR 解释器一致 ⇒ 三方等式成立）
                n += op.count;
            } else if (is_assert_op(op.kind)) {
                // ⚠️ `Record` **不**计入（它只记录、不判断 ⇒ ADR-076 D1）
                ++n;
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
