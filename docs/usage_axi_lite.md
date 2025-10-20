AXI-Lite Bundle的关键特性
1. 完整的协议支持
写地址通道 (AW)
写数据通道 (W)
写响应通道 (B)
读地址通道 (AR)
读数据通道 (R)
2. 灵活的宽度配置
可配置地址宽度 (32/64位等)
可配置数据宽度 (32/64位等)
3. 协议验证系统
is_axi_lite_v<T> - 完整AXI-Lite检查
is_axi_lite_write_v<T> - 写通道检查
is_axi_lite_read_v<T> - 读通道检查
4. 字段名检查
编译期字段存在性验证
完整的元数据支持
5. 与其他功能集成
自动命名支持
flip功能
connect功能
方向控制
