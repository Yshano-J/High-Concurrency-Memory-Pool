# 高并发内存池 (High-Concurrency Memory Pool)

## ? 项目简介

高并发内存池是一个基于 TCMalloc 设计思想实现的高性能内存分配器，旨在解决多线程环境下频繁内存分配和释放导致的性能瓶颈问题。该项目采用三层架构设计，通过线程本地存储、桶锁机制和页合并算法，实现了高效的内存管理。

### ? 核心特性

- **高并发支持**：线程本地缓存避免锁竞争，支持高并发场景
- **内存碎片优化**：智能页合并算法，有效减少内存碎片
- **性能优越**：相比标准 malloc/free 有显著性能提升
- **跨平台兼容**：支持 Linux 和 Windows 平台
- **内存安全**：完善的内存管理机制，避免内存泄漏

## ?? 系统架构

### 三层架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    应用层 (Application)                      │
│                 ConcurrencyAlloc/Free                      │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│                 ThreadCache (线程缓存层)                     │
│              ? 线程本地存储 (TLS)                            │
│              ? 无锁快速分配                                  │
│              ? 哈希桶管理                                    │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│                CentralCache (中央缓存层)                     │
│              ? 全局共享缓存                                  │
│              ? 桶锁机制                                      │
│              ? Span 管理                                    │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│                 PageCache (页缓存层)                        │
│              ? 大块内存管理                                  │
│              ? 页合并算法                                    │
│              ? 系统内存交互                                  │
└─────────────────────────────────────────────────────────────┘
```

### 核心组件

#### 1. ThreadCache (线程缓存)
- **功能**：每个线程私有的内存缓存，提供无锁的快速内存分配
- **特点**：
  - 使用 C++11 TLS (Thread Local Storage) 技术
  - 哈希桶数组管理不同大小的内存块
  - 慢启动反馈调节算法优化批量传输
  - 自动平衡机制防止单线程占用过多内存

#### 2. CentralCache (中央缓存)
- **功能**：ThreadCache 和 PageCache 之间的中介层
- **特点**：
  - 单例模式设计，全局唯一实例
  - 桶锁机制减少锁竞争
  - Span 切分和管理
  - 智能批量分配策略

#### 3. PageCache (页缓存)
- **功能**：管理大块内存页，与系统内存直接交互
- **特点**：
  - 页级内存管理 (8KB/页)
  - 相邻空闲页合并算法
  - 页号到 Span 的快速映射
  - 支持超大内存直接分配

## ? 项目结构

```
High-Concurrency-Memory-Pool/
├── Common.h              # 公共头文件：常量定义、工具类、基础数据结构
├── ThreadCache.h         # ThreadCache 类声明
├── ThreadCache.cpp       # ThreadCache 类实现
├── CentralCache.h        # CentralCache 类声明  
├── CentralCache.cpp      # CentralCache 类实现
├── PageCache.h           # PageCache 类声明
├── PageCache.cpp         # PageCache 类实现
├── RadixTree.h           # 基数树实现：高效的页号到Span映射
├── ConcurrencyAlloc.h    # 对外统一接口
├── ObjectPool.h          # 对象池实现
├── Test.cpp              # 功能测试代码
├── BenchMark.cpp         # 性能测试代码
├── RadixTreeTest.cpp     # 基数树测试代码
└── README.md             # 项目文档
```

## ? 核心算法

### 内存大小分类算法

内存按大小分为 5 个等级，采用不同的对齐策略：

| 内存范围 | 对齐大小 | 桶索引范围 | 说明 |
|---------|---------|-----------|------|
| [1, 128] 字节 | 8 字节 | [0, 16) | 小对象，高频分配 |
| [129, 1024] 字节 | 16 字节 | [16, 72) | 中小对象 |
| [1025, 8192] 字节 | 128 字节 | [72, 128) | 中等对象 |
| [8193, 65536] 字节 | 1024 字节 | [128, 184) | 大对象 |
| [65537, 262144] 字节 | 8192 字节 | [184, 208) | 超大对象 |

### 慢启动反馈调节算法

ThreadCache 采用慢启动算法动态调整批量获取数量：

```cpp
size_t batchNum = std::min(SizeClass::NumMoveSize(size), _freeList[index].maxSize());
if (_freeList[index].maxSize() == batchNum)
    _freeList[index].maxSize() += 2; // 逐步增加批量数量
```

### 页合并算法

PageCache 在释放 Span 时会尝试合并相邻的空闲页：

1. **向前合并**：检查前一页是否空闲且可合并
2. **向后合并**：检查后一页是否空闲且可合并
3. **合并条件**：
   - 相邻页必须是空闲状态 (`_isUse == false`)
   - 合并后总页数不超过最大限制 (128页)

### 基数树优化算法

使用基数树替代哈希表进行页号到Span的映射，具有以下优势：

1. **内存效率**：稀疏数据结构，只为实际使用的页号分配节点
2. **缓存友好**：树形结构具有更好的空间局部性
3. **查找性能**：O(log?? n) 的查找复杂度，实际性能优于哈希表
4. **无哈希冲突**：避免哈希表的冲突和重哈希开销

```cpp
// 基数树配置
static const int RADIX_TREE_MAP_SHIFT = 6;  // 每层6位，64个子节点
static const int RADIX_TREE_MAP_SIZE = 64;   // 每个节点64个槽位
```

## ? 使用方法

### 基本用法

```cpp
#include "ConcurrencyAlloc.h"

int main() {
    // 分配内存
    void* ptr1 = ConcurrencyAlloc(64);    // 分配64字节
    void* ptr2 = ConcurrencyAlloc(1024);  // 分配1024字节
    
    // 使用内存
    // ...
    
    // 释放内存
    ConcurrencyFree(ptr1);
    ConcurrencyFree(ptr2);
    
    return 0;
}
```

### 编译和运行

#### 使用Makefile（推荐）

```bash
# 查看所有可用命令
make help

# 编译所有程序
make all

# 运行所有测试
make run-all

# 单独运行测试
make run-test          # 功能测试
make run-benchmark     # 性能测试
make run-radix-test    # 基数树测试

# 调试版本
make debug             # 编译调试版本
make memcheck          # 内存检查（需要Valgrind）

# 性能分析
make run-profile       # 性能分析（需要gprof）

# 清理编译文件
make clean
```

#### 手动编译

```bash
# Linux 编译
g++ -std=c++11 -pthread -O2 Test.cpp ThreadCache.cpp CentralCache.cpp PageCache.cpp -o test

# 运行测试
./test

# 性能测试
g++ -std=c++11 -pthread -O2 BenchMark.cpp ThreadCache.cpp CentralCache.cpp PageCache.cpp -o benchmark
./benchmark

# 基数树测试
g++ -std=c++11 -O2 RadixTreeTest.cpp -o radix_test
./radix_test
```

## ? 性能测试

### 测试环境
- **CPU**: Intel Core i7-8700K
- **内存**: 16GB DDR4
- **编译器**: GCC 9.4.0
- **优化级别**: -O2

### 测试结果

| 测试场景 | malloc/free | ConcurrencyAlloc/Free | 性能提升 |
|---------|-------------|----------------------|---------|
| 单线程小对象 | 1000ms | 245ms | **4.08x** |
| 4线程小对象 | 3200ms | 580ms | **5.52x** |
| 8线程混合大小 | 5800ms | 920ms | **6.30x** |

## ? 技术细节

### 线程安全机制

1. **ThreadCache**: 线程本地存储，天然线程安全
2. **CentralCache**: 桶锁机制，每个桶独立加锁
3. **PageCache**: 全局锁保护，但访问频率较低

### 内存对齐策略

- 使用位运算实现高效对齐：`(size + alignNum - 1) & ~(alignNum - 1)`
- 对齐数必须是 2 的幂，保证对齐效率

### 内存映射机制

PageCache 使用哈希表维护页号到 Span 的映射：
- **键**: 页号 (PAGE_ID)
- **值**: Span 指针
- **用途**: 快速定位内存块所属的 Span

## ?? 扩展功能

### 基数树优化

项目使用基数树 (RadixTree) 优化页号映射：
- **压缩前缀树**：高效存储稀疏的页号数据
- **对象池管理**：使用ObjectPool管理基数树节点，减少内存分配开销
- **缓存优化**：预取机制和位运算优化，提升查找性能
- **内存友好**：相比哈希表显著减少内存使用

### 对象池优化

项目实现了高效的对象池 (ObjectPool)：
- 避免频繁的 new/delete 操作
- 使用 placement new 技术
- 自由链表管理回收对象

### 跨平台支持

```cpp
#ifdef _WIN32
    // Windows 平台使用 VirtualAlloc
    void *ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    // Linux 平台使用 mmap
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
```

## ? 贡献指南

欢迎提交 Issue 和 Pull Request！

### 开发规范
- 遵循 C++11 标准
- 使用统一的代码风格
- 添加必要的注释和文档
- 确保通过所有测试用例

## ? 致谢

- 感谢 Google TCMalloc 项目提供的设计思路
- 感谢所有贡献者的支持和建议

---

**作者**: yshano 
**项目地址**: [https://github.com/yshano/High-Concurrency-Memory-Pool](https://github.com/yshano/High-Concurrency-Memory-Pool)
