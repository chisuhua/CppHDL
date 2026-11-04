// tests/fixtures/component_naming/pos_layer_macro.cpp
//
// 正向 fixture：`CH_HIERARCHY_LAYER("...")` **带名字**时 MUST 编译通过，
// 且 `module_type_name()` MUST 返回该名字。
//
// ⇒ 这是「成层 + 命名一体」的**正向**判据；反向（缺名 MUST 编译失败）见
// `neg_layer_macro_missing_name.cpp`。

#include "ch.hpp"

#include <cassert>
#include <string>

class FixtureLayered : public ch::Component {
public:
    CH_HIERARCHY_LAYER("fixture_layer");

    FixtureLayered(ch::Component* parent = nullptr, const std::string& name = "layered")
        : ch::Component(parent, name) {}

    void describe() override {}
};

int main() {
    FixtureLayered c;
    assert(c.module_type_name() == "fixture_layer");
    return 0;
}
