/**
 * @file if_stmt.h
 * @brief 语句块风格的条件执行 —— `_if(cond).then(body).elif(cond)...else_()...end()`
 *
 * ⚠️ **语义模型**（**ADR-051**：`docs/adr/ADR-051-chlib-conditional-semantics.md`）：
 *   条件块内的赋值 MUST 被解释为「**在该分支条件下，该目标取该值**」
 *   ⇒ ⇒ 框架把各分支的候选值合成为**一条** `select` 链 ⇒ ⇒ **先匹配者胜**
 *   ⇒ ⇒ 目标只有**一个**写入点 ⇒ `ch_reg` 的**单 next 源**契约**零变化**
 *   ⇒ ⇒ **`core/` 零改动**（拦截点在 `chlib` 层）。
 *
 * ## 用法（**必须**经 `conditional_assign` 捕获）
 *
 * ```cpp
 * ch_reg<ch_uint<8>> count(0_d);
 *
 * _if(io().en)
 *     .then([&] { conditional_assign(count).next() = count + 1_d; })
 *     .else_()
 *     .then([&] { conditional_assign(count).next() = 0_d; })
 *     .end();
 * ```
 *
 * ⚠️ **未走 `conditional_assign` 的普通 `reg->next = v` / `io().x = v`
 *   **不会**被条件化**（= **无条件**生效）⇒ ⇒ 有静态护栏检出
 *   （`scripts/verify_if_stmt_wiring.sh`）。
 *
 * ## 捕获语义（**不是**「条件成立才执行」）
 *
 * `end()` MUST 遍历**所有**分支体以**收集候选值** ⇒ ⇒ 分支体在**建图期**执行
 * ⇒ ⇒ 分支体 MUST **无副作用**（`std::cout` / 文件 IO / 改全局状态）。
 *
 * ## 默认尾部
 *
 * - **寄存器**：无 `else_` 分支 ⇒ 未匹配时**保持**（当前值）；
 * - **输出端口**：MUST 有 `else_` 候选值 ⇒ ⇒ 缺失 ⇒ **构建期抛异常**
 *   （输出**不能**「保持」⇒ 那是**组合环** ⇒ 拓扑排序 CHFATAL ⇒ 难诊断）。
 *
 * ⚠️ 该判据是 **target 级**（被捕获的输出 target 无 `else_` 候选值），
 * **不**是块级（块有 `else_` 但该输出无默认候选 ⇒ 组合环仍可达）。
 */

#pragma once

#include "chlib/guard_contribution.h"
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
#include "utils/logger.h"
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace chlib {

using namespace ch::core;

/**
 * @brief 语句块风格的条件执行（**守卫贡献**模型，ADR-051）
 */
class conditional_block {
private:
    struct branch_info {
        ch_bool condition;
        std::function<void()> body;
        bool is_else = false;
    };

    /// 一个被条件赋值的**目标**（寄存器或输出端口）
    struct target_entry {
        lnodeimpl *target = nullptr;  // 目标节点（reg 的 proxy / output 的 impl）
        lnodeimpl *reg = nullptr;     // 非空 ⇒ 目标是寄存器
        /// 按**分支顺序**追加的候选值（**不含** `else_` 分支）
        std::vector<guard_contribution> contribs;
        bool has_default = false;      // 是否拿到过 `else_` 候选值
        lnodeimpl *default_value = nullptr;
        /// 合成回调：把贡献合成为**一条**链并返回其节点（**宽度相关** ⇒ 模板）
        std::function<lnodeimpl *(const std::vector<guard_contribution> &,
                                  lnodeimpl *, bool)>
            composer;
        /// 提交回调：把合成结果**写入**目标（**唯一**写入点 / 驱动点）
        std::function<void(lnodeimpl *)> committer;
    };

    std::vector<branch_info> branches_;
    std::vector<target_entry> targets_;
    bool finalized_ = false;
    bool walking_ = false;
    size_t current_branch_idx_ = 0;
    std::source_location creation_loc_;

    /// 激活块**栈**（ADR-051 D7：嵌套 ⇒ 内层 `end()` MUST NOT 清掉外层）
    static thread_local std::vector<conditional_block *> block_stack_;

public:
    /// ⚠️ **条件参数 MUST 接受任意硬件操作数**（ADR-051 实施期实测补充）：
    ///    原签名是 `const ch_bool &` ⇒ 而 **`ch_in<ch_bool>` 不能绑到
    ///    `const ch_bool&`**（实测：`invalid initialization of reference`）
    ///    ⇒ ⇒ DSL 对「条件来自输入端口」这个**最自然**的用例**不可用**
    ///    ⇒ ⇒ 改为模板参数 ⇒ `port` / `ch_bool` / 任何 1 位硬件表达式皆可。
    template <typename C>
    explicit conditional_block(const C &condition,
                               std::source_location sloc = std::source_location::current())
        : creation_loc_(sloc) {
        branches_.push_back({ch_bool(to_operand(condition).impl()), nullptr, false});
        block_stack_.push_back(this);
    }

    /// ⚠️ **禁止复制**：复制会让栈里的 `this` 指向**另一个**对象 ⇒ 捕获会落错块
    conditional_block(const conditional_block &) = delete;
    conditional_block &operator=(const conditional_block &) = delete;

    /// ⚠️ `noexcept(false)`：析构函数默认 noexcept ⇒ 析构期抛异常会
    /// `std::terminate` ⇒ 必须显式关闭 ⇒ 否则「输出缺默认」的检出变成 abort
    /// （不可捕获 ⇒ 不可测）。⚠️ **展开期（`uncaught_exceptions() > 0`）不抛**
    /// ⇒ 避免 terminate。
    ~conditional_block() noexcept(false) {
        if (!finalized_ && !walking_ && !branches_.empty() &&
            std::uncaught_exceptions() == 0) {
            end();
        }
        pop_self();
    }

    conditional_block &then(std::function<void()> body) {
        if (branches_.empty()) {
            throw std::runtime_error("No condition for then branch");
        }
        branches_.back().body = std::move(body);
        return *this;
    }

    template <typename C>
    conditional_block &elif(const C &condition) {
        if (branches_.empty()) {
            throw std::runtime_error("No condition for elif branch");
        }
        if (branches_.back().is_else) {
            throw std::runtime_error("Cannot add elif after else");
        }
        branches_.push_back({ch_bool(to_operand(condition).impl()), nullptr, false});
        return *this;
    }

    conditional_block &else_() {
        if (branches_.empty()) {
            throw std::runtime_error("No condition for else branch");
        }
        if (branches_.back().is_else) {
            throw std::runtime_error("Else branch already added");
        }
        branches_.push_back({ch_bool(true), nullptr, true});
        return *this;
    }

    /**
     * @brief 终结条件块：遍历**所有**分支体收集候选值 ⇒ 合成**一条**链 ⇒ 写入
     *
     * ⚠️ **机制修正**（ADR-051 §1.2 ②）：`finalized_` MUST 在**遍历分支体
     *    之后**才置位 ⇒ 否则捕获期间 `is_active()` 恒 false ⇒ 捕获永不触发
     *    ⇒ ⇒ 条件被**彻底忽略**。
     */
    void end() {
        if (finalized_ || walking_) {
            return;
        }
        walking_ = true;
        try {
            validate();
            // 遍历**所有**分支体（**捕获**语义；分支体在**建图期**执行）
            for (size_t i = 0; i < branches_.size(); ++i) {
                current_branch_idx_ = i;
                if (branches_[i].body) {
                    branches_[i].body();
                }
            }
            // 合成 + 写入（**唯一**写入点 / 驱动点）
            finalize_targets();
        } catch (...) {
            finalized_ = true;
            walking_ = false;
            pop_self();
            throw;
        }
        walking_ = false;
        finalized_ = true;
        pop_self();
    }

    /// 当前是否处于**激活**的条件块内（捕获机制据此决定「收集」还是「直写」）
    static bool is_active() {
        conditional_block *top = active();
        return top != nullptr;
    }

    /// 当前**激活**的条件块（栈顶且未终结）—— `nullptr` ⇒ 不在任何条件块内
    static conditional_block *active() {
        if (block_stack_.empty()) {
            return nullptr;
        }
        conditional_block *top = block_stack_.back();
        return top->finalized_ ? nullptr : top;
    }

    /// 捕获一个候选值（由 `conditional_assign` 调用）
    ///
    /// ⚠️ 公开是为了让 `conditional_assignment`（另一个模板类）能调用它
    ///    ⇒ **不是**面向用户的 API。
    void capture(lnodeimpl *target, lnodeimpl *reg, lnodeimpl *value,
                 const std::function<lnodeimpl *(const std::vector<guard_contribution> &,
                                                 lnodeimpl *, bool)> &composer,
                 const std::function<void(lnodeimpl *)> &committer) {
        const branch_info &br = branches_[current_branch_idx_];
        target_entry *entry = nullptr;
        for (auto &t : targets_) {
            if (t.target == target) {
                entry = &t;
                break;
            }
        }
        if (!entry) {
            targets_.push_back(
                target_entry{target, reg, {}, false, nullptr, composer, committer});
            entry = &targets_.back();
        }
        if (br.is_else) {
            if (entry->has_default) {
                CHFATAL_EXCEPTION(
                    "chlib::_if: the same target received more than one default "
                    "(else) value");
            }
            entry->has_default = true;
            entry->default_value = value;
        } else {
            entry->contribs.push_back({br.condition, value});
        }
    }

private:
    /// 合成 + 写入（**唯一**写入点 / 驱动点）
    void finalize_targets() {
        for (auto &t : targets_) {
            if (!t.reg && !t.has_default) {
                // ⚠️ **target 级**判据（ADR-051 D3）：输出端口没有 `else_` 候选值
                //    ⇒ 「保持」= 引用自身 = **组合环** ⇒ 拓扑排序 CHFATAL（难诊断）
                //    ⇒ ⇒ 在**构建期**显式拒绝。
                CHFATAL_EXCEPTION(
                    "chlib::_if: an output port was conditionally assigned but has "
                    "no else/default value (holding an output would be a "
                    "combinational loop) => add an else_() branch");
            }
            lnodeimpl *chain = t.composer(t.contribs, t.default_value, t.has_default);
            // ⚠️ **嵌套**（ADR-051 D7）：若外层还有**激活**的条件块 ⇒ 本块合成的
            //    这**一条**表达式 MUST 作为**外层**该分支的**候选值**参与合成
            //    ⇒ ⇒ **不**在此写入（否则内层会**直接写**目标 ⇒ 外层的条件被跳过）。
            conditional_block *outer = outer_active();
            if (outer) {
                outer->capture(t.target, t.reg, chain, t.composer, t.committer);
            } else {
                t.committer(chain);
            }
        }
    }

    /// 外层的**激活**条件块（栈中自己的前一项）—— `nullptr` ⇒ 本块是最外层
    conditional_block *outer_active() const {
        for (size_t i = 0; i < block_stack_.size(); ++i) {
            if (block_stack_[i] == this) {
                if (i == 0) {
                    return nullptr;
                }
                conditional_block *outer = block_stack_[i - 1];
                return outer->finalized_ ? nullptr : outer;
            }
        }
        return nullptr;
    }

    /// 把**自己**从激活栈里摘掉（幂等）
    void pop_self() {
        for (auto it = block_stack_.rbegin(); it != block_stack_.rend(); ++it) {
            if (*it == this) {
                block_stack_.erase(it.base());
                return;
            }
        }
    }

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
thread_local std::vector<conditional_block *> conditional_block::block_stack_;

// 便捷函数：创建语句块风格的条件执行
template <typename C>
inline auto _if(const C &condition) {
    return conditional_block(condition);
}

/**
 * @brief 条件赋值的**统一**入口（ADR-051 D4）
 *
 * ⚠️ **改名理由**：原名 `conditional_reg_assign` 表达「只对寄存器」⇒ 而输出
 *    端口**同样**需要条件化 ⇒ ⇒ 统一入口 `conditional_assign`（零活消费者 ⇒
 *    改名零成本 ⇒ 依 Zero-Debt + ADR-043 **不留** deprecated 别名）。
 *
 * 用法：`conditional_assign(target).next() = value;`
 */
template <typename T> class conditional_assignment {
public:
    /// `next` 代理：支持 `conditional_assign(x).next() = value`
    struct next_proxy {
        conditional_assignment *self;

        template <typename U> void operator=(const U &value) const {
            self->assign(value);
        }
    };

    explicit conditional_assignment(ch_reg<T> &reg) : reg_(&reg) {}
    explicit conditional_assignment(port<T, output_direction> &out) : out_(&out) {}

    next_proxy next() { return next_proxy{this}; }

private:
    ch_reg<T> *reg_ = nullptr;
    port<T, output_direction> *out_ = nullptr;

    template <typename U> void assign(const U &value) {
        conditional_block *blk = conditional_block::active();
        if (!blk) {
            // 不在任何条件块内 ⇒ **直写**（与既有行为逐位一致）
            if (reg_) {
                // ⚠️ `operator->` 只对**类类型对象**生效 ⇒ `reg_->next`
                //    （`reg_` 是**指针**）**不成立** ⇒ 必须 `(*reg_)->next`。
                (*reg_)->next = value;
            } else {
                (*out_) <<= value;
            }
            return;
        }
        // 在条件块内 ⇒ **捕获**候选值（不写 `next`）
        lnodeimpl *value_node = to_operand(value).impl();
        if (reg_) {
            // ⚠️ **必须按值捕获 `reg`**（`conditional_assignment` 是**临时对象**
            //    ⇒ 捕获 `this` 会**悬垂** ⇒ 实测：`regimpl_node_ is null`）。
            ch_reg<T> *reg = reg_;
            blk->capture(reg->impl(), reg->impl()->src(0), value_node,
                         [reg](const std::vector<guard_contribution> &contribs,
                               lnodeimpl *def, bool has_def) -> lnodeimpl * {
                             constexpr unsigned W = ch_width_v<T>;
                             ch_uint<W> tail = has_def
                                                   ? ch_uint<W>(def)
                                                   : ch_uint<W>(reg->impl());
                             return compose_guard_chain<W>(contribs, tail).impl();
                         },
                         [reg](lnodeimpl *chain) {
                             // ← **唯一**写入点
                             (*reg)->next = ch_uint<ch_width_v<T>>(chain);
                         });
        } else {
            port<T, output_direction> *out = out_;
            blk->capture(out->impl(), nullptr, value_node,
                         [out](const std::vector<guard_contribution> &contribs,
                               lnodeimpl *def, bool has_def) -> lnodeimpl * {
                             constexpr unsigned W = ch_width_v<T>;
                             // 输出**必须**有显式默认（否则组合环 ⇒ 构建期拒绝）
                             ch_uint<W> tail = ch_uint<W>(def);
                             return compose_guard_chain<W>(contribs, tail).impl();
                         },
                         [out](lnodeimpl *chain) {
                             // ← **唯一**驱动点
                             (*out) <<= ch_uint<ch_width_v<T>>(chain);
                         });
        }
    }
};

// 条件赋值的便捷函数（**寄存器**）
template <typename T> auto conditional_assign(ch_reg<T> &reg) {
    return conditional_assignment<T>(reg);
}

// 条件赋值的便捷函数（**输出端口**）
template <typename T>
auto conditional_assign(port<T, output_direction> &out) {
    return conditional_assignment<T>(out);
}

} // namespace chlib
