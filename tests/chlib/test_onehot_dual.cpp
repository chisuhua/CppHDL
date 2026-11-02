// ⚠️⚠️ **路线 B（新文件）—— `chlib/onehot.h` 首次进双后端 parity**（2026-11-02）
//
// ## 立项理由
//
// ① **零覆盖**：实测「含 `DUAL_BACKEND_TEST_PAIR(` 调用」的源文件全集里**无任何一个**
//    include `chlib/onehot.h` ⇒ ⇒ 该组件在双后端矩阵里**完全未被验证**。
// ② **既有单后端测试不可转换**：`tests/chlib/test_onehot.cpp` 的 7 个用例用了
//    **21 个 Catch2 SECTION** ⇒ ⇒ 命中 `dual-backend-test-coverage` 的 R6 排除项
//    "Catch2 SECTION re-entry"（宏的 body 在两个后端各跑一次 ⇒ SECTION 树会被
//    重复进入）⇒ ⇒ 本文件是**新写**（自带 DUT 与期望），不是转换。
// ③ **与既有 dual 文件正交**：`tests/chlib/test_arithmetic_encoding_dual.cpp` 覆盖的是
//    `chlib/converter.h` 的 `binary_encoder` / `binary_decoder` /
//    `binary_to_onehot` / `onehot_to_binary` ⇒ ⇒ **与 `chlib/onehot.h` 的
//    `onehot_dec` / `onehot_enc` 是不同组件**（不同的头、不同的 select 链结构）。
//
// ## 为什么 one-hot 值得双后端钉（具体理由）
//
// `onehot_dec` / `onehot_enc` 都是 **N 级 `select` 链**（每级一个 `bit_select` +
// 一个 `==` 比较 + 一个 `select`）：
// - 链的**方向**决定语义：`onehot_dec` 从 i=0 迭代到 i=N-1 ⇒ **最后一条语句是
//   最外层** ⇒ ⇒ **最高**置位胜出；⚠️ 而 `chlib/bitwise.h` 的
//   `first_set_bit_detector` 从 i=N-1 迭代到 0 ⇒ ⇒ **最低**置位胜出
//   ⇒ ⇒ **两个组件的"谁胜出"约定相反** ⇒ 这类方向差异正是 codegen
//   （嵌套三元 vs 逐节点求值）最容易分叉的地方。
// - `onehot_enc` 每级还要算 `make_literal<1,N>() << i`（**运行期移位**）。
//
// ## ⚠️ **不钉「未定义」行为（如实登记）**
//
// `include/chlib/onehot.h` 的文档明写：「如果没有位被设置或者设置了多个位，则输出
// **未定义**」⇒ ⇒ 本文件**只断言单置位（合法 one-hot）输入**⇒ ⇒ **不**把
// 「全 0 ⇒ 0」或「多位置位 ⇒ 最高位胜出」写成断言（那是**实现当前行为**，不是契约
// ⇒ 断言它 = 把未定义行为冻结成契约 ⇒ 将来合法的实现变更会被判红）。
// ⇒ 判别力由**合法输入上的精确译码/编码值**提供（链方向错 ⇒ 译码值错）。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// - `onehot_dec<N>`：输入恰好一位为 1 时 ⇒ 输出 = **该位的位置**（0-based）。
//   N=4 ⇒ out 宽度 = `compute_idx_width(4)` = **2 bit**。
// - `onehot_enc<N>`：输入索引 i ⇒ 输出 = `1 << i`。N=4 ⇒ in 宽度 2 bit / out 4 bit。
//
// ⚠️ **向量表放在文件作用域**（陷阱 T3）：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/onehot.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT 3：**函数式** `onehot_dec` / `onehot_enc`（同一组件内两个端口）──
// ⚠️ 输入来自**端口** ⇒ select 链在**运行期**求值。
class OneHotFunctionsDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> onehot_in; ch_in<ch_uint<2>> index_in;
         ch_out<ch_uint<2>> decoded; ch_out<ch_uint<4>> encoded;)

    OneHotFunctionsDut(ch::Component *parent = nullptr,
                       const std::string &name = "onehot_functions")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        chlib::onehot_dec<4> dec;
        chlib::onehot_enc<4> enc;
        io().decoded <<= dec(ch_uint<4>(io().onehot_in.impl()));
        io().encoded <<= enc(ch_uint<2>(io().index_in.impl()));
    }
};

// 合法 one-hot 输入（单置位）⇒ 译码值 = 该位位置
struct DecExpect {
    uint64_t onehot_in;
    uint64_t decoded;
};
const DecExpect kDec[] = {{0x1, 0}, {0x2, 1}, {0x4, 2}, {0x8, 3}};

// 索引 ⇒ one-hot 编码
struct EncExpect {
    uint64_t index_in;
    uint64_t encoded;
};
const EncExpect kEnc[] = {{0, 0x1}, {1, 0x2}, {2, 0x4}, {3, 0x8}};

}  // namespace

// ───────────── pair 1：`onehot_dec_module<4>`（库组件，零 DUT 复制）─────────────
DUAL_BACKEND_TEST_PAIR(
    chlib::onehot_dec_module<4>,
    "chlib onehot_dec_module：单置位输入译码为位位置（两后端一致）",
    "[chlib][onehot][decoder][module][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kDec) {
            sim.set_input_value(io.in, tc.onehot_in);
            sim.tick();
            INFO("onehot_in=0x" << std::hex << tc.onehot_in);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out)) ==
                    tc.decoded);
        }
    });

// ───────────── pair 2：`onehot_enc_module<4>`（库组件，零 DUT 复制）─────────────
DUAL_BACKEND_TEST_PAIR(
    chlib::onehot_enc_module<4>,
    "chlib onehot_enc_module：索引编码为 one-hot 位型（两后端一致）",
    "[chlib][onehot][encoder][module][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kEnc) {
            sim.set_input_value(io.in, tc.index_in);
            sim.tick();
            INFO("index_in=" << tc.index_in);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out)) ==
                    tc.encoded);
        }
    });

// ───────────── pair 3：**函数式** `onehot_dec` / `onehot_enc` ─────────────
DUAL_BACKEND_TEST_PAIR(
    OneHotFunctionsDut,
    "chlib onehot 函数式接口：译码与编码（两后端一致）",
    "[chlib][onehot][function][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kDec) {
            sim.set_input_value(io.onehot_in, tc.onehot_in);
            sim.set_input_value(io.index_in, 0);
            sim.tick();
            INFO("onehot_in=0x" << std::hex << tc.onehot_in);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.decoded)) ==
                    tc.decoded);
        }
        for (const auto &tc : kEnc) {
            sim.set_input_value(io.onehot_in, 0x1);
            sim.set_input_value(io.index_in, tc.index_in);
            sim.tick();
            INFO("index_in=" << tc.index_in);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.encoded)) ==
                    tc.encoded);
        }
    });
