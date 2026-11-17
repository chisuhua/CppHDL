// examples/riscv-mini/emit_rtl_hier.cpp
//
// **RV32I SoC 的层次化 RTL 发射入口**（诉求 ⑤「层次化输出示例」的产品级交付面）。
//
// ⚠️ **为什么需要它**：层次化输出此前只有**测试级**载体
//   （`examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp` 在用例内发射）⇒ ⇒
//   **用户拿不到产物**（无 CLI ⇒ ⇒ 拿不到 `.v` 文件 ⇒ ⇒ 也无独立 TB 可跑）
//   ⇒ ⇒ 「层次化输出」停在**特性**层，没到**产品**层。
//   ⇒ ⇒ 本文件把它落到产品层：**CLI 发射器 + 独立 TB + 覆盖登记**。
//
// ⚠️ **与扁平发射器（`emit_rtl.cpp`）的关系**：**共用同一个设计定义**
//   （`Rv32iPipelineTop`）⇒ ⇒ 唯一差别 = **发射形态**（扁平 vs 层次化）⇒ ⇒
//   两种产物的**顶层端口表同源**（同一个 `make_flat_module_view`）⇒ ⇒
//   **同一个独立 Verilog TB 可同时吃两种产物**（顶层 module 名同为 `top`）。
//   ⇒ ⇒ 这条「同源」是**可机械判定**的：manifest 的 `port_table[]` 逐字逐序
//   等于产物的顶层端口表（护栏 `scripts/verify_examples_rtl_sim.sh` ⑧）。
//
// ⚠️ **本文件是薄壳**：发射逻辑**全部**在库层
//   （`ch::rtl_emit::run_emit_cli` + `--hierarchical`）⇒ ⇒
//   **用户在自己的可执行文件里登记自己的设计 + 加一个 flag** ⇒ ⇒ 同一路径。
//
// 用法：
//   build/examples/riscv-mini/emit_rtl_rv32i_soc_hier --out <dir>
//       [--emit-hygiene] [--emit-tb-stub --tb-stub-hint riscv-mini-rv32i-soc-hier]

#include "rv32i_pipeline_top.h"

#include "rtl_emit/cli.h"

#include <memory>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 0 ? argc : 0));
    // ⚠️ **强制层次化**：本 target 的**唯一职责**就是发层次化产物 ⇒ ⇒
    //   flag 放在**最前** ⇒ ⇒ 调用方无法误关掉它（没有 `--no-hierarchical` 选项）
    args.emplace_back("--hierarchical");
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    const ch::rtl_emit::DesignFactory factory =
        [](ch::core::context *) -> std::unique_ptr<ch::Component> {
        return std::make_unique<Rv32iPipelineTop>(nullptr, "top");
    };
    return ch::rtl_emit::run_emit_cli(args, factory);
}
