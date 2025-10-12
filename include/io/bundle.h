// include/io/bundle.h
#ifndef IO_BUNDLE_H
#define IO_BUNDLE_H

#include "core/bundle.h"
#include "core/io.h"
#include "core/logic.h"

namespace ch { namespace core {

// === Stream Bundle (Valid/Ready Handshake) ===
template<typename T>
struct stream_bundle : public bundle {
    ch_out<T> data;      // 数据
    ch_out<bool> valid;  // 有效信号
    ch_in<bool> ready;   // 就绪信号
    
    stream_bundle(const std::string& name_prefix = "") 
        : data(name_prefix + "data")
        , valid(name_prefix + "valid")
        , ready(name_prefix + "ready") {
        CHDBG("StreamBundle created with prefix: %s", name_prefix.c_str());
    }
    
    // 拷贝构造函数
    stream_bundle(const stream_bundle& other)
        : data(other.data)
        , valid(other.valid)
        , ready(other.ready) {
        CHDBG("StreamBundle copied");
    }
    
    // 赋值操作符
    stream_bundle& operator=(const stream_bundle& other) {
        if (this != &other) {
            data = other.data;
            valid = other.valid;
            ready = other.ready;
        }
        return *this;
    }
    
    // 实现翻转接口
    std::unique_ptr<bundle> flip() const override {
        auto flipped = std::make_unique<stream_bundle<T>>();
        flipped->data = data.flip();
        flipped->valid = valid.flip();
        flipped->ready = ready.flip();
        return flipped;
    }
    
    // 验证所有端口是否有效
    bool is_valid() const override {
        return data.is_valid() && valid.is_valid() && ready.is_valid();
    }
};

// === Clock and Reset Bundle ===
struct clock_reset_bundle : public bundle {
    ch_in<bool> clock;   // 时钟输入
    ch_in<bool> reset;   // 复位输入 (高电平有效)
    
    clock_reset_bundle(const std::string& name_prefix = "") 
        : clock(name_prefix + "clock")
        , reset(name_prefix + "reset") {
        CHDBG("ClockResetBundle created with prefix: %s", name_prefix.c_str());
    }
    
    clock_reset_bundle(const clock_reset_bundle& other)
        : clock(other.clock)
        , reset(other.reset) {
        CHDBG("ClockResetBundle copied");
    }
    
    clock_reset_bundle& operator=(const clock_reset_bundle& other) {
        if (this != &other) {
            clock = other.clock;
            reset = other.reset;
        }
        return *this;
    }
    
    // 翻转接口
    std::unique_ptr<bundle> flip() const override {
        auto flipped = std::make_unique<clock_reset_bundle>();
        flipped->clock = clock.flip();
        flipped->reset = reset.flip();
        return flipped;
    }
    
    bool is_valid() const override {
        return clock.is_valid() && reset.is_valid();
    }
};

// === AXI-Lite Address Channel Bundle ===
template<typename AddrWidth>
struct axi_lite_addr_bundle : public bundle {
    ch_out<AddrWidth> addr;     // 地址
    ch_out<bool> valid;         // 有效信号
    ch_in<bool> ready;          // 就绪信号
    ch_out<ch_uint<3>> prot;    // 保护类型
    
    axi_lite_addr_bundle(const std::string& name_prefix = "") 
        : addr(name_prefix + "addr")
        , valid(name_prefix + "valid")
        , ready(name_prefix + "ready")
        , prot(name_prefix + "prot") {
        CHDBG("AXILiteAddrBundle created with prefix: %s", name_prefix.c_str());
    }
    
    axi_lite_addr_bundle(const axi_lite_addr_bundle& other)
        : addr(other.addr)
        , valid(other.valid)
        , ready(other.ready)
        , prot(other.prot) {
        CHDBG("AXILiteAddrBundle copied");
    }
    
    axi_lite_addr_bundle& operator=(const axi_lite_addr_bundle& other) {
        if (this != &other) {
            addr = other.addr;
            valid = other.valid;
            ready = other.ready;
            prot = other.prot;
        }
        return *this;
    }
    
    std::unique_ptr<bundle> flip() const override {
        auto flipped = std::make_unique<axi_lite_addr_bundle<AddrWidth>>();
        flipped->addr = addr.flip();
        flipped->valid = valid.flip();
        flipped->ready = ready.flip();
        flipped->prot = prot.flip();
        return flipped;
    }
    
    bool is_valid() const override {
        return addr.is_valid() && valid.is_valid() && ready.is_valid() && prot.is_valid();
    }
};

// === AXI-Lite Data Channel Bundle ===
template<typename DataWidth>
struct axi_lite_data_bundle : public bundle {
    ch_out<DataWidth> data;     // 数据
    ch_out<ch_uint<(DataWidth::value + 7) / 8>> strb;  // 字节使能
    ch_out<bool> valid;         // 有效信号
    ch_in<bool> ready;          // 就绪信号
    ch_in<bool> resp;           // 响应信号
    
    axi_lite_data_bundle(const std::string& name_prefix = "") 
        : data(name_prefix + "data")
        , strb(name_prefix + "strb")
        , valid(name_prefix + "valid")
        , ready(name_prefix + "ready")
        , resp(name_prefix + "resp") {
        CHDBG("AXILiteDataBundle created with prefix: %s", name_prefix.c_str());
    }
    
    axi_lite_data_bundle(const axi_lite_data_bundle& other)
        : data(other.data)
        , strb(other.strb)
        , valid(other.valid)
        , ready(other.ready)
        , resp(other.resp) {
        CHDBG("AXILiteDataBundle copied");
    }
    
    axi_lite_data_bundle& operator=(const axi_lite_data_bundle& other) {
        if (this != &other) {
            data = other.data;
            strb = other.strb;
            valid = other.valid;
            ready = other.ready;
            resp = other.resp;
        }
        return *this;
    }
    
    std::unique_ptr<bundle> flip() const override {
        auto flipped = std::make_unique<axi_lite_data_bundle<DataWidth>>();
        flipped->data = data.flip();
        flipped->strb = strb.flip();
        flipped->valid = valid.flip();
        flipped->ready = ready.flip();
        flipped->resp = resp.flip();
        return flipped;
    }
    
    bool is_valid() const override {
        return data.is_valid() && strb.is_valid() && valid.is_valid() && 
               ready.is_valid() && resp.is_valid();
    }
};

// 添加更多实用的 Bundle
    // FIFO 接口 Bundle
template<typename T>
struct fifo_bundle : public bundle {
    ch_out<T> data_out;
    ch_out<bool> empty;
    ch_in<bool> read_en;
    ch_in<T> data_in;
    ch_out<bool> full;
    ch_in<bool> write_en;
    
    fifo_bundle(const std::string& prefix = "") 
        : data_out(prefix + "data_out")
        , empty(prefix + "empty")
        , read_en(prefix + "read_en")
        , data_in(prefix + "data_in")
        , full(prefix + "full")
        , write_en(prefix + "write_en") {}
        
    std::unique_ptr<bundle> flip() const override {
        auto flipped = std::make_unique<fifo_bundle<T>>();
        flipped->data_out = data_out.flip();
        flipped->empty = empty.flip();
        flipped->read_en = read_en.flip();
        flipped->data_in = data_in.flip();
        flipped->full = full.flip();
        flipped->write_en = write_en.flip();
        return flipped;
    }
    
    bool is_valid() const override {
        return data_out.is_valid() && empty.is_valid() && 
                read_en.is_valid() && data_in.is_valid() &&
                full.is_valid() && write_en.is_valid();
    }
};
}} // namespace ch::core

namespace ch {
    template<typename T>
    using stream = ch::core::stream_bundle<T>;
    
    using clock_reset = ch::core::clock_reset_bundle;

    template<typename T>
    using fifo = ch::core::fifo_bundle<T>;
   
    template<typename AddrWidth>
    using axi_lite_addr = ch::core::axi_lite_addr_bundle<AddrWidth>;
    
    template<typename DataWidth>
    using axi_lite_data = ch::core::axi_lite_data_bundle<DataWidth>;

}


#endif // IO_BUNDLE_H
