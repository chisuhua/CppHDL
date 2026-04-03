// #include "catch_amalgamated.hpp"
// #include "ch.hpp"
// #include "simulator.h"
// 
// using namespace ch;
// using namespace ch::core;
// 
// TEST_CASE("Debug ch_uint<32> comparison", "[debug]") {
//     // 创建一个简单的组件来测试比较操作
//     struct TestCompare : public ch::Component {
//         __io(
//             ch_in<ch_uint<32>> a, "a",
//             ch_in<ch_uint<32>> b, "b",
//             ch_out<ch_bool> less, "less"
//         )
//         
//         TestCompare(ch::Component* parent = nullptr, const std::string& name = "test_compare")
//             : ch::Component(parent, name) {}
//         
//         void create_ports() override {
//             new (io_storage_) io_type;
//         }
//         
//         void describe() override {
//             io().less = io().a < io().b;
//         }
//     };
//     
//     ch::ch_device<TestCompare> unit;
//     ch::Simulator sim(unit.context());
//     
//     sim.set_input_value(unit.instance().io().a, 10);
//     sim.set_input_value(unit.instance().io().b, 20);
//     
//     sim.tick();
//     
//     auto less = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().less));
//     auto a_val = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().a));
//     auto b_val = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().b));
//     
//     std::cout << "a=" << a_val << ", b=" << b_val << std::endl;
//     std::cout << "less=" << less << std::endl;
//     std::cout << "Expected: less=1 (because 10 < 20)" << std::endl;
//     
//     REQUIRE(less == 1);
// }
