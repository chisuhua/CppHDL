// tests/test_emit_cli_modes.cpp
//
// **发射入口的模式标记 / `--out` 取值校验**（`rtl_emit::strip_mode_flags` ·
// `rtl_emit::require_out_dir`）的判别性用例。
//
// ⚠️ **为什么这两个 API 在库层**（2026-11-19）：模式标记的剥离规则与 `--out` 的取值
//   校验都是**产品无关的规则** ⇒ ⇒ 每个 tb 各写一份必然漂移 —— 实测：SoC 侧 2026-11-18
//   查实并修了「`--out --emit-hygiene` 会把 flag 当目录名」，Lite 侧**漏修**
//   ⇒ ⇒ `--emit-rtl --out --emit-hygiene` 在 Lite 侧 **exit 0 且静默建出
//   `--emit-hygiene/` 目录**（fail-open）⇒ ⇒ 本文件把该形态钉成**必红**的判据。
//
// ⚠️ **期望值 MUST 手推**（禁抄实现输出）。

#include "catch_amalgamated.hpp"

#include "rtl_emit/cli.h"

#include <string>
#include <vector>

using ch::rtl_emit::require_out_dir;
using ch::rtl_emit::strip_mode_flags;

namespace {

bool contains(const std::vector<std::string> &v, const std::string &s) {
    for (const auto &x : v) {
        if (x == s) return true;
    }
    return false;
}

}  // namespace

// ── strip_mode_flags ──────────────────────────────────────────────────────

TEST_CASE("cli: strip_mode_flags 位置无关 · 其余 argv 原序保留", "[rtl][cli]") {
    // ⚠️ 标记在**中间**与在**末尾**都要能剥掉（position independence）
    const std::vector<std::string> args = {"--out", "dir", "--emit-rtl", "--emit-hygiene",
                                           "--check-products"};
    std::vector<std::string> hit;
    const auto rest = strip_mode_flags(args, {"--emit-rtl", "--check-products"}, &hit);

    REQUIRE(rest.size() == 3);
    REQUIRE(rest[0] == "--out");
    REQUIRE(rest[1] == "dir");
    REQUIRE(rest[2] == "--emit-hygiene");
    REQUIRE(hit.size() == 2);
    REQUIRE(contains(hit, "--emit-rtl"));
    REQUIRE(contains(hit, "--check-products"));
}

TEST_CASE("cli: strip_mode_flags 保留**未登记**的 flag（不得误吞）", "[rtl][cli]") {
    const std::vector<std::string> args = {"--emit-hygiene", "--no-emit-hygiene", "--out", "d"};
    const auto rest = strip_mode_flags(args, {"--emit-rtl"}, nullptr);
    REQUIRE(rest == args);
}

TEST_CASE("cli: strip_mode_flags 无命中 ⇒ 原样返回", "[rtl][cli]") {
    const std::vector<std::string> args = {"--out", "d"};
    const auto rest = strip_mode_flags(args, {"--emit-rtl"}, nullptr);
    REQUIRE(rest == args);
}

// ── require_out_dir ──────────────────────────────────────────────────────

TEST_CASE("cli: require_out_dir 正常取值 ⇒ 通过并回填", "[rtl][cli]") {
    const std::vector<std::string> args = {"--out", "/tmp/ok", "--emit-hygiene"};
    std::string out;
    std::string err;
    REQUIRE(require_out_dir(args, &out, &err));
    REQUIRE(out == "/tmp/ok");
    REQUIRE(err.empty());
}

TEST_CASE("cli: require_out_dir 的取值**不能是另一个 flag**（★ 本次修复的核心形态）",
          "[rtl][cli]") {
    // ⚠️ `--out --emit-hygiene` ⇒ 必须**拒绝** ⇒ 否则会把 flag 当目录名
    //   ⇒ ⇒ 静默建出 `--emit-hygiene/`（实测：Lite 侧改前 exit 0 且真建了目录）
    const std::vector<std::string> args = {"--out", "--emit-hygiene"};
    std::string out;
    std::string err;
    REQUIRE_FALSE(require_out_dir(args, &out, &err));
    REQUIRE_FALSE(err.empty());
    REQUIRE(out.empty());
}

TEST_CASE("cli: require_out_dir 缺取值（末尾）⇒ 拒绝", "[rtl][cli]") {
    const std::vector<std::string> args = {"--emit-hygiene", "--out"};
    std::string out;
    std::string err;
    REQUIRE_FALSE(require_out_dir(args, &out, &err));
    REQUIRE_FALSE(err.empty());
}

TEST_CASE("cli: require_out_dir 完全缺 `--out` ⇒ 拒绝", "[rtl][cli]") {
    const std::vector<std::string> args = {"--emit-hygiene"};
    std::string out;
    std::string err;
    REQUIRE_FALSE(require_out_dir(args, &out, &err));
    REQUIRE_FALSE(err.empty());
}

TEST_CASE("cli: require_out_dir 空串取值 ⇒ 拒绝", "[rtl][cli]") {
    const std::vector<std::string> args = {"--out", ""};
    std::string out;
    std::string err;
    REQUIRE_FALSE(require_out_dir(args, &out, &err));
    REQUIRE_FALSE(err.empty());
}

TEST_CASE("cli: require_out_dir 的 out_dir 参数可为 nullptr（只要判据）", "[rtl][cli]") {
    const std::vector<std::string> args = {"--out", "/tmp/ok"};
    std::string err;
    REQUIRE(require_out_dir(args, nullptr, &err));
}
