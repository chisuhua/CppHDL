所有节点构建遵循统一模式：

上下文获取：通过 ch::core::ctx_curr_ 获取当前上下文
ID分配：调用 context::next_node_id() 获取唯一ID
节点创建：使用 context::create_node<T>() 模板方法
存储管理：节点存储在 context::node_storage_ 中
代理包装：多数节点通过 proxyimpl 提供统一接口
仿真指令：每个节点实现 create_instruction() 方法生成仿真指令
