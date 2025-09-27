// include/codegen.h
#ifndef CODEGEN_H
#define CODEGEN_H

#include "core/context.h"
#include <string>
#include <fstream>

namespace ch {

void toVerilog(const std::string& filename, ch::core::context* ctx);

} // namespace ch

#endif // CODEGEN_H
