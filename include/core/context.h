#ifndef CH_CORE_CONTEXT_H
#define CH_CORE_CONTEXT_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <cstdint>
#include <functional>
#include <iostream>
#include <source_location> // C++20, or use a custom fallback

#include "types.h" // Include types.h first

namespace ch { namespace core {
    class lnodeimpl;
    class litimpl;
    class inputimpl;
    class outputimpl;
    class regimpl;
    class opimpl;
    class proxyimpl;
    class selectimpl;
    class memimpl;
    class moduleimpl;
    class moduleportimpl;
    class ioimpl;
    class cdimpl;
    class udfimpl;
    class udfportimpl;
    class clockimpl;
    class resetimpl;
    enum class ch_op;
    class context;

    extern context* ctx_curr_;

    // RAII helper for context switching
    class ctx_swap {
    public:
        explicit ctx_swap(context* new_ctx);
        ~ctx_swap();
        ctx_swap(const ctx_swap&) = delete;
        ctx_swap& operator=(const ctx_swap&) = delete;

    private:
        context* old_ctx_;
    };

    extern bool debug_context_lifetime;

    class context {
    public:
        explicit context(const std::string& name = "unnamed", context* parent = nullptr);
        ~context();

        template <typename T, typename... Args>
        T* create_node(Args&&... args) {
            static_assert(std::is_base_of_v<lnodeimpl, T>, "T must derive from lnodeimpl");
            // Generate a unique ID for the new node
            uint32_t new_id = next_node_id_++;
            auto node = std::make_unique<T>(new_id, std::forward<Args>(args)..., this); // Pass ID, other args, and *this* context
            T* raw_ptr = node.get();
            nodes_.push_back(std::move(node)); // Store in owned list
            node_map_[raw_ptr->id()] = raw_ptr; // Map ID to raw pointer for quick lookup
            if (debug_context_lifetime) { // --- NEW: Debug print ---
                std::cout << "[context::create_node] Created node ID " << new_id << " (" << raw_ptr->name() << ") of type " << static_cast<int>(raw_ptr->type()) << " in context " << this << std::endl;
            }
            return raw_ptr;
        }

        uint32_t node_id() { return next_node_id_++; }

        // --- Factory methods for specific node types (example) ---
        litimpl* create_literal(const sdata_type& value, const std::string& name = "literal", const std::source_location& sloc = std::source_location::current());
        inputimpl* create_input(uint32_t size, const std::string& name, const std::source_location& sloc = std::source_location::current());
        outputimpl* create_output(uint32_t size, const std::string& name, const std::source_location& sloc = std::source_location::current());

        // --- Accessor Methods for Simulator/Compiler ---
        const std::vector<std::unique_ptr<lnodeimpl>>& get_nodes() const { return nodes_; }

        // Get the list of output nodes (likely needed by simulator to build eval_list)
        // const std::vector<outputimpl*>& get_outputs() const { return outputs_; } // Example

        lnodeimpl* get_node_by_id(uint32_t id) const {
            auto it = node_map_.find(id);
            return (it != node_map_.end()) ? it->second : nullptr;
        }

        // --- Topological Sort for Simulation Order ---
        std::vector<lnodeimpl*> get_eval_list() const;

        void print_debug_info() const {
            std::cout << "[context::print_debug_info] Context " << this << ", name: " << name_ << ", nodes: " << nodes_.size() << std::endl;
        }

        // --- Clock/Reset Management ---
        clockimpl* current_clock(const std::source_location& sloc);
        resetimpl* current_reset(const std::source_location& sloc);
        void set_current_clock(clockimpl* clk);
        void set_current_reset(resetimpl* rst);

    private:
        std::vector<std::unique_ptr<lnodeimpl>> nodes_; // Owns all nodes in this context
        std::unordered_map<uint32_t, lnodeimpl*> node_map_; // Quick lookup: ID -> Node*
        uint32_t next_node_id_ = 0; // Counter for generating unique node IDs
        // std::vector<outputimpl*> outputs_; // List of output ports for simulation (example)

        // State for managing current clock/reset during describe()
        clockimpl* current_clock_ = nullptr;
        resetimpl* current_reset_ = nullptr;

        // --- Helper for Topological Sort ---
        void topological_sort_visit(lnodeimpl* node, std::vector<lnodeimpl*>& sorted,
                                    std::unordered_map<lnodeimpl*, bool>& visited,
                                    std::unordered_map<lnodeimpl*, bool>& temp_mark) const;

        // --- Initialization Logic ---
        void init(/* optional: name, sloc */);

    private:
        std::string name_;
        context* parent_ = nullptr;
    };


}} // namespace ch::core

#endif // CH_CORE_CONTEXT_H
