// tests/test_verilog_module_emitter.cpp
//
// 层次化 RTL 输出 · T3-③（emitter 拆分 + 时钟/复位名参数化）的判据。
//
// 本 change 是**纯重构**（`.openspec.yaml` 显式 `skip_specs: true`）⇒ ⇒ 判据全部是
// 「拆分前后等价」+「参数化真的生效」两条：
//
//   · `verilogwriter` 的输出 MUST == `ModuleEmitter` + `make_flat_module_view` 的输出
//     （**逐字节**）⇒ ⇒ 拆分是搬移，不是改写；
//   · `ModuleView.clock_name` / `reset_name` SHOULD 真的改到发射文本里
//     （子 module 的 `always_ff` MUST NOT 引用 RTL 顶层的 `default_clock` ——
//      那是**本模块里不存在的信号**，Verilator 会直接报未声明标识符）。
//
// ⚠️ 本文件**不改** `tests/golden/` 的既有产物：扁平产物的「逐字节不变」判据在
//    `tests/golden/verilog/` 之外的**基线比对**里（见 change 的 tasks 3.1）。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "codegen_verilog.h"
#include "device.h"

#include <sstream>
#include <string>

using namespace ch;
using namespace ch::core;

namespace {

// 覆盖两条 always_ff 路径：print_reg（寄存器更新）与 print_mem_read_port（sread）。
class EmitDut : public Component {
public:
    __io(ch_in<ch_uint<4>> addr; ch_in<ch_uint<8>> din; ch_in<ch_bool> en;
         ch_out<ch_uint<8>> dout; ch_out<ch_uint<8>> q;)

    ch_reg<ch_uint<8>> r{0_d};
    ch_reg<ch_bool> en_r{false};
    ch_mem<ch_uint<8>, 16> mem{"mem"};

    EmitDut(Component* p = nullptr, const std::string& n = "top") : Component(p, n) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        en_r->next = io().en;
        r->next = select(io().en, r + io().din, r);
        mem.write(io().addr, io().din, io().en);
        io().q <<= r;
        io().dout <<= mem.sread(io().addr, en_r);
    }
};

std::string emit_flat(context* ctx) {
    std::ostringstream os;
    verilogwriter w(ctx);
    w.print(os);
    return os.str();
}

std::string emit_view(context* ctx, ModuleView view) {
    std::ostringstream os;
    ch::ModuleEmitter e(ctx, std::move(view));
    e.print(os);
    return os.str();
}

void rename_clock_reset(ModuleView& view, const std::string& clk, const std::string& rst) {
    for (auto& kv : view.names) {
        if (kv.first->type() == lnodetype::type_clock) {
            kv.second = clk;
        } else if (kv.first->type() == lnodetype::type_reset) {
            kv.second = rst;
        }
    }
    view.clock_name = clk;
    view.reset_name = rst;
}

}   // namespace

// ═════════════════════════════════════════════════════════════════════════
// 拆分等价：`verilogwriter` == `ModuleEmitter(make_flat_module_view(ctx))`
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("module emitter: the flat writer is a pure delegation", "[verilog][emitter]") {
    ch::ch_device<EmitDut> dev;

    const std::string via_writer = emit_flat(dev.context());
    const std::string via_emitter =
        emit_view(dev.context(), ch::make_flat_module_view(dev.context()));

    INFO("writer:\n" << via_writer);
    REQUIRE(via_emitter == via_writer);

    // 模块名与端口都还在（防「两边一起空转」）
    REQUIRE(via_emitter.find("module top (") != std::string::npos);
    REQUIRE(via_emitter.find("endmodule") != std::string::npos);
    REQUIRE(via_emitter.find("always_ff") != std::string::npos);
}

// ═════════════════════════════════════════════════════════════════════════
// 时钟 / 复位名参数化：MUST 改到**每一处** always_ff（含 mem 读端口那条）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("module emitter: clock and reset names are parameterized", "[verilog][emitter]") {
    ch::ch_device<EmitDut> dev;

    auto view = ch::make_flat_module_view(dev.context());
    rename_clock_reset(view, "clk_i", "rst_ni");
    const std::string text = emit_view(dev.context(), view);

    INFO("emitted:\n" << text);

    // ① 端口用新名
    REQUIRE(text.find("input clk_i") != std::string::npos);
    REQUIRE(text.find("input rst_ni") != std::string::npos);

    // ② 每一处 always_ff 都用新名
    REQUIRE(text.find("posedge clk_i or posedge rst_ni") != std::string::npos);
    REQUIRE(text.find("if (!rst_ni)") != std::string::npos);

    // ③ ⚠️ 旧名 MUST **彻底消失** —— 子 module 里引用 RTL 顶层的
    //    `default_clock` 是**未声明标识符**（Verilator 直接报错）
    REQUIRE(text.find("default_clock") == std::string::npos);
    REQUIRE(text.find("default_reset") == std::string::npos);

    // ④ mem 的 `sread` 那条 `always_ff` 也在内（两条 always_ff 都换了名）
    std::size_t posedge_count = 0;
    for (std::size_t at = text.find("posedge clk_i");
         at != std::string::npos;
         at = text.find("posedge clk_i", at + 1)) {
        ++posedge_count;
    }
    INFO("posedge clk_i occurrences: " << posedge_count);
    REQUIRE(posedge_count >= 2);
}
