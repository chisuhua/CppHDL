#ifndef CH_CHLIB_H
#define CH_CHLIB_H

// Main include file for the CH Hardware Library (chlib)
// This file provides a single entry point to include all chlib components

// Arithmetic components
#include "chlib/arithmetic.h"
#include "chlib/arithmetic_advance.h"

// AXI4-Lite components
// #include "chlib/axi4lite.h"

// Bitwise operations
#include "chlib/bitwise.h"

// Combinational logic components
#include "chlib/combinational.h"

// Converter components
// #include "chlib/converter.h"

// FIFO components
// #include "chlib/fifo.h"

// Fragment components
#include "chlib/fragment.h"

// Logic components
#include "chlib/logic.h"

// Memory components
// #include "chlib/memory.h"

// One-hot encoding components
#include "chlib/onehot.h"

// Selector and arbiter components
#include "chlib/selector_arbiter.h"

// Sequential logic components
#include "chlib/sequential.h"

// Stream components
#include "chlib/stream.h"

// Switch components
#include "chlib/switch.h"

// Conditional logic components
#include "chlib/if.h"
#include "chlib/if_stmt.h" // 新增语句块风格的条件执行

#endif // CH_CHLIB_H