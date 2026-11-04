// tests/fixtures/component_naming/neg_layer_macro_missing_name.cpp
//
// 反向 fixture：「成层却忘了命名」MUST **编译期不可表达**
// （ADR-059 §2.2.1 第 ① 档 · ADR-061 决策①）。
//
// ⇒ `CH_HIERARCHY_LAYER` MUST 要求一个名字参数 ⇒ ⇒ 无参形态 MUST 编译失败。
//
// ⚠️ 本文件**故意**不能编译 ⇒ ⇒ 它**MUST NOT** 被注册进 CMake / ctest。
//    护栏 `scripts/verify_component_naming.sh` 断言它编译失败**且**失败点
//    指向本文件（否则「写坏了」会被误读成「判据生效」）。

#include "ch.hpp"

#include <string>

class FixtureBadLayer : public ch::Component {
public:
    // ⚠️ 缺名字 ⇒ MUST 编译失败（宏要求一个参数）
    CH_HIERARCHY_LAYER;

    FixtureBadLayer(ch::Component* parent = nullptr, const std::string& name = "bad")
        : ch::Component(parent, name) {}

    void describe() override {}
};

int main() {
    FixtureBadLayer c;
    return c.module_type_name().empty() ? 1 : 0;
}
