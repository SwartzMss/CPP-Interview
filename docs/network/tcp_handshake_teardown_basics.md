---
title: TCP 三次握手 / 四次挥手基础问答
tags:
  - 网络
  - TCP
---

## 三次握手基础

- TCP 三次握手的完整流程是怎样的？  
  1) 客户端发送 SYN（同步序列号）并进入 `SYN_SENT`。  
  2) 服务端收到后回 SYN+ACK，记录半连接进入 `SYN_RCVD`。  
  3) 客户端收到后回 ACK，双方进入 `ESTABLISHED`，服务端将连接从半连接队列转入全连接队列。

- 每一次握手分别起什么作用？  
  第一次：客户端表明要建立连接并告知自己的初始序列号。  
  第二次：服务端确认收到客户端 SYN（ACK=客户端 ISN+1），同时告知自己的 ISN（SYN）。  
  第三次：客户端确认收到服务端的 SYN+ACK，告知服务端“你可以开始发数据了”，确保服务端不会因旧 SYN 重放而误建连接。

- 三次握手中 SYN、ACK 各自代表什么含义？  
  SYN 标志位表示“我要同步序列号、请求建立连接”；ACK 标志位表示当前报文包含确认号 `ack`，确认对端的数据或 SYN。

- 初始序列号（ISN）是做什么用的？  
  确定双方发送序列空间起点，防止旧连接的延迟报文被当作新连接数据；通常基于时间/随机函数生成以降低序列号预测风险。

- SYN 包里通常会携带哪些 TCP 选项？  
  常见有 MSS、窗口扩大因子（Window Scale）、时间戳（RFC 7323，用于 RTT 估计/PAWS 防旧包）、SACK 允许、TCP Fast Open（如启用）。这些只在握手时协商，后续沿用。

## 四次挥手基础

- TCP 四次挥手的完整流程是怎样的？  
  1) 主动关闭方发送 FIN，进入 `FIN_WAIT_1`。  
  2) 被动关闭方回 ACK，进入 `CLOSE_WAIT`；主动方收到后进入 `FIN_WAIT_2`。  
  3) 被动关闭方处理完剩余数据后发送 FIN，进入 `LAST_ACK`。  
  4) 主动关闭方回 ACK，进入 `TIME_WAIT`；等待 2MSL 后真正关闭。被动方收到 ACK 即 `CLOSED`。

- 为什么断开连接比建立连接多一次？  
  关闭是全双工的，每个方向都需要单独发 FIN/收 ACK，拆成两对消息以允许一端先“说完再挂断”；因此需要四次而非三次。

- FIN 和 ACK 在挥手中的角色分别是什么？  
  FIN 表示“我这一侧的数据发送完毕，关闭发送方向”；ACK 确认对端 FIN 已收到，避免重复挥手和旧 FIN 干扰。

- TCP 为什么要支持“半关闭”？  
  允许一端停止发送但继续接收（例如客户端发送完请求后仍要接收服务端的响应），提高灵活性；若不支持半关闭，无法表达“我不再发数据但还要收”的常见模式。

## 握手状态

- TCP 在三次握手过程中会经过哪些状态？  
  主动方：`CLOSED` → 发送 SYN 后 `SYN_SENT` → 收到 SYN+ACK 后 `ESTABLISHED`。  
  被动方：`LISTEN` → 收到 SYN 后 `SYN_RCVD` → 收到第三次 ACK 后 `ESTABLISHED`。

- SYN_SENT 状态表示什么？  
  主动发起连接后等待对端回应 SYN+ACK，表明“我想建连，正在等确认”。

- SYN_RECV 状态表示什么？  
  被动端收到 SYN 并回了 SYN+ACK，等待客户端的最终 ACK，尚未完全建立；连接位于半连接队列。

- 服务端在什么时候进入 ESTABLISHED？  
  当收到客户端对 SYN+ACK 的 ACK 后，从 `SYN_RCVD` 转为 `ESTABLISHED`，此时才可收发数据。

## 挥手状态

- TCP 在四次挥手过程中会经过哪些状态？  
  主动关闭方：`ESTABLISHED` → 发 FIN 后 `FIN_WAIT_1` → 收 ACK 后 `FIN_WAIT_2` → 收对端 FIN 后回 ACK 进入 `TIME_WAIT` → 超过 2MSL 后 `CLOSED`。  
  被动关闭方：`ESTABLISHED` → 收 FIN 后回 ACK 进入 `CLOSE_WAIT` → 发 FIN 后进入 `LAST_ACK` → 收 ACK 后 `CLOSED`。

- FIN_WAIT_1 和 FIN_WAIT_2 的区别是什么？  
  `FIN_WAIT_1`：已发 FIN，等待对端 ACK。  
  `FIN_WAIT_2`：对端已确认 FIN，等待对端发 FIN 关闭它的发送方向。

- CLOSE_WAIT 是什么时候进入的？  
  当被动方收到对端 FIN 并回 ACK 后进入，表示“对方不再发数据，我还可以发完自己的数据”。

- LAST_ACK 是谁进入的？  
  被动关闭方在发送自己的 FIN 后进入 `LAST_ACK`，等待对端对该 FIN 的 ACK。

- TIME_WAIT 是什么时候进入的？  
  主动关闭方在收到对端 FIN 并回 ACK 后进入 `TIME_WAIT`，等待 2MSL 以防止 ACK 丢失或旧报文干扰。

## TIME_WAIT 深度题（高频重点）

- 为什么需要 TIME_WAIT？  
  确保最后一个 ACK 能被对端收到（若丢失还能重发），并让旧连接的延迟报文在 2MSL 内自然过期，避免污染后续新连接。

- 为什么 TIME_WAIT 要等 2MSL？  
  1 个 MSL 用于旧报文在网络中传播并被丢弃；再加 1 个 MSL 预留 ACK 重传往返时间，保证对端重传 FIN 时我们仍在等待并能回应。

- 为什么 TIME_WAIT 在主动关闭方？  
  主动关闭方发出最后的 ACK，只有它才知道是否需要重传该 ACK；因此由它持有 TIME_WAIT 保证可靠收尾。

- 如果没有 TIME_WAIT 会发生什么？  
  旧连接的 FIN/ACK 或数据包可能被迟到的新连接误收，导致异常关闭或数据错乱；对端若重传 FIN 也得不到 ACK，出现“半关闭”悬挂。

- TIME_WAIT 多是 bug 吗？  
  不一定。短连接或客户端发起连接的场景出现大量 TIME_WAIT 很常见，是协议设计。需要分析是否吞吐设计问题（过多短连）或端口复用策略不足。

- TIME_WAIT 会不会耗尽端口？  
  在高 QPS 短连接下可能耗尽主动方的本地端口。常见缓解：连接复用/长连接、负载分担到更多源 IP/端口、合理的 TIME_WAIT 回收策略。

- 能不能消除 TIME_WAIT？  
  不能彻底消除，只能谨慎缩短或复用。手段包括：  
  - 应用层复用连接或使用连接池，减少触发次数；  
  - 拆分源地址/端口（多 IP、SO_REUSEPORT）；  
  - 调整内核参数如 `tcp_tw_reuse`（仅对主动连接生效），`tcp_fin_timeout` 适度下调；  
  - 部署在负载均衡/代理后端，减少直接外连短链接数量。  
  需权衡旧包污染风险，避免激进关闭 TIME_WAIT。

## CLOSE_WAIT 深度题（线上事故高频）

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
