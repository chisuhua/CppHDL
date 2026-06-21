# fix-perf-subprocess-isolation

F2: Eliminate K1 ORC JIT in-process pollution by running each TC in a separate child process via std::system('perf_tests --tc=NN'). Each DUT gets a fresh process with no cross-DUT LLVM ORC state, so perf_regression JIT threshold can be restored from 4.0x to 1.6x and baseline.json regenerated in clean state. Workaround for K1 (root cause remains under investigation in fix-jit-orc-state-leak).
