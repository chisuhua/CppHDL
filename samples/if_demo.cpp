/**
 * @file if_demo.cpp
 * @brief chlib 条件构造的**优先级 / 互斥**语义（表达式形式 + 语句块形式）
 *
 * ⚠️ **重写原因**（2026-10-30）：此前本 sample 用的是**已不存在 / 已改名**的 API，
 *    且**更危险**的是：它**编译不过**这件事掩盖了一个**语义缺陷**。
 *
 * ① `priority_if_then` —— 已**物理移除**（ADR-051 D8：与 `if_then` **逐行等价**
 *    ⇒ 名字 over-promise ⇒ 依 Zero-Debt + ADR-043 直接删、无 deprecation 别名期）
 *    ⇒ ⇒ 本 sample 演示的「优先级」语义改由 `if_then` 表达（**语义未变**）；
 * ② `.else_(lambda)` —— `else_` 接受的是**值**；lambda 走 `.then(...)`；
 * ③ `always_ff(io().clk.posedge(), ...)` —— 框架**没有** `always_ff`
 *    ⇒ ⇒ `ch_reg` 用**隐式时钟**（`next` 在每个时钟沿被采样）⇒ ⇒ 该写法的
 *    语义「每拍保持」**本就多余**（`ch_reg` 的 `next` 默认就是保持）；
 *    ⇒ ⇒ 且 `posedge` 成员已从 `port` 移除；
 * ④ `next_type::value()` —— 不存在（`ch_reg::operator->` 返回 `next_proxy`，
 *    其 `next` 是**可写**的）；
 * ⑤ 🔴 **条件被彻底忽略**（比编译错误更危险）：`_if(io().en).then([&] {
 *    reg_count->next = ...; })` 走的是**普通赋值** ⇒ ⇒ **未经**
 *    `conditional_assign` 捕获 ⇒ ⇒ 条件**不参与**（ADR-051 §1.2 ② 的双重
 *    失效形态）⇒ ⇒ 即便改到当时的 DSL，计数器也**永远在计数**（与意图相反）。
 *    ⇒ ⇒ 现改为 `conditional_assign(reg_count).next() = ...`。
 *
 * ⚠️ **与 `conditional_demo` 的分工**（避免两个 sample 演示同一件事）：
 *    `conditional_demo` 演示「**语句块形式 + 嵌套**」；
 *    本 sample 演示「**表达式形式**的**优先级 / 互斥**」+「**直接赋给 `ch_out`」**
 *    （E1，2026-10-30 修复）+「**端口作条件**」（E3，同）+「**裸 literal**」
 *    （`conditional_assign` 的 value 接受裸 literal，2026-10-30 修复）。
 *
 * ⚠️ **自检**：本 sample MUST 以 **0** 退出（运行护栏只查退出码 ⇒ 返回 0 会让
 *    行为退化被判 PASS ⇒ 这正是它长期「假绿」的机制）⇒ ⇒ 断言失败 ⇒ 返回 1。
 */

#include "ch.hpp"
#include "chlib/if.h"
#include "chlib/if_stmt.h"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"
#include <cstdio>

using namespace ch::core;
using namespace chlib;

namespace {

// ===========================================================================
// ① 表达式形式（`if_then` / `elif` / `else_`）—— **先匹配者胜**
// ===========================================================================
//
// ⚠️ 「**先匹配者胜**」= 分支的**书写顺序**就是优先级（ADR-051 D4/D8）。
//    ⚠️ 原 `priority_if_then` 之所以被删，正是因为它与 `if_then` **逐行等价**
//    ⇒ ⇒ 「优先级」从来不是 `priority_if_then` 提供的**额外能力**，
//    而是 `if_then` **本来就有的**语义 ⇒ ⇒ 由本 sample 的**用例**钉住它。
class PriorityMux : public ch::Component {
public:
    __io(ch_in<ch_bool> is_zero;      // 命中 ⇒ 0
         ch_in<ch_bool> is_max;       // 命中 ⇒ 255
         ch_out<ch_uint<8>> out;)

    PriorityMux(ch::Component *parent = nullptr,
                const std::string &name = "priority_mux")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // ⚠️ **两条新能力同时被演示**（均于 2026-10-30 修复）：
        //   ① **E3**：条件是 `ch_in<ch_bool>` **端口** ⇒ 此前**编译不过**
        //      （形参曾是 `const ch_bool &` ⇒ 端口绑不上）；
        //   ② **E1**：`multi_if` **直接赋给 `ch_out`** ⇒ 此前**编译不过**
        //      （`ch_out::operator=` 走 `get_lnode(value)` ⇒ 而 `multi_if`
        //      无 `impl()` ⇒ 无匹配重载）。
        //
        // ⚠️ **分支值 MUST 同类型 `T`** ⇒ 三个分支都显式收敛到 `ch_uint<8>`
        //    （ADR-052 D5：结果类型恒为 `T`）。
        io().out = if_then<ch_uint<8>>(io().is_zero, ch_uint<8>(0_d))
                       .elif(io().is_max, ch_uint<8>(255_d))
                       .else_(ch_uint<8>(1_d));
    }
};

// ===========================================================================
// ② 语句块形式（`_if` + `conditional_assign`）—— 条件**真的**被求值
// ===========================================================================
class ConditionalCounter : public ch::Component {
public:
    __io(ch_in<ch_bool> rst, en;
         ch_out<ch_uint<8>> count;)

    ConditionalCounter(ch::Component *parent = nullptr,
                       const std::string &name = "counter")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> reg_count(0_d);

        // ⚠️ 条件赋值 MUST 经 `conditional_assign` 捕获 ⇒ 才会被条件化。
        //    未走捕获点的普通 `reg->next = ...` 是**无条件**的
        //    ⇒ 静态护栏 `scripts/verify_if_stmt_wiring.sh` 会检出。
        //
        // ⚠️ **裸 literal 可用**（`0_d`）—— `conditional_assign` 的 value
        //    MUST 接受裸 literal（2026-10-30 修复 ⇒ 经 `to_operand` 归一化）；
        //    此前裸 literal **编译不过**（`ch_literal_impl` 无 `get_lnode` 重载）
        //    ⇒ ⇒ 连 `if_stmt.h` **自己的头注释示例**都编译不过。
        //
        // ⚠️ **先匹配者胜** ⇒ `rst` 压过 `en`；未匹配 ⇒ **保持**
        //    （寄存器无 `else_` ⇒ 默认尾部 = 保持）。
        _if(io().rst)
            .then([&] { conditional_assign(reg_count).next() = 0_d; })
            .elif(io().en)
            .then([&] {
                // ⚠️ 显式收窄到 8 位（`reg_count + 1` 是 **9 位** ⇒ 带进位位）
                //    ⇒ 不显式收窄 ⇒ **静默截断**（ADR-052 D1 要治的病）。
                conditional_assign(reg_count).next() =
                    ch_uint<8>(reg_count + ch_uint<8>(1_d));
            })
            .end();

        io().count = reg_count;
    }
};

uint64_t read_port(ch::Simulator &sim, ch::core::lnodeimpl *ln) {
    auto it = sim.data_map().find(ln->id());
    if (it == sim.data_map().end()) {
        return UINT64_MAX;
    }
    return static_cast<uint64_t>(it->second);
}

int failures = 0;

void check(bool ok, const char *what) {
    if (!ok) {
        std::printf("  [FAIL] %s\n", what);
        ++failures;
    } else {
        std::printf("  [ok]   %s\n", what);
    }
}

} // namespace

int main() {
    std::printf("CppHDL Conditional Priority Demo\n");
    std::printf("=================================\n");

    // ---------------------------------------------------------------------
    // ① 表达式形式：优先级 = **分支的书写顺序**
    // ---------------------------------------------------------------------
    {
        std::printf("[表达式形式] if_then / elif / else_（先匹配者胜）\n");
        ch::ch_device<PriorityMux> dev;
        ch::Simulator sim(dev.context());
        auto *out = dev.instance().io().out.impl();

        // ⚠️ 手推：
        //   * `is_zero=1` ⇒ 命中**第一个**分支 ⇒ out = **0**
        //   * `is_max=1`（且 is_zero=0）⇒ 命中**第二个** ⇒ out = **255**
        //   * 两个都为 0 ⇒ 落到 `else_` ⇒ out = **1**
        //   * ⚠️ **两个都为 1** ⇒ 「先匹配者胜」⇒ 仍是 **0**
        //     （⇒ 这是「顺序即优先级」这条契约的**判别性**用例：
        //       若实现的链序反了 ⇒ 会取 **255** ⇒ 转红。
        //       ⚠️ 如实登记：真实设计中 `is_zero` 与 `is_max` **互斥**
        //       ⇒ 「同时为真」只可能由 TB 人为驱动 ⇒ ⇒ 本用例验证的是
        //       **契约本身**（顺序即优先级），不是某个真实场景。）
        sim.set_input_value(dev.instance().io().is_zero, 1u);
        sim.set_input_value(dev.instance().io().is_max, 0u);
        sim.tick(1);
        check(read_port(sim, out) == 0u, "is_zero=1 ⇒ 0");

        sim.set_input_value(dev.instance().io().is_zero, 0u);
        sim.set_input_value(dev.instance().io().is_max, 1u);
        sim.tick(1);
        check(read_port(sim, out) == 255u, "is_max=1 ⇒ 255");

        sim.set_input_value(dev.instance().io().is_max, 0u);
        sim.tick(1);
        check(read_port(sim, out) == 1u, "都不命中 ⇒ else_ ⇒ 1");

        sim.set_input_value(dev.instance().io().is_zero, 1u);
        sim.set_input_value(dev.instance().io().is_max, 1u);
        sim.tick(1);
        check(read_port(sim, out) == 0u, "两者同时命中 ⇒ **先匹配者胜** ⇒ 0");
    }

    // ---------------------------------------------------------------------
    // ② 语句块形式：条件**真的**被求值
    // ---------------------------------------------------------------------
    {
        std::printf("[语句块形式] _if + conditional_assign\n");
        ch::ch_device<ConditionalCounter> dev;
        ch::Simulator sim(dev.context());
        auto *count = dev.instance().io().count.impl();

        // ⚠️ 手推：
        //   * rst=0, en=0 ⇒ **未匹配** ⇒ 寄存器**保持** ⇒ 0
        //     ⇒ ⚠️ **判别性**：若条件被忽略（走普通 `reg->next =`）
        //       ⇒ 每拍无条件 +1 ⇒ 这里会读到 **1** ⇒ 转红。
        sim.set_input_value(dev.instance().io().rst, 0u);
        sim.set_input_value(dev.instance().io().en, 0u);
        sim.tick(1);
        check(read_port(sim, count) == 0u, "rst=0,en=0 ⇒ 保持 0（**条件真的被求值**）");

        //   * en=1 ⇒ 加一 ⇒ 1
        sim.set_input_value(dev.instance().io().en, 1u);
        sim.tick(1);
        check(read_port(sim, count) == 1u, "en=1 ⇒ 计数 1");

        //   * 再来一拍 ⇒ 2
        sim.tick(1);
        check(read_port(sim, count) == 2u, "en=1 ⇒ 计数 2");

        //   * rst=1 ⇒ 清零 ⇒ 0（**先匹配者胜** ⇒ rst 压过 en）
        sim.set_input_value(dev.instance().io().rst, 1u);
        sim.tick(1);
        check(read_port(sim, count) == 0u, "rst=1 ⇒ 清零 0");
    }

    std::printf("\n");
    if (failures != 0) {
        std::printf("[FAIL] %d 项自检未通过\n", failures);
        return 1;
    }
    std::printf("[PASS] 全部自检通过\n");
    return 0;
}
