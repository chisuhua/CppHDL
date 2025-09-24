// core/stream_fifo.h
#pragma once

#include "component.h"
#include "stream.h" // ✅ 包含 Stream 定义
#include "sync_fifo.h" // ✅ 包含 Stream 定义

namespace ch {
namespace core {

// ✅ 基于 SyncFifo 的 Stream FIFO
template<typename T, int DEPTH>
class StreamFifo : public Component {
public:
    __in(ch_bool) clk;
    __in(ch_bool) rst;
    Stream<T> io_sink;   // 生产者 -> FIFO (输入)
    Stream<T> io_source; // FIFO -> 消费者 (输出)

private:
    SyncFifo<T, DEPTH> fifo_impl_;

public:
    StreamFifo(const std::string& path_name = "fifo") 
        : Component(path_name), 
          fifo_impl_(this, path_name + ".fifo_impl_") {}

    bool is_full() const {
        return fifo_impl_.io.full;
    }

    bool is_empty() const {
        return fifo_impl_.io.empty;
    }

    void describe() override {
        ch_pushcd(clk, rst, true);

        std::cout << "  [DEBUG StreamFifo]@" << global_simulation_cycle 
            << " rst_ptr_  " << (void*)&io.rst << std::endl;

        fifo_impl_.io.clk = clk;
        fifo_impl_.io.rst = rst;

        // 连接写端口 (Sink)
        fifo_impl_.io.write_en = io_sink.io.valid;
        fifo_impl_.io.write_data = io_sink.io.payload;
        // 连接读端口 (Source)
        fifo_impl_.io.read_en = io_source.io.ready;

        fifo_impl_.describe();

        io_sink.io.ready = !fifo_impl_.io.full;
        io_source.io.valid = !fifo_impl_.io.empty;
        io_source.io.payload = fifo_impl_.io.read_data;


        if (io_sink.io.valid && !fifo_impl_.io.full) {
            std::cout << "  [StreamFifo] Writing data: " << (unsigned)io_sink.io.payload << std::endl;
        }

        if (io_source.io.valid && io_source.io.ready) {
            std::cout << "  [StreamFifo] Reading data: " << (unsigned)io_source.io.payload << std::endl;
        }
        std::cout << "  [DEBUG] SyncFifo has " << fifo_impl_.get_all_regs().size() << " regs." << std::endl;
        std::cout << "  [DEBUG] StreamFifo has " << this->get_all_regs().size() << " regs." << std::endl;
        ch_popcd();
    }
};

} // namespace core
} // namespace ch
