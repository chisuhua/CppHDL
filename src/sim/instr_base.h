// include/sim/instr_base.h
#ifndef INSTR_BASE_H
#define INSTR_BASE_H

#include <cstdint> // For uint32_t

#include <unordered_map> // If defining data_map_t here
namespace ch { namespace core { class lnodeimpl; } }
#include "types.h" // Include if sdata_type is defined there

namespace ch {

using data_map_t = std::unordered_map<uint32_t /* node_id */, sdata_type>;

// Base class for all simulation instructions
class instr_base {
public:
    explicit instr_base(uint32_t size) : size_(size) {}
    virtual ~instr_base() = default;
    virtual void eval(const data_map_t& data_map) = 0; // NEW
    uint32_t size() const { return size_; }
private:
    uint32_t size_; // Size of the data this instruction operates on
};

} // namespace ch

#endif // INSTR_BASE_H
