#ifndef AST_NODES_H
#define AST_NODES_H

#include "lnodeimpl.h"
#include "types.h"
#include <memory>
#include <source_location>
#include <string>

// 只前向声明，不在头文件中包含具体指令头文件

namespace ch::core {

// Forward declare missing types (to avoid including their headers)
class clockimpl;
class resetimpl;
class selectimpl;
class memimpl;
class mem_read_port_impl;
class mem_write_port_impl;
class moduleimpl;
class moduleportimpl;
class ioimpl;
class cdimpl;
class udfimpl;
class udfportimpl;
class proxyimpl; // Add forward declaration for proxyimpl

// --- regimpl ---
class regimpl : public lnodeimpl {
public:
    regimpl(uint32_t id, uint32_t size, uint32_t cd, lnodeimpl *rst,
            lnodeimpl *clk_en, lnodeimpl *rst_val, lnodeimpl *next,
            lnodeimpl *init_val, const std::string &name,
            const std::source_location &sloc, context *ctx)
        : lnodeimpl(id, lnodetype::type_reg, size, ctx, name, sloc), cd_(cd),
          rst_(rst), clk_en_(clk_en), rst_val_(rst_val), init_val_(init_val) {
        if (next)
            add_src(next);
    }

    uint32_t cd() const { return cd_; }
    lnodeimpl *rst() const { return rst_; }
    lnodeimpl *clk_en() const { return clk_en_; }
    lnodeimpl *rst_val() const { return rst_val_; }
    lnodeimpl *get_init_val() const { return init_val_; }

    void set_next(lnodeimpl *next) {
        if (!next)
            return;
        if (num_srcs() > 0) {
            set_src(0, next);
        } else {
            add_src(next);
        }
    }

    lnodeimpl *get_next() const { return num_srcs() > 0 ? src(0) : nullptr; }

    // Explicit link to proxy node
    void set_proxy(proxyimpl *proxy) { proxy_ = proxy; }
    proxyimpl *get_proxy() const { return proxy_; }

    // 声明创建指令的方法，实现在cpp文件中
    std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const override;

private:
    uint32_t cd_;
    lnodeimpl *rst_ = nullptr;
    lnodeimpl *clk_en_ = nullptr;
    lnodeimpl *rst_val_ = nullptr;
    lnodeimpl *init_val_ = nullptr; // Store init_val as dedicated member
    proxyimpl *proxy_ = nullptr;    // Explicit link to proxy node
};

// --- opimpl ---
class opimpl : public lnodeimpl {
public:
    opimpl(uint32_t id, uint32_t size, ch_op op, bool is_signed, lnodeimpl *lhs,
           lnodeimpl *rhs, const std::string &name,
           const std::source_location &sloc, context *ctx)
        : lnodeimpl(id, lnodetype::type_op, size, ctx, name, sloc), op_(op),
          is_signed_(is_signed) {
        if (lhs)
            add_src(lhs);
        if (rhs)
            add_src(rhs);
    }

    opimpl(uint32_t id, uint32_t size, ch_op op, bool is_signed,
           lnodeimpl *operand, const std::string &name,
           const std::source_location &sloc, context *ctx)
        : lnodeimpl(id, lnodetype::type_op, size, ctx, name, sloc), op_(op),
          is_signed_(is_signed) {
        if (operand)
            add_src(operand);
    }

    ch_op op() const { return op_; }
    bool is_signed() const { return is_signed_; }
    lnodeimpl *lhs() const { return src(0); }
    lnodeimpl *rhs() const { return src(1); }

    // 声明创建指令的方法，实现在cpp文件中
    std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const override;

private:
    ch_op op_;
    bool is_signed_;
};

// --- proxyimpl ---
class proxyimpl : public lnodeimpl {
public:
    proxyimpl(uint32_t id, lnodeimpl *src, const std::string &name,
              const std::source_location &sloc, context *ctx)
        : lnodeimpl(id, lnodetype::type_proxy, src ? src->size() : 0, ctx, name,
                    sloc) {
        if (src)
            add_src(src);
    }

    proxyimpl(uint32_t id, uint32_t size, const std::string &name,
              const std::source_location &sloc, context *ctx)
        : lnodeimpl(id, lnodetype::type_proxy, size, ctx, name, sloc) {}

    void write(int /*dst_start_bit*/, lnodeimpl *src_node,
               int /*src_start_bit*/, int /*bit_count*/,
               const std::source_location & /*sloc*/) {
        if (src_node) {
            if (num_srcs() > 0) {
                set_src(0, src_node);
            } else {
                add_src(src_node);
            }
        }
    }

    // 声明创建指令的方法，实现在cpp文件中
    std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const override;
};

// --- inputimpl ---
class inputimpl : public lnodeimpl {
public:
    inputimpl(uint32_t id, uint32_t size, const sdata_type &init_val,
              const std::string &name, const std::source_location &sloc,
              context *ctx)
        : lnodeimpl(id, lnodetype::type_input, size, ctx, name, sloc),
          value_(init_val) {}

    const sdata_type &value() const { return value_; }
    void set_value(const sdata_type &val) { value_ = val; }
    void set_driver(lnodeimpl *drv) {
        driver_ = drv;
        add_src(drv);
    }
    lnodeimpl *driver() const { return driver_; }

    // 声明创建指令的方法，实现在cpp文件中
    std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const override;

private:
    sdata_type value_;
    lnodeimpl *driver_ = nullptr;
};

// --- outputimpl ---
class outputimpl : public lnodeimpl {
public:
    outputimpl(uint32_t id, uint32_t size, lnodeimpl *src,
               const sdata_type &init_val, const std::string &name,
               const std::source_location &sloc, context *ctx)
        : lnodeimpl(id, lnodetype::type_output, size, ctx, name, sloc),
          value_(init_val) {
        if (src)
            add_src(src);
    }

    const sdata_type &value() const { return value_; }
    void set_value(const sdata_type &val) { value_ = val; }
    lnodeimpl *src_driver() const { return src(0); }

    // 声明创建指令的方法，实现在cpp文件中
    std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const override;

private:
    sdata_type value_;
};

// --- litimpl ---
class litimpl : public lnodeimpl {
public:
    litimpl(uint32_t id, const sdata_type &value, const std::string &name,
            const std::source_location &sloc, context *ctx)
        : lnodeimpl(id, lnodetype::type_lit, value.bitwidth(), ctx, name, sloc),
          value_(value) {}

    const sdata_type &value() const { return value_; }
    bool is_zero() const { return value_.is_zero(); }
    bool is_const() const override { return true; }

    bool equals(const lnodeimpl &other) const override {
        if (this->type() != other.type())
            return false;
        if (!lnodeimpl::equals(other))
            return false;
        auto *other_lit = static_cast<const litimpl *>(&other);
        return this->value_ == other_lit->value_;
    }

    // 文字节点不需要仿真指令（值已经在数据缓冲区中）
    std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const override {
        return nullptr; // 文字节点不需要执行指令
    }

private:
    sdata_type value_;
};

inline bool is_litimpl_one(lnodeimpl *node) {
    if (!node)
        return false;
    if (!node->is_const())
        return false;
    auto *lit_node = static_cast<litimpl *>(node);
    return lit_node->value().is_one();
}

class muximpl : public lnodeimpl {
public:
    muximpl(uint32_t id, uint32_t size, lnodeimpl *cond, lnodeimpl *true_val,
            lnodeimpl *false_val, const std::string &name,
            const std::source_location &sloc, context *ctx)
        : lnodeimpl(id, lnodetype::type_mux, size, ctx, name, sloc) {
        if (cond)
            add_src(cond);
        if (true_val)
            add_src(true_val);
        if (false_val)
            add_src(false_val);
    }

    lnodeimpl *condition() const { return src(0); }
    lnodeimpl *true_value() const { return src(1); }
    lnodeimpl *false_value() const { return src(2); }

    // 声明创建指令的方法，实现在cpp文件中
    std::unique_ptr<ch::instr_base>
    create_instruction(ch::data_map_t &data_map) const override;
};

} // namespace ch::core

#include "clockimpl.h"
#include "resetimpl.h"

#endif // AST_NODES_H
