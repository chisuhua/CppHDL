// src/sim/instr_reg.cpp
#include "instr_reg.h"
#include "types.h"
#include "logger.h"
#include <cassert>
#include <unordered_map> // For data_map_t if needed for debugging
#include <iomanip>       // For std::hex, std::setw, std::setfill

using namespace ch::core;

namespace ch
{

    // --- instr_reg Constructor ---
    instr_reg::instr_reg(sdata_type* current_buf, uint32_t size, sdata_type* next_buf,
                         uint32_t cd, sdata_type* clk_en_buf, sdata_type* rst_buf, 
                         sdata_type* rst_val_buf)
        : instr_base(size), current_buf_(current_buf), next_buf_(next_buf),
          clk_en_buf_(clk_en_buf), rst_buf_(rst_buf), rst_val_buf_(rst_val_buf),
          current_node_id_(static_cast<uint32_t>(-1)), next_node_id_(static_cast<uint32_t>(-1)),
          cd_(cd)
    {
        // Store the node IDs instead of the pointers
        CHDBG_FUNC();
        CHINFO("Created register instruction for size=%u", size);
    }

    // --- instr_reg::eval with data_map lookup and truncation using bv_assign_truncate ---
    void instr_reg::eval()
    {
        CHDBG_FUNC();

        // 检查是否有复位信号且为激活状态
        if (rst_buf_ && !rst_buf_->is_zero()) {
            // 应用复位值
            if (rst_val_buf_) {
                *current_buf_ = *rst_val_buf_;
            }
            return;
        }

        // 检查时钟使能信号
        if (clk_en_buf_ && clk_en_buf_->is_zero()) {
            // 时钟使能为低，不更新寄存器
            return;
        }

        // 更新寄存器值
        if (next_buf_) {
            // 如果有next值，则使用next值更新当前值
            *current_buf_ = *next_buf_;
        }
        // 否则保持当前值不变（寄存器特性）
    }
} // namespace ch