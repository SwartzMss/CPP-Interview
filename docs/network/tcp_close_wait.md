---
title: CLOSE_WAIT 深度题（线上事故高频）
tags:
  - 网络
  - TCP
---

- CLOSE_WAIT 多说明什么问题？  
  说明本端已收到对端 FIN 并确认，但自己还没发 FIN 关闭发送方向，常因业务未关闭 socket 或线程卡住。

- CLOSE_WAIT 是协议问题还是代码问题？  
  多数是代码/资源管理问题：对端主动关闭后，应用层未调用 `close`/`shutdown`，连接停留在 `CLOSE_WAIT`。

- CLOSE_WAIT 会自动消失吗？  
  不会。除非应用主动发送 FIN 关闭，否则会一直停留，可能耗尽 fd。

- CLOSE_WAIT 和 TIME_WAIT 哪个更危险？  
  `CLOSE_WAIT` 更像应用层 bug，会占用 fd/内存直到进程崩溃；`TIME_WAIT` 是协议预期，主要是端口压力。

- 什么情况下服务端会大量进入 CLOSE_WAIT？  
  对端（通常客户端）主动断开，服务端收到 FIN 后未及时 `close`，例如连接池泄漏、处理线程阻塞、未注册断开事件、长时间卡在业务逻辑。

- recv 返回 0 和 CLOSE_WAIT 有什么关系？  
  `recv` 返回 0 表示对端已关闭发送方向（FIN 到达且已确认）；此刻本端进入 `CLOSE_WAIT`，应尽快完成剩余处理并 `close`。

- CLOSE_WAIT 会导致什么线上故障？  
  fd/内存泄漏、线程阻塞、accept 失败（`EMFILE`）、新连接被拒、服务端吞吐下降甚至崩溃。

- 如何快速定位 CLOSE_WAIT 的根因？  
  - `ss -tanp | grep CLOSE-WAIT` 看持有进程/端口；  
  - `lsof -p <pid> | grep TCP` 查具体 fd；  
  - 打印堆栈/火焰图，确认卡住的逻辑（可能在读写、锁、DB 调用）；  
  - 检查连接生命周期管理：是否缺少 `close`，是否异常路径未释放，连接池归还逻辑是否被跳过。
