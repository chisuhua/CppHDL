// src/simulator.cpp
#include  "simulator.h"
#include  "ast_nodes.h"
#include  "lnodeimpl.h"
#include  "sim/instr_base.h" // Include base instruction class
#include  "sim/instr_op.h"  // Include specific instruction classes (add, sub, etc.)
#include  "sim/instr_reg.h" // Include register instruction class
#include  "sim/instr_proxy.h" // Include proxy instruction class
#include  "sim/instr_io.h"  // Include input/output instruction class (if placeholders exist)
#include  <cassert>
#include  <iostream> // For debugging if needed
#include  <cstring> // For memcpy if needed for initial simple ops

namespace ch {

Simulator::Simulator(ch::core::context* ctx) : ctx_(ctx) {
    std::cout << "[Simulator::ctor] Received context " << ctx_ << std::endl; // --- NEW: Debug print ---
    initialize(); // Perform the compilation step (IR -> Instructions + Buffers)
}
Simulator::~Simulator() = default;  // 👈 现在安全了！

void Simulator::initialize() {
    std::cout << "[Simulator::initialize] Starting initialization for context " << ctx_ << std::endl;
    eval_list_ = ctx_->get_eval_list(); // Get topologically sorted list of IR nodes
    std::cout << "[Simulator::initialize] Retrieved eval_list with " << eval_list_.size() << " nodes." << std::endl;
    instr_map_.clear(); // Clear previous instructions if any
    data_map_.clear();  // Clear previous data buffers if any

    // 1. Allocate data buffers for all nodes in the eval_list
    std::cout << "[Simulator::initialize] Phase 1: Allocating data buffers." << std::endl;
    for (auto* node : eval_list_) {
        std::cout << "[Simulator::initialize] Processing node ID " << node->id() << " (ptr " << node << ")" << std::endl; // --- NEW: Debug print ---
        uint32_t node_id = node->id();
        uint32_t size = node->size();
        sdata_type buffer(0, size); // Create an sdata_type buffer (bitvector) with correct size, initialized to 0
        // Handle initial values for literals
        if (node->type() == ch::core::lnodetype::type_lit) {
            auto* lit_node = static_cast<ch::core::litimpl*>(node);
            std::cout << "[Simulator::initialize] Node " << node_id << " is literal, copying value." << std::endl; // --- NEW: Debug print ---
            // Use the literal's value directly
            buffer = lit_node->value();
        }
        data_map_[node_id] = buffer; // Store the buffer in the map
        std::cout << "[Simulator::initialize] Allocated buffer for node " << node_id << std::endl; // --- NEW: Debug print ---
    }
    std::cout << "[Simulator::ctor] Finished data_map_ initialization loop." << std::endl;

    // 2. Create instruction objects for all nodes in the eval_list
    std::cout << "[Simulator::initialize] Phase 2: Creating instructions." << std::endl;
    for (auto* node : eval_list_) {
        std::cout << "[Simulator::initialize] Creating instruction for node ID " << node->id() << " (ptr " << node << ")" << std::endl; // --- NEW: Debug print ---
        uint32_t node_id = node->id();
        std::unique_ptr<ch::instr_base> instr = nullptr;

        switch (node->type()) {
            case ch::core::lnodetype::type_op: {
                auto* op_node = static_cast<ch::core::opimpl*>(node);
                sdata_type* dst_buf = &data_map_.at(node_id); // Destination buffer for result
                sdata_type* src0_buf = &data_map_.at(op_node->lhs()->id()); // Buffer for LHS operand
                sdata_type* src1_buf = &data_map_.at(op_node->rhs()->id()); // Buffer for RHS operand
                // Create specific instruction based on the operation type
                switch (op_node->op()) {
                    case ch::core::ch_op::add:
                        instr = std::make_unique<ch::instr_op_add>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::sub:
                        instr = std::make_unique<ch::instr_op_sub>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::mul:
                        instr = std::make_unique<ch::instr_op_mul>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::and_:
                        instr = std::make_unique<ch::instr_op_and>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::or_:
                        instr = std::make_unique<ch::instr_op_or>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::xor_:
                        instr = std::make_unique<ch::instr_op_xor>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::not_:
                        // Unary not needs different constructor
                        // Example: instr = std::make_unique<ch::instr_op_not>(dst_buf, node->size(), src0_buf);
                        break;
                    case ch::core::ch_op::eq:
                        instr = std::make_unique<ch::instr_op_eq>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::ne:
                        instr = std::make_unique<ch::instr_op_ne>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::lt:
                        instr = std::make_unique<ch::instr_op_lt>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::le:
                        instr = std::make_unique<ch::instr_op_le>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::gt:
                        instr = std::make_unique<ch::instr_op_gt>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    case ch::core::ch_op::ge:
                        instr = std::make_unique<ch::instr_op_ge>(dst_buf, node->size(), src0_buf, src1_buf);
                        break;
                    // Add cases for other operations as needed (div, mod)
                    default:
                        std::cerr << "[Simulator::initialize] Warning: Unsupported op type: " << static_cast<int>(op_node->op()) << std::endl;
                        break;
                }
                break;
            }
            case ch::core::lnodetype::type_reg: {
                auto* reg_node = static_cast<ch::core::regimpl*>(node);
                // NEW: Get node IDs to pass to the instr_reg constructor
                uint32_t current_node_id = node_id; // ID of the regimpl node itself
                uint32_t next_node_id = -1; // Invalid ID as default
                if (auto* next_node = reg_node->get_next()) {
                    next_node_id = next_node->id(); // ID of the 'next' source node (e.g., add_op proxy)
                }
                // Create instr_reg by passing node IDs
                instr = std::make_unique<ch::instr_reg>(current_node_id, node->size(), next_node_id);
                break;
            }
            case ch::core::lnodetype::type_proxy: {
                // A proxy typically copies the value from its source (src(0)) to its own buffer
                auto* proxy_node = static_cast<ch::core::proxyimpl*>(node);
                sdata_type* dst_buf = &data_map_.at(node_id); // Buffer for the proxy itself
                sdata_type* src_buf = nullptr;
                if (node->num_srcs() > 0) {
                    src_buf = &data_map_.at(node->src(0)->id()); // Buffer of the source node
                }
                // If no source, it's a placeholder or error state
                if (src_buf) {
                     instr = std::make_unique<ch::instr_proxy>(dst_buf, node->size(), src_buf);
                } else {
                    // Handle proxy with no source if possible, maybe just no-op or copy self?
                    // For now, treat as no-op if no source.
                    // instr = std::make_unique<ch::instr_proxy>(dst_buf, node->size(), dst_buf); // Self-assign or zero?
                }
                break;
            }
            case ch::core::lnodetype::type_input: {
                // Input instruction might just hold a pointer to an external value or be a no-op during eval
                // For simulation, input values are typically set by the user before eval() or by external drivers
                // The instr_input could potentially update its buffer from an external source
                sdata_type* dst_buf = &data_map_.at(node_id);
                // instr = std::make_unique<ch::instr_input>(dst_buf, node->size());
                break; // Placeholder - input handling might be separate or before eval()
            }
            case ch::core::lnodetype::type_output: {
                // Output instruction might just copy its source's value to its own buffer, similar to proxy
                // Or it might drive an external output port (if simulation is connected to a testbench)
                auto* output_node = static_cast<ch::core::outputimpl*>(node);
                sdata_type* dst_buf = &data_map_.at(node_id);
                sdata_type* src_buf = nullptr;
                if (node->num_srcs() > 0) {
                    src_buf = &data_map_.at(node->src(0)->id());
                }
                if (src_buf) {
                    // Use proxy instruction for simplicity, or a specific output instruction
                    instr = std::make_unique<ch::instr_proxy>(dst_buf, node->size(), src_buf); // Or specific output instr
                }
                break;
            }
            case ch::core::lnodetype::type_lit: {
                // Literal instruction might be a no-op or just ensure its value is in data_map_
                // No specific eval needed, value is already set during buffer allocation
                // Could potentially create an instr_lit that does nothing or just verifies the value is set.
                // For now, skip creating an instruction for literals.
                break;
            }
            default:
                std::cerr << "[Simulator::initialize] Warning: Unsupported node type: " << static_cast<int>(node->type()) << std::endl;
                break;
        }
        if (instr) {
            instr_map_[node_id] = std::move(instr); // Store the created instruction
            //std::cout << "[Simulator::initialize] Stored instruction for node_id:" << node_id << *node << std::endl;
        }
    }
}

void Simulator::eval() {
    std::cout << "[Simulator::eval] Starting evaluation loop." << std::endl; // --- NEW: Debug print ---
    // Iterate through the topologically sorted list of instructions and execute them
    for (auto* node : eval_list_) {
        std::cout << "[Simulator::eval] Processing node ID " << node->id() << " (ptr " << node << ")" << std::endl; // --- NEW: Debug print ---
        uint32_t node_id = node->id();
        auto it = instr_map_.find(node_id);
        if (it != instr_map_.end() && it->second) {
            std::cout << "[Simulator::eval] Found instruction for node " << node_id << ", calling eval()." << std::endl; // --- NEW: Debug print ---
            it->second->eval(data_map_); // Call the instruction's eval method
        } else {
            // This should ideally not happen if initialize was successful and all nodes have instructions
            std::cerr << "[Simulator::eval] Warning: Instruction not found or null for node ID: " << node_id << std::endl;
            // Depending on the design, this might be acceptable for certain node types (e.g., literals).
        }
    }
    std::cout << "[Simulator::eval] Finished evaluation loop." << std::endl; // --- NEW: Debug print ---
}

void Simulator::tick() {
    std::cout << "[Simulator::tick] Calling eval()." << std::endl; // --- NEW: Debug print ---
    eval(); // Evaluate all combinational logic and prepare next values for registers
    // In CASH-style, the register update often happens implicitly during eval()
    // for `instr_reg` if the `eval` logic copies next to current.
    // If register update is separate, uncomment below:
    // update_registers();
}

uint64_t Simulator::get_value_by_name(const std::string & name) const {
    for (auto & node_ptr : ctx_->get_nodes()) {
        if (node_ptr->name() == name) {
            uint32_t node_id = node_ptr->id();
            auto it = data_map_.find(node_id);
            if (it != data_map_.end()) {
                // Return the first block of the bitvector value
                const auto& bv = it->second.bv_;
                if (bv.num_words() > 0) {
                    return bv.words()[0];
                }
            }
        }
    }
    return 0;
}

} // namespace ch
