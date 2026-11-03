// ⚠️⚠️ **`mux` 的槽位宽度 MUST NOT 被 `eval` 改变**（2026-11-03 · B2 根因修复的判据）
//
// ## 缺陷（实测定位，见 `.rddf/improvements/cross-dut-bit-select-out-of-range-2026-11.md`）
//
// `include/ast/instr_mux.h` 的 `eval()` 曾写成：
//
//     if (!cond_->is_zero()) *dst_ = *true_val_;      // ← 拷贝赋值
//     else                   *dst_ = *false_val_;
//
// ⇒ ⇒ `bitvector::operator=` 会**把 dst 的宽度改成源的宽度**（RESIZE）⇒ ⇒
// 而 `mux` 节点的宽度 = `max(真分支宽, 假分支宽)`（`build_mux`）⇒ ⇒
// **较窄的分支被选中时，整个 mux 槽位被压窄**。
//
// ## 为什么这个缺陷极难发现（三层掩盖）
//
// 1. **只在 interpreter 侧**：Verilog 的 `assign d = c ? t : f;` 天然保持声明宽度
//    ⇒ ⇒ 这是**两后端分歧** ⇒ ⇒ 双后端 parity 能抓（本文件正是如此）。
// 2. **取决于求值顺序**：`eval_combinational` 里 `mux` 与消费它的节点谁先
//    ⇒ ⇒ 换个激励 / **同一进程内多建一个 DUT**（node id 全局唯一 ⇒ 拓扑序
//    tie-break 变）就可能时好时坏 ⇒ ⇒ 表现为「6 条 stderr 越界错误」的噪声。
// 3. **越界分支写 0** ⇒ ⇒ 若被选中的窄分支其高位本来就是 0
//    ⇒ ⇒ **结果碰巧正确** ⇒ ⇒ 「静默的错误值」。
//
// ## 本文件的判据（每一条都对应一个具体失败模式）
//
// | 断言 | 抓什么 |
// |---|---|
// | `mux_out` 在「宽分支 → 窄分支」后**高位归零** | ① `operator=` 把槽位压窄（下游按声明宽度读会拿到旧高位）；② **反方向**：若用 `assign_truncate` 但它的零扩展路径**不清高位**（`bv_assign_truncate` 在「同 word 内 src < dst」时 `zero_start_word == zero_end_word` ⇒ **不清位**）⇒ ⇒ 这条会红 |
// | `bit2/bit1` 在窄分支被选中时**不越界**且为 0 | `operand_width` 被压成 1 ⇒ ⇒ `bit_select(m, 1|2)` 越界 |
// | 两后端逐拍一致 | 上述任一在 interpreter 侧成立而 Verilog 侧不成立 ⇒ ⇒ 红 |
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == 2 × 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

namespace {

// ===========================================================================
// DUT：mux（真分支 3 位 / 假分支 1 位）+ 高位取位
// ===========================================================================
class MuxWidthDut : public ch::Component {
public:
    __io(ch_in<ch_bool> cond;
         ch_in<ch_uint<3>> tval;   // 真分支：3 位
         ch_in<ch_uint<1>> fval;   // 假分支：**1 位**（窄）
         ch_out<ch_uint<3>> mux_out;
         ch_out<ch_bool> bit2;     // mux 结果的 bit2 —— 修复前会越界
         ch_out<ch_bool> bit1;)

    MuxWidthDut(ch::Component *parent = nullptr,
                const std::string &name = "mux_width_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_uint<3> m = select(io().cond, io().tval, io().fval);
        io().mux_out <<= m;
        // ⚠️ 这两个取位就是**判别点**：m 的槽位若被压成 1 位
        //    ⇒ ⇒ bit_select(m, 1) / (m, 2) **越界**（打印错误并写 0）
        io().bit2 <<= bit_select(m, 2);
        io().bit1 <<= bit_select(m, 1);
    }
};

// ===========================================================================
// 拍表（**手推**：mux = 选中分支零扩展到 3 位）
//   {cond, tval(3b), fval(1b) | mux_out(3b), bit2, bit1}
// ===========================================================================
struct MuxCase {
    uint64_t cond, tval, fval, mux_out, bit2, bit1;
};
const MuxCase kMuxCases[] = {
    // 宽分支：tval 完整输出
    {1, 0b111, 0, 0b111, 1, 1},
    // ★ 关键：上一步 bit2/bit1 都是 1 ⇒ 这一步切到窄分支
    //   ⇒ ⇒ 若槽位**压窄**或**不清高位** ⇒ ⇒ mux_out 会残留 0b110 / 0b010
    {0, 0b111, 0, 0b000, 0, 0},
    // 窄分支 = 1 ⇒ ⇒ 零扩展到 0b001（高两位 MUST 为 0）
    {0, 0b111, 1, 0b001, 0, 0},
    // 宽分支 0b101 ⇒ ⇒ bit2=1 bit1=0
    {1, 0b101, 1, 0b101, 1, 0},
    // 再切窄分支（此时 bit2=1）⇒ ⇒ MUST 归零
    {0, 0b101, 0, 0b000, 0, 0},
    // 宽分支 0b010 ⇒ ⇒ bit2=0 bit1=1
    {1, 0b010, 0, 0b010, 0, 1},
    // 窄分支 = 1 ⇒ ⇒ 0b001（bit2=0 bit1=0）
    {0, 0b010, 1, 0b001, 0, 0},
};

}  // namespace

DUAL_BACKEND_TEST_PAIR(
    MuxWidthDut,
    "mux：槽位宽度 MUST 保持（窄分支被选中时高位归零 + 高位取位不越界）",
    "[core][mux][width][dual][b2]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kMuxCases) {
            sim.set_input_value(io.cond, tc.cond);
            sim.set_input_value(io.tval, tc.tval);
            sim.set_input_value(io.fval, tc.fval);
            sim.tick();
            INFO("cond=" << tc.cond << " tval=0b" << std::oct << tc.tval
                         << " fval=" << tc.fval);
            // ① 槽位宽度保持 ⇒ ⇒ 值（含高位）正确
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.mux_out)) ==
                    tc.mux_out);
            // ② 高位取位不越界 ⇒ ⇒ 值正确（越界时实现写 0 ⇒ ⇒ 这里能抓到「该 1 却 0」）
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.bit2)) ==
                    tc.bit2);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.bit1)) ==
                    tc.bit1);
        }
    });
