// src/simulator.cpp
#include "simulator.h"
#include "ast/ast_nodes.h"
#include "ast/instr_mem.h"
#include "ast/mem_port_impl.h"
#include "core/lnodeimpl.h"
#include "logger.h"
#include "types.h"
#include "utils/destruction_manager.h"
#include <cassert>
#include <iostream>

namespace ch {

Simulator::Simulator(ch::core::context *ctx) : ctx_(ctx) {
    CHDBG_FUNC();
    CHREQUIRE(ctx_ != nullptr, "Context cannot be null");

    // Register with destruction manager
    std::cout << "Registering simulator " << this << std::endl;
    std::cout.flush();

    initialize();
}

Simulator::~Simulator() {
    ch::pre_static_destruction_cleanup();
    ch::detail::set_static_destruction();

    // Explicitly disconnect to prevent accessing destroyed context
    disconnect();
}

void Simulator::disconnect() {
    CHDBG_FUNC();

    // Check if we're in static destruction phase
    if (ch::detail::in_static_destruction()) {
        // During static destruction, minimize operations to prevent segfaults
        // Just mark as disconnected and return immediately
        disconnected_ = true;
        return;
    }

    if (disconnected_) {
        return;
    }

    // Mark as disconnected first to prevent any further operations
    disconnected_ = true;

    // Clear all data that references the context
    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();
    eval_list_.clear();

    // Clear the context pointer
    if (!ctx_)
        delete ctx_;
    ctx_ = nullptr;

    // Mark as uninitialized
    initialized_ = false;
}

void Simulator::initialize() {
    CHDBG_FUNC();

    // 检查context是否有效
    if (initialized_ || disconnected_ || !ctx_) {
        update_instruction_pointers();
        return;
    }

    eval_list_ = ctx_->get_eval_list();
    CHDBG("Retrieved eval_list with %zu nodes", eval_list_.size());

    instr_map_.clear();
    instr_cache_.clear();
    data_map_.clear();

    // 清空所有分类指令列表
    default_clock_instr_ = nullptr;
    other_clock_instr_list_.clear();
    input_instr_list_.clear();
    sequential_instr_list_.clear();
    combinational_instr_list_.clear();

    // 分配数据缓冲区
    for (auto *node : eval_list_) {
        if (disconnected_)
            return;

        CHCHECK_NULL(node, "Null node found in evaluation list, skipping");

        if (!node)
            continue;

        uint32_t node_id = node->id();
        uint32_t size = node->size();
        CHDBG("Processing node ID %u with size %u", node_id, size);

        if (node->type() == ch::core::lnodetype::type_lit) {
            auto *lit_node = static_cast<ch::core::litimpl *>(node);
            data_map_[node_id] = lit_node->value();
            CHDBG("Set literal value for node %u", node_id,
                  data_map_[node_id].to_string_verbose().c_str());
        } else {
            data_map_[node_id] = ch::core::sdata_type(0, size);
            CHDBG("Allocated buffer for node %u, width %u, value %s", node_id,
                  size, data_map_[node_id].to_string_verbose().c_str());

            // 如果这是默认时钟节点，则保存其数据指针
            if (ctx_->has_default_clock() &&
                node_id == ctx_->get_default_clock()->id()) {
                default_clock_data_ = &data_map_[node_id];
                *default_clock_data_ = ch::core::sdata_type(0, 1);
                CHDBG("Saved default clock data pointer for node %u", node_id);
            }
        }
    }

    // 第一步：创建所有内存实体指令对象
    for (auto *node : eval_list_) {
        if (!node || disconnected_)
            continue;

        uint32_t node_id = node->id();

        auto instr = node->create_instruction(data_map_);

        if (instr.get()) {
            // 设置指令ID
            instr_map_[node_id] = instr.get();
            instr_cache_[node_id] = std::move(instr);
            CHDBG("Created instruction for node %u", node_id);

            // 根据节点类型分类
            switch (node->type()) {
            case ch::core::lnodetype::type_input:
                input_instr_list_.emplace_back(node_id, instr_map_[node_id]);
                break;
            case ch::core::lnodetype::type_reset:
                reset_instr_list_.emplace_back(node_id, instr_map_[node_id]);
                break;
            case ch::core::lnodetype::type_clock:
                // 区分默认时钟和其他输入节点
                if (ctx_->has_default_clock() &&
                    node_id == ctx_->get_default_clock()->id()) {
                    default_clock_instr_ = instr_map_[node_id];
                } else {
                    other_clock_instr_list_.emplace_back(node_id,
                                                         instr_map_[node_id]);
                }
                break;

            case ch::core::lnodetype::type_reg:
            case ch::core::lnodetype::type_mem_read_port:
            case ch::core::lnodetype::type_mem_write_port:
                sequential_instr_list_.emplace_back(node_id,
                                                    instr_map_[node_id]);
                break;
            default:
                // 其他所有节点归类为组合逻辑节点
                combinational_instr_list_.emplace_back(node_id,
                                                       instr_map_[node_id]);
                break;
            }

        } else {
            CHDBG("No instruction created for node %u (type: %s)", node_id,
                  ch::core::to_string(node->type()));
        }
    }

    if (sequential_instr_list_.empty()) {
        default_clock_instr_ = nullptr;
    }

    // 第二步：初始化内存端口指令对象，传递内存指令对象指针
    for (auto *node : eval_list_) {
        if (!node || disconnected_)
            continue;

        // 处理内存端口节点
        if (node->type() == ch::core::lnodetype::type_mem_read_port ||
            node->type() == ch::core::lnodetype::type_mem_write_port) {

            uint32_t node_id = node->id();
            auto it = instr_map_.find(node_id);
            if (it != instr_map_.end()) {
                // 获取父内存节点ID
                if (node->type() == ch::core::lnodetype::type_mem_read_port) {
                    auto *port_node =
                        static_cast<ch::core::mem_read_port_impl *>(node);
                    uint32_t parent_mem_id = port_node->parent()->id();

                    // 查找对应的内存指令对象
                    auto mem_it = instr_map_.find(parent_mem_id);
                    if (mem_it == instr_map_.end()) {
                        CHERROR(
                            "Simulator find a memport without mem instruction");
                        return;
                    }
                    // 初始化端口指令对象
                    if (auto *async_read_port =
                            dynamic_cast<instr_mem_async_read_port *>(
                                it->second)) {
                        async_read_port->set_mem_ptr(
                            reinterpret_cast<ch::instr_mem *>(mem_it->second));
                    } else if (auto *sync_read_port =
                                   dynamic_cast<instr_mem_sync_read_port *>(
                                       it->second)) {
                        sync_read_port->set_mem_ptr(
                            reinterpret_cast<ch::instr_mem *>(mem_it->second));
                    } else {
                        CHERROR("Simulator find a memport with invalid type ");
                        return;
                    }
                } else if (node->type() ==
                           ch::core::lnodetype::type_mem_write_port) {
                    auto *port_node =
                        static_cast<ch::core::mem_write_port_impl *>(node);
                    uint32_t parent_mem_id = port_node->parent()->id();

                    // 查找对应的内存指令对象
                    auto mem_it = instr_map_.find(parent_mem_id);
                    if (mem_it == instr_map_.end()) {
                        CHERROR("Simulator not initialized or disconnected");
                        return;
                    }
                    // 初始化端口指令对象
                    if (auto *write_port =
                            dynamic_cast<instr_mem_write_port *>(it->second)) {
                        write_port->set_mem_ptr(
                            reinterpret_cast<ch::instr_mem *>(mem_it->second));
                    } else {
                        CHERROR("Simulator find a memport with invalid type ");
                        return;
                    }
                }
            }
        }
    }

    initialized_ = true;
    CHINFO("Simulator initialization completed successfully");
}

void Simulator::update_instruction_pointers() {
    CHDBG_FUNC();

    if (disconnected_)
        return;

    instr_map_.clear();
    for (const auto &pair : instr_cache_) {
        if (disconnected_)
            return;
        instr_map_[pair.first] = pair.second.get();
    }
    CHDBG("Updated %zu instruction pointers", instr_map_.size());
}

void Simulator::eval_sequential() {
    CHDBG_FUNC();

    for (const auto &[node_id, instr] : sequential_instr_list_) {
        instr->eval();
        CHDBG("Evaluating sequential instruction for node %u, %s", node_id,
              data_map_[node_id].to_string_verbose().c_str());
    }

    CHDBG("Evaluation sequential completed");
}

void Simulator::eval_combinational() {
    CHDBG_FUNC();
    // 执行输入节点指令
    for (const auto &[node_id, instr] : input_instr_list_) {
        instr->eval();
        CHDBG("Evaluating input instruction for node %u: %s", node_id,
              data_map_[node_id].to_string_verbose().c_str());
    }

    // 执行组合逻辑节点
    for (const auto &[node_id, instr] : combinational_instr_list_) {
        instr->eval();
        CHDBG("Evaluating combinational instruction for node %u: %s", node_id,
              data_map_[node_id].to_string_verbose().c_str());
    }

    CHDBG("Evaluation combinational completed");
}

void Simulator::tick() {
    CHDBG_FUNC();

    // 检查context是否有效
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }

    CHINFO("ticks count: %u", ticks_++);

    eval_combinational();

    if (default_clock_instr_) {
        eval();
        eval();
    }
}

void Simulator::eval() {
    CHDBG_FUNC();

    // 1. 首先执行默认时钟指令以更新时钟边沿状态
    CHDBG("Evaluating default clock instruction");
    default_clock_instr_->eval();

    // 2. 然后执行其他时钟指令
    for (const auto &[node_id, instr] : other_clock_instr_list_) {
        CHDBG("Evaluating other clock instruction for node %u", node_id);
        instr->eval();
    }

    // 对于包含时序逻辑的电路，按照特定顺序执行各类节点
    // 如果时钟为0，执行组合逻辑；如果时钟为1，执行时序逻辑
    if (default_clock_data_->is_zero()) {
        eval_combinational();
    } else {
        eval_sequential();
    }

    CHDBG("Executed post-clock eval based on clock state %llu",
          static_cast<unsigned long long>(
              static_cast<uint64_t>(*default_clock_data_)));
}

void Simulator::tick(size_t count) {
    CHDBG_FUNC();
    CHDBG_VAR(count);

    if (count == 0) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        if (disconnected_)
            return;
        CHDBG("Tick %zu/%zu", i + 1, count);
        tick();
    }
}

void Simulator::reset() {
    CHDBG_FUNC();

    // 检查context是否有效
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }

    for (auto &pair : data_map_) {
        if (disconnected_)
            return;
        uint32_t node_id = pair.first;
        auto *node = ctx_->get_node_by_id(node_id);
        if (node && node->type() != ch::core::lnodetype::type_lit) {
            pair.second = ch::core::sdata_type(0, pair.second.bitwidth());
        }
    }
    CHDBG("Simulator state reset completed");
}

const ch::core::sdata_type &
Simulator::get_value_by_name(const std::string &name) const {
    CHDBG_FUNC();
    CHDBG("Looking for node with name: %s", name.c_str());

    // 检查context是否有效
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        // Return a safe empty value instead of crashing
        static ch::core::sdata_type empty_value(0, 1);
        return empty_value;
    }

    if (name.empty()) {
        CHERROR("Node name cannot be empty");
        static ch::core::sdata_type empty_value(0, 1);
        return empty_value;
    }

    // 查找节点
    for (const auto &node_ptr : ctx_->get_nodes()) {
        if (!node_ptr || disconnected_)
            continue;
        if (node_ptr && node_ptr->name() == name) {
            uint32_t node_id = node_ptr->id();
            auto it = data_map_.find(node_id);
            if (it != data_map_.end()) {
                return it->second;
            }
            CHWARN("Data not found for node '%s' with ID %u", name.c_str(),
                   node_id);
            static ch::core::sdata_type empty_value(0, 1);
            return empty_value;
        }
    }

    CHWARN("Node with name '%s' not found", name.c_str());
    static ch::core::sdata_type empty_value(0, 1);
    return empty_value;
}

} // namespace ch