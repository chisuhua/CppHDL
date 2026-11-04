// include/component.h
#pragma once

#include "io.h"
#include "logger.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>

namespace ch { namespace core { class context; } }

namespace ch {

class Component;

/// 拼接子组件的**全路径**：父的全路径 + "." + 局部名。
///
/// ⚠️ MUST 用 `parent->name()`（**不是** `hierarchical_name()`）—— 后者会把
/// 「**已经是全路径**的 `name_`」再拼一次父路径 ⇒ ⇒ 深度 ≥3 出现
/// `top.top.m.lf` 这类重复前缀（ADR-059 D3）。
/// ⚠️ 这是**唯一**的全路径拼接点 ⇒ ⇒ 第二个拼接点 = 两个真源 = 前缀重复会复发。
std::string build_hierarchical_name(const Component* parent, const std::string& local_name);

/// 声明「**我是一个层**」**并**给出模块类型名 —— 二者一体。
///
/// ⚠️ 写这个宏就**必须**给名字 ⇒ ⇒ 「成层却忘了命名」这个组合**编译期即不可表示**
/// （ADR-059 §2.2.1 第 ① 档 · ADR-061 决策①）。
/// ⚠️ 宏体 MUST NOT 触碰 `this` ⇒ ⇒ 它是**类级**声明 ⇒ ⇒ 实例级的名字
/// （`ch_layer<Helper>` 的档 ③）**不能**用它。
/// ⚠️ `kChHierarchyLayerName` MUST 是**编译期可见**的（`static constexpr`）
/// ⇒ ⇒ `ch_layer<T>` 才能据此在「② 继承」与「③ 必须另给」之间**编译期**分派。
#define CH_HIERARCHY_LAYER(name_literal)                                       \
    static constexpr const char* kChHierarchyLayerName = name_literal;         \
    static constexpr bool kChIsLayer = true;                                   \
    const char* layer_name_literal() const override { return kChHierarchyLayerName; }

class Component : public std::enable_shared_from_this<Component> {
public:
    /// ⚠️ `name` 是**局部名**（实例在父下的名字）⇒ ⇒ 全路径由本构造函数计算
    /// （`build_hierarchical_name(parent, local_name_)`）⇒ ⇒ `name()` 恒为全路径。
    explicit Component(Component* parent = nullptr, const std::string& name = "");
    virtual ~Component();

    // Move operations: valid only before the object is managed by a shared_ptr.
    // Once owned by shared_ptr (e.g. via create_child / add_child), the
    // underlying shared_ptr control block is bound to the original object
    // address.  Moving the object after that point causes children's parent_
    // weak_ptrs to become stale (they still reference the move source).
    // Prefer moving the shared_ptr itself (std::move(shared_ptr<T>)) instead.
    Component(Component&& other) noexcept;
    Component& operator=(Component&& other) noexcept;

    virtual void create_ports() {}
    virtual void describe() = 0;
    void build(ch::core::context* external_ctx = nullptr);

    // 访问器
    ch::core::context* context() const { return ctx_; }
    std::shared_ptr<Component> parent() const { return parent_.lock(); }

    /// **全路径**（父路径 + "." + 局部名）—— 命名的**唯一真源**。
    const std::string& name() const { return name_; }

    /// **局部名**（实例在父下的名字，**不含**父路径）—— 与 `name()` 互不替代。
    const std::string& local_name() const { return local_name_; }

    /// **模块类型名**：类级 `CH_HIERARCHY_LAYER("...")` 给的名字；未声明成层时
    /// 回落 `typeid(*this).name()`。
    /// ⇒ ⇒ **同一类型的多实例返回相同值**（⇒ module 名由 `<module_type_name>_<序号>`
    /// 派生，序号 = 该组内实例**按实例路径排序**的次序 ⇒ ⇒ 确定性）。
    /// ⚠️ 返回**未 sanitize** 的字面量 ⇒ ⇒ **发射侧**（Verilog 模块名）负责 sanitize。
    /// ⚠️ `typeid` 的拼写是**实现相关**的 ⇒ ⇒ 跨编译器/跨平台**不可复算**
    /// ⇒ ⇒ 跨平台 golden 的比较范围 MUST 限定「**同一编译器 / 同一 ABI 内**」。
    std::string module_type_name() const;

    static Component* current() { return current_; }
    static void set_current(Component* comp) { current_ = comp; }

    // 统一的子组件管理方法 - 返回 shared_ptr
    template<typename T, typename... Args>
    std::shared_ptr<T> create_child(const std::string& name, Args&&... args) {
        CHDBG_FUNC();
        CHREQUIRE(!name.empty(), "Child component name cannot be empty");
        
        try {
            auto child = std::make_shared<T>(this, name, std::forward<Args>(args)...);
            children_shared_.push_back(std::static_pointer_cast<Component>(child));
            CHDBG("Created child component: %s", name.c_str());
            return child;
        } catch (const std::bad_alloc&) {
            CHERROR("Failed to allocate memory for child component: %s", name.c_str());
            return nullptr;
        } catch (const std::exception& e) {
            CHERROR("Failed to create child '%s': %s", name.c_str(), e.what());
            return nullptr;
        }
    }

    // ⚠️ `hierarchical_name()` 已**删除**（ADR-059 D3 / R2）：
    //   它把「**已经是全路径**的 `name_`」再拼一次父路径 ⇒ ⇒ 深度 ≥3 出现
    //   `top.top.m.lf`；且它的唯一消费者（`build_hierarchical_name`）已改用
    //   `parent->name()` ⇒ ⇒ 留着 = **第二个全路径拼接点** = 两个真源
    //   ⇒ ⇒ 前缀重复会**再回来**。替代物：`name()`（全路径）+ `local_name()`（局部名）。

    // 修改返回类型为 shared_ptr
    std::shared_ptr<Component> add_child(std::unique_ptr<Component> child);
    size_t child_count() const { return children_shared_.size(); }
    const std::vector<std::shared_ptr<Component>>& children() const { return children_shared_; }

protected:
    ch::core::context* ctx_;
    // Stored as weak_ptr to avoid a reference cycle: each child holds a
    // weak reference to its parent so the parent's lifetime is governed
    // solely by its owner (e.g. ch_device or children_shared_ of the
    // grandparent), not by its children.
    std::weak_ptr<Component> parent_;
    std::string local_name_;   // 局部名（实例在父下的名字）· **先于** name_ 声明
    std::string name_;         // 全路径（父路径 + "." + 局部名）
    std::vector<std::shared_ptr<Component>> children_shared_;
    bool ctx_owner_ = false;

protected:
    /// 类级模块类型名（由 `CH_HIERARCHY_LAYER` 覆写）⇒ 默认 `nullptr`
    /// ⇒ ⇒ 调用方回落到 `typeid(*this).name()`。
    /// ⚠️ MUST 是 **virtual** ⇒ ⇒ 派生类的覆写在**运行期**才被取到
    /// （⚠️ 构造函数里调虚函数会取到**基类**版本 ⇒ ⇒ 故本函数**不在**构造函数里调）。
    virtual const char* layer_name_literal() const { return nullptr; }

private:
    static thread_local Component* current_;
    bool built_ = false;
    bool destructing_ = false;  // 标记是否正在析构
    
    void build_internal(ch::core::context* target_ctx);

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
};

/// 拼接子组件的**全路径**（定义在类之后 —— 需要 `Component` 的完整类型）。
inline std::string build_hierarchical_name(const Component* parent, const std::string& local_name) {
    if (!parent) {
        return local_name;
    }

    // ⚠️ MUST 用 `name()`（父的**全路径**）—— **不是** `hierarchical_name()`
    // （后者会把「已是全路径的 name_」再拼一次父路径 ⇒ 深度 ≥3 重复前缀）。
    const std::string& parent_path = parent->name();
    if (parent_path.empty() || parent_path == "unnamed") {
        return local_name;
    }

    return parent_path + "." + local_name;
}

} // namespace ch