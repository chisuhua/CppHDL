// examples/vec_and_struct.cpp
#include "../core/min_cash.h"
#include "../core/component.h" // ✅ 确保包含 Component
#include <iostream>

using namespace ch::core;

// ✅ 定义一个像素结构体
struct Pixel {
    ch_uint<8> r{0};
    ch_uint<8> g{0};
    ch_bool    valid{false}; // 是否有效

    // 可选：为结构体提供一个打印函数
    friend std::ostream& operator<<(std::ostream& os, const Pixel& p) {
        os << "R:" << (unsigned)p.r << " G:" << (unsigned)p.g << " V:" << (unsigned)p.valid;
        return os;
    }
};

// ✅ 定义一个使用 ch_vec 和结构体的模块
struct ImageBufferModule : public Component { // ✅ 继承 Component
    static constexpr int BUFFER_SIZE = 4;

    __io(
        __in(ch_bool) clk;
        __in(ch_bool) rst;
        __in(Pixel) input_pixel; // ✅ 结构体作为输入
        __in(ch_bool) input_valid;
        __out(ch_uint<2>) write_ptr; // 写指针
        __out(ch_uint<2>) read_ptr; // 写指针
        __out(Pixel) output_pixel; // ✅ 结构体作为输出
        __out(ch_bool) output_valid;
    );

    void describe() override {
        ch_pushcd(io.clk, io.rst, true);

        // ✅ 使用 ch_vec 创建一个像素缓冲区
        static ch_reg<ch_vec<Pixel, BUFFER_SIZE>> buffer(this, "buffer", {}); // 初始化为空
        static ch_reg<ch_uint<2>> wptr(this, "wptr", 0); // 写指针
        static ch_reg<ch_uint<2>> rptr(this, "rptr", 0); // 读指针
        //
        std::cout << "  [DEBUG] Before logic - WPtr: " << (unsigned)*wptr << " RPtr: " << (unsigned)*rptr << std::endl;

        // 写逻辑
        if (io.input_valid) {
            std::cout << "  [DEBUG] Writing Pixel (" << io.input_pixel << ") to index " << (unsigned)*wptr << std::endl;
            buffer.next()[*wptr] = io.input_pixel; // ✅ 使用 vec_proxy
            wptr.next() = *wptr + 1;
            for (int i = 0; i < BUFFER_SIZE; ++i) {
                std::cout << "  [DEBUG] Buffer[" << i << "] = " << (*buffer)[i] << std::endl;
            }
        }

        const auto& current_buffer = *buffer;
        io.output_pixel = current_buffer[*rptr];
        std::cout << "  [DEBUG] Reading Pixel (" << current_buffer[*rptr] << ") from index " << (unsigned)*rptr << std::endl; // ✅ 修复
        //
        //io.output_pixel = (*buffer)[*rptr]; // ✅ 读取结构体
        //
        io.output_valid = true; // 简化，始终有效
        rptr.next() = *rptr + 1;

        // 输出写指针
        io.write_ptr = *wptr;
        io.read_ptr = *rptr;

        ch_popcd();
    }
};

int main() {
    std::cout << "=== Starting Simulation: Vector and Struct Support ===" << std::endl;

    ch_device<ImageBufferModule> device;

    for (int cycle = 0; cycle < 20; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;

        // 设置时钟和复位
        device.instance().io.clk = (cycle % 2);
        device.instance().io.rst = (cycle == 0);

        // 生成输入像素 (Cycle 1, 3, 5, 7)
        if (cycle >= 1 && cycle <= 7 && (cycle % 2) == 1) {
            device.instance().io.input_valid = true;
            device.instance().io.input_pixel.r = cycle * 10;
            device.instance().io.input_pixel.g = cycle * 5;
            device.instance().io.input_pixel.valid = true;
        } else {
            device.instance().io.input_valid = false;
        }

        // 执行硬件逻辑
        device.describe();
        device.tick();

        // 打印输出
        std::cout << "Write Ptr: " << (unsigned)device.instance().io.write_ptr << std::endl;
        std::cout << "Input Pixel: " << device.instance().io.input_pixel << std::endl;
        std::cout << "Read Ptr: " << (unsigned)device.instance().io.read_ptr << std::endl;
        std::cout << "Output Pixel: " << device.instance().io.output_pixel << std::endl;
        std::cout << "Output Valid: " << (unsigned)device.instance().io.output_valid << std::endl;
    }

    std::cout << "\n=== Simulation Complete ===" << std::endl;
    return 0;
}
