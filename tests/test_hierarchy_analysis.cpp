// tests/test_hierarchy_analysis.cpp
//
// 层次化 RTL 输出 · T3-②（分析层）的判据。
//
// 契约：
//   · capability `rtl-hierarchical-output`「不属于任何组件的节点 MUST 归顶层 module」
//   · ADR-058 §2.1（模块边界 = 所有权边界 · 顶层端口名与扁平产物逐字相同）
//   · ADR-060 §2.1（隐式端口方向矩阵）/ §2.2（悬空输入冒泡）
//   · ADR-061 决策②（inline 组件的端口落点）/ 决策④（命名沿用 ctx 级全局表）
//   · ADR-062 决策①（成层组件自己驱动自己的 ch_in ⇒ 响亮拒绝）
//
// ⚠️ 本阶段的**范围**：模块清单 + 节点折叠 + 四类端口 + 连接表 + 三条前置校验。
//    **隐式端口合成**（跨模块边上「子侧端点不是端口」/ 兄弟边 / 跨层 pass-through）
//    是 **T3-④** 的义务 ⇒ ⇒ 本层遇到它 MUST **响亮拒绝**
//    （MUST NOT 静默产出不完整的端口表 —— 否则 golden 会把缺陷合法化）。
//
// ⚠️ golden 是**逐字节**比对 ⇒ 顺序 MUST 确定性（模块拓扑序 / 端口与体内按节点 id /
//    实例按名字 / 边按 (from_module, from_node, to_module, to_node)）。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "hierarchy_analysis.h"
#include "module.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ═════════════════════════════════════════════════════════════════════════
// DUT ①：两层（Top → Sub，两者都成层）
// ═════════════════════════════════════════════════════════════════════════
class Sub : public ch::Component {
public:
    CH_HIERARCHY_LAYER("sub_unit");

    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    Sub(ch::Component* parent = nullptr, const std::string& name = "sub")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class Top2 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("top2_unit");

    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    ch::ch_module<Sub> sub{"sub"};

    Top2(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        sub.io().din <<= io().din;
        io().dout <<= sub.io().dout;
    }
};

// ═════════════════════════════════════════════════════════════════════════
// DUT ②：三层（Top → Mid → Leaf，全成层）—— **端口表手推的靶子**
// ═════════════════════════════════════════════════════════════════════════
class Leaf3 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("leaf3_unit");

    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    Leaf3(ch::Component* parent = nullptr, const std::string& name = "leaf")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class Mid3 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mid3_unit");

    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    ch::ch_module<Leaf3> leaf{"leaf"};

    Mid3(ch::Component* parent = nullptr, const std::string& name = "mid")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

class Top3 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("top3_unit");

    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    ch::ch_module<Mid3> mid{"mid"};

    Top3(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        mid.io().din <<= io().din;
        io().dout <<= mid.io().dout;
    }
};

// ═════════════════════════════════════════════════════════════════════════
// DUT ③：inline 折叠（Top 成层，Helper 未声明 ⇒ inline）
//   · Helper 的 `ch_out` MUST 落到最近的成层祖先（origin = inline_folded）
//   · Helper 的**无 driver** `ch_in` MUST 冒泡成 Top 的 input（origin = bubbled）
// ═════════════════════════════════════════════════════════════════════════
class Helper : public ch::Component {
public:
    // ⚠️ 无 `CH_HIERARCHY_LAYER` ⇒ 默认 inline
    __io(ch_in<ch_uint<8>> din; ch_in<ch_uint<8>> spare; ch_out<ch_uint<8>> dout;)

    Helper(ch::Component* parent = nullptr, const std::string& name = "helper")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }   // spare 故意不参与
};

class TopInline : public ch::Component {
public:
    CH_HIERARCHY_LAYER("top_inline_unit");

    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    ch::ch_module<Helper> helper{"helper"};

    TopInline(ch::Component* parent = nullptr, const std::string& name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        helper.io().din <<= io().din;          // 有 driver ⇒ 体内 wire（不是端口）
        // helper.io().spare 故意不驱动 ⇒ MUST 冒泡
        io().dout <<= helper.io().dout;        // 折叠成 Top 的 output 端口
    }
};

// ═════════════════════════════════════════════════════════════════════════
// DUT ④：`owner == nullptr` 的节点（`main()` 里不经组件建的）⇒ 归顶层 + CHWARN
// ═════════════════════════════════════════════════════════════════════════
class PlainUnit : public ch::Component {
public:
    CH_HIERARCHY_LAYER("plain_unit");

    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)

    PlainUnit(ch::Component* parent = nullptr, const std::string& name = "plain")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

// ═════════════════════════════════════════════════════════════════════════
// DUT ⑤：成层组件藏在 **inline 包装层** 里 ⇒ 实例名是多段相对路径
//   Top(LAYER) → Wrap(未声明 ⇒ inline) → Deep(LAYER)
//   ⇒ 实例名 MUST 是 `wrap_deep`（父 → 子方向拼接），
//     **不是** `deep_wrap`（方向反了在真实层次化里会让实例路径对不上）
// ═════════════════════════════════════════════════════════════════════════
class DeepLayer : public ch::Component {
public:
    CH_HIERARCHY_LAYER("deep_layer");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    DeepLayer(ch::Component* p = nullptr, const std::string& n = "deep") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class InlineWrap : public ch::Component {
public:
    // ⚠️ 未声明 ⇒ inline
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<DeepLayer> deep{"deep"};
    InlineWrap(ch::Component* p = nullptr, const std::string& n = "wrap") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        deep.io().din <<= io().din;
        io().dout <<= deep.io().dout;
    }
};

class WrapTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("wrap_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<InlineWrap> wrap{"wrap"};
    WrapTop(ch::Component* p = nullptr, const std::string& n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        wrap.io().din <<= io().din;
        io().dout <<= wrap.io().dout;
    }
};

// ═════════════════════════════════════════════════════════════════════════
// DUT ⑥：**成层**组件驱动自己的 `ch_in`（= 子 module 内对 input 赋值，非法 Verilog）
// ═════════════════════════════════════════════════════════════════════════
class SelfDriven : public ch::Component {
public:
    CH_HIERARCHY_LAYER("self_driven");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    SelfDriven(ch::Component* p = nullptr, const std::string& n = "selfdrv") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        io().din <<= ch_uint<8>(0x11_h);   // ⚠️ 自己驱动自己的输入端口
        io().dout <<= io().din;
    }
};

// ═════════════════════════════════════════════════════════════════════════
// 失败形态的 DUT
// ═════════════════════════════════════════════════════════════════════════
// F2：兄弟边（A.dout → B.din，A / B 都是 Top 的子模块）
class SibA : public ch::Component {
public:
    CH_HIERARCHY_LAYER("sib_a");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    SibA(ch::Component* p = nullptr, const std::string& n = "a") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class SibB : public ch::Component {
public:
    CH_HIERARCHY_LAYER("sib_b");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    SibB(ch::Component* p = nullptr, const std::string& n = "b") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class SiblingTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("sibling_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<SibA> a{"a"};
    ch::ch_module<SibB> b{"b"};
    SiblingTop(ch::Component* p = nullptr, const std::string& n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        a.io().din <<= io().din;
        b.io().din <<= a.io().dout;      // ⚠️ 兄弟边 ⇒ 需父层 wire（T3-④）
        io().dout <<= b.io().dout;
    }
};

// F3：跨层 pass-through（Top 的节点直接驱动孙模块的 input）
class LeafP : public ch::Component {
public:
    CH_HIERARCHY_LAYER("leaf_p");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    LeafP(ch::Component* p = nullptr, const std::string& n = "leaf") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class MidP : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mid_p");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<LeafP> leaf{"leaf"};
    MidP(ch::Component* p = nullptr, const std::string& n = "mid") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

class SkipLayerTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("skip_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<MidP> mid{"mid"};
    SkipLayerTop(ch::Component* p = nullptr, const std::string& n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.io().din <<= io().din;
        mid.instance().leaf.io().din <<= io().din;   // ⚠️ 跨层：Top → 孙（跳过 mid）
        io().dout <<= mid.io().dout;
    }
};

// F4：节点归属的组件不在 root 的子树里（组件建在同一个 ctx，但没 add_child）
class Stray : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    Stray(ch::Component* p = nullptr, const std::string& n = "stray") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class LonelyTop : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    LonelyTop(ch::Component* p = nullptr, const std::string& n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

// F5：已登记进组件树但**从未 build** 的子组件
class NeverBuiltKid : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    NeverBuiltKid(ch::Component* p = nullptr, const std::string& n = "kid")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class HostWithUnbuiltKid : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    std::shared_ptr<NeverBuiltKid> kid;

    HostWithUnbuiltKid(ch::Component* p = nullptr, const std::string& n = "top")
        : ch::Component(p, n) {
        kid = create_child<NeverBuiltKid>("kid");   // ⚠️ 只登记，**从不 build**
    }
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

// F1：跨 module 的 `ch_mem`（Top 持有 mem，Sub 读写）
class MemUser : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_user");
    __io(ch_in<ch_uint<4>> addr; ch_in<ch_uint<8>> wdata; ch_in<ch_bool> we;
         ch_out<ch_uint<8>> rdata;)
    ch_mem<ch_uint<8>, 16>* mem = nullptr;   // 由父组件注入

    MemUser(ch::Component* p = nullptr, const std::string& n = "user") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mem->write(io().addr, io().wdata, io().we);
        io().rdata <<= mem->aread(io().addr);
    }
};

class MemTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_top");
    __io(ch_in<ch_uint<4>> addr; ch_in<ch_uint<8>> wdata; ch_in<ch_bool> we;
         ch_out<ch_uint<8>> rdata;)

    ch_mem<ch_uint<8>, 16> mem{"mem"};
    // ⚠️ 子组件需要**注入** mem 指针 ⇒ 用工厂 + add_child 手工装配
    std::shared_ptr<MemUser> user;

    MemTop(ch::Component* p = nullptr, const std::string& n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        user->mem = &mem;
        user->io().addr <<= io().addr;
        user->io().wdata <<= io().wdata;
        user->io().we <<= io().we;
        io().rdata <<= user->io().rdata;
    }
};

// 在宿主 `describe()` **之前**装配子组件（需要 mem 指针注入 ⇒ 不能用成员 ch_module）
std::shared_ptr<MemTop> make_mem_top(ch::core::context* ctx) {
    auto top = std::make_shared<MemTop>(nullptr, "top");
    MemUser* raw = nullptr;
    {
        ch::core::ctx_swap swap(ctx);
        ch::Component::ConstructionFrame frame;
        auto child = std::make_unique<MemUser>(nullptr, "top.user");
        raw = child.get();
        raw->mem = &top->mem;      // ⚠️ MUST 在子组件 build **之前**注入
        top->user = std::static_pointer_cast<MemUser>(top->add_child(std::move(child)));
    }
    raw->build(ctx);               // 子组件必须先 build（有端口）
    return top;
}

// ── golden 工具 ──────────────────────────────────────────────────────────
std::string golden_path(const std::string& name) {
    return std::string(CPPHDL_GOLDEN_DIR) + "/" + name;
}

/// 与 golden 逐字节比对；`UPDATE_GOLDEN=1` 时写出（供首次生成 + 人工核对后用）。
void check_golden(const std::string& name, const std::string& actual) {
    const std::string path = golden_path(name);
    if (const char* env = std::getenv("UPDATE_GOLDEN"); env != nullptr && env[0] == '1') {
        std::ofstream(path, std::ios::binary) << actual;
        WARN("golden rewritten: " << path);
        return;
    }
    std::ifstream in(path, std::ios::binary);
    INFO("golden path: " << path);
    REQUIRE(in.good());   // 缺失 ⇒ 用 UPDATE_GOLDEN=1 生成后**人工核对**再提交
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string expected = ss.str();
    INFO("golden: " << path);
    REQUIRE(actual == expected);
}

}   // namespace

// ═════════════════════════════════════════════════════════════════════════
// 两层：模块清单 / 端口表 / 连接表
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchy analysis: two layers", "[hierarchy][analysis]") {
    ch::ch_device<Top2> dev;
    auto& top = dev.instance();

    auto a = ch::analyze_hierarchy(top);

    REQUIRE(a.modules.size() == 2);
    // 拓扑序：叶子在前、根在最后
    REQUIRE(a.modules[0].key == "top.sub");
    REQUIRE(a.modules[0].module_type_name == "sub_unit");
    REQUIRE_FALSE(a.modules[0].is_root);
    REQUIRE(a.modules[1].key == "top");
    REQUIRE(a.modules[1].is_root);
    REQUIRE(a.modules[1].module_type_name == "top2_unit");

    // 根模块有且只有 1 个实例
    REQUIRE(a.modules[1].instances.size() == 1);
    REQUIRE(a.modules[1].instances[0].instance_name == "sub");
    REQUIRE(a.modules[1].instances[0].module_key == "top.sub");

    // 两条跨模块边都被记录（父驱动子 + 子驱动父）
    REQUIRE(a.edges.size() == 2);
    bool saw_parent_drives_child = false;
    bool saw_child_drives_parent = false;
    for (const auto& c : a.modules[1].instances[0].connections) {
        if (c.into_child) {
            saw_parent_drives_child = true;
        } else {
            saw_child_drives_parent = true;
        }
    }
    REQUIRE(saw_parent_drives_child);
    REQUIRE(saw_child_drives_parent);

    // clock/reset 归顶层，且**不告警**
    REQUIRE(a.warnings.empty());

    check_golden("two_layers.txt", a.to_text());
}

// ═════════════════════════════════════════════════════════════════════════
// 三层：端口表**手推**（见下方注释）+ 中间层必须是独立 module
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchy analysis: three layers", "[hierarchy][analysis]") {
    ch::ch_device<Top3> dev;
    auto& top = dev.instance();

    auto a = ch::analyze_hierarchy(top);

    // ⚠️ golden 比对放在**最前**：这份 golden 是**人按 ADR-060 §2.1 逐项推导**
    //    写的（见 change 的 design.md §4.1）⇒ ⇒ 分析层任何一处端口算错（方向 /
    //    归属 / 顺序 / 名字）都会先在这里报告 golden diff，
    //    而不是被后续某条结构断言挡住 ⇒ ⇒ 判据的**判别力**才落在 golden 上。
    check_golden("three_layers.txt", a.to_text());

    REQUIRE(a.modules.size() == 3);
    REQUIRE(a.modules[0].key == "top.mid.leaf");
    REQUIRE(a.modules[1].key == "top.mid");
    REQUIRE(a.modules[2].key == "top");
    REQUIRE(a.modules[2].is_root);

    // 每层各 1 个实例，且嵌套关系正确
    REQUIRE(a.modules[2].instances.size() == 1);
    REQUIRE(a.modules[2].instances[0].instance_name == "mid");
    REQUIRE(a.modules[1].instances.size() == 1);
    REQUIRE(a.modules[1].instances[0].instance_name == "leaf");

    // 手推的端口表（每个成层 module 都是 `din` + `dout`；clock/reset 只在根）
    for (std::size_t i = 0; i < 3; ++i) {
        INFO("module: " << a.modules[i].key);
        std::size_t ins = 0;
        std::size_t outs = 0;
        for (const auto& p : a.modules[i].ports) {
            if (p.origin == ch::HierarchyPortOrigin::ClockReset) {
                continue;   // 根模块的 clock/reset 单独断言（下方）
            }
            if (p.is_input) {
                ++ins;
            } else {
                ++outs;
            }
            // 每个模块自己的 `ch_in` / `ch_out` ⇒ origin = Self（**不**冒泡、**不**折叠）
            REQUIRE(p.origin == ch::HierarchyPortOrigin::Self);
            REQUIRE(p.width == 8);
        }
        REQUIRE(ins == 1);
        REQUIRE(outs == 1);
    }
    // 根模块额外有 clock + reset（context 级节点 ⇒ origin = ClockReset）
    std::size_t clock_reset = 0;
    for (const auto& p : a.modules[2].ports) {
        if (p.origin == ch::HierarchyPortOrigin::ClockReset) {
            ++clock_reset;
            REQUIRE(p.is_input);
        }
    }
    REQUIRE(clock_reset == 2);

    // 4 条跨模块边（每层一对）
    REQUIRE(a.edges.size() == 4);
}

// ═════════════════════════════════════════════════════════════════════════
// inline 折叠：端口落到最近的成层祖先 + 悬空输入冒泡
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchy analysis: inline folding and input bubbling",
          "[hierarchy][analysis][inline]") {
    ch::ch_device<TopInline> dev;
    auto& top = dev.instance();

    auto a = ch::analyze_hierarchy(top);

    // Helper 未声明成层 ⇒ 不产生 module
    REQUIRE(a.modules.size() == 1);
    REQUIRE(a.modules[0].key == "top");
    REQUIRE(a.modules[0].instances.empty());

    bool saw_folded_out = false;
    bool saw_bubbled_in = false;
    for (const auto& p : a.modules[0].ports) {
        if (p.origin == ch::HierarchyPortOrigin::InlineFolded) {
            saw_folded_out = true;
            REQUIRE_FALSE(p.is_input);
        }
        if (p.origin == ch::HierarchyPortOrigin::Bubbled) {
            saw_bubbled_in = true;
            REQUIRE(p.is_input);
            // 冒泡端口的名字 MUST 来自 **扁平发射器的全局命名表**（ADR-061 决策④）
            // ⇒ 组件路径前缀被 sanitize 成 `_`
            REQUIRE(p.name.rfind("top_helper_", 0) == 0);
        }
        // inline 的**有 driver** `ch_in` MUST 是体内 wire（不是端口）
        if (p.is_input && p.origin == ch::HierarchyPortOrigin::Self &&
            p.name.find("helper") != std::string::npos) {
            FAIL("driven inline ch_in must be a body wire, not a port: " << p.name);
        }
    }
    REQUIRE(saw_folded_out);
    REQUIRE(saw_bubbled_in);

    // inline 组件之间没有跨模块边
    REQUIRE(a.edges.empty());
    REQUIRE(a.warnings.empty());

    check_golden("inline_folding.txt", a.to_text());
}

// ═════════════════════════════════════════════════════════════════════════
// `owner == nullptr` 的节点 ⇒ 归顶层 + CHWARN（capability 契约）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchy analysis: ownerless nodes go to the top module with a warning",
          "[hierarchy][analysis][ownerless]") {
    ch::ch_device<PlainUnit> dev;

    // 在 `main()` 里不经组件建一个节点（下游 ChipForge 桥接 plugin 的真实形态）
    ch_uint<8> stray;
    {
        ch::core::ctx_swap swap(dev.context());
        stray = ch_uint<8>(0x5A_h);
    }
    REQUIRE(stray.impl() != nullptr);
    REQUIRE(stray.impl()->get_parent() == nullptr);

    auto a = ch::analyze_hierarchy(dev.instance());

    // 归顶层 ⇒ 顶层模块的 body 里有它
    bool in_body = false;
    for (const auto& n : a.modules[0].body) {
        if (n.node == stray.impl()) {
            in_body = true;
        }
    }
    REQUIRE(in_body);

    // 且 MUST 有告警点名（归属空洞必须可观测）
    REQUIRE(a.warnings.size() == 1);
    REQUIRE(a.warnings[0].find(stray.impl()->name()) != std::string::npos);

    // clock/reset 虽也是 `owner == nullptr`，但 MUST NOT 告警
    for (const auto& w : a.warnings) {
        REQUIRE(w.find("default_clock") == std::string::npos);
        REQUIRE(w.find("default_reset") == std::string::npos);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// 实例名 = 父 → 子 方向的**多段相对路径**（方向反了就与真实层次路径对不上）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchy analysis: instance names follow parent-to-child order",
          "[hierarchy][analysis][instance]") {
    ch::ch_device<WrapTop> dev;
    auto a = ch::analyze_hierarchy(dev.instance());

    // Wrap inline ⇒ 只有 Top + Deep 两个 module
    REQUIRE(a.modules.size() == 2);
    REQUIRE(a.modules[0].key == "top.wrap.deep");
    REQUIRE(a.modules[1].is_root);

    REQUIRE(a.modules[1].instances.size() == 1);
    REQUIRE(a.modules[1].instances[0].instance_name == "wrap_deep");
    REQUIRE(a.modules[1].instances[0].module_key == "top.wrap.deep");
    REQUIRE(a.modules[1].instances[0].connections.size() == 2);

    check_golden("inline_instance_path.txt", a.to_text());
}

// ═════════════════════════════════════════════════════════════════════════
// fail-closed：需要**隐式端口合成**的形态 ⇒ 响亮拒绝（T3-④ 的义务）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchy analysis: sibling edges are rejected until T3-4",
          "[hierarchy][analysis][reject]") {
    ch::ch_device<SiblingTop> dev;
    try {
        ch::analyze_hierarchy(dev.instance());
        FAIL("sibling edge MUST be rejected");
    } catch (const std::runtime_error& e) {
        INFO("message: " << e.what());
        REQUIRE(std::string(e.what()).find("sibling") != std::string::npos);
    }
}

TEST_CASE("hierarchy analysis: cross-layer pass-through edges are rejected until T3-4",
          "[hierarchy][analysis][reject]") {
    ch::ch_device<SkipLayerTop> dev;
    try {
        ch::analyze_hierarchy(dev.instance());
        FAIL("cross-layer pass-through MUST be rejected");
    } catch (const std::runtime_error& e) {
        INFO("message: " << e.what());
        // 跨层边既不是「父→直接子」也不是「直接子→父」⇒ 归入 non-adjacent 拒绝
        REQUIRE(std::string(e.what()).find("sibling / non-adjacent") != std::string::npos);
    }
}

TEST_CASE("hierarchy analysis: cross-module ch_mem is rejected", "[hierarchy][analysis][reject]") {
    ch::core::context ctx("mem_ctx");
    std::shared_ptr<MemTop> top;
    {
        ch::core::ctx_swap swap(&ctx);
        ch::Component::ConstructionFrame frame;
        top = make_mem_top(&ctx);
    }
    top->build(&ctx);
    try {
        ch::analyze_hierarchy(*top);
        FAIL("cross-module ch_mem MUST be rejected");
    } catch (const std::runtime_error& e) {
        INFO("message: " << e.what());
        REQUIRE(std::string(e.what()).find("cross-module ch_mem") != std::string::npos);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// 前置校验 ①：节点归属的组件不在组件树里 ⇒ 抛（给 add_child 修法）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchy analysis: a node owned by an unregistered component is rejected",
          "[hierarchy][analysis][precheck]") {
    ch::core::context ctx("stray_ctx");

    std::unique_ptr<LonelyTop> top;
    std::unique_ptr<Stray> stray;
    {
        ch::core::ctx_swap swap(&ctx);
        ch::Component::ConstructionFrame frame;
        top = std::make_unique<LonelyTop>(nullptr, "top");
        stray = std::make_unique<Stray>(nullptr, "top.stray");
    }
    top->build(&ctx);
    stray->build(&ctx);          // ⚠️ 建了节点 / 端口，但**从不** add_child

    REQUIRE(stray->is_built());
    REQUIRE(top->child_count() == 0);
    try {
        ch::analyze_hierarchy(*top);
        FAIL("node owned by an unregistered component MUST be rejected");
    } catch (const std::runtime_error& e) {
        INFO("message: " << e.what());
        REQUIRE(std::string(e.what()).find("NOT registered in the component tree") !=
                std::string::npos);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// 前置校验 ②：已登记但从未 build ⇒ 有节点、无端口 ⇒ 抛
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchy analysis: a registered-but-never-built component is rejected",
          "[hierarchy][analysis][precheck]") {
    ch::ch_device<HostWithUnbuiltKid> dev;
    auto& top = dev.instance();

    REQUIRE(top.child_count() == 1);
    REQUIRE_FALSE(top.children()[0]->is_built());
    try {
        ch::analyze_hierarchy(top);
        FAIL("registered-but-never-built component MUST be rejected");
    } catch (const std::runtime_error& e) {
        INFO("message: " << e.what());
        REQUIRE(std::string(e.what()).find("never build()-ed") != std::string::npos);
    }
}

// ═════════════════════════════════════════════════════════════════════════
// 前置校验 ③：**成层**组件驱动自己的 `ch_in` ⇒ 非法（ADR-062 决策①）
// ═════════════════════════════════════════════════════════════════════════
TEST_CASE("hierarchy analysis: a layered component must not drive its own ch_in",
          "[hierarchy][analysis][precheck]") {
    ch::ch_device<SelfDriven> dev;
    try {
        ch::analyze_hierarchy(dev.instance());
        FAIL("a layered component driving its own ch_in MUST be rejected");
    } catch (const std::runtime_error& e) {
        INFO("message: " << e.what());
        REQUIRE(std::string(e.what()).find("drives its own") != std::string::npos);
    }
}
