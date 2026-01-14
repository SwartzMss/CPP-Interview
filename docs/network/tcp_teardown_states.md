---
title: TCP 挥手状态问答
tags:
  - 网络
  - TCP
---

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
