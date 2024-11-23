#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// 数据库文件头部结构
struct DBHeader {
    char magic[4];        // 魔数 "MMDB"
    uint32_t version;     // 版本号
    uint64_t size;        // 文件总大小
    uint64_t data_start;  // 数据区起始位置
    uint64_t free_start;  // 空闲链表起始位置
};

// 数据记录头部
struct RecordHeader {
    uint32_t size;        // 数据大小
    uint32_t flags;       // 标志位（0=有效，1=已删除）
    uint64_t next;        // 下一条记录的偏移量
};

class SimpleDB {
protected:
    int fd;               // 文件描述符
    void* addr;           // 映射基地址
    size_t mapped_size;   // 映射大小
    DBHeader* header;     // 文件头指针

public:
    SimpleDB(const char* filename);
    ~SimpleDB();

    // 基本操作
    uint64_t write(const void* data, size_t size);
    bool read(uint64_t pos, void* buffer, size_t* size);
    bool remove(uint64_t pos);

protected:
    // 内部工具方法
    void init_header();
    bool extend_mapping(size_t new_size);
    RecordHeader* get_record(uint64_t pos);
};