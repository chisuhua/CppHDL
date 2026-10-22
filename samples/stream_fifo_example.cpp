/**
 * @file stream_fifo_example.cpp
 * @brief Stream FIFO 演示 —— chlib 的 `stream_fifo`
 *
 * ⚠️ **关于「反压」的重要限制**（2026-10-22 实测）：
 * `chlib::stream_fifo()` 内部的**读使能是硬编码的 `pop_ready = true`**
 * （见 `include/chlib/stream.h`）⇒ 消费端恒就绪 ⇒ FIFO 永远在排空
 * ⇒ **`full` / `in_ready` 这条反压路径在当前 API 下不可达**。
 * ⇒ 本 sample 因此演示的是：FIFO 的**占用计数 / empty 状态**与
 *    **ready-valid 握手**；「FIFO 满 ⇒ 上游暂停」这一场景需要
 *    `stream_fifo` 的读使能外部化（**公开 API 变更 ⇒ 需 ADR**），
 *    已登记为独立 follow-up。
 *
 * ⚠️ API 漂移记录（2026-10-22）：
 * ① 旧 API 是 `stream_fifo<T, DEPTH>(clk, rst, input_stream)`；
 *    当前 API 是 `stream_fifo<T, DEPTH>(input_stream)` —— FIFO 内部已改用
 *    context 的**默认时钟/复位**。
 * ② 旧写法每步**重建** FIFO（因为旧 API 需要 clk/rst）；
 *    当前 API 只需**构建一次**，之后由 TB 驱动端口。
 * ③ 旧写法用 `ch_in<Stream<T>>` 作为组件端口；按仓库约定（见根 AGENTS.md
 *    的「Bundle as IO」），bundle 成员**不会**被注册为组件 IO ⇒
 *    组件端口改用**独立端口**（payload / valid / ready），stream 接口在
 *    `describe()` 内部由这些端口搭出来。
 *
 * ⚠️ **为什么 TB 侧的驱动信号用 `ch_uint` 而不是 `ch_in` 端口**：
 * `Simulator::set_input_value()` 只有 `ch_in<T>` 与 `ch_uint<N>` 两个重载，
 * **没有 `ch_bool`** ⇒ valid 用 `ch_uint<1>` 承载，再靠 `ch_uint::operator
 * ch_bool()` 隐式转换接进 `ch_stream`。
 */
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "simulator.h"
#include "chlib/stream.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// 用 chlib 的 stream_fifo 封装一个带反压的 FIFO
class StreamFifoComponent : public ch::Component {
public:
    // 写入侧（上游 → FIFO）—— 由 TB 通过 set_input_value() 驱动
    // ⚠️ MUST 用带初值的构造（`ch_uint<N> x(0_d)`）—— 默认构造得到的是
    //    **空节点**（null impl）⇒ 拿去做 `&` 运算会报
    //    "Invalid operands for binary operation"。
    ch_uint<8> in_payload = 0_d;
    ch_uint<1> in_valid = 0_d;

    __io(
        ch_out<ch_bool> in_ready;    // FIFO 未满 ⇒ 可以收
        ch_out<ch_uint<8>> out_payload;
        ch_out<ch_bool> out_valid;
        ch_out<ch_bool> empty;
        ch_out<ch_bool> full;
        ch_out<ch_uint<6>> occupancy;  // log2(32)+1 = 6 位
    )

    StreamFifoComponent(ch::Component *parent = nullptr,
                         const std::string &name = "stream_fifo_comp")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 把驱动信号接成一条 ch_stream
        // ⚠️ 这里 MUST 用 `=`（节点重绑定）而不是 `<<=`（数据流边）——
        //    chlib 的 `stream_fifo` 内部要对 `input_stream.valid` 做**值**运算
        //    （`valid & ready`），而 `<<=` 建立的是 proxy 节点（不是值节点）
        //    ⇒ 用 `<<=` 会得到 "Invalid operands for binary operation"。
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = in_payload;
        input_stream.valid = in_valid;   // ch_uint<1> ⇒ ch_bool（隐式转换）
        input_stream.ready = ch_bool(true);

        auto fifo_result = stream_fifo<ch_uint<8>, 32>(input_stream);

        // 反压：FIFO 满时不再接收
        io().in_ready = fifo_result.push_stream.ready;

        // 读出侧
        io().out_payload = fifo_result.pop_stream.payload;
        io().out_valid = fifo_result.pop_stream.valid;

        // 状态
        io().empty = fifo_result.empty;
        io().full = fifo_result.full;
        io().occupancy = fifo_result.occupancy;
    }
};

int main() {
    std::cout << "CppHDL Stream FIFO Example" << std::endl;
    std::cout << "=================================" << std::endl;

    ch::ch_device<StreamFifoComponent> dev;
    ch::Simulator sim(dev.context());

    auto &io = dev.instance().io();

    // 初始化输入
    sim.set_input_value(dev.instance().in_payload, 0);
    sim.set_input_value(dev.instance().in_valid, 0);

    std::cout << "\nInitial State:" << std::endl;
    std::cout << "FIFO empty: " << static_cast<uint64_t>(sim.get_port_value(io.empty))
              << ", FIFO full: " << static_cast<uint64_t>(sim.get_port_value(io.full))
              << ", Occupancy: " << static_cast<uint64_t>(sim.get_port_value(io.occupancy))
              << std::endl;

    std::cout << "\nSimulating Stream FIFO Operations with Backpressure:" << std::endl;

    // 模拟多个时钟周期，演示反压机制
    for (int cycle = 0; cycle < 10; cycle++) {
        std::cout << "\nCycle " << cycle << ":" << std::endl;

        // 检查 FIFO 是否满 / 接近满 —— 满了就暂停发送（反压）
        // ⚠️ 当前 `stream_fifo` 的读使能恒为 true ⇒ FIFO 一直在排空 ⇒
        //    `full` 实际上不可达 ⇒ 这个分支当前不会触发（见文件头说明）。
        bool fifo_full = static_cast<uint64_t>(sim.get_port_value(io.full)) != 0;
        auto occ = static_cast<uint64_t>(sim.get_port_value(io.occupancy));
        bool fifo_almost_full = occ >= 30;

        // 更新输入数据
        sim.set_input_value(dev.instance().in_payload, 0x10 + cycle);

        if (fifo_full || fifo_almost_full) {
            sim.set_input_value(dev.instance().in_valid, 0);
            std::cout << "FIFO is full/almost full, simulator pauses sending (backpressure)"
                      << std::endl;
        } else {
            sim.set_input_value(dev.instance().in_valid, 1);
            std::cout << "FIFO has space, simulator sends data" << std::endl;
        }

        std::cout << "Input ready (from FIFO): "
                  << static_cast<uint64_t>(sim.get_port_value(io.in_ready))
                  << ", FIFO empty: " << static_cast<uint64_t>(sim.get_port_value(io.empty))
                  << ", FIFO full: " << static_cast<uint64_t>(sim.get_port_value(io.full))
                  << ", Occupancy: " << occ << std::endl;
        std::cout << "Output valid: "
                  << static_cast<uint64_t>(sim.get_port_value(io.out_valid))
                  << ", Output payload: 0x" << std::hex
                  << static_cast<uint64_t>(sim.get_port_value(io.out_payload))
                  << std::dec << std::endl;

        // tick 函数会自动驱动默认时钟
        sim.tick();
    }

    // 自检：写入了数据 ⇒ FIFO 不应是空的（占用计数应 >= 1）
    auto occ = static_cast<uint64_t>(sim.get_port_value(io.occupancy));
    if (static_cast<uint64_t>(sim.get_port_value(io.empty)) != 0 || occ < 1) {
        std::cerr << "ERROR: FIFO must not be empty after writing data (occupancy="
                  << occ << ")" << std::endl;
        return 1;
    }

    std::cout << "\nStream FIFO example with backpressure completed successfully!"
              << std::endl;

    return 0;
}
