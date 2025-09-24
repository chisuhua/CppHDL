#pragma once

#include "../core/component.h" // ✅ 确保包含 Component 基类
#include <iostream>

namespace ch {
namespace core {

// ✅ 同步 FIFO 模板
template<typename T, int DEPTH>
class SyncFifo : public Component {
private:
    static constexpr int ADDR_WIDTH = (DEPTH > 1) ? (32 - __builtin_clz(DEPTH - 1)) : 1;
    static constexpr int PTR_WIDTH = ADDR_WIDTH + 1; // ✅ 关键：增加一位
    ch_reg<ch_uint<PTR_WIDTH>> wptr;
    ch_reg<ch_uint<PTR_WIDTH>> rptr;
    ch_mem<T, DEPTH> memory;

public:
    explicit SyncFifo(Component* parent, const std::string& path_name = "sync_fifo") 
        : Component(path_name)
        , wptr(this, "wptr", 0), rptr(this, "rptr", 0), memory(this) {
             if(parent) parent->add_child(this);
        }

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

        ch_bool is_full = (*wptr >> ADDR_WIDTH) != (*rptr >> ADDR_WIDTH) &&
                          (*wptr & ((1 << ADDR_WIDTH) - 1)) == (*rptr & ((1 << ADDR_WIDTH) - 1));
        ch_bool is_empty = *wptr == *rptr;

        std::cout << "  [DEBUG SyncFifo]@" << global_simulation_cycle 
            << " is_reset=" << io.rst
            << " rst_ptr_  " << (void*)&io.rst
            << " write_en=" << io.write_en
            << " read_en=" << io.read_en
            << " is_full=" << is_full
            << " is_empty=" << is_empty
            << " wptr=" << *wptr
            << " rptr=" << *rptr
            << std::endl;

        // ✅ 写逻辑
        if (io.write_en && !is_full) {
            memory[*wptr] = io.write_data;
            wptr.next() = *wptr + 1;
            std::cout << "  [DEBUG SyncFifo]@" << global_simulation_cycle << " SynFifo WriteData[" << *wptr << "]=" << *memory[*wptr] << std::endl;
        }

        // ✅ 读逻辑
        if (io.read_en && !is_empty) {
            ch_uint<ADDR_WIDTH> next_addr = static_cast<ch_uint<ADDR_WIDTH>>(*rptr + 1);
            io.read_data = *memory[next_addr];
            rptr.next() = *rptr + 1;
            std::cout << "  [DEBUG SyncFifo]@" << global_simulation_cycle << " SynFifo ReadData["  << next_addr << "]=" << *memory[next_addr] << std::endl;
        } else {
            io.read_data = *memory[static_cast<ch_uint<ADDR_WIDTH>>(*rptr)];
        }

        // ✅ 输出空满标志
        io.full = is_full;
        io.empty = is_empty;

        ch_popcd();
    }
};


} // namespace core
} // namespace ch
