// core/component.h
#pragma once

#include "min_cash.h"
#include <vector>

extern int global_simulation_cycle;

namespace ch {
namespace core {

// 前向声明
class ch_reg_base;

// Component 基类
class Component {
protected:
    std::string path_name_; // ✅ 新增：层次化路径名
    std::vector<ch_reg_base*> local_regs_;
    std::vector<Component*> children_;

public:
    Component(const std::string& path_name = "unnamed_component") 
        : path_name_(path_name) {
        std::cout << "  [Component] Created: " << path_name_ << std::endl;
    }
    virtual ~Component() = default;
    virtual void describe() = 0;

    void add_child(Component* child) {
        if (child) {
            children_.push_back(child);
        }
    }

    // ✅ 递归获取所有寄存器
    std::vector<ch_reg_base*> get_all_regs() const {
        std::vector<ch_reg_base*> all_regs = local_regs_;
        for (Component* child : children_) {
            auto child_regs = child->get_all_regs();
            all_regs.insert(all_regs.end(), child_regs.begin(), child_regs.end());
        }
        return all_regs;
    }

    // 供 ch_reg 在构造时调用，将自己注册到父组件
    void add_local_reg(ch_reg_base* reg) {
        if (reg) {
            local_regs_.push_back(reg);
            std::cout << "  [Component] Registered reg: " << (void*)reg 
                  << " to component: " << (void*)this << std::endl; // ✅ 打印注册信息
        }
    }

    // 供 ch_device 调用，获取所有寄存器
    const std::vector<ch_reg_base*>& get_local_regs() const {
        return local_regs_;
    }

    const std::string& get_path_name() const {
        return path_name_;
    }
};

// ============ 构造函数实现 ============
template<typename T>
ch_reg<T>::ch_reg(Component* parent, const std::string& name, const T& init) // ✅ 修正: 参数列表
    : current_value_(init), next_value_(init), path_name_("unnamed_reg") {
    const ClockDomain& current_domain = ClockDomainManager::instance().current_domain();
    clk_ptr_ = &current_domain.clk_signal;
    rst_ptr_ = &current_domain.rst_signal;
    posedge_ = current_domain.posedge;
    domain_id_ = current_domain.id;
    if (parent) {
        parent->add_local_reg(this);
        path_name_ = parent->get_path_name() + "." + name; // ✅ 构建完整路径
        std::cout << "  [ch_reg] Created " << path_name_ << "." << name
              << " rst_ptr_  " << (void*)rst_ptr_
              << "in ClockDomain " << domain_id_ << std::endl; // ✅ 打印 parent 指针
    } else {
        std::cerr << "  [ch_reg] WARNING: Created without a parent Component!" << std::endl;
    }
}

template<typename T, int N>
ch_mem<T, N>::ch_mem(Component* parent)
    : current_storage_(), next_storage_() {
    const ClockDomain& current_domain = ClockDomainManager::instance().current_domain();
    clk_ptr_ = &current_domain.clk_signal;
    rst_ptr_ = &current_domain.rst_signal;
    posedge_ = current_domain.posedge;
    domain_id_ = current_domain.id;
    std::cout << "  [ch_mem] Created with depth " << N << " in domain " << domain_id_ << std::endl;
    if (parent) {
        parent->add_local_reg(this);
    } else {
        std::cerr << "  [ch_mem] WARNING: Created without a parent Component!" << std::endl;
    }
}

} // namespace core
} // namespace ch
