// examples/sync_fifo.cpp
#include "../core/component.h" // ✅ 确保包含 Component 基类
#include <iostream>

using namespace ch::core;

// ✅ 同步 FIFO 模板
template<typename T, int DEPTH>
class SyncFifo : public Component {
private:
    static constexpr int ADDR_WIDTH = (DEPTH > 1) ? (32 - __builtin_clz(DEPTH - 1)) : 1;
    static constexpr int PTR_WIDTH = ADDR_WIDTH + 1; // ✅ 关键：增加一位

public:
    __io(
        __in(ch_bool) clk;
        __in(ch_bool) rst;
        // 写端口
        __in(ch_bool) write_en;
        __in(T) write_data;
        __out(ch_bool) full;
        // 读端口
        __in(ch_bool) read_en;
        __out(T) read_data;
        __out(ch_bool) empty;
    );

    void describe() override {
        ch_pushcd(io.clk, io.rst, true);

        // ✅ 声明读写指针和内存
        static ch_reg<ch_uint<PTR_WIDTH>> wptr(this, "wtpr", 0);
        static ch_reg<ch_uint<PTR_WIDTH>> rptr(this, "rtpr", 0);
        static ch_mem<T, DEPTH> memory(this);

        // ✅ 计算空满标志
        ch_bool is_full = (*wptr >> ADDR_WIDTH) != (*rptr >> ADDR_WIDTH) &&
                          (*wptr & ((1 << ADDR_WIDTH) - 1)) == (*rptr & ((1 << ADDR_WIDTH) - 1));
        ch_bool is_empty = *wptr == *rptr;

        // ✅ 写逻辑
        if (io.write_en && !is_full) {
            memory[*wptr] = io.write_data;
            wptr.next() = *wptr + 1;
        }

        // ✅ 读逻辑
        if (io.read_en && !is_empty) {
            rptr.next() = *rptr + 1;
            io.read_data = *memory[*rptr];
        } else {
            io.read_data = io.read_data;
        }

        // ✅ 输出空满标志
        io.full = is_full;
        io.empty = is_empty;

        ch_popcd();
    }
};

// ✅ 测试主函数
int main() {
    std::cout << "=== Starting Simulation: Sync FIFO ===" << std::endl;

    // 创建一个深度为4，数据宽度为8位的FIFO
    ch_device<SyncFifo<ch_uint<8>, 4>> device;

    for (int cycle = 0; cycle < 20; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;

        // 设置时钟和复位
        device.instance().io.clk = (cycle % 2);
        device.instance().io.rst = (cycle < 2);

        if (cycle >= 1 && cycle <= 7 && (cycle % 2) == 1) {
            device.instance().io.write_en = true;
            device.instance().io.write_data = (cycle + 1) * 10;
        } else {
            device.instance().io.write_en = false;
        }

        if (cycle >= 9 && cycle <= 15 && (cycle % 2) == 1) {
            device.instance().io.read_en = true;
        } else {
            device.instance().io.read_en = false;
        }

        // 执行硬件逻辑
        device.describe();
        device.tick();

        // 打印状态
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
/*
* 代码说明
模板参数：
T: FIFO 中存储的数据类型。
DEPTH: FIFO 的深度（容量）。
指针宽度计算：
PTR_WIDTH 使用 __builtin_clz (GCC内置函数) 计算指针所需的位宽。例如，深度为4时，指针需要2位。
核心逻辑：
写指针 (wptr) 和 读指针 (rptr) 都是寄存器。
内存 (memory) 使用 ch_mem 实现。
空标志 (empty)：当读写指针相等，且没有写入或正在读取时，FIFO为空。
满标志 (full)：当读写指针相等，且正在写入但没有读取时，FIFO为满。
写操作：在 write_en 有效且 FIFO 非满时，将 write_data 写入 memory[wptr]，然后 wptr 自增。
读操作：在 read_en 有效且 FIFO 非空时，rptr 自增。read_data 始终输出 memory[rptr] 的当前值（组合逻辑）。
*/
