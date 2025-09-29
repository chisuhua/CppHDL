// include/macros.h
#ifndef MACROS_H
#define MACROS_H

#include <new> // for placement new
//
// --- Final __io macro ---
#define __io(...) \
    struct io_type { __VA_ARGS__; }; \
    alignas(io_type) char io_storage_[sizeof(io_type)]; \
    io_type& io() { return *reinterpret_cast<io_type*>(io_storage_); }


// SpinalHDL-style factory functions (recommended over macros)
namespace ch {
    template<typename T> auto in(const T& = T{}) { return ch::core::ch_in<T>{}; }
    template<typename T> auto out(const T& = T{}) { return ch::core::ch_out<T>{}; }
}

// Keep old macros if needed
#define __in(...)   ch::core::ch_in<__VA_ARGS__>
#define __out(...)  ch::core::ch_out<__VA_ARGS__>


#endif // MACROS_H
