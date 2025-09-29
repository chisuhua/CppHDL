```
[ch_device<T>]
    |
    v
Component::build()
    |
    +-- ctx_ = new context()          // 存储
    +-- ctx_swap(ctx_.get())          // 激活 → ctx_curr_ = ctx_.get()
    +-- create_ports()                // 使用 ctx_curr_
    +-- describe()                    // 使用 ctx_curr_
    +-- ctx_swap(old_ctx)             // 恢复
    |
    v
Simulator / toVerilog
    |
    +-- component.context() → ctx_    // 读取
```
