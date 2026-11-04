// tests/fixtures/component_naming/neg_hierarchical_name.cpp
//
// 反向 fixture：`Component::hierarchical_name()` MUST **不存在**
// （ADR-059 D3 / R2）—— 它是「第二个全路径拼接点」= 两个真源
// ⇒ ⇒ 留着 = 前缀重复会复发。
//
// ⚠️ 本文件**故意**不能编译 ⇒ ⇒ 它**MUST NOT** 被注册进 CMake / ctest。

#include "ch.hpp"

#include <string>

// ⚠️ MUST 用**具体**子类：`ch::Component` 是抽象类（`describe()` 纯虚）
// ⇒ ⇒ 直接实例化它会报「cannot instantiate abstract type」—— 那是**无关错误**
// ⇒ ⇒ 会让「失败点指向本 fixture」这条判据**失真**（先例：
// `verify_conditional_assign_integrity.sh` 的 R0 同型坑）。
class Concrete : public ch::Component {
public:
    Concrete(ch::Component* parent = nullptr, const std::string& name = "c")
        : ch::Component(parent, name) {}
    void describe() override {}
};

int main() {
    Concrete c(nullptr, "x");
    // ⚠️ MUST 编译失败：`hierarchical_name` 不是 `Component` 的成员
    //    （且**只**因它而失败 ⇒ ⇒ 本文件 MUST NOT 有其它无关错误）
    return static_cast<int>(c.hierarchical_name().size());
}
