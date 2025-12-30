#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/reg.h"
#include "core/uint.h"
#include "simulator.h"

using namespace ch;
using namespace ch::core;

// 创建一个简单的测试组件，用于测试操作结果
template <unsigned N> class TestOpsComponent : public ch::Component {
public:
    __io(ch_in<ch_uint<N>> in_a; ch_in<ch_uint<N>> in_b;
         ch_out<ch_uint<N + 1>> result_out;)

        TestOpsComponent(ch::Component *parent = nullptr,
                         const std::string &name = "test_ops")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type(); }

    void describe() override {
        // 执行加法操作
        auto result = io().in_a + io().in_b;
        io().result_out = result;
    }
};

// 测试基本算术操作的结果正确性
TEST_CASE("Basic Arithmetic Operation Results",
          "[operation][result][runtime]") {
    SECTION("Addition Operation") {
        // 创建设备
        ch_device<TestOpsComponent<8>> device;
        Simulator simulator(device.context());

        // 设置输入值
        simulator.set_port_value(device.instance().io().in_a, 12);
        simulator.set_port_value(device.instance().io().in_b, 5);

        // 运行仿真
        simulator.tick();

        // 获取输出值
        auto output_value =
            simulator.get_port_value(device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(output_value) == 17);
    }
}

// 创建一个用于测试位操作的组件
class BitOpsTestComponent : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_data; ch_out<ch_uint<8>> and_result;
         ch_out<ch_uint<8>> or_result; ch_out<ch_uint<8>> xor_result;
         ch_out<ch_uint<8>> not_result;)

        BitOpsTestComponent(ch::Component *parent = nullptr,
                            const std::string &name = "bit_ops_test")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type(); }

    void describe() override {
        ctx_swap swap(context());
        // 按位操作
        auto mask1 = ch_uint<8>(0b11110000);
        auto mask2 = ch_uint<8>(0b00001111);
        auto mask3 = ch_uint<8>(0b10101010);

        io().and_result = io().in_data & mask1;
        io().or_result = io().in_data | mask2;
        io().xor_result = io().in_data ^ mask3;
        io().not_result = ~io().in_data;
    }
};

// 测试位操作的结果正确性
TEST_CASE("Bitwise Operation Results", "[operation][bitwise][runtime]") {
    SECTION("Bitwise Operations") {
        ch_device<BitOpsTestComponent> device;
        Simulator simulator(device.context());

        // 设置输入值
        simulator.set_port_value(device.instance().io().in_data, 0b11001100);

        // 运行仿真
        simulator.tick();

        // 检查按位与结果
        auto and_value =
            simulator.get_port_value(device.instance().io().and_result);
        REQUIRE(static_cast<uint64_t>(and_value) == 0b11000000);

        // 检查按位或结果
        auto or_value =
            simulator.get_port_value(device.instance().io().or_result);
        REQUIRE(static_cast<uint64_t>(or_value) == 0b11001111);

        // 检查按位异或结果
        auto xor_value =
            simulator.get_port_value(device.instance().io().xor_result);
        REQUIRE(static_cast<uint64_t>(xor_value) == 0b01100110);

        // 检查按位取反结果
        auto not_value =
            simulator.get_port_value(device.instance().io().not_result);
        REQUIRE(static_cast<uint64_t>(not_value) == 0b00110011);
    }
}

// 创建一个用于测试比较操作的组件
class ComparisonTestComponent : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_a; ch_in<ch_uint<8>> in_b;
         ch_out<ch_bool> eq_result; ch_out<ch_bool> ne_result;
         ch_out<ch_bool> gt_result; ch_out<ch_bool> lt_result;)

        ComparisonTestComponent(ch::Component *parent = nullptr,
                                const std::string &name = "comparison_test")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type(); }

    void describe() override {
        // 比较操作
        io().eq_result = io().in_a == io().in_b;
        io().ne_result = io().in_a != io().in_b;
        io().gt_result = io().in_a > io().in_b;
        io().lt_result = io().in_a < io().in_b;
    }
};

// 测试比较操作的结果正确性
TEST_CASE("Comparison Operation Results", "[operation][comparison][runtime]") {
    SECTION("Comparison Operations") {
        ch_device<ComparisonTestComponent> device;
        Simulator simulator(device.context());

        // 设置相等的输入值
        simulator.set_port_value(device.instance().io().in_a, 10);
        simulator.set_port_value(device.instance().io().in_b, 10);

        // 运行仿真
        simulator.tick();

        // 检查相等比较结果
        auto eq_value =
            simulator.get_port_value(device.instance().io().eq_result);
        REQUIRE(static_cast<uint64_t>(eq_value) == 1); // true

        // 检查不等比较结果
        auto ne_value =
            simulator.get_port_value(device.instance().io().ne_result);
        REQUIRE(static_cast<uint64_t>(ne_value) == 0); // false

        // 更改输入值测试大于和小于
        simulator.set_port_value(device.instance().io().in_a, 15);
        simulator.set_port_value(device.instance().io().in_b, 5);
        simulator.tick();

        // 检查大于比较结果
        auto gt_value =
            simulator.get_port_value(device.instance().io().gt_result);
        REQUIRE(static_cast<uint64_t>(gt_value) == 1); // true

        // 检查小于比较结果
        auto lt_value =
            simulator.get_port_value(device.instance().io().lt_result);
        REQUIRE(static_cast<uint64_t>(lt_value) == 0); // false
    }
}

// 创建一个用于测试移位操作的组件
template <unsigned N> class ShiftTestComponent : public ch::Component {
public:
    __io(ch_in<ch_uint<N>> in_data; ch_out<ch_uint<N>> shl_result;
         ch_out<ch_uint<N>> shr_result;)

        ShiftTestComponent(ch::Component *parent = nullptr,
                           const std::string &name = "shift_test")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type(); }

    void describe() override {
        // 移位操作
        io().shl_result = io().in_data << 2_d;
        io().shr_result = io().in_data >> 1_d;
    }
};

// 测试移位操作的结果正确性
TEST_CASE("Shift Operation Results", "[operation][shift][runtime]") {
    SECTION("Shift Operations") {
        ch_device<ShiftTestComponent<8>> device;
        Simulator simulator(device.context());

        // 设置输入值
        simulator.set_port_value(device.instance().io().in_data, 0b00110000);

        // 运行仿真
        simulator.tick();

        // 检查左移结果
        auto shl_value =
            simulator.get_port_value(device.instance().io().shl_result);
        REQUIRE(static_cast<uint64_t>(shl_value) == 0b11000000);

        // 检查右移结果
        auto shr_value =
            simulator.get_port_value(device.instance().io().shr_result);
        REQUIRE(static_cast<uint64_t>(shr_value) == 0b00011000);
    }
}

// 为每种测试类型创建专用的测试组件
template <typename TestType>
class OperationTestComponent : public ch::Component {
public:
    __io(typename TestType::input_ports inputs; ch_out<ch_uint<16>> result_out;)

        OperationTestComponent(ch::Component *parent = nullptr,
                               const std::string &name = "test_ops")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type(); }

    void describe() override {
        ctx_swap swap(context());
        TestType::perform_test(*this);
    }
};

// 各种测试类型的实现
struct ArithmeticTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a + component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct SubtractionTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a - component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct MultiplicationTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a * component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct NegationTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
    };

    static void perform_test(auto &component) {
        auto result = -component.io().inputs.a;
        component.io().result_out = result;
    }
};

struct BitwiseAndTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a & component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct BitwiseOrTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a | component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct BitwiseXorTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a ^ component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct BitwiseNotTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
    };

    static void perform_test(auto &component) {
        auto result = ~component.io().inputs.a;
        component.io().result_out = result;
    }
};

struct EqualityTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a == component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct InequalityTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a != component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct GreaterThanTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a > component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct GreaterEqualTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a >= component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct LessThanTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a < component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct LessEqualTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a <= component.io().inputs.b;
        component.io().result_out = result;
    }
};

struct LeftShiftTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a << 2_d;
        component.io().result_out = result;
    }
};

struct RightShiftTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
    };

    static void perform_test(auto &component) {
        auto result = component.io().inputs.a >> 1_d;
        component.io().result_out = result;
    }
};

struct BitsExtractTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
    };

    static void perform_test(auto &component) {
        auto result = bits<ch_uint<8>, 6, 2>(component.io().inputs.a);
        component.io().result_out = result;
    }
};

struct ConcatTest {
    using input_ports = struct {
        ch_in<ch_uint<3>> a;
        ch_in<ch_uint<5>> b;
    };

    static void perform_test(auto &component) {
        auto result = concat(component.io().inputs.a, component.io().inputs.b);
        component.io().result_out = result;
    }
};

struct ZeroExtendTest {
    using input_ports = struct {
        ch_in<ch_uint<3>> a;
    };

    static void perform_test(auto &component) {
        auto result = zext<8>(component.io().inputs.a);
        component.io().result_out = result;
    }
};

struct SignExtendTest {
    using input_ports = struct {
        ch_in<ch_uint<3>> a;
    };

    static void perform_test(auto &component) {
        auto result = sext<ch_uint<3>, 8>(component.io().inputs.a);
        component.io().result_out = result;
    }
};

struct AndReduceTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
    };

    static void perform_test(auto &component) {
        auto result = and_reduce(component.io().inputs.a);
        component.io().result_out = result;
    }
};

struct OrReduceTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
    };

    static void perform_test(auto &component) {
        auto result = or_reduce(component.io().inputs.a);
        component.io().result_out = result;
    }
};

struct XorReduceTest {
    using input_ports = struct {
        ch_in<ch_uint<8>> a;
    };

    static void perform_test(auto &component) {
        auto result = xor_reduce(component.io().inputs.a);
        component.io().result_out = result;
    }
};

struct MuxTest {
    using input_ports = struct {
        ch_in<ch_bool> cond;
        ch_in<ch_uint<8>> a;
        ch_in<ch_uint<8>> b;
    };

    static void perform_test(auto &component) {
        auto result = select(component.io().inputs.cond,
                             component.io().inputs.a, component.io().inputs.b);
        component.io().result_out = result;
    }
};

struct RegisterAddTest {
    using input_ports = struct {};

    static void perform_test(auto &component) {
        // 创建并初始化寄存器，初始值分别为10和5
        ch_reg<ch_uint<8>> reg_a(10);
        ch_reg<ch_uint<8>> reg_b(5);
        // 将寄存器的输出相加并连接到输出端口
        component.io().result_out = reg_a + reg_b;
    }
};

// 测试各种操作的运行结果正确性
TEST_CASE("Operation Result Correctness", "[operation][result][runtime]") {
    SECTION("Arithmetic Operations") {
        ch_device<OperationTestComponent<ArithmeticTest>> add_device;
        Simulator add_simulator(add_device.context());
        // 添加输入值设置
        add_simulator.set_port_value(add_device.instance().io().inputs.a, 12);
        add_simulator.set_port_value(add_device.instance().io().inputs.b, 5);
        add_simulator.tick();
        auto add_value =
            add_simulator.get_port_value(add_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(add_value) == 17);

        ch_device<OperationTestComponent<SubtractionTest>> sub_device;
        Simulator sub_simulator(sub_device.context());
        sub_simulator.set_port_value(sub_device.instance().io().inputs.a, 12);
        sub_simulator.set_port_value(sub_device.instance().io().inputs.b, 5);
        sub_simulator.tick();
        auto sub_value =
            sub_simulator.get_port_value(sub_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(sub_value) == 7);

        ch_device<OperationTestComponent<MultiplicationTest>> mul_device;
        Simulator mul_simulator(mul_device.context());
        mul_simulator.set_port_value(mul_device.instance().io().inputs.a, 12);
        mul_simulator.set_port_value(mul_device.instance().io().inputs.b, 5);
        mul_simulator.tick();
        auto mul_value =
            mul_simulator.get_port_value(mul_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(mul_value) == 60);

        ch_device<OperationTestComponent<NegationTest>> neg_device;
        Simulator neg_simulator(neg_device.context());
        neg_simulator.set_port_value(neg_device.instance().io().inputs.a, 12);
        neg_simulator.tick();
        auto neg_value =
            neg_simulator.get_port_value(neg_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(neg_value) == 244);
    }

    SECTION("Bitwise Operations") {
        ch_device<OperationTestComponent<BitwiseAndTest>> and_device;
        Simulator and_simulator(and_device.context());
        and_simulator.set_port_value(and_device.instance().io().inputs.a, 12);
        and_simulator.set_port_value(and_device.instance().io().inputs.b, 5);
        and_simulator.tick();
        auto and_value =
            and_simulator.get_port_value(and_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(and_value) == 4);

        ch_device<OperationTestComponent<BitwiseOrTest>> or_device;
        Simulator or_simulator(or_device.context());
        or_simulator.set_port_value(or_device.instance().io().inputs.a, 12);
        or_simulator.set_port_value(or_device.instance().io().inputs.b, 5);
        or_simulator.tick();
        auto or_value =
            or_simulator.get_port_value(or_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(or_value) == 13);

        ch_device<OperationTestComponent<BitwiseXorTest>> xor_device;
        Simulator xor_simulator(xor_device.context());
        xor_simulator.set_port_value(xor_device.instance().io().inputs.a, 12);
        xor_simulator.set_port_value(xor_device.instance().io().inputs.b, 5);
        xor_simulator.tick();
        auto xor_value =
            xor_simulator.get_port_value(xor_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(xor_value) == 9);

        ch_device<OperationTestComponent<BitwiseNotTest>> not_device;
        Simulator not_simulator(not_device.context());
        not_simulator.set_port_value(not_device.instance().io().inputs.a, 12);
        not_simulator.tick();
        auto not_value =
            not_simulator.get_port_value(not_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(not_value) == 243);
    }

    SECTION("Comparison Operations") {
        ch_device<OperationTestComponent<EqualityTest>> eq_device;
        Simulator eq_simulator(eq_device.context());
        eq_simulator.set_port_value(eq_device.instance().io().inputs.a, 12);
        eq_simulator.set_port_value(eq_device.instance().io().inputs.b, 12);
        eq_simulator.tick();
        auto eq_value =
            eq_simulator.get_port_value(eq_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(eq_value) == 1);

        ch_device<OperationTestComponent<InequalityTest>> ne_device;
        Simulator ne_simulator(ne_device.context());
        ne_simulator.set_port_value(ne_device.instance().io().inputs.a, 12);
        ne_simulator.set_port_value(ne_device.instance().io().inputs.b, 5);
        ne_simulator.tick();
        auto ne_value =
            ne_simulator.get_port_value(ne_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(ne_value) == 1);

        ch_device<OperationTestComponent<GreaterThanTest>> gt_device;
        Simulator gt_simulator(gt_device.context());
        gt_simulator.set_port_value(gt_device.instance().io().inputs.a, 12);
        gt_simulator.set_port_value(gt_device.instance().io().inputs.b, 5);
        gt_simulator.tick();
        auto gt_value =
            gt_simulator.get_port_value(gt_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(gt_value) == 1);

        ch_device<OperationTestComponent<GreaterEqualTest>> ge_device;
        Simulator ge_simulator(ge_device.context());
        ge_simulator.set_port_value(ge_device.instance().io().inputs.a, 12);
        ge_simulator.set_port_value(ge_device.instance().io().inputs.b, 12);
        ge_simulator.tick();
        auto ge_value =
            ge_simulator.get_port_value(ge_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(ge_value) == 1);

        ch_device<OperationTestComponent<LessThanTest>> lt_device;
        Simulator lt_simulator(lt_device.context());
        lt_simulator.set_port_value(lt_device.instance().io().inputs.a, 5);
        lt_simulator.set_port_value(lt_device.instance().io().inputs.b, 12);
        lt_simulator.tick();
        auto lt_value =
            lt_simulator.get_port_value(lt_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(lt_value) == 1);

        ch_device<OperationTestComponent<LessEqualTest>> le_device;
        Simulator le_simulator(le_device.context());
        le_simulator.set_port_value(le_device.instance().io().inputs.a, 12);
        le_simulator.set_port_value(le_device.instance().io().inputs.b, 12);
        le_simulator.tick();
        auto le_value =
            le_simulator.get_port_value(le_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(le_value) == 1);
    }

    SECTION("Shift Operations") {
        ch_device<OperationTestComponent<LeftShiftTest>> shl_device;
        Simulator shl_simulator(shl_device.context());
        shl_simulator.set_port_value(shl_device.instance().io().inputs.a, 12);
        shl_simulator.tick();
        auto shl_value =
            shl_simulator.get_port_value(shl_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(shl_value) == 48);

        ch_device<OperationTestComponent<RightShiftTest>> shr_device;
        Simulator shr_simulator(shr_device.context());
        shr_simulator.set_port_value(shr_device.instance().io().inputs.a, 12);
        shr_simulator.tick();
        auto shr_value =
            shr_simulator.get_port_value(shr_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(shr_value) == 6);
    }

    SECTION("Bit Operations") {
        // ch_device<OperationTestComponent<BitsExtractTest>> bits_device;
        // Simulator bits_simulator(bits_device.context());
        // bits_simulator.tick();
        // auto bits_value = bits_simulator.get_port_value(
        //     bits_device.instance().io().result_out);
        // REQUIRE(static_cast<uint64_t>(bits_value) == 26);
    }

    SECTION("Concatenation Operation") {
        ch_device<OperationTestComponent<ConcatTest>> concat_device;
        Simulator concat_simulator(concat_device.context());
        concat_simulator.set_port_value(concat_device.instance().io().inputs.a,
                                        5);
        concat_simulator.set_port_value(concat_device.instance().io().inputs.b,
                                        26);
        concat_simulator.tick();
        auto concat_value = concat_simulator.get_port_value(
            concat_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(concat_value) == 186);
    }

    SECTION("Extension Operations") {
        // ch_device<OperationTestComponent<ZeroExtendTest>> zext_device;
        // Simulator zext_simulator(zext_device.context());
        // zext_simulator.tick();
        // auto zext_value =
        // zext_simulator.get_port_value(zext_device.instance().io().result_out);
        // REQUIRE(static_cast<uint64_t>(zext_value) == 5);

        // ch_device<OperationTestComponent<SignExtendTest>> sext_device;
        // Simulator sext_simulator(sext_device.context());
        // sext_simulator.tick();
        // auto sext_value =
        // sext_simulator.get_port_value(sext_device.instance().io().result_out);
        // REQUIRE(static_cast<uint64_t>(sext_value) == 253);
    }

    SECTION("Reduction Operations") {
        ch_device<OperationTestComponent<AndReduceTest>> and_reduce_device;
        Simulator and_reduce_simulator(and_reduce_device.context());
        and_reduce_simulator.set_port_value(
            and_reduce_device.instance().io().inputs.a, 255);
        and_reduce_simulator.tick();
        auto and_reduce_value = and_reduce_simulator.get_port_value(
            and_reduce_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(and_reduce_value) == 1);

        ch_device<OperationTestComponent<OrReduceTest>> or_reduce_device;
        Simulator or_reduce_simulator(or_reduce_device.context());
        or_reduce_simulator.set_port_value(
            or_reduce_device.instance().io().inputs.a, 12);
        or_reduce_simulator.tick();
        auto or_reduce_value = or_reduce_simulator.get_port_value(
            or_reduce_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(or_reduce_value) == 1);

        ch_device<OperationTestComponent<XorReduceTest>> xor_reduce_device;
        Simulator xor_reduce_simulator(xor_reduce_device.context());
        // 使用值13 (二进制 00001101)，各位异或: 0^0^0^0^1^1^0^1 = 1
        xor_reduce_simulator.set_port_value(
            xor_reduce_device.instance().io().inputs.a, 13);
        xor_reduce_simulator.tick();
        auto xor_reduce_value = xor_reduce_simulator.get_port_value(
            xor_reduce_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(xor_reduce_value) == 1);
    }

    SECTION("Mux Operation") {
        ch_device<OperationTestComponent<MuxTest>> mux_device;
        Simulator mux_simulator(mux_device.context());
        // 测试条件为 true 的情况，应该选择 a 输入
        mux_simulator.set_port_value(mux_device.instance().io().inputs.cond, 1);
        mux_simulator.set_port_value(mux_device.instance().io().inputs.a, 12);
        mux_simulator.set_port_value(mux_device.instance().io().inputs.b, 5);
        mux_simulator.tick();
        auto mux_value =
            mux_simulator.get_port_value(mux_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(mux_value) == 12);
        // 测试条件为 false 的情况，应该选择 b 输入
        mux_simulator.set_port_value(mux_device.instance().io().inputs.cond, 0);
        mux_simulator.set_port_value(mux_device.instance().io().inputs.a, 12);
        mux_simulator.set_port_value(mux_device.instance().io().inputs.b, 5);
        mux_simulator.tick();
        mux_value =
            mux_simulator.get_port_value(mux_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(mux_value) == 5);
    }
}

// 测试寄存器操作的运行结果正确性
TEST_CASE("Register Operation Results", "[operation][register][runtime]") {
    SECTION("Register Assignment and Operations") {
        ch_device<OperationTestComponent<RegisterAddTest>> reg_add_device;
        Simulator reg_add_simulator(reg_add_device.context());
        ch::toDAG("test_operation_results_reg.dot", reg_add_device.context());
        reg_add_simulator.tick(); // 第二个tick更新寄存器值
        auto reg_add_value = reg_add_simulator.get_port_value(
            reg_add_device.instance().io().result_out);
        REQUIRE(static_cast<uint64_t>(reg_add_value) == 15);
    }
}