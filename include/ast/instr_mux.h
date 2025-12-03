// include/instr/mux.h
#ifndef INSTR_MUX_H
#define INSTR_MUX_H

#include "instr_base.h"
#include "core/types.h"
#include <iostream>

namespace ch {

class instr_mux : public instr_base {
public:
    instr_mux(ch::core::sdata_type* dst, uint32_t size,
              ch::core::sdata_type* cond, 
              ch::core::sdata_type* true_val, 
              ch::core::sdata_type* false_val)
        : instr_base(size), dst_(dst), cond_(cond), true_val_(true_val), false_val_(false_val) {}

    void eval() override {
        if (!dst_ || !cond_ || !true_val_ || !false_val_) {
            std::cerr << "[MUX] Error: Null pointer encountered!" << std::endl;
            return;
        }

        // Perform the mux operation: if cond != 0 then true_val else false_val
        if (!cond_->is_zero()) {
            *dst_ = *true_val_;
        } else {
            *dst_ = *false_val_;
        }
    }

private:
    ch::core::sdata_type* dst_;
    ch::core::sdata_type* cond_;
    ch::core::sdata_type* true_val_;
    ch::core::sdata_type* false_val_;
};

} // namespace ch

#endif // INSTR_MUX_H