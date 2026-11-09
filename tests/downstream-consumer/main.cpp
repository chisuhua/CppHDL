// tests/downstream-consumer/main.cpp
//
// **下游视角的消费者 TU**（issue #28 的处置 · P14）。
//
// ⚠️ **为什么这个文件存在**：上游的仓内测试用的是**源码树**的 include 面
//   （`-I include -I include/core -I include/ast ...`）⇒ ⇒ 它**证明不了**
//   「装到 prefix 之后下游还能编译」⇒ ⇒ 而 issue #28 的破坏**正是在下游**发生的
//   （下游 `FsmBase : PluginBase` 按值持有 FSM ⇒ 编译失败 ⇒ 上游零信号）。
//
// ⇒ ⇒ 本文件由 `scripts/verify_downstream_consumer.sh` 使用：把上游
//   `cmake --install` 到**临时 prefix** ⇒ 用**独立 CMake 工程**（`CMakeLists.txt`
//   同目录）编译并运行本 TU ⇒ ⇒ 三个真实差异都覆盖：
//     ① 不同 include 前缀（`<chlib/state_machine.h>` ⇒ `<prefix>/include/cpp-hdl/...`）
//     ② 独立工程（不是本仓 target）
//     ③ 真链接（`libcpphdl.a`）
//
// ⚠️ **三种持有形态 MUST 都被覆盖**（缺一种 ⇒ 删一类真实用法）：
//   ① 局部变量 ② `unique_ptr` ③ **按值成员 + 虚析构基类**（下游 ChipForge 的形态）
//
// ⚠️ 不要把它注册成 ctest target 之外的「可选」项：脚本是 **fail-closed** 的
//   （fixture 缺失 ⇒ 退出 1）。ctest 侧由 `tests/CMakeLists.txt` 的
//   `downstream_consumer` 条目承载（⇒ `CTEST_TOTAL` 锚点覆盖缩面）。

#include <chlib/state_machine.h>

#include <component.h>
#include <core/context.h>

#include <memory>
#include <type_traits>

namespace {

enum class DownstreamState : unsigned char { IDLE, RUN, DONE };

using SmDefault = chlib::ch_state_machine<DownstreamState, 3>;
using SmComposable =
    chlib::ch_state_machine<DownstreamState, 3,
                            chlib::dtor_detection::explicit_only>;

// 形态 ③：**虚析构基类 + 按值持有** —— 与下游 `FsmBase : PluginBase` 同形
// （基类析构**不必**显式 noexcept：`= default` 是**隐式** noexcept ⇒ 同样触发）
class PluginBase {
public:
    virtual ~PluginBase() = default;
    virtual void run() = 0;
};

class Plugin : public PluginBase {
public:
    SmComposable sm_;
    void run() override {}
};

static_assert(std::is_nothrow_destructible_v<SmComposable>,
              "composable form MUST be noexcept-destructible");
static_assert(std::is_nothrow_destructible_v<Plugin>,
              "Plugin (virtual dtor base + by-value FSM) MUST be noexcept");
static_assert(!std::is_nothrow_destructible_v<SmDefault>,
              "default form MUST stay noexcept(false) (ADR-050 D5)");

} // namespace

int main() {
    ch::core::context ctx("downstream_consumer");
    ch::core::ctx_swap swap(&ctx);

    SmDefault local;                            // 形态 ①：局部变量
    auto heap = std::make_unique<SmDefault>();  // 形态 ②：unique_ptr
    Plugin plugin;                              // 形态 ③：按值成员
    plugin.run();
    (void)local;
    return 0;
}
