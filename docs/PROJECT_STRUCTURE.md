# 项目结构说明

## ? 目录结构

```
High-Concurrency-Memory-Pool/
├── include/                 # 头文件目录
│   ├── Common.h            # 公共定义和工具类
│   ├── ThreadCache.h       # 线程缓存类声明
│   ├── CentralCache.h      # 中央缓存类声明
│   ├── PageCache.h         # 页缓存类声明
│   ├── RadixTree.h         # 基数树实现
│   ├── ObjectPool.h        # 对象池实现
│   └── ConcurrencyAlloc.h  # 对外统一接口
├── src/                    # 源文件目录
│   ├── ThreadCache.cpp     # 线程缓存实现
│   ├── CentralCache.cpp    # 中央缓存实现
│   └── PageCache.cpp       # 页缓存实现
├── tests/                  # 测试文件目录
│   ├── Test.cpp           # 功能测试
│   ├── BenchMark.cpp      # 性能测试
│   └── RadixTreeTest.cpp  # 基数树测试
├── docs/                   # 文档目录
│   ├── README.md          # 详细项目文档
│   └── PROJECT_STRUCTURE.md # 项目结构说明
├── build/                  # 编译输出目录
│   ├── test              # 功能测试可执行文件
│   ├── benchmark         # 性能测试可执行文件
│   └── radix_test        # 基数树测试可执行文件
├── Makefile               # 构建脚本
└── README.md              # 项目简介
```

## ? 目录职责

### include/ - 头文件目录
包含所有的头文件，提供清晰的接口定义：

- **Common.h**: 项目的基础设施
  - 常量定义 (MAX_MEMORYSIZE, MAX_BUCKETSIZE等)
  - 工具函数 (NextObj, SystemAlloc等)
  - 基础数据结构 (FreeList, SizeClass, Span, SpanList)

- **ThreadCache.h**: 线程本地缓存
  - 每线程私有的内存缓存
  - 无锁快速分配接口

- **CentralCache.h**: 中央缓存
  - 全局共享的中央缓存
  - 单例模式设计

- **PageCache.h**: 页缓存
  - 大块内存页管理
  - 页合并算法

- **RadixTree.h**: 基数树优化
  - 高效的页号到Span映射
  - 替代哈希表的稀疏数据结构

- **ObjectPool.h**: 对象池
  - 高效的对象内存管理
  - 避免频繁new/delete

- **ConcurrencyAlloc.h**: 统一接口
  - 对外提供的内存分配接口
  - 自动选择分配策略

### src/ - 源文件目录
包含核心功能的实现：

- **ThreadCache.cpp**: 线程缓存实现
  - 慢启动反馈调节算法
  - 与CentralCache的交互

- **CentralCache.cpp**: 中央缓存实现
  - Span的切分和管理
  - 桶锁机制

- **PageCache.cpp**: 页缓存实现
  - 页的分配和回收
  - 相邻页合并算法

### tests/ - 测试目录
包含各种测试程序：

- **Test.cpp**: 基础功能测试
  - 验证内存池的基本功能
  - 正确性测试

- **BenchMark.cpp**: 性能基准测试
  - 与标准malloc/free对比
  - 多线程性能测试

- **RadixTreeTest.cpp**: 基数树专项测试
  - 基数树正确性验证
  - 与哈希表性能对比

### docs/ - 文档目录
包含项目文档：

- **README.md**: 完整的项目文档
- **PROJECT_STRUCTURE.md**: 项目结构说明

### build/ - 编译输出目录
存放编译生成的可执行文件和中间文件：

- 所有编译产物都在此目录
- 便于清理和管理
- 避免污染源码目录

## ? 构建系统

### Makefile特性
- **模块化设计**: 清晰的目录变量定义
- **自动依赖**: 自动处理头文件依赖关系
- **多种构建模式**: 发布版、调试版、性能分析版
- **集成测试**: 一键运行所有测试
- **代码质量工具**: 集成静态分析和格式化

### 常用命令
```bash
# 编译所有程序
make all

# 运行所有测试
make run-all

# 调试版本
make debug

# 内存检查
make memcheck

# 性能分析
make run-profile

# 代码检查
make cppcheck

# 清理
make clean
```

## ? 优势

### 1. 清晰的模块分离
- 头文件与实现分离
- 测试代码独立
- 文档集中管理

### 2. 易于维护
- 目录结构清晰
- 职责明确
- 便于扩展

### 3. 开发友好
- 快速定位文件
- 编译产物隔离
- 统一的构建流程

### 4. 专业化
- 符合C++项目标准结构
- 便于团队协作
- 易于CI/CD集成

## ? 开发工作流

### 1. 添加新功能
```bash
# 1. 在include/目录添加头文件
# 2. 在src/目录添加实现文件
# 3. 在tests/目录添加测试文件
# 4. 更新Makefile（如需要）
# 5. 编译测试
make all && make run-all
```

### 2. 调试问题
```bash
# 编译调试版本
make debug

# 内存检查
make memcheck

# 性能分析
make run-profile
```

### 3. 代码质量
```bash
# 静态分析
make cppcheck

# 代码格式化
make format
```

这种结构化的组织方式大大提升了项目的可维护性和开发效率！