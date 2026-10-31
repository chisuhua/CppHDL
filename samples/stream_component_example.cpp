/**
 * @file stream_component_example.cpp
 * @brief Stream FIFO 的**组件化**用法 —— 演示「FIFO 满 ⇒ 上游暂停」的反压场景
 *
 * ⚠️ **2026-10-31 重写**（change `chlib-stream-ready-ownership-2026-10`，
 *    ADR-054 **形态 E**）。此前本 sample 处于**禁用清单**
 *    （`samples/disabled_samples.txt`），归位条件 = 「`chlib::stream_fifo`
 *    读使能外部化」落地 ⇒ ⇒ 现已达成 ⇒ ⇒ 归位。
 *
 * ⚠️ **重写的原因**（旧版本同时踩了 4 个坑）：
 *    ① 旧 API `stream_fifo<T, D>(clk, rst, in)` ⇒ ⇒ 现 API 是
 *       `stream_fifo<T, D>(in)`（内部已改用 context 的默认时钟/复位）；
 *    ② `Stream<T>` ⇒ ⇒ 现类型名是 `ch_stream<T>`；
 *    ③ `context::get_default_clk()` / `get_default_rst()` ⇒ ⇒ 已改名
 *       `get_default_clock()` / `get_default_reset()`；
 *    ④ ⚠️ **旧版用 `ch_in<Stream<T>>` / `ch_out<Stream<T>>` 做组件端口**
 *       ⇒ ⇒ **违反仓库约定**（根 AGENTS.md「Bundle as IO」：bundle 成员
 *       **不会**被注册为组件 IO ⇒ ⇒ 其信号**不会**成为生成 Verilog 的
 *       **顶层端口** ⇒ ⇒ 外部 TB 无法驱动）⇒ ⇒ 改用**独立端口**
 *       （payload / valid / ready）。
 *
 * ⚠️ **本 sample 演示什么**：**「FIFO 满 ⇒ 上游暂停」**这条反压路径。
 *    ⚠️ 此前它**不可达** —— `stream_fifo` 的读使能硬编码为 `true`
 *    ⇒ ⇒ 消费端恒就绪 ⇒ ⇒ FIFO 永远在排空 ⇒ ⇒ `full` 永不置位
 *    ⇒ ⇒ 「上游暂停」分支**从不触发** ⇒ ⇒ 本 sample 的**全部价值**依赖它。
 *
 * ⚠️ **为什么必须有自检断言**：samples 护栏（`scripts/run_all_samples.sh`）
 *    **只查退出码** ⇒ ⇒ 行为退化而 `return 0` 会被判 PASS
 *    ⇒ ⇒ 这正是本仓长期「假绿」的机制 ⇒ ⇒ 断言失败 MUST `return 1`。
 */
#include "ch.hpp"
#include "chlib/stream.h"
#include "component.h"
#include "device.h"
#include "simulator.h"
#include <iostream>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {
// FIFO 深度 —— 取 4 ⇒ 4 拍写满 ⇒ 演示紧凑且可手推
constexpr unsigned kDepth = 4;
// `occupancy` 的位宽 = `compute_idx_width(DEPTH) + 1`（`chlib/stream.h`）
constexpr unsigned kOccWidth = 3;  // compute_idx_width(4) + 1 = 3
}  // namespace

// 用 chlib 的 stream_fifo 封装一个**带反压**的 FIFO
class StreamFifoComponent : public ch::Component {
public:
    // ⚠️ **TB 驱动的输入用「成员」而非端口**（与 `samples/stream_fifo_example.cpp`
    //    同一模式）⇒ ⇒ 由 `sim.set_input_value(dev.instance().<member>, v)` 驱动。
    //    ⚠️ 用 `ch_uint<1>`（而非 `ch_bool`）承载布尔量 ⇒ ⇒ 因为
    //    `Simulator::set_input_value()` **没有 `ch_bool` 重载**。
    //    ⚠️ MUST 用带初值的构造（`ch_uint<N> x = 0_d;`）—— 默认构造得到的是
    //    **空节点**（null impl）⇒ ⇒ 拿去做值运算会报
    //    "Invalid operands for binary operation"。
    ch_uint<8> in_payload = 0_d;
    ch_uint<1> in_valid = 0_d;
    // 读出侧（消费端）的就绪信号 —— **驱动 FIFO 的读使能**
    ch_uint<1> out_ready = 0_d;

    __io(
        // 写入侧就绪（FIFO 未满 ⇒ 上游可以发）⇒ ⇒ **反压的出口**
        ch_out<ch_bool> in_ready;
        // 读出侧
        ch_out<ch_uint<8>> out_payload;
        ch_out<ch_bool> out_valid;
        // 状态
        ch_out<ch_bool> empty;
        ch_out<ch_bool> full;
        ch_out<ch_uint<kOccWidth>> occupancy;
    )

    StreamFifoComponent(ch::Component *parent = nullptr,
                        const std::string &name = "stream_fifo_comp")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 把驱动信号接成一条 ch_stream
        // ⚠️ 这里 MUST 用 `=`（节点重绑定）而不是 `<<=`（数据流边）——
        //    chlib 的 `stream_fifo` 内部要对 `input_stream.valid` 做**值**运算
        //    ⇒ ⇒ `<<=` 建立的是 proxy 节点（不是值节点）⇒ ⇒ 用 `<<=`
        //    会得到 "Invalid operands for binary operation"。
        ch_stream<ch_uint<8>> input_stream;
        input_stream.payload = in_payload;
        input_stream.valid = in_valid;   // ch_uint<1> ⇒ ch_bool（隐式转换）
        // ⚠️ **故意不驱动** `input_stream.ready` ⇒ ⇒ FIFO MUST NOT 依赖它
        //    （⚠️ 写使能 MUST 只用 `valid` —— 否则 `valid & ready` 是
        //    **先读后写** ⇒ ⇒ `ready` 是空 proxy ⇒ ⇒ node_builder 报
        //    "Invalid operands for binary operation" ⇒ ⇒ 组合子完全不可用）

        auto fifo_result = stream_fifo<ch_uint<8>, kDepth>(input_stream);

        // ⚠️⚠️ **ADR-054 形态 E（默认值 + 可覆盖）**：把消费端的 ready
        //    **覆盖**到 FIFO 的读使能字段 ⇒ ⇒ 消费端不就绪 ⇒ ⇒ FIFO
        //    **不弹出** ⇒ ⇒ **反压真正生效**（此前读使能硬编码为 `true`
        //    ⇒ ⇒ 恒就绪 ⇒ ⇒ 永远排空 ⇒ ⇒ `full` 不可达）。
        //    ⚠️ 形参是**模板** + `to_operand` 归一化 ⇒ ⇒ 端口 / 硬件类型 /
        //    表达式**都**可用（ADR-053 / E3 的同型教训）。
        stream_fifo_connect_ready(fifo_result, out_ready);

        // ⚠️ **上游侧的反压出口**：FIFO 未满 ⇒ 上游可以发
        //    （`push_stream.ready` = `!full`）⇒ ⇒ 满了 TB 就 MUST 暂停发送。
        io().in_ready <<= fifo_result.push_stream.ready;

        // 读出侧
        io().out_payload <<= fifo_result.pop_stream.payload;
        io().out_valid <<= fifo_result.pop_stream.valid;

        // 状态
        io().empty <<= fifo_result.empty;
        io().full <<= fifo_result.full;
        io().occupancy <<= fifo_result.occupancy;
    }
};

int main() {
    std::cout << "CppHDL Stream Component Example (with backpressure)" << std::endl;
    std::cout << "======================================================" << std::endl;

    ch::ch_device<StreamFifoComponent> dev;
    ch::Simulator sim(dev.context());
    auto &io = dev.instance().io();

    auto in_ready = [&] { return static_cast<uint64_t>(sim.get_port_value(io.in_ready)); };
    auto full = [&] { return static_cast<uint64_t>(sim.get_port_value(io.full)); };
    auto empty = [&] { return static_cast<uint64_t>(sim.get_port_value(io.empty)); };
    auto occ = [&] { return static_cast<uint64_t>(sim.get_port_value(io.occupancy)); };

    sim.set_input_value(dev.instance().in_payload, 0);
    sim.set_input_value(dev.instance().in_valid, 0);
    sim.set_input_value(dev.instance().out_ready, 0);

    // ⚠️⚠️ **必须先 tick 一次**才能读到组合输出的值 ——
    //    `Simulator::get_port_value()` 读的是 `data_map_` 里的**已求值**值
    //    ⇒ ⇒ 未经任何求值就读 ⇒ ⇒ 拿到**初值 0**
    //    ⇒ ⇒ ⚠️ 参照 sample（`stream_fifo_example.cpp`）在 tick **之前**打印
    //    「初始状态」⇒ ⇒ 它打印的 `empty` 其实是**错的**（count==0 ⇒ empty==1）
    //    ⇒ ⇒ 本 sample MUST 先 tick 再读 ⇒ ⇒ 且下面**断言**它。
    sim.tick();

    std::cout << "\nInitial State:" << std::endl;
    std::cout << "FIFO empty: " << empty() << ", FIFO full: " << full()
              << ", Occupancy: " << occ() << std::endl;

    // ⚠️ **自检 0**：初始状态 —— 空（FIFO 刚复位）· 不满 · **上游可发**
    if (empty() != 1 || full() != 0 || occ() != 0 || in_ready() != 1) {
        std::cerr << "ERROR: initial state MUST be empty=1 full=0 occupancy=0 "
                     "in_ready=1 (got empty="
                  << empty() << ", full=" << full() << ", occupancy=" << occ()
                  << ", in_ready=" << in_ready() << ")" << std::endl;
        return 1;
    }

    // ────────────────────────────────────────────────────────────────
    // 阶段 1：消费端**不就绪** ⇒ ⇒ FIFO 只进不出 ⇒ ⇒ 写满 ⇒ `full == 1`
    //         ⇒ ⇒ `in_ready == 0` ⇒ ⇒ **上游被要求暂停**
    // ────────────────────────────────────────────────────────────────
    std::cout << "\nStage 1: consumer NOT ready ⇒ FIFO fills up ⇒ upstream stalls\n";
    sim.set_input_value(dev.instance().out_ready, 0);
    for (unsigned i = 0; i < kDepth; ++i) {
        // ⚠️ 上游**只在** `in_ready` 时发送（协议）
        if (in_ready() != 1) {
            std::cerr << "ERROR: in_ready MUST be 1 before writing (i=" << i
                      << ", in_ready=" << in_ready() << ")" << std::endl;
            return 1;
        }
        sim.set_input_value(dev.instance().in_payload, 0x10 + i);
        sim.set_input_value(dev.instance().in_valid, 1);
        sim.tick();
        std::cout << "  wrote 0x" << std::hex << (0x10 + i) << std::dec
                  << " ⇒ occupancy=" << occ() << ", full=" << full() << std::endl;
    }

    // ⚠️ **自检 1**：写满 ⇒ `full == 1` ⇒ 上游**被要求暂停**
    //    （⚠️ 这是本 sample 的**核心断言** ⇒ ⇒ 此前该分支**从不触发**）
    if (full() != 1) {
        std::cerr << "ERROR: FIFO MUST be full after writing " << kDepth
                  << " items (full=" << full() << ")" << std::endl;
        return 1;
    }
    if (in_ready() != 0) {
        std::cerr << "ERROR: in_ready MUST be 0 when FIFO is full (in_ready="
                  << in_ready() << ")" << std::endl;
        return 1;
    }
    if (occ() != kDepth) {
        std::cerr << "ERROR: occupancy MUST be " << kDepth << " (got " << occ()
                  << ")" << std::endl;
        return 1;
    }

    // ⚠️ **自检 2**：上游**暂停**（valid 拉低）⇒ ⇒ 占用计数**不再增长**
    sim.set_input_value(dev.instance().in_valid, 0);
    sim.tick();
    if (occ() != kDepth) {
        std::cerr << "ERROR: occupancy MUST stay " << kDepth
                  << " while upstream is stalled (got " << occ() << ")" << std::endl;
        return 1;
    }

    // ────────────────────────────────────────────────────────────────
    // 阶段 2：消费端**就绪** ⇒ ⇒ FIFO 排空 ⇒ ⇒ `full` 清除 ⇒ 上游恢复
    // ────────────────────────────────────────────────────────────────
    std::cout << "\nStage 2: consumer ready ⇒ FIFO drains ⇒ upstream resumes\n";
    sim.set_input_value(dev.instance().out_ready, 1);
    sim.set_input_value(dev.instance().in_valid, 0);
    sim.tick();
    sim.tick();
    // ⚠️ **自检 3**：排空 2 拍 ⇒ 占用 4 → 2 ⇒ ⇒ `full` 清除 ⇒ `in_ready` 恢复
    if (occ() != kDepth - 2) {
        std::cerr << "ERROR: occupancy MUST be " << (kDepth - 2)
                  << " after draining 2 cycles (got " << occ() << ")" << std::endl;
        return 1;
    }
    if (full() != 0) {
        std::cerr << "ERROR: full MUST be 0 after draining (full=" << full()
                  << ")" << std::endl;
        return 1;
    }
    if (in_ready() != 1) {
        std::cerr << "ERROR: in_ready MUST be 1 again after draining (in_ready="
                  << in_ready() << ")" << std::endl;
        return 1;
    }
    std::cout << "  drained 2 ⇒ occupancy=" << occ() << ", full=" << full()
              << ", in_ready=" << in_ready() << std::endl;

    std::cout << "\nStream component example with backpressure completed successfully!"
              << std::endl;
    return 0;
}
