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



<img width="1023" height="304" alt="image" src="https://github.com/user-attachments/assets/d4f887cd-7630-4955-b971-8e828a532a44" />

<img width="1073" height="318" alt="image" src="https://github.com/user-attachments/assets/a8d6430c-5874-4857-a920-9ee1c9de5ea2" />

<img width="1463" height="766" alt="image" src="https://github.com/user-attachments/assets/a26fd17d-3f66-4ae9-8f19-ab5fce672736" />


## 后记（Postscript）

本项目对自研的并发内存池与标准 `malloc/free` 在多线程场景下进行了跨平台基准测试（Linux 64 位与 Windows 32 位，均为 Release 构建）。为了避免计时口径差异带来的误读，我们统一改用 `std::chrono::steady_clock` 统计毫秒级耗时，并确保两侧“线程数 / 轮次 / 每轮次数”完全一致后再对比。

### 结果要点

1. 稳定的相对优势：在典型的小对象高并发分配/回收工作负载下，自研内存池相较标准 `malloc/free` 通常可获得约 **3–5 倍**的吞吐提升与延迟降低（不同平台、不同尺寸分布会有波动）。
2. 跨平台一致性：两端均体现出“线程私有缓存 + 中心缓存 + 页级管理”的设计效果；无论在 Linux 还是 Windows，上述结构都显著减少了系统调用与锁竞争的频率。
3. 绝对值差异的来源：Windows 32 位下指针与结构体更小、缓存局部性更好，常见工作集更易进入 L1/L2，绝对耗时往往低于 Linux 64 位；这属于架构与实现差异导致的正常现象。

### 计时与口径

1. 统一问题说明：早期版本若使用 `clock()` 直接打印 tick 值，不同平台的 `CLOCKS_PER_SEC`（如 1000 与 1,000,000）会导致“单位看起来不一致”的假象。
2. 现行规范做法：使用 `steady_clock::now()` 取得起止时间，配合 `duration_cast<milliseconds>` 转换为毫秒统一输出；报表字段与日志文案严格匹配单位，避免歧义。

### 复现实验（建议）

1. 编译：使用 Release 配置（推荐 `-O2/-O3 -DNDEBUG`），关闭多余日志与断言；Windows 使用 Release 配置，Linux/WSL 建议 `-O3 -DNDEBUG -pthread`。
2. 负载：统一线程数、轮次与每轮次数；覆盖代表性尺寸分布（如 [1, 8 KiB] 的小对象）。
3. 环境：如在 WSL 下测试，优先 WSL2 或原生 Linux，以减少虚拟化/系统调用转译的干扰。

### 开发者笔记

1. 数据结构：`SpanList` 采用哨兵节点与无锁内部实现（由外层持桶锁），避免构造期递归与多余开销；`FreeList` 在空闲块头部复用指针存储降低元数据成本。
2. SizeClass 策略：分组对齐与位运算索引在实际负载下对 CPU 前端与分支预测友好，减少除法与取模。
3. 可维护性：统一计时、明确单位、固定基准参数能显著提高跨平台比较的可信度；建议将基准脚本与构建参数纳入 CI，以持续监控回归。

### 局限与展望

1. 局限：当前基准侧重小对象与短生命周期，对大块对象与跨 NUMA 的表现仍需扩展测试；碎片化长期演化与后台合并策略的影响需要更长时间窗口观测。
2. 展望：后续将评估 64 位架构下更激进的缓存对齐策略、页合并/拆分的启发式优化；同时考虑使用 C++20/26 的编译期表生成（如 `consteval`）进一步降低运行期开销，并引入更完备的可视化 profiling 报告以辅助定位热点路径。

**小结：**在面向高并发的小对象分配场景中，本项目的内存池在两大主流平台上均表现出稳定而显著的优势。统一计时口径与测试参数后，结果具有良好的可重复性，可作为后续优化与工程落地的可靠基线。



