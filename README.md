# ConcurrentMemoryPool

[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![Windows](https://img.shields.io/badge/Windows-10%2B-blue.svg)](https://www.microsoft.com/en-us/windows)
[![Linux](https://img.shields.io/badge/Linux-5.4%2B-blue.svg)](https://www.linux.org/)

高性能并发内存分配器，参考Google TCMalloc架构设计，专为解决多线程高并发场景下系统默认分配器的全局锁竞争与内存碎片问题而生。

## 🚀 项目简介

ConcurrentMemoryPool是一个高性能内存分配器，旨在替代多线程应用中的系统默认分配器（`malloc`/`free`）。它采用三级缓存架构，使大多数小对象分配实现"零锁竞争"，显著提升并发工作负载下的性能。

该项目遵循Google TCMalloc架构，同时添加了内存自动归还、系统遥测和伪共享消除等高级特性，以实现最优性能。

## 🌟 核心特性

### ✨ 极致性能架构

- **三级缓存体系**:
  - **ThreadCache (L1)**: 基于线程局部存储(TLS)的私有缓存，绝大多数小对象分配实现"零锁竞争"。
  - **CentralCache (L2)**: 采用细粒度"桶锁"管理跨线程内存调度，显著降低锁粒度。
  - **PageCache (L3)**: 负责向OS申请大块内存并进行Span切割与合并，有效缓解内存外部碎片。

- **底层优化**:
  - **伪共享消除**: 核心数据结构(`ThreadCache`, `SpanList`)按64字节强制对齐，确保每个线程的缓存和锁独占CPU缓存行，避免多核环境下的缓存失效风暴。
  - **基数树(Radix Tree)**: 替代哈希表实现PageID到Span的O(1)映射，大幅提升元数据查询效率并支持无锁读取。

### 📊 高可用与可观测性

- **内存归还机制(Memory Decay)**:
  - 引入Trim机制，智能识别并释放长时间闲置的Span物理内存(Decommit)给操作系统，同时保留虚拟地址映射。
  - 实测：在突发流量结束后的空闲期，可自动回收约70%的空闲缓存内存，防止长期运行服务的资源泄露。

- **全链路遥测(Telemetry)**:
  - 提供`GetStatistics`接口，实时监控系统申请总量、缓存Span/Page数量及碎片率，实现内存池的"白盒化"监控。

### ⚙️ 工程化实践

- **自举内存池(ObjectPool)**: 专为管理内部元数据设计的定长内存池，解决内存池启动时的递归依赖死锁问题。
- **现代化构建系统**: 基于CMake实现Windows/Linux跨平台构建支持。

## 🛠️ 构建与运行

### 环境要求

- **编译器**: MSVC (Visual Studio 2019+) / GCC 9+ / Clang 10+
- **构建工具**: CMake 3.15+

### 快速开始

```bash
git clone https://github.com/yourname/ConcurrentMemoryPool.git
cd ConcurrentMemoryPool
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
# 运行基准测试
./Benchmark
```

## 📊 性能基准

**测试环境**: Windows 10 / Visual Studio 2022 (Release x64)  
**测试场景**: 4线程并发，10轮次，每轮100,000次分配/释放操作(总计400万次)

| 分配器                   | 耗时(毫秒) | 性能提升  |
| ------------------------ | ---------- | --------- |
| 系统`malloc`/`free`      | 9924       | 基准线    |
| **ConcurrentMemoryPool** | **629**    | **15倍** |

*真实测试结果展示了在并发分配/释放工作负载下的显著性能提升。*

## 💡 使用示例

### 基础用法

```cpp
#include "ConcurrentAlloc.h"

void example_function() {
    void* p = ConcurrentAlloc(128); // 自动使用ThreadCache
    // ... 业务逻辑 ...
    ConcurrentFree(p);
}
```

### 维护与监控

```cpp
#include "PageCache.h"

void maintenance_task() {
    // 1. 获取内存统计信息
    auto stats = PageCache::GetInstance()->GetStatistics();
    
    // 2. 释放未使用的物理内存
    PageCache::GetInstance()->Trim();
}
```

## 📜 许可证

本项目采用MIT许可证 - 详情请参阅[LICENSE](LICENSE)文件。

## 📬 贡献指南

欢迎贡献！请随时提交问题或拉取请求。

1. Fork仓库
2. 创建功能分支 (`git checkout -b feature/awesome-feature`)
3. 提交更改 (`git commit -am 'Add some awesome feature'`)
4. 推送到分支 (`git push origin feature/awesome-feature`)
5. 创建拉取请求

## 📚 参考资料

- Google TCMalloc: https://github.com/gperftools/gperftools
- C++内存管理: https://www.boost.org/doc/libs/1_83_0/doc/html/interprocess/allocators.html

---

**注意**: 这是一个高性能内存分配器，专为多线程应用中的生产环境使用而设计。它并非旨在替代所有场景下的系统分配器，而是在特定高并发工作负载中提供显著的性能改进。
