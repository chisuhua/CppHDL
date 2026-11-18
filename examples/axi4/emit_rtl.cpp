// examples/axi4/emit_rtl.cpp
//
// **RTL 发射入口**（`examples-rtl-sim-coverage` E3/E5 · 方案 3）。
//
// ⚠️ **2026-11-17（`rtl-verification-single-source` C1）**：顶层组件 `AxiLiteTop`
//   已抽到 `include/axi4/axi4_lite_example_top.h` ⇒ ⇒ 本文件**不再**用
//   `#define main` + `#include "tb_axi4_lite_example.cpp"` 的旧手法（那是「类定义在 .cpp
//   里」的绕路）⇒ ⇒ 现在只做**注册设计 + 调库**。
//
// 用法：
//   build/examples/axi4/emit_rtl_axi4_lite --out <dir>
//       [--emit-hygiene] [--emit-tb-verif examples/axi4/tb_axi4_lite_example.verif]

#include "axi4/axi4_lite_example_top.h"
#include "rtl_emit/cli.h"

#include <memory>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    const ch::rtl_emit::DesignFactory factory =
        [](ch::core::context *) -> std::unique_ptr<ch::Component> {
        return std::make_unique<axi4::example::AxiLiteTop>(nullptr, "top");
    };
    return ch::rtl_emit::run_emit_cli(args, factory);
}
