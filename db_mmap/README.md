# Memory Mapped Database

基于内存映射(mmap)技术实现的三种数据库，分别提供基础、优化和索引功能。

## 数据库类型

### 1. SimpleDB - 基础版本
基于内存映射的简单数据库实现，提供基础的数据存储和访问功能。

特性：
- 基于mmap的文件映射
- 记录的写入、读取和删除
- 自动文件大小管理
- 简单的空闲空间管理

### 2. OptimizedDB - 优化版本
在SimpleDB基础上添加了多项性能优化特性。

特性：
- 页面缓存（LRU策略）
- 写入缓冲
- 异步刷新
- 批量写入支持
- 大页面支持
- 内存访问优化
- 后台刷新线程

优化点：
- 使用页面缓存减少磁盘访问
- 批量写入提高写入性能
- 异步刷新减少IO等待
- 使用大页面减少TLB缺失

### 3. IndexedDB - 索引版本
在OptimizedDB基础上添加了B+树索引支持。

特性：
- B+树索引结构
- 快速键值查找
- 范围查询支持
- 顺序遍历支持
- 自动索引维护

索引优势：
- O(log n)的查找复杂度
- 支持范围查询
- 支持顺序访问
- 数据自动平衡

## 编译和使用

### 编译要求
- C++11或更高版本
- POSIX兼容系统（Linux/Unix）
- pthread支持

### 编译命令 
```bash
g++ -o db_test simple_db.cpp main.cpp -pthread

# 写入数据
./db_test indexed write "Hello World"

# 读取数据
./db_test indexed read 1

# 范围查询
./db_test indexed range 1 10

# 批量写入测试
./db_test optimized batch 1000 "Record-"
```