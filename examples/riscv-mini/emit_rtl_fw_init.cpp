// examples/riscv-mini/emit_rtl_fw_init.cpp
//
// **RTL 发射入口 · 带固件初值**（`firmware-load-chain` **L2 = 路线 A** / **L6**）。
//
// ⚠️ 与 `emit_rtl.cpp` 的差别：**只有一处** —— 构造 SoC 时把固件的
//   **文本 / 数据两段镜像**作为 TCM 的 `ch_mem::init_data` 传进去
//   ⇒ ⇒ 产物里自动出现 `initial begin mem[i] = … end`（`print_mem_init`）
//   ⇒ ⇒ **TB 不需要任何装载动作**（零 `prog_*` 写）· **顶层端口表一字不变**。
//
// 用法：
//   build/examples/riscv-mini/emit_rtl_rv32i_soc_fw_init --out <dir>
//
// ⇒ ⇒ 产物级判据见 `scripts/verify_rv32i_fw_dump.sh` 的 **D7**
//   （I-TCM / D-TCM 的 `initial` 初值块 MUST 携带**手推**的固件字）。

#include "rv32i_fw_demo.h"
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
        // ⚠️ **固件不进端口**：走构造参数 ⇒ ⇒ `init_data` ⇒ ⇒ 产物自带 `initial`
        return std::make_unique<Rv32iPipelineTop>(
            nullptr, "top", rv32i_fw_demo::text(), rv32i_fw_demo::data());
    };
    return ch::rtl_emit::run_emit_cli(args, factory);
}
