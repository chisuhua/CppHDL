// src/simulator_trace.cpp
// Phase 6: Trace + VCD + signal collection moved out of simulator.cpp.
// Owns: toString(TraceType), load_trace_config_from_file, should_trace_node,
//       collect_signals, trace, allocate_new_trace_block,
//       write_to_trace_block, toVCD.
#include "simulator.h"
#include "ast/ast_nodes.h"
#include "core/lnodeimpl.h"
#include "logger.h"
#include "types.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <set>
#include <unordered_set>

namespace ch {

std::string toString(TraceType type) {
    switch (type) {
    case TraceType::REG:
        return "REG";
    case TraceType::INPUT:
        return "INPUT";
    case TraceType::OUTPUT:
        return "OUTPUT";
    case TraceType::CLOCK:
        return "CLOCK";
    case TraceType::RESET:
        return "RESET";
    case TraceType::TAP:
        return "TAP";
    default:
        return "UNKNOWN";
    }
}

void Simulator::load_trace_config_from_file(const std::string &config_file) {
    CHDBG_FUNC();
    CHINFO("Loading trace configuration from file: %s", config_file.c_str());

    inipp::Ini<char> ini;
    std::ifstream is(config_file);
    if (!is.is_open()) {
        CHWARN("Could not open configuration file: %s", config_file.c_str());
        return;
    }

    ini.parse(is);

    // 遍历所有section，每个section代表一个模块实例
    for (const auto &section_pair : ini.sections) {
        const std::string &section_name = section_pair.first;
        const auto &section = section_pair.second;

        // 解析配置项
        int trace_on = 0;
        int trace_reg = 0;
        int trace_tap = 0; // 替换trace_wire为trace_tap
        int trace_input = 0;
        int trace_output = 0;
        int trace_clock = 0; // 新增clock配置
        int trace_reset = 0; // 新增reset配置

        inipp::get_value(section, "trace_on", trace_on);
        inipp::get_value(section, "trace_reg", trace_reg);
        inipp::get_value(section, "trace_tap", trace_tap);
        inipp::get_value(section, "trace_input", trace_input);
        inipp::get_value(section, "trace_output", trace_output);
        inipp::get_value(section, "trace_clock", trace_clock); // 新增clock配置
        inipp::get_value(section, "trace_reset", trace_reset); // 新增reset配置

        trace_configs_[section_name] = TraceConfig(
            trace_on, trace_reg, trace_tap, trace_input, trace_output,
            trace_clock, trace_reset); // 更新构造函数参数

        CHINFO("Loaded trace config for section '%s': trace_on=%d, "
               "trace_reg=%d, trace_tap=%d, trace_input=%d, trace_output=%d, "
               "trace_clock=%d, trace_reset=%d",
               section_name.c_str(), trace_on, trace_reg, trace_tap,
               trace_input, trace_output, trace_clock, trace_reset);
    }
}

bool Simulator::should_trace_node(uint32_t node_id,
                                  const std::string &node_name,
                                  TraceType trace_type) {
    // 如果全局跟踪未启用，直接返回false
    if (!trace_on_) {
        return false;
    }

    // 如果已经处理过这个节点，直接返回
    if (traced_nodes_.find(node_id) != traced_nodes_.end()) {
        return false;
    }

    // 首先检查指定路径的配置
    auto path_it = trace_configs_.find(node_name);
    if (path_it != trace_configs_.end()) {
        const TraceConfig &config = path_it->second;
        if (!config.trace_on)
            return false; // 路径级关闭

        // 根据传入的trace_type判断是否跟踪
        switch (trace_type) {
        case TraceType::REG:
            return config.trace_reg;
        case TraceType::INPUT:
            return config.trace_input;
        case TraceType::OUTPUT:
            return config.trace_output;
        case TraceType::CLOCK:
            return config.trace_clock;
        case TraceType::RESET:
            return config.trace_reset;
        case TraceType::TAP:
            return config.trace_tap;
        default:
            return config.trace_on;
        }
    }

    // 如果指定路径没有找到配置，则查找模块名配置
    std::string current_path = node_name;
    size_t pos;
    while ((pos = current_path.rfind('.')) != std::string::npos) {
        current_path = current_path.substr(0, pos);
        auto it = trace_configs_.find(current_path);
        if (it != trace_configs_.end()) {
            const TraceConfig &config = it->second;
            if (!config.trace_on)
                return false; // 模块级关闭

            switch (trace_type) {
            case TraceType::REG:
                return config.trace_reg;
            case TraceType::INPUT:
                return config.trace_input;
            case TraceType::OUTPUT:
                return config.trace_output;
            case TraceType::CLOCK:
                return config.trace_clock;
            case TraceType::RESET:
                return config.trace_reset;
            case TraceType::TAP:
                return config.trace_tap;
            default:
                return config.trace_on;
            }
        }
    }

    // 如果没有模块前缀，则查找"top"配置
    auto top_it = trace_configs_.find("top");
    if (top_it != trace_configs_.end()) {
        const TraceConfig &config = top_it->second;
        if (!config.trace_on)
            return false; // 顶层关闭

        switch (trace_type) {
        case TraceType::REG:
            return config.trace_reg;
        case TraceType::INPUT:
            return config.trace_input;
        case TraceType::OUTPUT:
            return config.trace_output;
        case TraceType::CLOCK:
            return config.trace_clock;
        case TraceType::RESET:
            return config.trace_reset;
        case TraceType::TAP:
            return config.trace_tap;
        default:
            return config.trace_on;
        }
    }

    // 如果没有任何配置，返回默认值（不跟踪）
    return false;
}

void Simulator::collect_signals() {
    CHDBG_FUNC();
    signals_.clear();
    traced_nodes_.clear();

    // 添加所有符合条件的信号，包括叶子节点
    for (auto *node : eval_list_) {

        // 根据节点类型和是否为叶子节点确定trace类型
        TraceType trace_type;
        switch (node->type()) {
        case ch::core::lnodetype::type_reg:
            trace_type = TraceType::REG;
            break;
        case ch::core::lnodetype::type_input:
            trace_type = TraceType::INPUT;
            break;
        case ch::core::lnodetype::type_output:
            trace_type = TraceType::OUTPUT;
            break;
        case ch::core::lnodetype::type_clock:
            trace_type = TraceType::CLOCK;
            if (node == ctx_->get_default_clock()) {
                signals_.push_back(std::make_pair(node->id(), node->name()));
                traced_nodes_.insert(node->id());
                continue;
            }
            break;
        case ch::core::lnodetype::type_reset:
            trace_type = TraceType::RESET;
            if (node == ctx_->get_default_reset()) {
                signals_.push_back(std::make_pair(node->id(), node->name()));
                traced_nodes_.insert(node->id());
                continue;
            }
            break;
        default:
            const auto &users = node->get_users();
            const auto &srcs = node->num_srcs();
            if (users.size() == 0 && srcs > 0) {
                trace_type = TraceType::TAP;
                break;
            } else {
                continue;
            }
        }

        bool should_trace =
            should_trace_node(node->id(), node->name(), trace_type);

        if (!should_trace) {
            CHINFO(
                "Skip signal for tracing: %s (ID: %u, Type: %s, trace_type %s)",
                node->name().c_str(), node->id(),
                ch::core::to_string(node->type()),
                toString(trace_type).c_str());
            continue;
        } else {
            signals_.push_back(std::make_pair(node->id(), node->name()));
            traced_nodes_.insert(node->id());
            CHINFO("Added signal for tracing: %s (ID: %u, Type: %s, trace_type "
                   "%s)",
                   node->name().c_str(), node->id(),
                   ch::core::to_string(node->type()),
                   toString(trace_type).c_str());
        }
    }

    CHINFO("Collected %zu signals for tracing", signals_.size());

    if (signals_.size() == 0) {
        trace_on_ = false;
        return;
    }

    // 计算总跟踪宽度
    trace_width_ = 0;
    for (const auto &signal_pair : signals_) {
        uint32_t node_id = signal_pair.first;
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            trace_width_ += it->second.bitwidth();
        }
    }

    // 初始化跟踪相关数据结构
    prev_values_.resize(signals_.size());
    for (auto &prev : prev_values_) {
        prev.first = nullptr;
        prev.second = 0;
    }

    // P0-1: 预计算信号数据指针，避免 trace() 中每次都做 hash lookup
    signal_data_ptrs_.resize(signals_.size());
    for (size_t i = 0; i < signals_.size(); ++i) {
        uint32_t node_id = signals_[i].first;
        auto it = data_map_.find(node_id);
        if (it != data_map_.end()) {
            signal_data_ptrs_[i] = &it->second;
        } else {
            signal_data_ptrs_[i] = nullptr;
        }
    }

    valid_mask_ = ch::core::sdata_type(0, signals_.size());
}

void Simulator::trace() {
    if (!trace_on_)
        return;

    CHDBG_FUNC();

    // 分配新的跟踪块（如果当前块已满）
    const size_t NUM_TRACES = 1024; // 每块存储的跟踪数量
    auto block_width = NUM_TRACES * trace_width_;
    if (nullptr == current_trace_block_ ||
        (current_trace_block_->size + trace_width_) > block_width) {
        allocate_new_trace_block();
    }

    // 记录跟踪数据
    valid_mask_.reset(); // 重置有效位掩码
    auto dst_block = current_trace_block_->data;
    auto dst_offset = current_trace_block_->size;
    dst_offset += (valid_mask_.bitwidth() + 31) / 32 *
                  4; // 跳过有效位掩码的空间（按4字节对齐）

    // 遍历所有信号 (P0-1优化版：使用预计算指针，避免hash lookup和临时对象)
    for (uint32_t i = 0, n = signals_.size(); i < n; ++i) {
        auto *curr_value_ptr = signal_data_ptrs_[i];
        if (!curr_value_ptr) {
            continue;
        }
        auto &curr_value = *curr_value_ptr;
        auto &prev = prev_values_[i];

        // 如果之前有值，比较是否发生变化
        if (prev.first) {
            // 直接比较原始字节，避免临时sdata_type对象构造/析构
            size_t byte_len = (curr_value.bitwidth() + 7) / 8;
            if (std::memcmp(curr_value.bitvector().data(), prev.first, byte_len) == 0) {
                continue;
            }
        }

        // 保存当前值的位置信息
        prev.first = dst_block;
        prev.second = dst_offset;

        // 复制信号值到跟踪缓冲区
        std::memcpy(dst_block + dst_offset, curr_value.bitvector().data(),
                    (curr_value.bitwidth() + 7) / 8); // 根据位宽计算字节数
        dst_offset += (curr_value.bitwidth() + 7) / 8; // 按字节对齐

        // 使用[]操作符设置valid_mask_的位
        valid_mask_.bitvector()[i] = true; // 标记该信号在此次仿真中有变化
    }

    // 将有效位掩码写入跟踪数据块
    std::memcpy(dst_block + current_trace_block_->size,
                valid_mask_.bitvector().data(),
                (valid_mask_.bitwidth() + 31) / 32 * 4); // 按4字节对齐

    // 更新跟踪块大小
    current_trace_block_->size = dst_offset;
}

void Simulator::allocate_new_trace_block() {
    // 释放旧的当前块（如果不是在blocks列表中）
    if (current_trace_block_) {
        // 如果当前块不是最后一个块，说明需要新开辟
        if (trace_blocks_.empty() ||
            trace_blocks_.back() != current_trace_block_) {
            delete current_trace_block_;
        }
    }

    current_trace_block_ = new TraceBlock(current_trace_block_size_);
    trace_blocks_.push_back(current_trace_block_);
    current_trace_block_used_ = 0;
}

void Simulator::write_to_trace_block(const std::string &data) {
    if (!current_trace_block_) {
        allocate_new_trace_block();
    }

    size_t data_size = data.size();
    if (current_trace_block_->size + data_size >
        current_trace_block_->capacity) {
        allocate_new_trace_block();
    }

    if (current_trace_block_ && current_trace_block_->size + data_size <=
                                    current_trace_block_->capacity) {
    memcpy(current_trace_block_->data + current_trace_block_->size,
           data.data(), data_size);
        current_trace_block_->size += data_size;
    }
}

void Simulator::toVCD(const std::string &filename) const {
    // 预留VCD输出功能的实现
    CHINFO("VCD output to file: %s", filename.c_str());

    if (!trace_on_) {
        CHWARN("Tracing is not enabled, nothing to export to VCD");
        return;
    }

    if (trace_blocks_.empty()) {
        CHWARN("No trace data available to export to VCD");
        return;
    }

    std::ofstream out(filename);
    if (!out.is_open()) {
        CHERROR("Failed to open file for VCD output: %s", filename.c_str());
        return;
    }

    // 输出VCD头部信息
    out << "$timescale 1 ns $end\n";
    out << "$scope module top $end\n";

    // 输出信号定义，使用更合适的信号标识符
    for (size_t i = 0; i < signals_.size(); ++i) {
        const auto &signal_info = signals_[i];
        auto signal_id = signal_info.first;
        auto signal_name = signal_info.second;

        // 确保信号名符合VCD规范，移除可能引起冲突的字符
        std::string sanitized_name = signal_name;
        for (char &c : sanitized_name) {
            if (c == ' ' || c == '.')
                c = '_';
        }

        auto signal_width = data_map_.at(signal_id).bitwidth();
        if (signal_width == 1) {
            out << "$var wire 1 " << static_cast<char>('!' + i) << " "
                << sanitized_name << " $end\n";
        } else {
            out << "$var wire " << signal_width << " "
                << static_cast<char>('!' + i) << " " << sanitized_name
                << " $end\n";
        }
    }

    out << "$upscope $end\n";
    out << "$enddefinitions $end\n";

    // 输出跟踪数据
    uint64_t t = 0; // 使用更大的类型避免溢出
    auto mask_width = signals_.size();

    // 遍历所有跟踪块
    for (const auto *trace_block : trace_blocks_) {
        auto src_block = trace_block->data;
        auto src_width = trace_block->size;
        uint32_t src_offset = 0;

        // 每个块包含多个时间点的数据
        while (src_offset < src_width) {
            // 读取有效位掩码（按4字节对齐）
            uint32_t mask_size_bytes = (mask_width + 31) / 32 * 4;
            const char *mask_ptr = src_block + src_offset;
            src_offset += mask_size_bytes;

            bool has_changes = false;
            // 遍历所有信号
            for (uint32_t i = 0, n = signals_.size(); i < n; ++i) {
                // 检查该信号在此时间点是否有变化
                uint32_t word_idx = i / 32;
                uint32_t bit_idx = i % 32;

                if (word_idx < (mask_size_bytes / sizeof(uint32_t))) {
                    // 读取掩码字
                    const uint32_t *mask_words =
                        reinterpret_cast<const uint32_t *>(mask_ptr);
                    bool valid = (mask_words[word_idx] >> bit_idx) & 1;

                    if (valid) {
                        if (!has_changes) {
                            out << '#' << t << '\n'; // 输出时间戳
                            has_changes = true;
                        }

                        auto signal_id = signals_[i].first;
                        auto signal_width = data_map_.at(signal_id).bitwidth();

                        // 读取信号值（按字节对齐）
                        uint32_t signal_bytes = (signal_width + 7) / 8;
                        const char *signal_ptr = src_block + src_offset;
                        src_offset += signal_bytes;

                        // 构建信号值
                        ch::core::sdata_type value(0, signal_width);
                        std::memcpy(
                            const_cast<ch::internal::bitvector<uint64_t> &>(
                                value.bitvector())
                                .data(),
                            signal_ptr,
                            std::min(
                                signal_bytes,
                                static_cast<uint32_t>(value.bitvector().size() *
                                                      sizeof(uint64_t))));

                        // 输出信号值
                        out << 'b';
                        // 输出二进制表示，从高位到低位
                        for (int j = signal_width - 1; j >= 0; --j) {
                            out << (value.bitvector()[static_cast<uint32_t>(j)]
                                        ? '1'
                                        : '0');
                        }
                        out << ' ' << static_cast<char>('!' + i)
                            << '\n'; // 输出信号ID
                    }
                }
            }

            t++; // 时间递增
        }
    }

    out.close();
    CHINFO("VCD file written successfully: %s", filename.c_str());
}

} // namespace ch
