---
title: TCP 握手状态问答
tags:
  - 网络
  - TCP
---

- TCP 在三次握手过程中会经过哪些状态？  
  主动方：`CLOSED` → 发送 SYN 后 `SYN_SENT` → 收到 SYN+ACK 后 `ESTABLISHED`。  
  被动方：`LISTEN` → 收到 SYN 后 `SYN_RCVD` → 收到第三次 ACK 后 `ESTABLISHED`。

- SYN_SENT 状态表示什么？  
  主动发起连接后等待对端回应 SYN+ACK，表明“我想建连，正在等确认”。

- SYN_RECV 状态表示什么？  
  被动端收到 SYN 并回了 SYN+ACK，等待客户端的最终 ACK，尚未完全建立；连接位于半连接队列。

- 服务端在什么时候进入 ESTABLISHED？  
  当收到客户端对 SYN+ACK 的 ACK 后，从 `SYN_RCVD` 转为 `ESTABLISHED`，此时才可收发数据。
