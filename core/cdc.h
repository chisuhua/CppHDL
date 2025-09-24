// core/cdc.h
#pragma once

#include "min_cash.h"

namespace ch {
namespace core {

// ✅ 双触发器同步器
// 用于将信号从源时钟域安全地同步到目标时钟域
template<typename T>
class Synchronizer : public Component {
public:
    __io(
        __in(T) d; // 输入数据
        __out(T) q; // 输出数据
    );

    explicit Synchronizer(Component* parent) {
        // 父组件指针在这里不需要使用，因为 Synchronizer 本身不是寄存器
        // 它内部的寄存器会在构造时自动向父组件注册
    }

    void describe() override {
        // 同步器需要在目标时钟域实例化
        // 我们假设 ch_pushcd 已经在外部设置好目标时钟域
        static ch_reg<T> stage1(this, "stage1", T(0));
        static ch_reg<T> stage2(this, "stage2", T(0));

        stage1.next() = io.d;
        stage2.next() = *stage1;
        io.q = *stage2;
    }
};

} // namespace core
} // namespace ch
