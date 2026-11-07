// src/hierarchy_equivalence.cpp
//
// **层次化 ≡ 扁平 逐拍等价性 helper 的公共 API 实现**（P8）。
//
// ⚠️ **为什么实现落在 `src/`**（而不是留在 `tests/`）：
//   · 用户要 **链接** `libcpphdl` 使用 ⇒ ⇒ 实现 MUST 是库的一部分。
//   · ⇒ ⇒ 公共头 `include/hierarchy_equivalence.h` **只**声明 ⇒ ⇒ **不**拖进
//     `core/verilator_backend.h`（★ 它在 `__cpp_lib_expected` 缺失时**显式 `#error`**
//     ⇒ ⇒ 会破 macOS/Windows 的 `#include` 契约）⇒ ⇒ ⇒ 那只在本 TU 里用。
//
// ⚠️ **行为等价**：本文件是 `tests/hier_equivalence.h` 的 `ch_test::hier::impl`
//   （T3-005 的实现）的**迁移** ⇒ ⇒ **新增**的只有 fail-closed 校验与被观测侧对账
//   ⇒ ⇒ ⇒ 行为等价由「既有 9 用例 / 414 断言仍全绿」承担。

#include "hierarchy_equivalence.h"

#include "hierarchy_equivalence_impl.h"

namespace ch::hier_eq {

ProductSet emit(ch::Component &root, const std::string &work_dir,
                 ch::Granularity granularity) {
    const auto products = ch::detail::hier_eq_impl::emit_products(root, work_dir, granularity);
    ProductSet out;
    out.ok = products.ok;
    out.error = products.error;
    out.hier_path = products.hier_path;
    out.ports = products.ports;
    out.signal_map = products.signal_map;
    return out;
}

Result compare(ch::core::context *ctx, const std::string &work_dir,
               const std::string &hier_path,
               const std::vector<ch::VerilatorPortSpec> &ports,
               const std::vector<DrivenInput> &inputs,
               const std::vector<Observation> &observations,
               const std::vector<std::vector<std::uint64_t>> &stimulus) {
    using namespace ch::detail::hier_eq_impl;

    const auto comparison =
        compare_products(ctx, work_dir, hier_path, ports, inputs, observations, stimulus);
    Result out;
    out.ok = comparison.ok;
    out.equal = comparison.equal;
    out.error = comparison.error;
    out.first_mismatch = comparison.first_mismatch;
    out.mismatch_count = comparison.mismatch_count;
    out.flat_trace = comparison.flat_trace;
    out.flat_product = comparison.flat_product;
    out.hier_product = comparison.hier_product;
    return out;
}

}  // namespace ch::hier_eq
