#pragma once
// ============ 硬件断言宏 ============
// 用于在 describe() 阶段检查硬件信号
#define ch_assert(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "❌ Hardware Assertion Failed: " << msg \
                      << " at " << __FILE__ << ":" << __LINE__ \
                      << " in cycle " << global_simulation_cycle << std::endl; \
            std::abort(); \
        } \
    } while(0)
