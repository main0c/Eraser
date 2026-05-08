# Qt5.6 + C++11 libzmq Server（带界面）

这是一个 `PULL` 模式的 server 示例，接收多个 client 发来的 protobuf 消息，并在 GUI 中实时展示：

- client 连接信息
- 磁盘信息
- 擦除进度 / 完成信息

## 协议设计（protobuf）

`proto/eraser.proto`：

- `ClientReport`
  - `client_id`：客户端唯一 ID
  - `host_name` / `ip`
  - `disks`：重复字段，支持多块盘
  - `erase`：当前擦除任务状态

建议发送策略：

1. **首次上线**：发送包含 `client_id + host_name + ip + disks` 的消息。
2. **进度上报**：每 1~2 秒发送一次 `erase.progress`。
3. **完成上报**：`erase.finished=true` 并填写 `erase.result`。
4. **幂等原则**：同一个 `task_id` 多次上报，server 以最新 `report_time_ms` 为准。

## 编译

依赖：

- Qt 5.6（Core/Widgets）
- protobuf（protoc + libprotobuf）
- libzmq
- cmake >= 3.5

```bash
mkdir build && cd build
cmake ..
make -j
./eraser_server
```

## 网络模型

- Server：`tcp://*:5555`，`ZMQ_PULL`
- Client：`tcp://<server-ip>:5555`，`ZMQ_PUSH`

优点：

- 多 client 并发简单
- 天然负载均衡
- 消息有边界，配合 protobuf 可靠解析

## 你可以继续扩展

- 在 server 端维护 `client_id -> 最新状态` 的表格视图（QTableWidget）
- 增加心跳超时（比如 10 秒未上报置灰）
- 对 `erase.progress` 做曲线图展示
- 存储历史记录到 SQLite
