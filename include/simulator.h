// include/simulator.h
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include  "core/context.h"
#include  "types.h" // Now includes bitvector
#include  "io.h"
#include  "reg.h"
#include  <unordered_map>
#include  <vector>
#include  <iostream>
#include  <memory> // For std::unique_ptr

// Forward declarations for instruction base class
namespace ch { class instr_base; }

namespace ch {

class Simulator {
public:
    explicit Simulator(ch::core::context* ctx);
    ~Simulator();  // 👈 声明但不定义！

    void tick();
    void eval(); // Executes one simulation step based on compiled instructions

    // Get value from an output port (uses data_map_ populated during initialization)
    template <typename T >
    uint64_t get_value(const ch::core::ch_logic_out <T > & port) const {
        std::cout << "[Simulator::get_value] Called for port impl " << port.impl() << std::endl; // --- NEW: Debug print ---
        auto* output_node = port.impl();
        if (!output_node) {
            std::cout << "[Simulator::get_value] Port impl is null." << std::endl; // --- NEW: Debug print ---
            return 0;
        }
        // In CASH-style, output node likely connects to a proxyimpl which represents the value
        auto* proxy = output_node->src(0); // Get the source of the outputimpl
        if (!proxy) return 0;
        std::cout << "[Simulator::get_value] Looking up proxy ID " << proxy->id() << " in data_map." << std::endl; // --- NEW: Debug print ---
        // Look up the proxy's value in the data_map_ using its ID
        auto it = data_map_.find(proxy->id());
        if (it != data_map_.end()) {
            // Return the first block of the bitvector value (for smaller values or debugging)
            std::cout << "[Simulator::get_value] Found value in data_map for proxy ID " << proxy->id() << std::endl; // --- NEW: Debug print ---
            const auto& bv = it->second.bv_;
            if (bv.num_words() > 0) {
                return bv.words()[0];
            }
        }
        std::cout << "[Simulator::get_value] Proxy ID " << proxy->id() << " not found in data_map." << std::endl; // --- NEW: Debug print ---
        return 0;
    }

    // Get value by node name (uses data_map_ populated during initialization)
    uint64_t get_value_by_name(const std::string & name) const;

private:
    // Initialization: Convert IR nodes to instructions and allocate data buffers
    void initialize();

    ch::core::context* ctx_;
    std::vector <ch::core::lnodeimpl* > eval_list_; // Topologically sorted IR nodes (for initialization mapping)
    std::unordered_map <uint32_t, std::unique_ptr<ch::instr_base> > instr_map_; // node_id -> instruction object
    std::unordered_map <uint32_t, ch::core::sdata_type > data_map_; // node_id -> simulation data buffer (bitvector)
};

} // namespace ch

#endif // SIMULATOR_H
