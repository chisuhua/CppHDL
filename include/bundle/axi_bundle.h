// include/io/axi_bundle.h
#ifndef CH_IO_AXI_BUNDLE_H
#define CH_IO_AXI_BUNDLE_H

#include "ch.hpp"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"

using namespace ch::core;

namespace ch {

// AXI地址通道Bundle (AW and AR channels)
template <uint32_t ADDR_WIDTH>
struct axi_addr_channel : public bundle_base<axi_addr_channel<ADDR_WIDTH>> {
    using Self = axi_addr_channel<ADDR_WIDTH>;
    ch_uint<ADDR_WIDTH> addr; // 地址
    ch_uint<3> prot;          // 保护类型
    ch_uint<4> cache;         // 缓存类型
    ch_uint<2> burst;         // 突发类型
    ch_uint<8> len;           // 突发长度
    ch_uint<4> size;          // 突发大小
    ch_bool lock;             // 锁定传输
    ch_bool valid;            // 有效信号
    ch_bool ready;            // 就绪信号

    // axi_addr_channel() = default;
    // explicit axi_addr_channel(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(addr, prot, cache, burst, len, size, lock, valid, ready)

    void as_master_direction() {
        // Master: 输出地址和其他控制信号，接收就绪信号
        this->make_output(addr, prot, cache, burst);
        this->make_output(len, size, lock, valid);
        this->make_input(ready);
    }

    void as_slave_direction() {
        // Slave: 输入地址和其他控制信号，输出就绪信号
        this->make_input(addr, prot, cache, burst);
        this->make_input(len, size, lock, valid);
        this->make_output(ready);
    }
};

// AXI写数据通道Bundle
template <uint32_t DATA_WIDTH>
struct axi_write_data_channel
    : public bundle_base<axi_write_data_channel<DATA_WIDTH>> {
    using Self = axi_write_data_channel<DATA_WIDTH>;
    ch_uint<DATA_WIDTH> data;     // 写数据
    ch_uint<DATA_WIDTH / 8> strb; // 字节选通信号
    ch_uint<DATA_WIDTH / 8> keep; // 数据有效信号
    ch_bool last;                 // 最后一个数据信号
    ch_bool valid;                // 有效信号
    ch_bool ready;                // 就绪信号

    // axi_write_data_channel() = default;
    // explicit axi_write_data_channel(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(data, strb, keep, last, valid, ready)

    void as_master_direction() {
        // Master: 输出数据和控制信号，接收就绪信号
        this->make_output(data, strb, keep, last);
        this->make_output(valid);
        this->make_input(ready);
    }

    void as_slave_direction() {
        // Slave: 输入数据和控制信号，输出就绪信号
        this->make_input(data, strb, keep, last);
        this->make_input(valid);
        this->make_output(ready);
    }
};

// AXI写响应通道Bundle
struct axi_write_resp_channel : public bundle_base<axi_write_resp_channel> {
    using Self = axi_write_resp_channel;
    ch_uint<2> resp; // 响应信号
    ch_bool valid;   // 有效信号
    ch_bool ready;   // 就绪信号

    // axi_write_resp_channel() = default;
    // explicit axi_write_resp_channel(const std::string &prefix) {
    //     this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(resp, valid, ready)

    void as_master_direction() {
        // Master: 输入响应信号，输出有效信号
        this->make_input(resp);
        this->make_output(valid);
        this->make_input(ready);
    }

    void as_slave_direction() {
        // Slave: 输出响应信号，输入有效信号
        this->make_output(resp);
        this->make_input(valid);
        this->make_output(ready);
    }
};

// AXI写事务通道Bundle
template <uint32_t ADDR_WIDTH, uint32_t DATA_WIDTH>
struct axi_write_channel
    : public bundle_base<axi_write_channel<ADDR_WIDTH, DATA_WIDTH>> {
    using Self = axi_write_channel<ADDR_WIDTH, DATA_WIDTH>;
    axi_addr_channel<ADDR_WIDTH> aw;      // 写地址通道
    axi_write_data_channel<DATA_WIDTH> w; // 写数据通道
    axi_write_resp_channel b;             // 写响应通道

    axi_write_channel() = default;
    explicit axi_write_channel(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(aw, w, b)

    void as_master_direction() {
        // Master: 控制整个写事务
        aw.as_master();
        w.as_master();
        b.as_slave();
    }

    void as_slave_direction() {
        // Slave: 响应整个写事务
        aw.as_slave();
        w.as_slave();
        b.as_master();
    }
};

} // namespace ch

#endif // CH_IO_AXI_BUNDLE_H