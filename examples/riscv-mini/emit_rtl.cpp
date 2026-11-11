// examples/riscv-mini/emit_rtl.cpp
//
// **RTL 发射入口**（`examples-rtl-sim-coverage` **E3/E5** · 方案 3）。
//
// ⚠️ 与其它 example 的差别：**该组件在头文件里**（`src/rv32i_pipeline_top.h`）
//   ⇒ ⇒ 不需要「include 整个 .cpp + 屏蔽 main」那套 ⇒ ⇒ 直接 include 头文件即可
//   ⇒ ⇒ **example 源码零改动**。
//
// 用法：
//   build/examples/riscv-mini/emit_rtl_rv32i_soc --out <dir>
//       [--emit-hygiene] [--emit-tb-stub --tb-stub-hint riscv-mini-rv32i-soc]

#include "rv32i_pipeline_top.h"

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
        return std::make_unique<Rv32iPipelineTop>(nullptr, "top");
    };
    return ch::rtl_emit::run_emit_cli(args, factory);
}
