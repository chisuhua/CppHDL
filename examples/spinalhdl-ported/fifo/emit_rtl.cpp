// examples/spinalhdl-ported/fifo/emit_rtl.cpp
//
// **RTL 发射入口**（`examples-rtl-sim-coverage` **E3/E5** · 方案 3：
// 「每 example 自带 `emit_rtl.cpp`」⇒ ⇒ 独立 target ⇒ ⇒ 不碰 example 的 `main()`
// ⇒ ⇒ 也不违反 C1 的「工具 MUST NOT 链 `examples/**`」约束
// （这个 target **在** examples 里 ⇒ ⇒ 它**就是** example 的一部分）。
//
// ⚠️ **为什么用「include 整个 .cpp」而不是「抽头文件」**：
//   该 example 的顶层组件类（`Top`）**定义在 .cpp 里**（不是头文件）
//   ⇒ ⇒ 要在**另一个 TU** 里构造它，就得把组件声明搬进头文件（动 example 的结构）
//   ⇒ ⇒ 而 `#define main ...` + `#include "<impl>.cpp"` 让本 TU **复用同一份定义**
//   ⇒ ⇒ example 的源码**零改动**（只在本文件里屏蔽它的 `main`）。
//
// ⚠️ **`main` 被改名** ⇒ ⇒ example 的演示入口仍然照常编译进本二进制
//   （只是名字变了 ⇒ 不会被当入口点）⇒ ⇒ 本 target 的行为**只有**「发射 RTL」。
//
// 用法：
//   build/examples/spinalhdl-ported/fifo/emit_rtl_emit_rtl_fifo_stream --out <dir>
//       [--emit-hygiene] [--emit-tb-stub --tb-stub-hint fifo_stream_example]

// ⚠️ 先屏蔽被包含实现里的 `main`（它不是本 target 的入口）
#define main cpphdl_example_main_disabled
#include "stream_fifo_example.cpp"
#undef main

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
        return std::make_unique<Top>(nullptr, "top");
    };
    return ch::rtl_emit::run_emit_cli(args, factory);
}
