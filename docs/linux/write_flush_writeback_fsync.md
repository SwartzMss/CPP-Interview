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
2) writeback 失控：写速率 > 磁盘能力，脏页堆积 → 业务线程被拖入慢路径。  
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
