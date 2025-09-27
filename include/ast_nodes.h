#ifndef AST_NODES_H
#define AST_NODES_H

// Include lnodeimpl first, which now includes context.h and types.h
#include "lnodeimpl.h"
// #include "core/context.h" // Already included via lnodeimpl.h
// #include "types.h"        // Already included via context.h

#include <string>
#include <source_location> // C++20

namespace ch { namespace core {

// --- Specific IR Node Implementations ---

// --- regimpl: Register Node (参考 DESIGN_phase2.md 和 cash) ---
class regimpl : public lnodeimpl {
public:
    // Constructor: Requires id, size, clock domain, reset, etc.
    // The 'next' source is typically the 0th source (src(0))
    // Initial value might be handled by a preceding literal node or stored separately.
    regimpl(uint32_t id, uint32_t size, uint32_t cd, lnodeimpl* rst, lnodeimpl* clk_en,
            lnodeimpl* rst_val, lnodeimpl* next, lnodeimpl* init_val, // init_val as a source node
            const std::string& name, const std::source_location& sloc, context* ctx) // Add context* parameter
        : lnodeimpl(id, lnodetype::type_reg, size, ctx, name, sloc), // Pass context* to base
          cd_(cd), rst_(rst), clk_en_(clk_en), rst_val_(rst_val) {
        // Add sources: initial value first (if provided), then next value source.
        // DESIGN_phase2.md showed init_val passed to create_node, implying it's part of the node state.
        // Let's add init_val as src(0) and next as src(1) for clarity in this example.
        if (init_val) add_src(init_val); // src(0): initial value
        if (next) add_src(next);         // src(1): next value (if provided during construction)
        // The simulation engine needs to know how to handle src(0) vs src(1) for a regimpl.
        // During describe, reg->next = ... connects the RHS to src(1) (or replaces src(1)).
    }

    // Accessor for clock domain index (or pointer if stored differently)
    uint32_t cd() const { return cd_; }
    lnodeimpl* rst() const { return rst_; }
    lnodeimpl* clk_en() const { return clk_en_; }
    lnodeimpl* rst_val() const { return rst_val_; }

    // Setter for the 'next' source (as used in reg->next = ... logic)
    // DESIGN_phase2.md showed: reg->add_src(next_node);
    void set_next(lnodeimpl* next) {
        if (next) {
            if (num_srcs() > 1) {
                // Replace the 'next' source at index 1
                set_src(1, next);
            } else if (num_srcs() == 1) {
                // If only init_val exists (src(0)), add next as src(1)
                add_src(next);
            } else {
                // If no sources exist, add next as src(0) - this is less likely after construction with init_val
                add_src(next);
            }
        }
    }

    // Getter for the 'next' source (typically src(1) if init_val is src(0))
    lnodeimpl* get_next() const {
        if (num_srcs() > 1) {
            return src(1);
        }
        return nullptr; // No 'next' source connected yet or only init_val exists
    }

    // Getter for the 'initial value' source (typically src(0))
    lnodeimpl* get_init_val() const {
        if (num_srcs() > 0) {
            return src(0);
        }
        return nullptr; // No initial value source
    }

private:
    uint32_t cd_;           // Clock domain index (借鉴 cash concept)
    lnodeimpl* rst_;        // Reset signal node (借鉴 cash concept)
    lnodeimpl* clk_en_;     // Clock enable signal node (借鉴 cash concept)
    lnodeimpl* rst_val_;    // Reset value node (借鉴 cash concept)
    // lnodeimpl* init_val_; // Initial value node (could be src(0) or stored separately)
};

// --- opimpl: Operation Node (e.g., Add, Sub, And, Or) (参考 DESIGN_phase2.md 和 cash) ---
class opimpl : public lnodeimpl {
public:
    // Constructor: Requires id, operation type, size, signedness, and source operands
    opimpl(uint32_t id, uint32_t size, ch_op op, bool is_signed, lnodeimpl* lhs, lnodeimpl* rhs,
           const std::string& name, const std::source_location& sloc, context* ctx) // Add context* parameter
        : lnodeimpl(id, lnodetype::type_op, size, ctx, name, sloc), // Pass context* to base
          op_(op), is_signed_(is_signed) {
        // Add source operands (lhs and rhs)
        if (lhs) add_src(lhs); // src(0): left hand side
        if (rhs) add_src(rhs); // src(1): right hand side
        // The order of adding sources (lhs first, then rhs) is important for operations.
    }

    ch_op op() const { return op_; }
    bool is_signed() const { return is_signed_; }

    // Accessors for left and right hand side sources (assuming they are src(0) and src(1))
    lnodeimpl* lhs() const { return src(0); }
    lnodeimpl* rhs() const { return src(1); }

private:
    ch_op op_;          // Type of operation (add, sub, etc.) (借鉴 cash)
    bool is_signed_;    // Whether the operation treats inputs as signed (借鉴 cash)
};

// --- proxyimpl: Proxy Node (参考 DESIGN_phase2.md 和 cash) ---
class proxyimpl : public lnodeimpl {
public:
    // Constructor 1: Create proxy without initial source
    proxyimpl(uint32_t id, uint32_t size, const std::string& name, const std::source_location& sloc, context* ctx) // Add context* parameter
        : lnodeimpl(id, lnodetype::type_proxy, size, ctx, name, sloc) {} // Pass context* to base

    // Constructor 2: Create proxy and connect it to a source node
    proxyimpl(uint32_t id, lnodeimpl* src, const std::string& name, const std::source_location& sloc, context* ctx) // Add context* parameter
        : lnodeimpl(id, lnodetype::type_proxy, src->size(), ctx, name, sloc) { // Pass context* to base
        if (src) {
            add_src(src); // Connect this proxy to the given source (src(0))
        }
    }

    // Method to connect this proxy to a source (useful during construction or modification)
    // DESIGN_phase2.md showed: next->write(0, reg, 0, init.size(), sloc);
    void write(int dst_start_bit, lnodeimpl* src_node, int src_start_bit, int bit_count, const std::source_location& sloc) {
         // This method conceptually copies bits from src_node to this proxy's value.
         // In the IR graph, it often means making src_node the source of this proxy.
         if (src_node) {
             if (num_srcs() > 0) {
                 // If a source already exists, replace it.
                 set_src(0, src_node);
             } else {
                 add_src(src_node);
             }
         }
         // The bit manipulation aspects (dst_start_bit, src_start_bit, bit_count)
         // might be handled during simulation or code generation, not necessarily in the basic graph connection.
    }
};

// --- inputimpl ---
class inputimpl : public lnodeimpl {
public:
    // Constructor: size + initial value + name
    inputimpl(uint32_t id, uint32_t size, const sdata_type& init_val,
              const std::string& name, const std::source_location& sloc, context* ctx)
        : lnodeimpl(id, lnodetype::type_input, size, ctx, name, sloc)
        , value_(init_val) {}

    const sdata_type& value() const { return value_; }
    void set_value(const sdata_type& val) { value_ = val; }

    // Optional: record driver (for multi-module connection)
    void set_driver(lnodeimpl* drv) { driver_ = drv; }
    lnodeimpl* driver() const { return driver_; }

private:
    sdata_type value_;
    lnodeimpl* driver_ = nullptr; // 驱动源（来自父模块 output）
};

// --- outputimpl ---
class outputimpl : public lnodeimpl {
public:
    // Constructor: size + src + initial value + name
    outputimpl(uint32_t id, uint32_t size, lnodeimpl* src, const sdata_type& init_val,
               const std::string& name, const std::source_location& sloc, context* ctx)
        : lnodeimpl(id, lnodetype::type_output, size, ctx, name, sloc)
        , value_(init_val) {
        if (src) add_src(src);
    }

    const sdata_type& value() const { return value_; }
    void set_value(const sdata_type& val) { value_ = val; }

    lnodeimpl* src_driver() const { return src(0); }

private:
    sdata_type value_;
};

// --- litimpl: Literal Constant Node (参考 DESIGN_phase2.md 和 cash) ---
class litimpl : public lnodeimpl {
public:
    // Constructor: Requires id, the constant value object and source location
    // The size comes from the value object itself.
    litimpl(uint32_t id, const sdata_type& value, const std::string& name, const std::source_location& sloc, context* ctx) // Add context* parameter
        : lnodeimpl(id, lnodetype::type_lit, value.bitwidth(), ctx, name, sloc), // Pass context* to base
          value_(value) {
        // Literals typically have no source nodes (num_srcs() == 0).
        // The value is held internally.
    }

    const sdata_type& value() const { return value_; }
    // Helper to check if the literal is zero
    bool is_zero() const { return value_.is_zero(); }

    // Override is_const to return true
    bool is_const() const override { return true; }

    // Override equals for literals
    bool equals(const lnodeimpl& other) const override {
        if (!lnodeimpl::equals(other)) return false; // Check base properties (type, size, name, srcs)
        // The lnodeimpl::equals already checked srcs, which should be empty for literals.
        // Now check if the other node is also a litimpl and compare values.
        if (auto* other_lit = dynamic_cast<const litimpl*>(&other)) {
            // Compare the sdata_type values (assuming sdata_type has operator==)
            return this->value_ == other_lit->value_;
        }
        // If other is not a litimpl, they cannot be equal even if base props match.
        // This handles cases where two nodes have same base props but different derived types.
        return false;
    }

private:
    sdata_type value_; // The actual constant value (e.g., 2, 1 in the Counter example) (借鉴 cash)
};

}} // namespace ch::core

#endif // AST_NODES_H
