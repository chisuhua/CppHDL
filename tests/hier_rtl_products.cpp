// tests/hier_rtl_products.cpp
//
// 层次化 RTL 输出的**产物生成器**（供 `scripts/verify_hierarchical_rtl.sh` 使用）。
//
// ⚠️ **为什么是独立可执行文件而不是 Catch2 用例**：
//   护栏需要在**真实进程**里生成产物、再交给 verilator 做 R1–R6 的判据
//   ⇒ ⇒ 若把生成逻辑塞进测试用例，护栏只能靠「跑一遍 ctest 再猜输出路径」
//   ⇒ ⇒ 那是**假绿**温床（路径靠约定、失败不可分辨）。生成器把「产出」与
//   「判据」分开：本文件只管**产出**，判据全在护栏脚本里。
//
// ⚠️ **MUST NOT 注册成 ctest 用例**（`tests/CMakeLists.txt` 里只 `add_executable`）
//   ⇒ ⇒ 否则 `CTEST_TOTAL` 锚点会因一个非判据的程序而漂移。
//
// 用法：
//   hier_rtl_products <out_dir>                 生成 3 个设计（三层 / 行 1 / 冒泡）
//   hier_rtl_products <out_dir> --check-reject mem
//                                              只验证「跨模块 ch_mem 被拒」：要求
//                                              结果是 `ok == false` 且错误串含
//                                              `cross-module ch_mem`；成功 ⇒ exit 0
//
// 退出码：0 = 全部成功；非 0 = 有设计生成失败或拒绝判据不成立。

#include "ch.hpp"
#include "codegen_verilog.h"
#include "device.h"
#include "hierarchy_verilog.h"
#include "module.h"

#include <cstdio>
#include <iostream>
#include <string>

using namespace ch;
using namespace ch::core;

namespace {

// ── 设计 ①：三层（每层都成层 · 叶子有时序 ⇒ clock + reset）──────────────
//    期望树：top → mid → leaf（3 module / 2 实例）
class L3 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("l3_unit");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> r{0_d};
    L3(ch::Component *p = nullptr, const std::string &n = "leaf") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        r->next = io().din;
        io().dout <<= r;
    }
};

class M3 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("m3_unit");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<L3> leaf{"leaf"};
    M3(ch::Component *p = nullptr, const std::string &n = "mid") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        leaf.io().din <<= io().din;
        io().dout <<= leaf.io().dout;
    }
};

class T3 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("t3_unit");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<M3> mid{"mid"};
    T3(ch::Component *p = nullptr, const std::string &n = "top") : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        mid.io().din <<= io().din;
        io().dout <<= mid.io().dout;
    }
};

// ── 设计 ②：行 1（父层直接改子层寄存器的 next ⇒ 子侧合成 input `__if_<n>`）──
class InnerRow1 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("inner_row1");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch_reg<ch_uint<8>> acc{0_d};
    InnerRow1(ch::Component *p = nullptr, const std::string &n = "inner")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= acc; }
};

class OuterRow1 : public ch::Component {
public:
    CH_HIERARCHY_LAYER("outer_row1");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<InnerRow1> inner{"inner"};
    OuterRow1(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        inner.io().din <<= io().din;
        inner.instance().acc->next = io().din;   // ⚠️ 父层改子层寄存器 next
        io().dout <<= inner.io().dout;
    }
};

// ── 设计 ③：冒泡（中间的 `leaf.din` 悬空 ⇒ 逐级冒泡到顶层）────────────────
class BubbleLeaf : public ch::Component {
public:
    CH_HIERARCHY_LAYER("bubble_leaf");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    BubbleLeaf(ch::Component *p = nullptr, const std::string &n = "leaf")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= io().din; }
};

class BubbleMid : public ch::Component {
public:
    CH_HIERARCHY_LAYER("bubble_mid");
    __io(ch_out<ch_uint<8>> dout;)
    ch::ch_module<BubbleLeaf> leaf{"leaf"};
    BubbleMid(ch::Component *p = nullptr, const std::string &n = "mid")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= leaf.io().dout; }   // leaf.din 悬空
};

class BubbleTop : public ch::Component {
public:
    CH_HIERARCHY_LAYER("bubble_top");
    __io(ch_in<ch_uint<8>> din; ch_out<ch_uint<8>> dout;)
    ch::ch_module<BubbleMid> mid{"mid"};
    BubbleTop(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().dout <<= mid.io().dout; }
};

// ── 拒绝形态：跨模块 `ch_mem`（父层读子层持有的 mem）────────────────────
class MemChild : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_child");
    __io(ch_in<ch_uint<4>> addr; ch_out<ch_uint<8>> rdata;)
    ch_mem<ch_uint<8>, 16> mem{"mem"};
    MemChild(ch::Component *p = nullptr, const std::string &n = "child")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override { io().rdata <<= mem.aread(io().addr); }
};

class MemParent : public ch::Component {
public:
    CH_HIERARCHY_LAYER("mem_parent");
    __io(ch_in<ch_uint<4>> addr; ch_out<ch_uint<8>> rdata;)
    ch::ch_module<MemChild> child{"child"};
    MemParent(ch::Component *p = nullptr, const std::string &n = "top")
        : ch::Component(p, n) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        child.io().addr <<= io().addr;
        io().rdata <<= child.instance().mem.aread(io().addr);   // ⚠️ 跨模块 mem
    }
};

std::string join(const std::string &dir, const std::string &name) {
    return dir + "/" + name;
}

// 生成一个设计；失败 ⇒ 打印原因并返回 false
template <typename T>
bool emit_one(const std::string &dir, const std::string &name) {
    const std::string path = join(dir, name);
    std::remove(path.c_str());
    ch::ch_device<T> dev;
    auto r = ch::toVerilogHierarchical(path, dev.instance());
    if (!r.ok) {
        std::cerr << "error: '" << name << "' failed: " << r.error << "\n";
        return false;
    }
    if (r.files.size() != 1 || r.files[0] != path) {
        std::cerr << "error: '" << name << "' wrote unexpected files ("
                  << r.files.size() << ")\n";
        return false;
    }
    return true;
}

bool check_reject_mem(const std::string &dir) {
    const std::string path = join(dir, "reject_mem.v");
    std::remove(path.c_str());
    ch::ch_device<MemParent> dev;
    auto r = ch::toVerilogHierarchical(path, dev.instance());
    if (r.ok) {
        std::cerr << "error: cross-module ch_mem was NOT rejected\n";
        return false;
    }
    if (r.error.find("cross-module ch_mem") == std::string::npos) {
        std::cerr << "error: rejection message misses 'cross-module ch_mem': " << r.error
                  << "\n";
        return false;
    }
    if (!r.files.empty()) {
        std::cerr << "error: rejection still reported files\n";
        return false;
    }
    std::cout << "reject-ok: " << r.error << "\n";
    return true;
}

}   // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0]
                  << " <out_dir> [--check-reject mem]\n";
        return 2;
    }
    const std::string dir = argv[1];

    if (argc >= 4 && std::string(argv[2]) == "--check-reject") {
        if (std::string(argv[3]) != "mem") {
            std::cerr << "error: unknown reject kind '" << argv[3] << "'\n";
            return 2;
        }
        return check_reject_mem(dir) ? 0 : 1;
    }
    if (argc > 2) {
        std::cerr << "error: unknown option '" << argv[2] << "'\n";
        return 2;
    }

    bool ok = true;
    ok = emit_one<T3>(dir, "three_layers.v") && ok;
    ok = emit_one<OuterRow1>(dir, "row1_implicit.v") && ok;
    ok = emit_one<BubbleTop>(dir, "bubble.v") && ok;
    if (!ok) {
        return 1;
    }
    std::cout << "products-ok: three_layers.v row1_implicit.v bubble.v\n";
    return 0;
}
