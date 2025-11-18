// include/sim/instr_base.h
#ifndef INSTR_BASE_H
#define INSTR_BASE_H

#include <cstdint>
#include <unordered_map>

namespace ch { 
    namespace core { 
        struct sdata_type; 
    } 
}

namespace ch {

struct data_map_t : public std::unordered_map<uint32_t, ch::core::sdata_type> {
    using std::unordered_map<uint32_t, ch::core::sdata_type>::unordered_map;
};

// Base class for all simulation instructions
class instr_base {
public:
    explicit instr_base(uint32_t size) : size_(size) {}
    virtual ~instr_base() = default;
    virtual void eval(const data_map_t& data_map) = 0;
    
    // New overload for dual-map evaluation
    virtual void eval(const data_map_t& read_map, data_map_t& write_map) {
        // Default implementation - just call the single-map version with read_map
        eval(read_map);
    }
    
    uint32_t size() const { return size_; }
private:
    uint32_t size_;
};

} // namespace ch

#endif // INSTR_BASE_H