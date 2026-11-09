// tests/test_chlib_composability_api.cpp
//
// **ADR-067 决策 ⑤ 的机械判据**（`chlib` 析构异常规格 vs 类型可组合性）。
//
// ⚠️ **为什么这个文件存在**（★ 它是 ADR-067 §5 的**主判据**）：
//   「可组合性」= 「**公共头**暴露的类型，在**仓外**的 include 路径下**仍可**作按值成员」
//   ⇒ ⇒ 仓内测试若能靠内部路径蒙混过关 ⇒ 就不证明「公共面自足」⇒ ⇒
//   ⇒ 本文件**只** include 公共头（`chlib.h` · `component.h` · `device.h` · `module.h`）
//   ⇒ ⇒ **真编译 + 真链接 + 真运行** ⇒ ⇒ 与 `tests/test_public_equivalence_api.cpp` 同款形态。
//
// ⚠️ **判据分三面**（缺一面 ⇒ 「绿」无判别力）：
//
//   ① **编译期**（`static_assert` · 文件作用域）：默认形态**类型身份**不变 · 默认形态仍
//      `noexcept(false)`（ADR-050 D5）· 可组合形态 `noexcept` · `std::optional` 的**落差**
//      （包装一层就静默变 noexcept ⇒ ⇒ 比「编译期撞墙」更危险 ⇒ 必须钉死）·
//      **`ch::Component` 基类 + 按值持有**可编译（★ **与下游同形** —— 下游的
//      `FsmBase : PluginBase` 就是这个形状）；
//   ② **运行期**：显式 `check_unresolved()` 抛**可捕获**异常 · resolve 后不再抛 ·
//      **异常展开期析构 MUST NOT 终止**（⇒ 不新增「双异常 ⇒ terminate」路径）；
//   ③ **子进程**：析构期的**终止兜底** ⇒ ⇒ `std::terminate()` ⇒ SIGABRT
//      ⇒ ⇒ **同进程观测不到 abort** ⇒ ⇒ 由**父进程**派生子进程跑 `[probe]` 标签用例
//      ⇒ ⇒ 父进程断言子进程死于 **SIGABRT**。
//
// ⚠️ **检出非幂等**（ADR-067 决策 ② 语义 4）：`check_unresolved()` 是**只读**的
//   ⇒ `catch` 后**不 resolve** ⇒ 析构期兜底**照常终止** ⇒ ⇒ 用例里 `catch` 后**必须**
//   resolve（⇒ 这也是 ADR-067 推荐的恢复路径）。

// ⚠️ **include 集 = 下游口径**（`chlib/state_machine.h` + `component.h` + `core/context.h`）
//   ⇒ ⇒ 与 ChipForge 的 `multi_cycle_fsm.h` **逐字同形**（`<chlib/state_machine.h>` +
//   `<core/context.h>`）⇒ ⇒ 「下游能不能编译」这件事由**本文件**当场证明。
// ⚠️ **不** include 聚合头 `chlib.h`：本仓有**两个** `chlib` 命名空间
//   （`::chlib` 与 `ch::chlib`）⇒ ⇒ 聚合头会把 `ch::chlib` 带进作用域
//   ⇒ ⇒ `chlib::X` 变成**歧义引用**（`include/bundle/stream_bundle_member_inlines.h`
//   的头注释已登记该不一致 ⇒ 「**独立立项**」⇒ 本 change **不动**命名空间布局）。
#include "chlib/state_machine.h"

#include "catch_amalgamated.hpp"

#include "component.h"
#include "core/context.h"
#include "device.h"
#include "module.h"

#include <unistd.h>

#include <csignal>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

using namespace ch::core;
// ⚠️ **不** `using namespace ch;` ⇒ ⇒ 它把 `ch::chlib` 带进作用域 ⇒ 与全局 `::chlib`
//    **撞名** ⇒ ⇒ `chlib::` 变成**歧义引用**（`ch::chlib::` vs `::chlib::`）
// ⚠️ 同样**不** `using namespace chlib;`（`ch::Component` 等一律显式限定）

namespace {

enum class TestState : uint8_t { STATE_A, STATE_B, STATE_C };

} // namespace

// ⚠️ 别名放**文件作用域**（类型别名无链接 ⇒ 无 ODR 风险）⇒ ⇒ TEST_CASE 可直接用
using SmDefault = chlib::ch_state_machine<TestState, 3>;
using SmComposable = chlib::ch_state_machine<TestState, 3,
                                            chlib::dtor_detection::explicit_only>;

// ---------------------------------------------------------------------------
// ① 编译期判据（**文件作用域** ⇒ 编译不过就是编译不过）
// ---------------------------------------------------------------------------

// 默认形态的**类型身份**不变（⇒ 「默认形态零变更」在类型层可判）
static_assert(std::is_same_v<SmDefault,
                             chlib::ch_state_machine<TestState, 3,
                                                     chlib::dtor_detection::automatic>>,
              "default form MUST be the SAME TYPE as the `automatic` specialization");

// 默认形态**逐字保留 ADR-050 D5**（析构期可捕获检出 ⇒ 析构必须 `noexcept(false)`）
static_assert(!std::is_nothrow_destructible_v<SmDefault>,
              "default form MUST keep a throwing destructor (ADR-050 D5)");

// 可组合形态 ⇒ 析构 `noexcept` ⇒ **可作按值成员 / 基类**
static_assert(std::is_nothrow_destructible_v<SmComposable>,
              "composable form MUST have a noexcept destructor");

// ⚠️ **静默降级的落差**（★ 钉死它）：`SmDefault` 本身不可组合，但**包一层 `optional`
//    就变成 noexcept** ⇒ ⇒ 编译期**无诊断** + 运行期 `terminate`
//    ⇒ ⇒ 比「编译期撞墙」**更危险** ⇒ ⇒ MUST 让这个落差**恒可见**
static_assert(!std::is_nothrow_destructible_v<SmDefault> &&
                  std::is_nothrow_destructible_v<std::optional<SmDefault>>,
              "the optional-wrapping gap MUST stay pinned (silent degradation)");

// ★ **与下游同形**：`ch::Component`（`include/component.h` 的 `virtual ~Component();`
//   ⇒ 隐式 noexcept）作基类 + **按值**持有一个可组合 FSM ⇒ 派生类的隐式析构 MUST noexcept
//   ⇒ ⇒ 否则「looser exception specification on overriding virtual function」
struct ComposableDut : ch::Component {
    SmComposable sm_;
    ComposableDut(ch::Component *parent = nullptr,
                  const std::string &name = "dut")
        : ch::Component(parent, name) {}
    void describe() override {}
};

static_assert(std::is_nothrow_destructible_v<ComposableDut>,
              "a Component-derived class holding the FSM BY VALUE MUST be noexcept");

// ---------------------------------------------------------------------------
// ② 运行期判据
// ---------------------------------------------------------------------------

TEST_CASE("composable form: default form is unchanged and the composable form is "
          "noexcept",
          "[chlib][dtor][composability][adr067]") {
    // 编译期事实的运行期复述（让用例**非空**且在断言失败时给出可读信息）
    REQUIRE(std::is_same_v<SmDefault,
                           chlib::ch_state_machine<TestState, 3,
                                                   chlib::dtor_detection::automatic>>);
    REQUIRE_FALSE(std::is_nothrow_destructible_v<SmDefault>);
    REQUIRE(std::is_nothrow_destructible_v<SmComposable>);
    REQUIRE(std::is_nothrow_destructible_v<ComposableDut>);
}

TEST_CASE("composable form: check_unresolved() MUST throw a catchable error and "
          "stop throwing once resolved",
          "[chlib][dtor][composability][adr067]") {
    auto ctx = std::make_unique<ch::core::context>("composability_ctx");
    ch::core::ctx_swap swap(ctx.get());

    SmComposable sm;
    ch_reg<ch_uint<8>> reg(0_d, "reg");
    sm.contrib(reg, TestState::STATE_A, ch_uint<8>(1_d));

    // 显式检查 ⇒ **可捕获**异常（这是 `explicit_only` 形态唯一的可捕获检出路径）
    REQUIRE_THROWS_AS(sm.check_unresolved(), std::runtime_error);

    // ⚠️ 检出**非幂等** ⇒ catch 之后**必须** resolve ⇒ 否则析构期兜底会 terminate
    //    （⇒ 这一行同时验证了 ADR-067 推荐的恢复路径）
    reg->next = sm.resolve_hold(reg);
    REQUIRE_NOTHROW(sm.check_unresolved());
}

TEST_CASE("composable form: destruction during unwinding MUST NOT terminate",
          "[chlib][dtor][composability][adr067]") {
    // ⚠️ 展开期（`uncaught_exceptions() > 0`）⇒ 兜底**不终止**
    //    ⇒ ⇒ 原始异常照常传播（⇒ 不新增「双异常 ⇒ terminate」路径）
    REQUIRE_THROWS_AS(
        [] {
            auto ctx = std::make_unique<ch::core::context>("unwind_ctx");
            ch::core::ctx_swap swap(ctx.get());
            SmComposable sm;
            ch_reg<ch_uint<8>> reg(0_d, "reg");
            sm.contrib(reg, TestState::STATE_A, ch_uint<8>(1_d));
            throw std::runtime_error("boom");
        }(),
        std::runtime_error);
}

TEST_CASE("default form: destruction MUST still throw a catchable error",
          "[chlib][dtor][composability][adr067]") {
    // ⚠️ **默认形态逐字保留 ADR-050 D5** ⇒ 析构期抛**可捕获**异常
    REQUIRE_THROWS_AS(
        [] {
            auto ctx = std::make_unique<ch::core::context>("default_ctx");
            ch::core::ctx_swap swap(ctx.get());
            SmDefault sm;
            ch_reg<ch_uint<8>> reg(0_d, "reg");
            sm.contrib(reg, TestState::STATE_A, ch_uint<8>(1_d));
            // 离开作用域 ⇒ 析构 ⇒ 抛（可捕获）
        }(),
        std::runtime_error);
}

// ---------------------------------------------------------------------------
// ③ 子进程判据：析构期的**终止兜底**
// ---------------------------------------------------------------------------

TEST_CASE("composable form: unresolved contributions MUST terminate at destruction",
          "[chlib][dtor][composability][adr067][probe]") {
    const char *probe_env = "CPPHDL_DTOR_COMPOSABILITY_PROBE";

    if (std::getenv(probe_env) == nullptr) {
        // ---- 父进程：派生子进程跑本用例 ⇒ 子进程在析构期 abort ⇒ 断言其死于 SIGABRT
        // ⚠️ **必须先在父进程里解析出可执行文件的绝对路径** ⇒ ⇒ 不能直接把
        //    `/proc/self/exe` 写进命令行 ⇒ ⇒ 那是 **shell** 自己的 exe
        //    （`/proc/self/exe` 由**执行 exec 的那个进程**解析 ⇒ shell 会把自己再跑一遍
        //    ⇒ ⇒ 子进程退出码 0 ⇒ ⇒ 断言恒绿 ⇒ ⇒ **假绿陷阱**）。
        char exe_path[4096] = {};
        ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len <= 0) {
            SKIP("cannot resolve /proc/self/exe (non-Linux?) => the terminate "
                 "backstop is not observable in-process");
        }
        std::string cmd = std::string(probe_env) + "=1 '" +
                          std::string(exe_path, static_cast<size_t>(len)) +
                          "' \"[probe]\" > /dev/null 2>&1";
        int rc = std::system(cmd.c_str());
        REQUIRE(rc != 0);
        // ⚠️ `std::system` 返回**原始 wait status**（glibc）⇒ ⇒ **两种形态都要认**：
        //    ① 子进程被**信号**直接杀死 ⇒ `status & 0x7f` == 信号编号；
        //    ② **shell** 把信号的死亡折成退出码 `128 + 信号` ⇒ `(status >> 8)` == `128 + 信号`
        //       （★ 实测：`sh -c` 会 exec 掉自己 ⇒ 子进程被信号杀死时，shell 往往以
        //         **退出码 134** 正常退出 ⇒ ⇒ 只判 ① 会**恒红** ⇒ ⇒ 两形态都判）。
        bool killed_by_sigabrt = ((rc & 0x7f) == SIGABRT) ||
                                 ((rc >> 8) == (128 + SIGABRT));
        REQUIRE(killed_by_sigabrt);
        return;
    }

    // ---- 子进程：造一个「有未 resolve 贡献」的可组合 FSM ⇒ 离开作用域 ⇒ 析构 ⇒ terminate
    auto ctx = std::make_unique<ch::core::context>("probe_ctx");
    ch::core::ctx_swap swap(ctx.get());
    SmComposable sm;
    ch_reg<ch_uint<8>> reg(0_d, "reg");
    sm.contrib(reg, TestState::STATE_A, ch_uint<8>(1_d));
    // 离开作用域 ⇒ 析构 ⇒ 未 resolve ⇒ 兜底 ⇒ `std::terminate()` ⇒ SIGABRT
}
