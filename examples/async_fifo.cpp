// examples/async_fifo.cpp
#include "../core/min_cash.h"
#include "../core/component.h"
#include "../core/cdc.h" // ✅ 包含新的同步器
#include <iostream>

using namespace ch::core;

// ✅ 异步 FIFO 模板
template<typename T, int DEPTH>
class AsyncFifo : public Component {
private:
    static constexpr int ADDR_WIDTH = (DEPTH > 1) ? (32 - __builtin_clz(DEPTH - 1)) : 1;
    static constexpr int PTR_WIDTH = ADDR_WIDTH + 1; // 用于区分空满
    static constexpr int GRAY_WIDTH = PTR_WIDTH;     // 格雷码宽度与指针宽度相同

public:
    __io(
        // 写时钟域
        __in(ch_bool) clk_wr;
        __in(ch_bool) rst_wr;
        __in(ch_bool) write_en;
        __in(T) write_data;
        __out(ch_bool) full;
        // 读时钟域
        __in(ch_bool) clk_rd;
        __in(ch_bool) rst_rd;
        __in(ch_bool) read_en;
        __out(T) read_data;
        __out(ch_bool) empty;
    );

    void describe() override {
        // ✅ 写时钟域
        ch_pushcd(io.clk_wr, io.rst_wr, true);
        static ch_reg<ch_uint<PTR_WIDTH>> wptr(this, "wtpr", 0);
        static ch_mem<T, DEPTH> memory(this);

        // 将写指针转换为格雷码，准备同步到读时钟域
        ch_uint<GRAY_WIDTH> wptr_gray = bin_to_gray(*wptr);

        if (io.write_en && !io.full) {
            memory[static_cast<ch_uint<ADDR_WIDTH>>(*wptr)] = io.write_data;
            wptr.next() = *wptr + 1;
        }
        ch_popcd();

        // ✅ 读时钟域
        ch_pushcd(io.clk_rd, io.rst_rd, true);
        static ch_reg<ch_uint<PTR_WIDTH>> rptr(this, "rptr", 0);

        // 将读指针转换为格雷码
        ch_uint<GRAY_WIDTH> rptr_gray = bin_to_gray(*rptr);

        // 实例化同步器，同步写指针（从写时钟域到读时钟域）
        static Synchronizer<ch_uint<GRAY_WIDTH>> sync_wptr_to_rd(this);
        sync_wptr_to_rd.io.d = wptr_gray; // 连接输入
        ch_uint<GRAY_WIDTH> wptr_gray_sync = sync_wptr_to_rd.io.q; // 获取同步后的输出

        // 读逻辑
        if (io.read_en && !io.empty) {
            rptr.next() = *rptr + 1;
        }
        io.read_data = *memory[static_cast<ch_uint<ADDR_WIDTH>>(*rptr)];
        ch_popcd();
        // ✅ 读时钟域
        ch_pushcd(io.clk_rd, io.rst_rd, true);
        static Synchronizer<ch_uint<GRAY_WIDTH>> sync_rptr_to_wr(this);
        sync_rptr_to_wr.io.d = rptr_gray;
        ch_uint<GRAY_WIDTH> rptr_gray_sync = sync_rptr_to_wr.io.q;

        // 将同步后的格雷码转换回二进制
        ch_uint<PTR_WIDTH> rptr_sync_bin = gray_to_bin(rptr_gray_sync);
        ch_bool is_full = (*wptr >> ADDR_WIDTH) != (rptr_sync_bin >> ADDR_WIDTH) &&
                          (*wptr & ((1 << ADDR_WIDTH) - 1)) == (rptr_sync_bin & ((1 << ADDR_WIDTH) - 1));
        io.full = is_full;
        ch_popcd();

        // ✅ 在读时钟域计算 empty 标志
        ch_pushcd(io.clk_rd, io.rst_rd, true);
        ch_uint<PTR_WIDTH> wptr_sync_bin = gray_to_bin(wptr_gray_sync);
        ch_bool is_empty = wptr_sync_bin == *rptr;
        io.empty = is_empty;
        ch_popcd();
    }
};

// ✅ 测试主函数
int main() {
    std::cout << "=== Starting Simulation: Async FIFO ===" << std::endl;

    ch_device<AsyncFifo<ch_uint<8>, 4>> device;

    for (int cycle = 0; cycle < 30; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;

        // 生成两个不同相位的时钟
        device.instance().io.clk_wr = (cycle % 2);
        device.instance().io.clk_rd = (cycle % 3); // 不同时钟域

        device.instance().io.rst_wr = (cycle < 2);
        device.instance().io.rst_rd = (cycle < 3);

        // 在写时钟域的上升沿写入数据
        if (cycle >= 2 && cycle <= 8 && (cycle % 2) == 1) {
            device.instance().io.write_en = true;
            device.instance().io.write_data = (cycle + 1) * 10;
        } else {
            device.instance().io.write_en = false;
        }

        // 在读时钟域的上升沿读取数据
        if (cycle >= 10 && (cycle % 3) == 0) {
            device.instance().io.read_en = true;
        } else {
            device.instance().io.read_en = false;
        }

        device.describe();
        device.tick();

        std::cout << "Write En: " << (unsigned)device.instance().io.write_en
                  << " Data: " << (unsigned)device.instance().io.write_data << std::endl;
        std::cout << "Read En: " << (unsigned)device.instance().io.read_en
                  << " Data: " << (unsigned)device.instance().io.read_data << std::endl;
        std::cout << "Full: " << (unsigned)device.instance().io.full
                  << " Empty: " << (unsigned)device.instance().io.empty << std::endl;
    }

    std::cout << "\n=== Simulation Complete ===" << std::endl;
    return 0;
}
