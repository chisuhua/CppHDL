#pragma once

// Main include file for the CH Hardware Library (chlib)
// This file provides a single entry point to include all chlib components

// Arithmetic components
#include "chlib/arithmetic.h"
#include "chlib/arithmetic_advance.h"

// Assert / Debug
#include "chlib/assert.h"

// Bitwise operations
#include "chlib/bitwise.h"

// Combinational logic components
#include "chlib/combinational.h"

// FIFO components
#include "chlib/fifo.h"

// Logic components
#include "chlib/logic.h"

// Memory components
#include "chlib/memory.h"

// One-hot encoding components
#include "chlib/onehot.h"

// Selector and arbiter components
#include "chlib/selector_arbiter.h"

// Sequential logic components
#include "chlib/sequential.h"

// Simulator trace helpers
#include "chlib/simulator_trace.h"

// State machine DSL
#include "chlib/state_machine.h"

// Stream components
#include "chlib/stream.h"
#include "chlib/stream_arbiter.h"
#include "chlib/stream_builder.h"
#include "chlib/stream_operators.h"
#include "chlib/stream_pipeline.h"
#include "chlib/stream_width_adapter.h"

// Switch components
#include "chlib/switch.h"

// Conditional logic components
#include "chlib/if.h"
#include "chlib/if_stmt.h" // 新增语句块风格的条件执行

// Pipeline components
#include "chlib/pipeline.h"

// Stream member inline implementations (must be after stream_bundle.h and stream_pipeline.h)
#include "bundle/stream_bundle_member_inlines.h"
