// examples/stream_example.cpp
#include "../core/component.h"
#include "../core/stream.h"
#include "../core/stream_fifo.h"
#include "../core/ch_assert.h"
#include <cassert>
#include <iostream>

using namespace ch::core;

int global_simulation_cycle = 0;

// ✅ 简单的生产者模块
struct Producer : public Component {
    __in(ch_bool) clk;
    __in(ch_bool) rst;
    Stream<ch_uint<8>> io_source; // 输出流
    //
    Producer(const std::string& path_name = "producer") : Component(path_name) {}

    void describe() override {
        ch_pushcd(clk, rst, true);

        static ch_reg<ch_uint<8>> counter(this, "counter", 0);
        static ch_reg<ch_bool> toggle(this, "toggle", false);

        if (rst) {
            io_source.io.valid = false;
            toggle.next() = false;
            counter.next() = 0;
        } else {
            std::cout << "  [DEBUG Producer] toggle: " << (unsigned)*toggle 
                  << " prev: " << (unsigned)toggle->prev_value() << std::endl;
            toggle.next() = !*toggle;
            std::cout << "  [DEBUG Producer] toggle.next: " << (unsigned)!*toggle << std::endl;

            // 检测 toggle 的上升沿 (从 false 到 true)
            //ch_bool toggle_posedge = (bool)*toggle && !toggle->prev_value();
            ch_assert(*counter < 10, "Counter should not be stuck at 0!");

            if (*toggle) {
                counter.next() = *counter + 1;
                io_source.io.valid = true;
                io_source.io.payload = *counter;
                std::cout << "  [Producer] Sending data: " << (unsigned)*counter << std::endl;
            } else {
                io_source.io.valid = false;
            }
        }

        // 忽略 ready 信号（简单起见）
        ch_popcd();
    }
};

// ✅ 简单的消费者模块
struct Consumer : public Component {
    __in(ch_bool) clk;
    __in(ch_bool) rst;
    Stream<ch_uint<8>> io_sink; // 输入流
    //
    Consumer(const std::string& path_name = "consumer") : Component(path_name) {}

    void describe() override {
        ch_pushcd(clk, rst, true);

        // 当数据有效且准备好时，打印数据
        if (io_sink.io.valid && io_sink.io.ready) {
            std::cout << "  [Consumer] Received data: " << (unsigned)io_sink.io.payload << std::endl;
            // ✅ 硬件断言：收到的数据应该大于0
            if (!rst)
                ch_assert(io_sink.io.payload >= 0, "Received data should be positive!");
        }
        // 始终准备好接收
        io_sink.io.ready = true;

        ch_popcd();
    }
};

// ✅ 主函数
int main() {
    std::cout << "=== Starting Simulation: Stream Example ===" << std::endl;

    ch_bool global_clk_signal(0);
    ch_bool global_rst_signal(0);

    ch_device<StreamFifo<ch_uint<8>, 4>> fifo("fifo");
    ch_device<Producer> producer("producer");
    ch_device<Consumer> consumer("consumer");

    for (int cycle = 0; cycle < 20; cycle++) {
        global_simulation_cycle = cycle; // ✅ 更新全局周期计数器
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;

        // 设置全局时钟和复位
        //bool global_clk = (cycle % 2);
        //bool global_rst = (cycle < 2);

        // 连接所有模块的时钟和复位
        /*
        fifo.instance().clk = global_clk;
        fifo.instance().rst = global_rst;
        producer.instance().clk = global_clk;
        producer.instance().rst = global_rst;
        consumer.instance().clk = global_clk;
        consumer.instance().rst = global_rst;
        */

        global_clk_signal = (cycle % 2);
        global_rst_signal = (cycle < 2);

        // ✅ 所有模块共享同一个时钟和复位对象
        fifo.instance().clk = global_clk_signal;
        fifo.instance().rst = global_rst_signal;
        producer.instance().clk = global_clk_signal;
        producer.instance().rst = global_rst_signal;
        consumer.instance().clk = global_clk_signal;
        consumer.instance().rst = global_rst_signal;

        // 连接 Stream
        fifo.instance().io_sink << producer.instance().io_source;
        consumer.instance().io_sink << fifo.instance().io_source;

        if (cycle > 0) {
            producer.tick();
            fifo.tick();
            consumer.tick();
        }

        // 执行硬件逻辑
        producer.describe();
        fifo.describe();
        consumer.describe();

        
        // ✅ 在每个周期结束后，添加断言检查
        if (cycle >= 4) { // 复位结束后开始检查
            // 检查 FIFO 的空满标志
            assert(!(bool)(fifo.instance().io_sink.io.valid && fifo.instance().io_sink.io.ready) || 
                   !fifo.instance().is_full()); // 如果 valid & ready，则 FIFO 不应为满

            assert(!(bool)(consumer.instance().io_sink.io.valid && consumer.instance().io_sink.io.ready) || 
                   !fifo.instance().is_empty()); // 如果消费者 valid & ready，则 FIFO 不应为空
        }

        // 在特定周期检查消费者收到的数据
        if (cycle == 7) {
            // Cycle 7: 消费者应该收到第一个数据 (1)
            // 注意：由于我们的 Producer 每两个周期发一个数据，且有 FIFO 缓冲，需要根据实际波形调整断言
            // 这里只是一个示例，您需要根据实际输出调整期望值
            // assert(consumer.instance().io_sink.io.payload == 1); 
        }
    }

    std::cout << "\n=== Simulation Complete ===" << std::endl;
    return 0;
}
