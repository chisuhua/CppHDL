// tools/cpphdl-emit-rtl/main.cpp
//
// **RTL 发射的薄 CLI**（`rtl-emit-product-tooling` **C1**）。
//
// ⚠️ **本仓二进制 MUST NOT 链 `examples/**`**（C1 硬要求）⇒ ⇒ 本工具用
//   **自己的**小设计 ⇒ ⇒ 工具**不依赖** examples ⇒ ⇒ `examples/` 改了不会
//   影响它 ⇒ ⇒ 它可以当**自测载体**。
//
// ⚠️ **这是「使用者」，不是「库」**：真正的能力在 `ch::rtl_emit::run_emit_cli()`
//   （`include/rtl_emit/cli.h`）⇒ ⇒ 用户在自己的可执行文件里**登记**自己的设计
//   ⇒ ⇒ 退出码 / 失败语义**全仓一致**。
//
// ⚠️ **退出码（fail-closed）**：0 成功 · 1 运行失败或有诊断 · 2 用法错误。
//   ⇒ ⇒ 详见 `include/rtl_emit/cli.h` 的文件头。

#include "ch.hpp"
#include "component.h"
#include "rtl_emit/cli.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace ch::core;

namespace {

// ── 工具自带的**小设计**（刻意简单 ⇒ ⇒ 产物可肉眼核对）────────────────
// ⚠️ 端口**全部具名** ⇒ ⇒ 产物端口名可读 ⇒ ⇒ 工具本身就是 N1 约定的一个实例。
class Leaf : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>>  data_in{"data_in"};
        ch_out<ch_uint<8>> data_out{"data_out"};
    )

    Leaf(ch::Component *parent = nullptr, const std::string &name = "leaf")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().data_out <<= io().data_in;
    }
};

class Top : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      prog_we{"prog_we"};
        ch_in<ch_uint<8>>   prog_data{"prog_data"};
        ch_out<ch_uint<8>>  result{"result"};
    )

    ch::ch_module<Leaf> leaf{"leaf"};

    Top(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        leaf.io().data_in <<= io().prog_data;
        // ⚠️ **BLOCKER-1 的常驻回归靶**（C6 · 见 `.rddf/reviews/b1-batch-verify-2026-11.md`）：
        //   `select()` 的**两个臂都是字面量** ⇒ ⇒ 产物是
        //   `assign x = cond ? lit_a : lit_b;` ⇒ ⇒ `print_mux` **不内联**字面量
        //   （按名字引用）⇒ ⇒ 剪枝若误判「使用者是 mux ⇒ 内联」就会把这两个 lit
        //   连同声明一起剪掉 ⇒ ⇒ 产物**引用未声明标识符** ⇒ ⇒ 在
        //   `` `default_nettype none `` 下**直接编译失败**（实测：RV32I 扁平 +
        //   层次化双双 exit=1）。
        //   ⇒ ⇒ 本设计（C5 的 **F8** lint 门）由此成为该缺陷的**常驻回归**。
        auto swapped = select(io().prog_we, ch_uint<8>(1_d), ch_uint<8>(2_d));
        io().result <<= leaf.io().data_out + swapped;
    }
};

}   // namespace

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

    const int rc = ch::rtl_emit::run_emit_cli(args, factory);
    if (rc == 2) {
        ch::rtl_emit::print_cli_usage("cpphdl-emit-rtl");
    }
    return rc;
}
