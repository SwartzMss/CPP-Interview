---
title: 高并发服务写路径（write → flush → writeback → fsync）时序
tags:
  - linux
  - io
---

## 一、整体时序图（核心）

文字版时序图（面试最实用）：

```
业务线程
│
│ write(fd, data)
│──────────────────────────────▶
│   拷贝数据到 Page Cache
│   标记 dirty page
│
│  【这里 write() 已返回】
│  【数据还在内存中】
│
│ flush / fflush (可选)
│──────────────────────────────▶
│   用户缓冲 → Page Cache
│
│  【仍然不保证落盘】
│
│                 （异步）
│                 writeback 线程
│                 ───────────────▶
│                   把 dirty page 写入磁盘
│                   清理 dirty 标记
│
│ fsync(fd)
│──────────────────────────────▶
│   等待：
│     1. 相关 dirty page writeback 完成
│     2. inode / metadata 落盘
│     3. 设备 cache flush
│
│  【fsync 返回 = 持久化承诺】
│
└──────────────────────────────▶ 磁盘
```

## 二、逐段踩坑解释

**① write() —— 快，但不安全**  
`write(fd, buf, len);`  
- 数据：用户态 → Page Cache，标记脏页。  
- 返回即走，不代表写盘。  
- 典型误解：write 成功 ≠ 数据安全。

**② flush / fflush —— 推进，不保证**  
`fflush(fp);`  
- 把用户态 buffer 推到 Page Cache 或触发写流程。  
-.flush 不等于 writeback；断电仍可能丢。  
- flush 解决“路径通不通”，不是“数据安不安全”。

**③ writeback —— 真正写盘，但你通常没感知**  
- 内核后台线程按 dirty_ratio/时间阈值异步刷盘。  
- 危险：脏页太多 → 写线程被拉去刷盘，write/fsync 卡住，线程进 D 状态，RT 抖动。

**④ fsync() —— 唯一的“我等你写完”**  
`fsync(fd);`  
- 强制 writeback，等待写完成与元数据落盘，刷新设备缓存。  
- fsync 返回成功 = Linux 语义上的持久化边界。

## 三、高并发服务需警惕的三件事

1) fsync 太频繁：每请求 fsync、多线程并发 fsync → IO 队列打爆，RT 爆炸。  
2) writeback 失控：写速率 > 磁盘能力，脏页堆积 → 业务线程被拖入慢路径。写() 只有在“系统还能兜住”的时候才是非阻塞的；一旦脏页失控，内核会故意把 write() 变慢来保护系统。  
3) flush 当 fsync 用：误以为 flush=持久化，实际从未等落盘，导致数据丢失事故。

## 四、对照表（面试官常问）

| 阶段      | 是否快 | 是否持久 | 是否阻塞 |
|-----------|--------|----------|----------|
| write     | ✅     | ❌       | ❌       |
| flush     | ✅     | ❌       | ❌       |
| writeback | ❌     | ⚠️      | ⚠️      |
| fsync     | ❌     | ✅       | ✅       |

## 五、30 秒口述画图版

写路径是 write 把数据写入 page cache 并标记为脏页，flush 只是把用户缓冲推进到内核，不提供持久性保证。真正的写盘在 writeback，由内核异步完成。fsync 是同步点，会等待相关脏页和元数据落盘并刷新设备缓存，fsync 返回才算持久化。高并发服务要控制 writeback 与 fsync 频率，避免业务线程被拖入慢路径。

## 六、如何定位类似问题（实战版）

### 先背一张总览判断表

| 现象 | 第一怀疑 |
| --- | --- |
| RT 飙升、QPS 掉、CPU 不高 | IO / writeback |
| load 高但 CPU idle | D 状态（IO wait） |
| write()/fsync() 偶发卡死 | writeback 或 block IO |
| 日志/落盘多时服务抖 | fsync 频繁 |
| 内存充足但系统慢 | page cache / dirty page |

### 第一步：确认是不是 IO / writeback

1) `vmstat 1` —— 首选  
关注 `b`（D 状态线程）、`wa`（IO 等待）、`bo`（写 IO）。  
结论模版：`b>0 且 wa 上升` → IO 阻塞，而非 CPU。

2) `iostat -x 1` —— 看磁盘瓶颈  
关注 `%util`（占满度）、`await`（延迟 ms）、`avgqu-sz`（队列深度）。  
特征：`%util≈100% + await 飙升` → writeback/fsync 放大尾延迟。

### 第二步：确认脏页 / writeback 是否失控

3) `/proc/meminfo`  
`Dirty` 大：写得快；`Writeback` 跟不上：刷得慢；两者都高：危险。

4) `/proc/vmstat`  
看 `nr_dirty`、`nr_writeback`、`pgwriteback`、`pgwriteback_throttled`。出现 throttled 基本实锤 writeback 在拖慢写线程。

5) `cat /proc/pressure/io`（PSI）  
`some/full` 的 `avg10/60/300`。`full` 抬头 → 系统整体被 IO 卡住。

### 第三步：是不是 fsync 在害你？

6) `strace -p <pid> -T -e trace=fsync,fdatasync`  
看到 fsync 频繁且每次几十 ms → fsync 放大 RT。

7) `perf top`（高阶）  
关键符号：`blk_mq_*`、`submit_bio`、`ext4_writepages`、`balance_dirty_pages`。出现 `balance_dirty_pages` ≈ writeback throttling。

### 第四步：线程是否被拖进 D 状态

8) `ps -eo pid,stat,comm | grep D` 或 `top`  
业务线程大量 D 态 → 内核 IO 阻塞，而非业务逻辑慢。

### 快速判断口诀

- RT 抖 + fsync 慢 → fsync 问题  
- RT 抖 + write 慢 + D 状态 → writeback 问题  
- flush 多 fsync 少 → 可能丢数据但不一定慢  
- `bo` 高 + `Dirty` 高 → writeback 在追赶

### 面试话术（完整定位路径示例）

我会先用 vmstat 和 iostat 判断是否 IO 阻塞；再看 meminfo/vmstat 的 dirty 与 writeback 是否失控；结合 PSI 的 IO 压力确认系统层面是否被拖慢。如果怀疑 fsync，就用 strace/perf 看 fsync 频率和耗时，并核对 D 状态线程数，最终把 RT 抖动和具体 IO 机制对应起来。
