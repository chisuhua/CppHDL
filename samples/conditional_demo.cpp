/**
 * @file conditional_demo.cpp
 * @brief chlib 条件构造的两种风格（表达式形式 + 语句块形式）
 *
 * ⚠️ **重写原因**（2026-10-30，change `chlib-conditional-semantics-2026-10`，
 *    实施 **ADR-051**）：此前本 sample 用的是**已不存在**的 API
 *    （`seq_if` / `reg_if` / `.endif()`）⇒ 且它演示的**语句块风格**在旧实现里
 *    **条件被彻底忽略**（`end()` 先置 `finalized_` 再遍历分支体 ⇒ 捕获机制
 *    永不触发）⇒ ⇒ 即便改到当时的 DSL，计数器也**永远不计数**（「绿 ≠ 对」）。
 *
 * ⇒ 现按 **ADR-051** 的语义重写：
 *   ① **表达式形式**（`if_then`）—— 行为**未变**；
 *   ② **语句块形式**（`_if` + `conditional_assign`）—— 条件**真的**被求值。
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
#include <memory>

using namespace ch::core;
using namespace chlib;

namespace {

// ===========================================================================
// 表达式风格（`if_then`）—— 行为未变
// ===========================================================================
class ConditionalALU : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> a, b;
         ch_in<ch_uint<2>> op; // 00=add, 01=sub, 10=and, 11=or
         ch_out<ch_uint<8>> result;)

    ConditionalALU(ch::Component *parent = nullptr,
                   const std::string &name = "alu")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // ⚠️ **两处显式转换都是必需的**（**表达式形式的两个既有缺陷**，
        //    **独立**于 ADR-051 —— 它只裁决**语句块**形式 ⇒ 已登记为独立缺陷）：
        //
        // ① `multi_if` **不能**直接赋给 `ch_out`
        //    （实测：`io().x = if_then(...)` ⇒ 编译错误 `multi_if has no member
        //    named 'impl'` ⇒ 走了 bundle 的 `get_lnode` 重载）⇒ ⇒ 必须先经
        //    `operator T()` 转成 `ch_uint<8>` 再赋值；
        //
        // ② `multi_if<T>` 要求**所有分支值同类型 T** ⇒ 而 `a + b` 是 **9 位**
        //    （进位多一位）、`a - b` / `a | b` 是 **8 位** ⇒ ⇒ 推导冲突
        //    （实测：`multi_if<ch_uint<9>>` 与 `ch_uint<8>` 不匹配）⇒ ⇒ 每个
        //    分支值 MUST 显式收敛到**同一宽度**。
        io().result = ch_uint<8>(if_then(io().op == 0_d,
                                          ch_uint<8>(io().a + io().b))
                                    .elif(io().op == 1_d,
                                          ch_uint<8>(io().a - io().b))
                                    .elif(io().op == 2_d,
                                          ch_uint<8>(io().a & io().b))
                                    .else_(ch_uint<8>(io().a | io().b)));
    }
};

// ===========================================================================
// 语句块风格（`_if` + `conditional_assign`）—— 条件**真的**被求值
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
        // 优先级 = **先匹配者胜**（分支顺序即优先级）：
        //   `rst` ⇒ 清零 → `en` ⇒ 加一 → 否则**保持**。
        _if(io().rst)
            .then([&] { conditional_assign(reg_count).next() = ch_uint<8>(0_d); })
            .elif(io().en)
            .then([&] {
                conditional_assign(reg_count).next() =
                    reg_count + ch_uint<8>(1_d);
            })
            .end();

        io().count = reg_count;
    }
};

// ===========================================================================
// 嵌套语句块 —— 内层先合成一条表达式，再作为外层该分支的候选值
// ===========================================================================
class NestedConditional : public ch::Component {
public:
    __io(ch_in<ch_bool> sel, valid;
         ch_out<ch_uint<8>> out;)

    NestedConditional(ch::Component *parent = nullptr,
                      const std::string &name = "nested_cond")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<8>> reg_out(0_d);

        _if(io().valid)
            .then([&] {
                _if(io().sel)
                    .then([&] { conditional_assign(reg_out).next() = ch_uint<8>(11_d); })
                    .else_()
                    .then([&] { conditional_assign(reg_out).next() = ch_uint<8>(12_d); })
                    .end();
            })
            .else_()
            .then([&] { conditional_assign(reg_out).next() = ch_uint<8>(13_d); })
            .end();

        io().out = reg_out;
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
    std::printf("CppHDL Conditional Statement Demo\n");
    std::printf("=================================\n");

    // ---------------------------------------------------------------------
    // ① 表达式形式（`if_then`）
    // ---------------------------------------------------------------------
    {
        std::printf("[表达式形式] if_then / elif / else_\n");
        ch::ch_device<ConditionalALU> dev;
        ch::Simulator sim(dev.context());
        auto *res = dev.instance().io().result.impl();

        sim.set_input_value(dev.instance().io().a, 12u);
        sim.set_input_value(dev.instance().io().b, 10u);

        sim.set_input_value(dev.instance().io().op, 0u);
        sim.tick(1);
        check(read_port(sim, res) == 22u, "op=0 ⇒ a+b = 22");

        sim.set_input_value(dev.instance().io().op, 1u);
        sim.tick(1);
        check(read_port(sim, res) == 2u, "op=1 ⇒ a-b = 2");

        sim.set_input_value(dev.instance().io().op, 2u);
        sim.tick(1);
        check(read_port(sim, res) == 8u, "op=2 ⇒ a&b = 8");

        sim.set_input_value(dev.instance().io().op, 3u);
        sim.tick(1);
        check(read_port(sim, res) == 14u, "op=3 ⇒ a|b = 14");
    }

    // ---------------------------------------------------------------------
    // ② 语句块形式（`_if` + `conditional_assign`）—— 条件**真的**被求值
    // ---------------------------------------------------------------------
    {
        std::printf("[语句块形式] _if + conditional_assign\n");
        ch::ch_device<ConditionalCounter> dev;
        ch::Simulator sim(dev.context());
        auto *count = dev.instance().io().count.impl();

        // rst=0, en=0 ⇒ 未匹配 ⇒ **保持**（0）
        sim.set_input_value(dev.instance().io().rst, 0u);
        sim.set_input_value(dev.instance().io().en, 0u);
        sim.tick(1);
        check(read_port(sim, count) == 0u, "rst=0,en=0 ⇒ 保持 0");

        // rst=0, en=1 ⇒ 加一 ⇒ 1
        sim.set_input_value(dev.instance().io().en, 1u);
        sim.tick(1);
        check(read_port(sim, count) == 1u, "en=1 ⇒ 计数 1");

        // 再来一拍 ⇒ 2
        sim.tick(1);
        check(read_port(sim, count) == 2u, "en=1 ⇒ 计数 2");

        // rst=1 ⇒ 清零 ⇒ 0（**先匹配者胜** ⇒ rst 压过 en）
        sim.set_input_value(dev.instance().io().rst, 1u);
        sim.tick(1);
        check(read_port(sim, count) == 0u, "rst=1 ⇒ 清零 0");
    }

    // ---------------------------------------------------------------------
    // ③ 嵌套语句块
    // ---------------------------------------------------------------------
    {
        std::printf("[嵌套] 内层先合成，再作为外层候选值\n");
        ch::ch_device<NestedConditional> dev;
        ch::Simulator sim(dev.context());
        auto *out = dev.instance().io().out.impl();

        sim.set_input_value(dev.instance().io().valid, 1u);
        sim.set_input_value(dev.instance().io().sel, 1u);
        sim.tick(1);
        check(read_port(sim, out) == 11u, "valid=1,sel=1 ⇒ 11");

        sim.set_input_value(dev.instance().io().sel, 0u);
        sim.tick(1);
        check(read_port(sim, out) == 12u, "valid=1,sel=0 ⇒ 12");

        sim.set_input_value(dev.instance().io().valid, 0u);
        sim.tick(1);
        check(read_port(sim, out) == 13u, "valid=0 ⇒ 13（外层 else）");
    }

    std::printf("\n");
    if (failures != 0) {
        std::printf("[FAIL] %d 项自检未通过\n", failures);
        return 1;
    }
    std::printf("[PASS] 全部自检通过\n");
    return 0;
}
