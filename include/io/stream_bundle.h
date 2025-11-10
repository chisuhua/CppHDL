// include/io/stream_bundle.h
#ifndef CH_IO_STREAM_BUNDLE_H
#define CH_IO_STREAM_BUNDLE_H

#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/uint.h"
#include "core/bool.h"

namespace ch::core {

template<typename T>
struct stream_bundle;

// 前向声明完成后再定义
template<typename T>
struct stream_bundle : public bundle_base<stream_bundle<T>> {
    using Self = stream_bundle<T>;
    T payload;
    ch_bool valid;
    ch_bool ready;

    stream_bundle() = default;
    explicit stream_bundle(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    static constexpr auto __bundle_fields() { 
        return std::make_tuple( 
            CH_BUNDLE_FIELD_ENTRY(Self, payload),
            CH_BUNDLE_FIELD_ENTRY(Self, valid),
            CH_BUNDLE_FIELD_ENTRY(Self, ready)
        ); 
    }

    void as_master() override {
        // Master: 输出数据，接收就绪信号
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        // Slave: 接收数据，发送就绪信号
        this->make_input(payload, valid);
        this->make_output(ready);
    }
};

} // namespace ch::core

#endif // CH_IO_STREAM_BUNDLE_H