// core/stream.h
#pragma once

#include "min_cash.h"

namespace ch {
namespace core {

// ✅ Stream 接口定义
template<typename T>
struct Stream {
    __io(
        __in(ch_bool) valid;   // 数据有效信号
        __out(ch_bool) ready;  // 准备好接收信号
        __in(T) payload;       // 数据载荷
    );

    // ✅ 连接操作符 (sink << source)
    void operator<<(Stream<T>& source) {
        io.valid = source.io.valid;
        source.io.ready = io.ready;
        io.payload = source.io.payload;
    }
};

} // namespace core
} // namespace ch
