#ifndef CHLIB_AXI4LITE_H
#define CHLIB_AXI4LITE_H

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/operators_runtime.h"
#include "core/uint.h"
#include "chlib/logic.h"
#include <cassert>

using namespace ch::core;

namespace chlib {

/**
 * AXI4-Lite Write Address Channel Signals
 */
template <unsigned ADDR_WIDTH>
struct Axi4LiteWriteAddr {
    ch_uint<ADDR_WIDTH> awaddr;      // Write address
    ch_uint<3> awprot;              // Write protection
    ch_bool awvalid;                // Write address valid
    ch_bool awready;                // Write address ready
};

/**
 * AXI4-Lite Write Data Channel Signals
 */
template <unsigned DATA_WIDTH>
struct Axi4LiteWriteData {
    ch_uint<DATA_WIDTH> wdata;       // Write data
    ch_uint<DATA_WIDTH/8> wstrb;    // Write strobes
    ch_bool wlast;                  // Write last
    ch_bool wvalid;                 // Write valid
    ch_bool wready;                 // Write ready
};

/**
 * AXI4-Lite Write Response Channel Signals
 */
template <unsigned DATA_WIDTH>
struct Axi4LiteWriteResp {
    ch_uint<2> bresp;               // Write response
    ch_bool bvalid;                 // Write response valid
    ch_bool bready;                 // Write response ready
};

/**
 * AXI4-Lite Read Address Channel Signals
 */
template <unsigned ADDR_WIDTH>
struct Axi4LiteReadAddr {
    ch_uint<ADDR_WIDTH> araddr;      // Read address
    ch_uint<3> arprot;              // Read protection
    ch_bool arvalid;                // Read address valid
    ch_bool arready;                // Read address ready
};

/**
 * AXI4-Lite Read Data Channel Signals
 */
template <unsigned DATA_WIDTH>
struct Axi4LiteReadData {
    ch_uint<DATA_WIDTH> rdata;       // Read data
    ch_uint<2> rresp;               // Read response
    ch_bool rlast;                  // Read last
    ch_bool rvalid;                 // Read valid
    ch_bool rready;                 // Read ready
};

/**
 * AXI4-Lite Interface (Master side)
 */
template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH>
struct Axi4LiteMaster {
    // Write address channel
    Axi4LiteWriteAddr<ADDR_WIDTH> aw;
    // Write data channel
    Axi4LiteWriteData<DATA_WIDTH> w;
    // Write response channel
    Axi4LiteWriteResp<DATA_WIDTH> b;
    // Read address channel
    Axi4LiteReadAddr<ADDR_WIDTH> ar;
    // Read data channel
    Axi4LiteReadData<DATA_WIDTH> r;
};

/**
 * AXI4-Lite Interface (Slave side)
 */
template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH>
struct Axi4LiteSlave {
    // Write address channel
    Axi4LiteWriteAddr<ADDR_WIDTH> aw;
    // Write data channel
    Axi4LiteWriteData<DATA_WIDTH> w;
    // Write response channel
    Axi4LiteWriteResp<DATA_WIDTH> b;
    // Read address channel
    Axi4LiteReadAddr<ADDR_WIDTH> ar;
    // Read data channel
    Axi4LiteReadData<DATA_WIDTH> r;
};

// TODO: Axi4LiteMemorySlave needs API updates (ch_ram, ch_reg<ch_bool>::clk/rst)
// template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH>
// class Axi4LiteMemorySlave {
// ... (implementation commented out until API is updated)
// };

// TODO: Axi4LiteSimpleMaster needs API updates
// template <unsigned ADDR_WIDTH, unsigned DATA_WIDTH>
// class Axi4LiteSimpleMaster {
// ... (implementation commented out until API is updated)
// };


} // namespace chlib

#endif // CHLIB_AXI4LITE_H