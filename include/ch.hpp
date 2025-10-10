#ifndef CH_HPP
#define CH_HPP

// --- Core (dependencies first) ---
//#include "types.h"        // Define sdata_type first
#include "core/context.h" // Define context (depends on types.h)
#include "core/lnodeimpl.h"    // Define base lnodeimpl (depends on context.h/types.h)
#include "ast/ast_nodes.h"    // Define specific nodes (depends on lnodeimpl.h/context.h/types.h)
#include "device.h"       // Define ch_device (depends on component.h, core/deviceimpl.h)
#include "component.h"    // Define Component base class
#include "module.h"    // Define Component base class

// --- Hardware Primitives ---
#include "traits.h"   // Include first as others depend on lnode/get_lnode (depends on lnodeimpl.h)
#include "logic.h"   // Include first as others depend on lnode/get_lnode (depends on lnodeimpl.h)
#include "logger.h"  // Define __io, __in, __out macros (defines the macros used by io.h and user code)
#include "io.h"      // Define ch_logic_in/out (depends on logic.h, lnodeimpl.h, macros.h for type aliases)
#include "reg.h"     // Define ch_reg (depends on logic.h, bitbase.h, lnodeimpl.h, ast_nodes.h if regimpl is used directly)
#include "bitbase.h" // Define ch_uint, operators (depends on logic.h, lnodeimpl.h)
//#include "numbase.h" // If ch_uint is defined here, include it
//#include "next.h"    // If ch_nextEn is defined here

// --- Simulation (when ready) ---
// #include "simulator.h"
// #include "tracer.h"

// --- Namespace aliases (optional) ---
// namespace chh = ch::core;
// ===========================================================================
// IO 宏定义
// ===========================================================================

#define __io(...) \
    struct io_type { __VA_ARGS__; }; \
    alignas(io_type) char io_storage_[sizeof(io_type)]; \
    [[nodiscard]] io_type& io() { return *reinterpret_cast<io_type*>(io_storage_); }

namespace core {
    template<typename T> 
    [[nodiscard]] auto in(const T& = T{}) { return ch::core::ch_in<T>{}; }
    
    template<typename T> 
    [[nodiscard]] auto out(const T& = T{}) { return ch::core::ch_out<T>{}; }
}

#define __in(...)   ch::core::ch_in<__VA_ARGS__>
#define __out(...)  ch::core::ch_out<__VA_ARGS__>


#endif // CH_HPP
