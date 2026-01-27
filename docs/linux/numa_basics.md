---
title: NUMA 基础与调优要点
tags:
  - linux
  - numa
---

## 一、NUMA 是什么（能口述的版本）

- Non-Uniform Memory Access：每个 CPU 节点（socket）有自己的本地内存，访问本节点内存延迟/带宽最佳，跨节点要经过互联（QPI/UPI/Infinity Fabric）。  
- “NUMA penalty” 本质：跨节点访问 → 更高延迟、更低带宽、更多链路竞争。  
- OS 视角：节点 = 一组 CPU + 一段本地内存，进程/线程/页都有“节点归属”。

## 二、为什么会踩坑

- 线程跑在节点 A，但大块内存第一次分配时由节点 B 上的线程触发，页落在节点 B；后续访问跨节点，RT 抖动。  
- 绑定 CPU 但没绑定内存（或反之）→ 局部性被破坏。  
- 默认 “first-touch” 策略：谁第一次写页，页就落在哪个 NUMA 节点；初始化线程和工作线程节点不一致时最容易出问题。  
- 跨节点 cache coherency 也要额外流量，写密集场景影响更大。

## 三、常见现象与判定

- 单机多路 CPU，业务线程数上去后延迟陡升，但 CPU 占用不高；`perf`/`mpstat` 看到 `NUMA other`、`remote node` 访问多。  
- `numastat -p <pid>`：`numa_miss` / `numa_foreign` 升高，说明跨节点访问明显。  
- `perf stat -e node-local-misses,remote-node-misses` 或 `perf mem record` 能直观看到本地/远程比例。  
- `/sys/devices/system/node/` 下看拓扑，确认节点数与 CPU 亲和关系。

## 四、工程调优手段（按成本排序）

1) **进程/线程绑核 + 绑内存**  
   - `numactl -C 0-15 -m 0 ./app` 或在代码里 `sched_setaffinity` + `mbind/set_mempolicy`。  
   - 原则：线程跑在哪个节点，就在该节点分配主要热数据。
2) **初始化与工作线程同节点**  
   - 避免“初始化线程在 node0，工作线程在 node1”导致 first-touch 错位。  
   - 常见做法：在每个节点启动本地 worker，由它们自己完成大数组的首次写入。
3) **分片内存布局**  
   - 按节点拆分大数组/队列，线程只操作本节点分片，减少跨节点共享。  
   - MPSC/队列类结构，改为 per-node 队列 + 汇聚。
4) **跨节点共享无法避免时**  
   - 选择读多写少的数据结构，或用批量提交降低 coherency 流量。  
   - 把锁/队列放到读多写少的节点，或使用无锁但减少跨节点写的设计。
5) **谨慎关闭 NUMA**  
   - 某些业务为简单直接会用 `numactl --interleave=all` 或 BIOS 关 NUMA，但这通常是吞吐换延迟的权衡，优先做局部性优化。

## 五、面试 30 秒速答范式

1) 先定义：NUMA = 每个 socket 有本地内存，远程访问更慢。  
2) 说明坑点：first-touch 错位、跨节点访问、coherency 放大延迟。  
3) 给定位手段：`numastat -p` 看 miss，`numa_miss` 高就是远程访问；`perf` 事件看 remote 访问。  
4) 给优化思路：绑核绑内存、初始化与工作线程同节点、按节点分片数据结构。  
5) 总结：NUMA 问题本质是“局部性被打破”，解决方案就是“让计算和数据在同一节点相遇”。
