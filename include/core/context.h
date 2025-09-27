#ifndef CH_CORE_CONTEXT_H
#define CH_CORE_CONTEXT_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <cstdint>
#include <functional> // for std::hash
#include <source_location> // C++20, or use a custom fallback

// Forward declarations
// struct source_location; // Use std::source_location instead
// class lnodeimpl;       // The base class for all IR nodes - Forward declared implicitly by being in ch::core namespace now
// class outputimpl; // Forward declare if get_outputs returns this type specifically - Now defined in ch::core namespace
// class regimpl;
// class opimpl;
// class inputimpl;
// class proxyimpl;
// class litimpl;
// class memimpl;
// class clockimpl;
// class resetimpl;
// struct sdata_type; // Forward declare sdata_type - Defined in types.h

#include "types.h" // Include types.h first

namespace ch { namespace core {

    // Forward declare specific node types needed by context methods
    class lnodeimpl;
    class outputimpl;
    class regimpl;
    class opimpl;
    class inputimpl;
    class proxyimpl;
    class litimpl;
    class memimpl;
    class clockimpl;
    class resetimpl;

    class context;

    // --- Forward Declaration for Context Switching ---
    // This is the global function used by deviceimpl to switch contexts.
    // Defined in src/core/context.cpp
    context* ctx_swap(context* new_ctx);

    // --- Context Class Definition ---
    class context {
    public:
        // Constructor: Initializes the context, sets up initial state.
        //context(/* optional: const std::string& name, const std::source_location& sloc */);
        explicit context(const std::string& name = "unnamed", context* parent = nullptr);
        // Destructor: Cleans up all nodes owned by this context.
        ~context();

        // --- Node Creation Methods ---
        // Generic template to create nodes of specific types.
        // This is the primary way to add nodes to the context's graph.
        template <typename T, typename... Args>
        T* create_node(Args&&... args) {
            static_assert(std::is_base_of_v<lnodeimpl, T>, "T must derive from lnodeimpl");
            // Generate a unique ID for the new node
            uint32_t new_id = next_node_id_++;
            auto node = std::make_unique<T>(new_id, std::forward<Args>(args)..., this); // Pass ID, other args, and *this* context
            T* raw_ptr = node.get();
            nodes_.push_back(std::move(node)); // Store in owned list
            node_map_[raw_ptr->id()] = raw_ptr; // Map ID to raw pointer for quick lookup
            return raw_ptr;
        }

        // --- Helper to generate unique node IDs ---
        uint32_t node_id() { return next_node_id_++; }

        // --- Factory methods for specific node types (example) ---
        litimpl* create_literal(const sdata_type& value, const std::string& name = "literal", const std::source_location& sloc = std::source_location::current());
        inputimpl* create_input(uint32_t size, const std::string& name, const std::source_location& sloc = std::source_location::current());
        outputimpl* create_output(uint32_t size, const std::string& name, const std::source_location& sloc = std::source_location::current());
        // Add other factory methods like create_input, create_reg, etc. here.

        // --- Accessor Methods for Simulator/Compiler ---
        // Get the list of all nodes in this context
        const std::vector<std::unique_ptr<lnodeimpl>>& get_nodes() const { return nodes_; }

        // Get the list of output nodes (likely needed by simulator to build eval_list)
        // const std::vector<outputimpl*>& get_outputs() const { return outputs_; } // Example

        // Get a specific node by its ID (for simulator's instr_map)
        lnodeimpl* get_node_by_id(uint32_t id) const {
            auto it = node_map_.find(id);
            return (it != node_map_.end()) ? it->second : nullptr;
        }

        // --- Topological Sort for Simulation Order ---
        // Generates a list of nodes sorted in topological order for simulation.
        // This ensures dependencies are evaluated before the nodes that use them.
        std::vector<lnodeimpl*> get_eval_list() const;

        // --- Clock/Reset Management ---
        // Methods to manage current clock/reset during describe()
        // These are likely used by ch_reg, ch_mem, etc., during their construction.
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

    // --- Global Context Pointer ---
    // This is the global variable that ctx_swap manipulates.
    // It holds the currently active context.
    extern context* ctx_curr_;

}} // namespace ch::core

#endif // CH_CORE_CONTEXT_H
