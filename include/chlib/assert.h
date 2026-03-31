/**
 * @file ch_assert.h
 * @brief CppHDL 断言系统 - 用于仿真时验证和调试
 */

#pragma once

#include "ch.hpp"
#include "simulator.h"
#include <iostream>

using namespace ch::core;

namespace ch {
namespace chlib {

/**
 * @brief 仿真时断言检查器组件
 */
class AssertChecker : public ch::Component {
public:
    __io(
        ch_in<ch_bool> condition;
        ch_in<ch_bool> enable;
        ch_out<ch_bool> failed;
    )
    
    AssertChecker(ch::Component* parent = nullptr, const std::string& name = "assert_checker")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        auto en = io().enable;
        auto cond = io().condition;
        auto is_failed = select(en, cond == ch_bool(false), ch_bool(false));
        io().failed <<= is_failed;
    }
};

} // namespace chlib
} // namespace ch

// 便捷宏
#define CH_ASSERT(cond, msg) \
    do { \
        std::cout << "[CH_ASSERT] " << msg << " @ " \
                  << __FILE__ << ":" << __LINE__ << std::endl; \
    } while(0)

#define ch_assert(cond, msg) CH_ASSERT(cond, msg)
