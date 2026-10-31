/**
 * @file stream_handshake_example.cpp
 * @brief Stream 握手示例 —— FIFO + 「内部电路忙」⇒ ⇒ 演示**双向**反压
 *
 * ⚠️ **2026-10-31 重写**（change `chlib-stream-ready-ownership-2026-10`，
 *    ADR-054 **形态 E**）。此前本 sample 处于**禁用清单**
 *    （`samples/disabled_samples.txt`），归位条件 = 「`chlib::stream_fifo`
 *    读使能外部化」落地 ⇒ ⇒ 现已达成 ⇒ ⇒ 归位。
 *
 * ⚠️ **重写的原因**（旧版本同时踩了 5 个坑）：
 *    ① 旧 API `stream_fifo<T, D>(clk, rst, in)` ⇒ ⇒ 现 API 是
 *       `stream_fifo<T, D>(in)`；
 *    ② `Stream<T>` ⇒ ⇒ 现类型名是 `ch_stream<T>`；
 *    ③ `context::get_default_clk()` / `get_default_rst()` ⇒ ⇒ 已改名；
 *    ④ `ch_reg` 的 `->clk` / `->rst` 成员**已移除** ⇒ ⇒ `ch_reg` 用
 *       **隐式时钟** ⇒ ⇒ 只需 `->next`；
 *    ⑤ ⚠️ **旧版用 `ch_in<Stream<T>>` / `ch_out<Stream<T>>` 做组件端口**
 *       ⇒ ⇒ **违反仓库约定**（根 AGENTS.md「Bundle as IO」）⇒ ⇒ 改用
 *       **独立端口**。
 *
 * ⚠️ **本 sample 的核心教学点**（也是最容易踩的坑）：
 *    组件内部有「忙」信号时 ⇒ ⇒ **FIFO 的读使能 MUST 用「有效消费端
 *    ready」= `out_ready & !busy`** ⇒ ⇒ 否则 FIFO 在 `out_valid == 0`
 *    的那些拍**照样弹出** ⇒ ⇒ **数据被丢掉且无任何报错** ⇒ ⇒ **静默失效**。
 *    ⇒ ⇒ 「消费端 ready」不是一个布尔量，而是「消费端**这一拍**能收」的判定。
 */
#include "ch.hpp"
#include "chlib/stream.h"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {
// FIFO 深度
constexpr unsigned kDepth = 8;
// 忙周期计数器的位宽（`ch_uint<3>` ⇒ 0..7 循环 ⇒ 每 8 拍忙 4 拍）
constexpr unsigned kBusyWidth = 3;
// `occupancy` 的位宽 = `compute_idx_width(DEPTH) + 1`（`chlib/stream.h`）
constexpr unsigned kOccWidth = 4;  // compute_idx_width(8) + 1 = 4
// 推入的数据个数
constexpr unsigned kPushCount = 8;
}  // namespace

class StreamHandshakeComponent : public ch::Component {
public:
    // ⚠️ **TB 驱动的输入用「成员」**（与 `samples/stream_fifo_example.cpp`
    //    同一模式）⇒ ⇒ 由 `sim.set_input_value(dev.instance().<member>, v)` 驱动。
    //    ⚠️ 用 `ch_uint<1>` 承载布尔量 ⇒ ⇒ `set_input_value()` 没有
    //    `ch_bool` 重载。
    ch_uint<8> in_payload = 0_d;
    ch_uint<1> in_valid = 0_d;
    // 消费端就绪（下游 TB 驱动）
    ch_uint<1> out_ready = 0_d;

    __io(
        // 上游侧就绪（FIFO 未满 ⇒ 上游可以发）⇒ ⇒ **反向**反压
        ch_out<ch_bool> in_ready;
        // 下游侧
        ch_out<ch_uint<8>> out_payload;
        ch_out<ch_bool> out_valid;
        // 内部忙标志（诊断用）
        ch_out<ch_bool> busy;
        ch_out<ch_uint<kOccWidth>> occupancy;
    )

    StreamHandshakeComponent(ch::Component *parent = nullptr,
                              const std::string &name = "stream_handshake_comp")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = in_payload;
        input_stream.valid = in_valid;   // ch_uint<1> ⇒ ch_bool（隐式转换）
        // ⚠️ **故意不驱动** `input_stream.ready` ⇒ ⇒ FIFO MUST NOT 依赖它

        auto fifo = stream_fifo<ch_uint<8>, kDepth>(input_stream);

        // 内部「忙」信号：每 8 拍忙 4 拍
        // ⚠️ `ch_reg` 用**隐式时钟**（`->clk` / `->rst` 成员已移除）
        ch_reg<ch_uint<kBusyWidth>> busy_counter(0_d);
        busy_counter->next = busy_counter + 1_d;
        ch_bool internal_busy = (busy_counter < make_literal<4>());

        // ⚠️⚠️⚠️ **本 sample 的核心**：FIFO 的读使能 MUST 用
        //    **「有效消费端 ready」** = 消费端 ready **AND** 非忙
        //    ⇒ ⇒ 否则 FIFO 在 `out_valid == 0` 的那些拍**照样弹出**
        //    ⇒ ⇒ ⇒ **数据被丢掉且无任何报错** ⇒ ⇒ **静默失效**
        //    ⇒ ⇒ 「消费端 ready」不是布尔量，而是「消费端**这一拍**能收」。
        ch_bool effective_ready = ch_bool(out_ready.impl()) & !internal_busy;
        stream_fifo_connect_ready(fifo, effective_ready);

        // 下游侧：忙时**拉低 valid**（数据保留在 FIFO 里）
        io().out_payload <<= fifo.pop_stream.payload;
        io().out_valid <<= (fifo.pop_stream.valid & !internal_busy);

        // 上游侧：FIFO 未满 ⇒ 上游可以发（**反向**反压）
        io().in_ready <<= fifo.push_stream.ready;

        // 诊断
        io().busy <<= internal_busy;
        io().occupancy <<= fifo.occupancy;
    }
};

int main() {
    std::cout << "CppHDL Stream Handshake Example with Backpressure" << std::endl;
    std::cout << "=================================================" << std::endl;

    ch::ch_device<StreamHandshakeComponent> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    auto in_ready = [&] { return static_cast<uint64_t>(sim.get_port_value(io.in_ready)); };
    auto out_valid = [&] { return static_cast<uint64_t>(sim.get_port_value(io.out_valid)); };
    auto out_payload = [&] { return static_cast<uint64_t>(sim.get_port_value(io.out_payload)); };
    auto busy = [&] { return static_cast<uint64_t>(sim.get_port_value(io.busy)); };
    auto occ = [&] { return static_cast<uint64_t>(sim.get_port_value(io.occupancy)); };

    sim.set_input_value(dev.instance().in_payload, 0);
    sim.set_input_value(dev.instance().in_valid, 0);
    sim.set_input_value(dev.instance().out_ready, 1);
    sim.tick();   // ⚠️ 必须先 tick 一次才能读到组合输出的值

    uint64_t expected = 0x10;   // 期望被弹出的下一个值
    uint64_t pushed = 0;        // 已推入的个数
    uint64_t popped = 0;        // 已弹出的个数

    for (int cycle = 0; cycle < 40 && popped < kPushCount; ++cycle) {
        // ⚠️ **在 tick 之前采样** ⇒ ⇒ 拿到的是「**正在被弹出**」的那个元素
        //    （FIFO 协议：`aread` 是 0-cycle ⇒ 沿**之后** payload 指向
        //    **下一个**队首 ⇒ ⇒ 消费者 MUST 在沿**之前**采样）
        auto v = out_valid();
        auto p = out_payload();
        auto b = busy();

        // ⚠️ **自检 1**：忙 ⇒ ⇒ `out_valid` MUST 为 0（数据保留在 FIFO 里）
        if (b == 1 && v != 0) {
            std::cerr << "ERROR: out_valid MUST be 0 while busy (cycle=" << cycle
                      << ", busy=1, out_valid=" << v << ")" << std::endl;
            return 1;
        }

        // ⚠️ **自检 2**：本拍弹出 ⇒ ⇒ payload MUST 是**期望的下一个值**
        //    （⇒ ⇒ 无丢失 · 无重复 · 无乱序）
        if (v == 1) {
            if (p != expected) {
                std::cerr << "ERROR: popped payload MUST be 0x" << std::hex << expected
                          << std::dec << " (got 0x" << std::hex << p << std::dec
                          << ", cycle=" << cycle << ")" << std::endl;
                return 1;
            }
            ++expected;
            ++popped;
        }

        // ⚠️ **自检 3**：占用计数 MUST NOT 超过深度（自保护）
        if (occ() > kDepth) {
            std::cerr << "ERROR: occupancy MUST NOT exceed " << kDepth
                      << " (got " << occ() << ")" << std::endl;
            return 1;
        }

        // 上游：仅在 `in_ready` 时推入（协议）
        if (in_ready() == 1 && pushed < kPushCount) {
            sim.set_input_value(dev.instance().in_payload, 0x10 + pushed);
            sim.set_input_value(dev.instance().in_valid, 1);
            ++pushed;
        } else {
            sim.set_input_value(dev.instance().in_valid, 0);
        }

        sim.tick();
    }

    // ⚠️ **自检 4**：推入的 MUST 全部弹出（**无丢失**）⇒ ⇒ 这正是
    //    「FIFO 的读使能 MUST 用有效消费端 ready」这条纪律的**可观测证据**
    if (popped != kPushCount) {
        std::cerr << "ERROR: all " << kPushCount
                  << " pushed items MUST be popped (popped=" << popped << ")"
                  << std::endl;
        return 1;
    }

    std::cout << "\nPushed " << pushed << " items, popped " << popped
              << " items (no loss, in order)" << std::endl;
    std::cout << "Stream handshake example with backpressure completed successfully!"
              << std::endl;
    return 0;
}
