// include/component.h
#pragma once

#include "io.h"
#include "logger.h"
#include <cstddef>
#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>

namespace ch { namespace core { class context; } }

namespace ch {

class Component;
template <typename T, typename... Args> class ch_module;

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

// ── 层级粒度 / 端口策略 / 悬空输入策略（ADR-061 决策⑤ · ADR-060 §2.4）─────────
//
// ⚠️ 这三个枚举 MUST 在**框架层**定义（MUST NOT 留给实现者自行发明）：
//    ⇒ 否则「默认值」会被各实现各自解释 ⇒ 产物不可预测。

/// 粒度总开关（**逃生门 + 对比测试用**）：
/// `ForceInline` ⇒ 全部组件 inline（⇒ 用于「与扁平逐字节对照」）；
/// `ForceLayer` ⇒ 全部成层（⇒ 极端形态 ⇒ 未声明者回退 `typeid` + `CHWARN`）。
enum class Granularity { RespectDeclarations, ForceInline, ForceLayer };

/// 跨层非端口边的处理策略（ADR-060）：默认**合成隐式端口 + 告警**。
enum class PortPolicy { SynthesizeWarn, SynthesizeFatal, Reject };

/// 悬空子输入的策略（ADR-060）：默认**向上冒泡**。
enum class UndrivenPolicy { Bubble, Reject };

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

    /// **节点归属的唯一真源**（ADR-059 §2.1 / D1）：
    /// 最内层「构造 / build 作用域」的组件；作用域为空时回落到 `current()`。
    ///
    /// ⚠️ MUST NOT 另立归属规则（「`parent_ == nullptr` ⇒ 归顶层」是**事后**规则
    /// ⇒ 归错层的节点会被"修好" ⇒ 掩盖真正的缺陷）。
    /// ⚠️ `lnodeimpl` 的 `parent_`、`ch_module` 的 parent、`io.h` 的 4 处方向守卫
    /// MUST 全部用它 ⇒ ⇒ 归属只有**一处**判定实现。
    static Component* node_owner();

    // ── 层级粒度（ADR-061 决策①⑤ · ADR-059 §2.2.1 四档）──────────────────
    //
    // 判定优先级：`Granularity` 总开关 > 实例级覆盖 > 类级声明（`CH_HIERARCHY_LAYER`）
    // ⇒ ⇒ **默认 inline**（未声明者不产生 module）。

    /// 本组件是否成为**独立 Verilog module**。
    /// ⚠️ MUST NOT 依赖装配点（「有父组件」/「无 active parent」都不影响判定）
    /// ⇒ ⇒ 成层判定与装配点**解耦**（ADR-061 决策⑥）。
    bool is_layer() const;

    /// 是否已经 `build()` 过（端口只在 build 期创建 ⇒ 「有节点、无端口」的组件
    /// MUST 被层次化工具**响亮拒绝**，不能当成空 module 发射）。
    bool is_built() const { return built_; }

    /// 粒度总开关（默认 `RespectDeclarations`）。
    static Granularity granularity();
    static void set_granularity(Granularity g);

    /// RAII：在作用域内临时切粒度总开关（**异常安全** ⇒ 嵌套亦可）。
    struct GranularityScope {
        explicit GranularityScope(Granularity g);
        ~GranularityScope();
        GranularityScope(const GranularityScope&) = delete;
        GranularityScope& operator=(const GranularityScope&) = delete;
    private:
        Granularity saved_;
    };

    /// 当前作用域栈深度（**只用于判据 / 诊断**）。
    /// ⚠️ 作用域 MUST 配对 ⇒ 组件外的深度 MUST 回到 0。
    static std::size_t scope_depth() { return scope_.size(); }

    /// RAII：把作用域栈截断回**构造本对象时的深度**。
    ///
    /// ⚠️ 为什么需要它：`Component` 的**基类构造先于派生类的成员构造**
    /// ⇒ 基类里压入的作用域帧必须活到「派生类成员构造完毕」才有意义，
    /// 而基类构造**无法**感知那一刻 ⇒ ⇒ 由**唯一知道「构件造完」的那一方**
    /// （`ch_device` / `ch_module` / `create_child`）在 `make_shared` /
    /// `make_unique` **返回后**截断。
    /// ⇒ ⇒ 直接构造（不经上述三者）的组件，其帧保留到**析构**（析构里兜底移除）
    /// ⇒ ⇒ 已知限制：该对象存活期间「组件外建的节点」会归属到它。
    struct ConstructionFrame {
        ConstructionFrame();
        ~ConstructionFrame();
        ConstructionFrame(const ConstructionFrame&) = delete;
        ConstructionFrame& operator=(const ConstructionFrame&) = delete;

    private:
        std::size_t depth_;
    };

    /// 登记一个「等拿到 ctx 再执行」的子组件 build（ADR-059 D2′）。
    ///
    /// ⚠️ 用途：`ch_module` 在**环境 ctx 为空**时（下游 `make_unique<Unit>()`
    /// 然后 `build(ctx)` 的常态 —— Unit 的 ctor 跑在 `build` 之前）MUST NOT
    /// 硬失败 ⇒ ⇒ 把子组件的 build 推迟到宿主 `build(ctx)` 时执行。
    void defer_build(std::function<void(ch::core::context*)> fn);

    // 统一的子组件管理方法 - 返回 shared_ptr
    template<typename T, typename... Args>
    std::shared_ptr<T> create_child(const std::string& name, Args&&... args) {
        CHDBG_FUNC();
        CHREQUIRE(!name.empty(), "Child component name cannot be empty");

        try {
            // ⚠️ D6③：create_child **不压任何作用域**（子组件的 ctor 自己压）
            // ⇒ ⇒ 这里只需要「构造完就截断」⇒ 否则子组件的作用域帧会残留
            // ⇒ 之后**每一个**节点归属被污染。
            ConstructionFrame frame;
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
    /// 作用域栈（ADR-059 §2.1）：`Component` 构造期 / `build_internal()` 期进入。
    /// ⚠️ **配对**：构造期由 `ConstructionFrame` 截断，build 期由 RAII 弹出
    /// ⇒ ⇒ 无泄漏（⇒ 不需要析构兜底 —— 但**直接构造**的组件由析构兜底移除）。
    static thread_local std::vector<Component*> scope_;

    /// 实例级声明（**唯一**写入者是 `ch_module` 的受保护构造路径）
    /// ⇒ ⇒ 公开面只有 `ch_layer<T>` / `ch_inline<T>`（MUST NOT 暴露 setter）。
    void set_instance_layer(bool is_layer);
    void set_instance_module_type_name(std::string name);
    template <typename T, typename... Args> friend class ch_module;

    bool built_ = false;
    bool destructing_ = false;  // 标记是否正在析构

    /// 构造期的**环境 ctx**（D2″ 分支 ① 的基准；可能为 `nullptr`）。
    ch::core::context* construction_ctx_ = nullptr;
    /// 首次 `build()` 绑定的 ctx（D2″ 分支 ② 的基准）。
    ch::core::context* build_ctx_ = nullptr;
    /// 实例级粒度覆盖是否已设置（`ch_layer` / `ch_inline` 写入）
    bool has_instance_layer_ = false;
    /// 实例级粒度值（`ch_inline` ⇒ false；`ch_layer` ⇒ true）
    bool instance_layer_ = false;
    /// 实例级模块类型名（空 ⇒ 未给 ⇒ 回落到类级字面量 / `typeid`）
    std::string instance_module_type_name_;
    /// `ForceLayer` 回退 `typeid` 的告警只发一次（`module_type_name()` 是 const）
    mutable bool force_layer_warned_ = false;
    /// 待执行（等 ctx）的子组件 build（D2′）。
    std::vector<std::function<void(ch::core::context*)>> deferred_builds_;

    void build_internal(ch::core::context* target_ctx);

    /// 从作用域栈移除本组件（**兜底**：直接构造的组件析构时清掉残留帧）。
    void erase_scope_frame();

    /// 把子组件的 `parent_` 补绑到本组件（ADR-059 D9）。
    /// ⚠️ 为什么不在 `add_child` 里一次做完：成员形态的 `ch_module` 在**宿主
    /// 构造期**创建子组件，而宿主那时**还没被 `shared_ptr` 接管**
    /// ⇒ `weak_from_this()` 为空 ⇒ ⇒ 补绑 MUST 推迟到 `build()`（那时已接管）。
    void rebind_children_parents();

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