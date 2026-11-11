// src/rtl_emit/cli.cpp
//
// 薄 CLI 的实现（见 `include/rtl_emit/cli.h`）。

#include "rtl_emit/cli.h"

#include "codegen_verilog.h"
#include "rtl_emit/manifest.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace ch {
namespace rtl_emit {

bool parse_cli_options(const std::vector<std::string> &args, CliOptions &out,
                       std::string &error) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string &a = args[i];
        auto need_value = [&](std::string &slot) -> bool {
            if (i + 1 >= args.size()) {
                error = "选项 " + a + " 缺少取值";
                return false;
            }
            slot = args[++i];
            return true;
        };
        if (a == "--help" || a == "-h") {
            out.help = true;
        } else if (a == "--out") {
            if (!need_value(out.out_dir)) return false;
        } else if (a == "--manifest") {
            if (!need_value(out.manifest_path)) return false;
        } else if (a == "--verilog") {
            if (!need_value(out.verilog_path)) return false;
        } else if (a == "--no-verilog") {
            out.emit_verilog = false;
        } else if (a == "--no-manifest") {
            out.emit_manifest = false;
        } else if (a == "--emit-hygiene") {
            out.emit_hygiene = true;
        } else if (a == "--allow-diagnostics") {
            out.allow_diagnostics = true;
        } else {
            error = "未知选项：" + a;
            return false;
        }
    }
    if (out.out_dir.empty()) {
        error = "--out 的取值不能为空";
        return false;
    }
    if (out.manifest_path.empty()) {
        out.manifest_path = out.out_dir + "/manifest.json";
    }
    if (out.verilog_path.empty()) {
        out.verilog_path = out.out_dir + "/top.v";
    }
    return true;
}

void print_cli_usage(const char *program_name) {
    std::cout
        << "用法：" << program_name << " [选项]\n"
        << "\n"
        << "  --out <dir>            输出目录（默认 rtl_out）\n"
        << "  --manifest <path>     manifest 落盘路径（默认 <out>/manifest.json）\n"
        << "  --verilog <path>      Verilog 落盘路径（默认 <out>/top.v）\n"
        << "  --no-verilog           不发射 Verilog\n"
        << "  --no-manifest          不写 manifest\n"
        << "  --allow-diagnostics    有诊断时仍返回 0（默认：诊断 ⇒ 返回 1）\n"
        << "  --emit-hygiene         产物卫生（default_nettype none + 元数据头 +\n"
        << "                         剪零引用信号 + proxy 恒等直通 + 不发 blanket\n"
        << "                         lint_off）—— ⚠️ **默认 off** ⇒ 默认路径产物逐字节不变\n"
        << "  -h, --help             本用法\n"
        << "\n"
        << "退出码：0 成功 · 1 运行失败或有诊断 · 2 用法错误（fail-closed）\n";
}

int run_emit_cli(const std::vector<std::string> &args, const DesignFactory &factory) {
    CliOptions options;
    std::string error;
    if (!parse_cli_options(args, options, error)) {
        std::cerr << "::error::" << error << "\n";
        return 2;
    }
    if (options.help) {
        print_cli_usage("cpphdl-emit-rtl");
        return 0;
    }
    if (!factory) {
        std::cerr << "::error::没有注册设计（factory 为空）\n";
        return 2;
    }

    // ── 输出目录（打不开 ⇒ 用法错误 ⇒ 退出码 2）──────────────────────
    std::error_code ec;
    std::filesystem::create_directories(options.out_dir, ec);
    if (ec) {
        std::cerr << "::error::无法创建输出目录 " << options.out_dir << "：" << ec.message()
                  << "\n";
        return 2;
    }

    // ── 建 ctx + 注册设计 ─────────────────────────────────────────────
    auto ctx = std::make_unique<core::context>("rtl_emit");
    std::unique_ptr<Component> root;
    {
        core::ctx_swap swap(ctx.get());
        root = factory(ctx.get());
        if (!root) {
            std::cerr << "::error::设计注册返回空（fail-closed）\n";
            return 2;
        }
        // ⚠️⚠️ **必须显式 `build()`** —— 用户注册的是**裸组件**（不是 `ch_device`）
        //   ⇒ ⇒ 没人会替它调 `build()` ⇒ ⇒ **不调 ⇒ `create_ports()` 与
        //   `describe()` 从未执行** ⇒ ⇒ **端口根本没被创建** ⇒ ⇒ manifest 是空的
        //   （实测踩中：初次实现漏了这一步 ⇒ ⇒ 清单里 0 个端口 + 1 条假诊断）。
        //   ⇒ ⇒ `ch_device` 内部做的正是这件事 ⇒ ⇒ 用户用 `ch_device` 时**不要**再调。
        root->build(ctx.get());
    }

    // ── manifest ─────────────────────────────────────────────────────
    const RtlManifest manifest = build_rtl_manifest(*root, ctx.get());

    if (options.emit_manifest) {
        std::ofstream out(options.manifest_path, std::ios::binary);
        if (!out) {
            std::cerr << "::error::无法写 manifest：" << options.manifest_path << "\n";
            return 2;
        }
        write_rtl_manifest_json(manifest, out);
        if (!out) {
            std::cerr << "::error::写 manifest 失败：" << options.manifest_path << "\n";
            return 1;
        }
    }

    // ── 产物（Verilog）────────────────────────────────────────────────
    if (options.emit_verilog) {
        // ⚠️ **产物卫生**开关（C6）—— ⚠️ **默认 off** ⇒ ⇒ 默认路径**逐字节不变**
        ch::EmitOptions emit_opts;
        emit_opts.emit_hygiene = options.emit_hygiene;
        toVerilog(options.verilog_path, ctx.get(), emit_opts);
        std::error_code fec;
        if (!std::filesystem::exists(options.verilog_path, fec) || fec) {
            std::cerr << "::error::产物未生成：" << options.verilog_path << "\n";
            return 1;
        }
    }

    // ── 诊断 ⇒ fail-closed（除非显式放行）─────────────────────────────
    if (!manifest.diagnostics.empty() && !options.allow_diagnostics) {
        for (const auto &d : manifest.diagnostics) {
            std::cerr << "::error::诊断[" << d.kind << "] 端口=" << d.port
                      << " owner=" << d.owner << "：" << d.detail << "\n";
        }
        std::cerr << "::error::存在 " << manifest.diagnostics.size()
                  << " 条诊断 ⇒ 退出码 1（fail-closed；--allow-diagnostics 可放行）\n";
        return 1;
    }

    std::cout << "OK：端口 " << manifest.ports.size() << " 个 · 诊断 "
              << manifest.diagnostics.size() << " 条\n";
    return 0;
}

}   // namespace rtl_emit
}   // namespace ch
