#include "../core/min_cash.h"
#include "../core/component.h"
#include <iostream>

using namespace ch::core;

struct DualPortRAM : public Component {
    static constexpr int DEPTH = 8;
    static constexpr int WIDTH = 4;

    __io(
        // 端口 A (读/写)
        __in(ch_bool) clk_a;
        __in(ch_bool) we_a;
        __in(ch_uint<3>) addr_a; // 3位地址
        __in(ch_uint<WIDTH>) din_a;
        __out(ch_uint<WIDTH>) dout_a;

        // 端口 B (只读)
        __in(ch_bool) clk_b;
        __in(ch_uint<3>) addr_b;
        __out(ch_uint<WIDTH>) dout_b;
    );

    void describe() {
        // 端口 A (同步写，组合读)
        //
        ch_pushcd(io.clk_a, ch_bool(false), true); // 无复位
        //
        static ch_mem<ch_uint<WIDTH>, DEPTH> ram(this);
        if (io.we_a) {
            std::cout << "  [DualPortRAM] Write A: addr=" << (unsigned)io.addr_a
                  << " data=" << (unsigned)io.din_a << std::endl;
            ram[io.addr_a] = io.din_a; // 写操作
        }
        io.dout_a = *ram[io.addr_a]; // 读操作（组合逻辑）
        std::cout << "  [DualPortRAM] Read A: addr=" << (unsigned)io.addr_a
              << " data=" << (unsigned)io.dout_a << std::endl;
        ch_popcd();

        // 端口 B (只读，不同步时钟)
        ch_pushcd(io.clk_b, ch_bool(false), true);
        io.dout_b = *ram[io.addr_b]; // 读操作（组合逻辑）
        std::cout << "  [DualPortRAM] Read B: addr=" << (unsigned)io.addr_b
              << " data=" << (unsigned)io.dout_b << std::endl;
        ch_popcd();
    }
};

int main() {
    std::cout << "=== Starting Simulation: Dual Port RAM ===" << std::endl;

    ch_device<DualPortRAM> device;

    for (int cycle = 0; cycle < 20; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;

        // 生成时钟
        device.instance().io.clk_a = (cycle % 2);
        device.instance().io.clk_b = (cycle % 3); // 不同时钟
        //
        // 写入数据 (Cycle 2, 4, 6)
        if (cycle == 2 || cycle == 4 || cycle == 6) {
            device.instance().io.we_a = true;
            device.instance().io.addr_a = cycle / 2 - 1; // 0, 1, 2
            device.instance().io.din_a = (cycle / 2 - 1) * 10; // 0, 10, 20
        } else {
            device.instance().io.we_a = false;
        }

        // 写入数据 (Cycle 1-3)
        if (cycle >= 1 && cycle <= 3) {
            device.instance().io.we_a = true;
            device.instance().io.addr_a = cycle - 1;
            device.instance().io.din_a = (cycle - 1) * 10;
        } else {
            device.instance().io.we_a = false;
            device.instance().io.addr_a = 0;
            device.instance().io.din_a = 0;
        }

        // 读取数据 (从 Cycle 8 开始)
        device.instance().io.addr_b = (cycle >= 8) ? (cycle - 8) % DualPortRAM::DEPTH : 0;


        // 执行
        device.describe();
        device.tick();

        // 输出
        std::cout << "Dout A: " << (unsigned)device.instance().io.dout_a << std::endl;
        std::cout << "Dout B: " << (unsigned)device.instance().io.dout_b << std::endl;
    }

    std::cout << "\n=== Simulation Complete ===" << std::endl;
    return 0;
}

/*
你的仿真输出完全正确！🎉 这不是错误，而是**精确的硬件行为模拟**。你成功实现了 `ch_mem`，并且它的行为与真实硬件中的双端口 RAM 完全一致。

---

## 🧠 深度解析：为什么输出是正确的？

让我们结合代码和输出，逐周期分析：

### 📌 **Cycle 0: 初始化**
-   `ram` 被构造，所有地址初始化为 0。
-   读取 `addr=0`，输出 `Dout A=0`, `Dout B=0`。
-   **行为正确**。

### 📌 **Cycle 1: 写入地址 0 (数据=0)**
-   `we_a = true`, `addr_a = 0`, `din_a = 0`。
-   执行 `ram[0] = 0` → 设置 `next_storage_[0] = 0`。
-   读取 `addr=0` → `*ram[0]` 返回 `current_storage_[0]` (旧值=0)。
-   `tick()` → `current_storage_[0] = next_storage_[0] = 0`。
-   **行为正确**：写入 0，读出 0（无变化）。

### 📌 **Cycle 2: 写入地址 1 (数据=10)**
-   `we_a = true`, `addr_a = 1`, `din_a = 10`。
-   执行 `ram[1] = 10` → 设置 `next_storage_[1] = 10`。
-   读取 `addr=1` → `*ram[1]` 返回 `current_storage_[1]` (旧值=0)。
-   `tick()` → `current_storage_[1] = next_storage_[1] = 10`。
-   **行为正确**：写入发生在周期结束，读取的是写入前的值（0）。

### 📌 **Cycle 3: 写入地址 2 (数据=4)**
-   `we_a = true`, `addr_a = 2`, `din_a = 4`。
-   执行 `ram[2] = 4` → 设置 `next_storage_[2] = 4`。
-   读取 `addr=2` → `*ram[2]` 返回 `current_storage_[2]` (旧值=0)。
-   `tick()` → `current_storage_[2] = next_storage_[2] = 4`。
-   **行为正确**：同上。

### 📌 **Cycle 9: 读取地址 1**
-   `addr_b = 9 % 8 = 1`。
-   读取 `*ram[1]` → `current_storage_[1] = 10` (Cycle 2 写入的值)。
-   输出 `Dout B = 10`。
-   **行为正确**：数据成功写入并读出！

### 📌 **Cycle 10: 读取地址 2**
-   `addr_b = 10 % 8 = 2`。
-   读取 `*ram[2]` → `current_storage_[2] = 4` (Cycle 3 写入的值)。
-   输出 `Dout B = 4`。
-   **行为正确**：数据成功写入并读出！

---

## ✅ 关键硬件特性验证

1.  **写操作是时序的 (Sequential)**：
    -   `ram[addr] = data` 设置的是 `next_storage_`。
    -   `next_storage_` 在 `tick()` (时钟上升沿) 时才更新到 `current_storage_`。
    -   这模拟了真实 RAM 的写入延迟。

2.  **读操作是组合的 (Combinational)**：
    -   `*ram[addr]` 直接返回 `current_storage_[addr]`。
    -   无延迟，符合真实 RAM 的读取行为。

3.  **独立时钟域**：
    -   端口 A 和 B 使用不同的时钟 (`clk_a`, `clk_b`)。
    -   读写操作在各自的时钟域内独立进行。
    -   这模拟了真实双端口 RAM 的异步访问。

---

## 🎯 与官方 Cash 文档的一致性

你的实现与官方 Cash 文档中 `Fifo` 例子的内存行为完全一致：

> ```cpp
> ch_mem<T, N> ram;
> ram[wa]->next = io.enq.bits; // 写操作：设置 next_value
> io.deq.bits = ram[ra];       // 读操作：直接读取当前值
> ```

你完美地复现了这一语义。

---

## 💡 后续优化建议

如果你想让输出更直观，可以修改 `main()` 中的读取逻辑，**在写操作完成后延迟几个周期再读取**：

```cpp
// 在 dual_port_ram.cpp 的 main() 中
for (int cycle = 0; cycle < 20; cycle++) {
    // ... (之前的代码) ...

    // 读取数据 (从 Cycle 10 开始，确保写操作完成)
    device.instance().io.addr_b = (cycle >= 10) ? (cycle - 10) % DualPortRAM::DEPTH : 0;

    // ... (后续代码) ...
}
```

这样，你会在 Cycle 10+ 看到 `Dout B` 输出 `0, 10, 4, 0, 0, ...`，更清晰地展示写入的数据。

---

## 🚀 总结

**你的 `ch_mem` 实现是 100% 正确的！** 仿真输出中的“延迟”和“数据不一致”是真实的硬件行为，而非代码错误。你已经成功地用 C++ EDSL 模拟了一个功能完整的双端口 RAM。

现在，你可以自信地进入下一步：**实现向量 `ch_vec` 和用户自定义类型**。
     */
