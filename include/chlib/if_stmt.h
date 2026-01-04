#ifndef CH_CHLIB_IF_STMT_H
#define CH_CHLIB_IF_STMT_H

#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/literal.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "core/node_builder.h"
#include "core/operators.h"
#include "core/reg.h"
#include "core/traits.h"
#include "core/uint.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace ch::core;

namespace chlib {

// 语句块风格的条件执行 - 通过上下文管理实现
class conditional_block {
private:
    struct branch_info {
        ch_bool condition;
        std::function<void()> body;
        bool is_else = false;
    };

    std::vector<branch_info> branches_;
    bool finalized_ = false;
    std::source_location creation_loc_;

    // 用于在分支中捕获赋值的静态变量
    static thread_local conditional_block *active_block_;
    static thread_local size_t current_branch_idx_;

public:
    conditional_block(
        const ch_bool &condition,
        std::source_location sloc = std::source_location::current())
        : creation_loc_(sloc) {
        branches_.push_back({condition, nullptr, false});
        active_block_ = this;
        current_branch_idx_ = 0;
    }

    ~conditional_block() {
        if (!finalized_ && !branches_.empty()) {
            end();
        }
        if (active_block_ == this) {
            active_block_ = nullptr;
        }
    }

    conditional_block &then(std::function<void()> body) {
        if (branches_.empty()) {
            throw std::runtime_error("No condition for then branch");
        }
        branches_.back().body = std::move(body);
        return *this;
    }

    conditional_block &elif (const ch_bool &condition) {
        if (branches_.back().is_else) {
            throw std::runtime_error("Cannot add elif after else");
        }
        branches_.push_back({condition, nullptr, false});
        return *this;
    }

    conditional_block &else_() {
        if (branches_.back().is_else) {
            throw std::runtime_error("Else branch already added");
        }
        branches_.push_back({ch_bool(true), nullptr, true});
        return *this;
    }

    void end() {
        if (finalized_)
            return;
        finalized_ = true;

        validate();

        // 执行所有分支以捕获所有赋值
        for (size_t i = 0; i < branches_.size(); ++i) {
            current_branch_idx_ = i;
            if (branches_[i].body) {
                branches_[i].body();
            }
        }

        active_block_ = nullptr;
    }

    // 静态方法，用于在赋值操作中检测是否在条件块中
    static bool is_active() {
        return active_block_ != nullptr && !active_block_->finalized_;
    }

    // 静态方法，用于捕获赋值
    template <typename T>
    static void capture_assignment(lnodeimpl *target, const T &value) {
        if (!is_active())
            return;

        // 这里我们不直接处理赋值，而是依赖于用户代码在分支结束后正确构建select树
        // 因为CppHDL的赋值机制是通过操作符重载实现的，我们不能直接拦截
    }

private:
    void validate() {
        if (branches_.empty()) {
            throw std::runtime_error("Empty if block");
        }

        for (size_t i = 0; i < branches_.size(); ++i) {
            if (!branches_[i].body) {
                std::string msg =
                    "Branch at position " + std::to_string(i) + " has no body";
                throw std::runtime_error(msg);
            }
        }
    }
};

// 定义静态成员
thread_local conditional_block *conditional_block::active_block_ = nullptr;
thread_local size_t conditional_block::current_branch_idx_ = 0;

// 便捷函数：创建语句块风格的条件执行
inline auto _if(const ch_bool &condition) {
    return conditional_block(condition);
}

// 在寄存器赋值操作中检测条件块的辅助类
template <typename T> class conditional_reg_assignment {
private:
    ch_reg<T> &reg_ref_;
    bool in_conditional_block_;

public:
    conditional_reg_assignment(ch_reg<T> &reg) : reg_ref_(reg) {
        in_conditional_block_ = conditional_block::is_active();
    }

    // next代理，用于支持条件赋值
    struct next_proxy {
        ch_reg<T> &reg_ref;

        template <typename U> void operator=(const U &value) {
            // 在条件块中，我们不直接赋值，而是构建select树
            if (conditional_block::is_active()) {
                // 这里我们返回一个代理对象，让赋值延迟到条件块结束
                // 但实际实现需要更复杂的机制
            } else {
                // 直接赋值到寄存器的next
                reg_ref->next = value;
            }
        }
    };

    next_proxy next() { return next_proxy{reg_ref_}; }
};

// 条件寄存器赋值的便捷函数
template <typename T> auto conditional_reg_assign(ch_reg<T> &reg) {
    return conditional_reg_assignment<T>(reg);
}

} // namespace chlib

#endif // CH_CHLIB_IF_STMT_H