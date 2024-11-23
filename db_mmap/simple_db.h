#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

// 数据库头部信息
struct DBHeader {
    uint32_t magic;          // 魔数，用于识别文件
    uint32_t version;        // 版本号
    uint64_t record_count;   // 记录数量
    uint64_t free_list;      // 空闲列表头
    uint64_t data_size;      // 数据区大小
};

// 记录结构
struct Record {
    uint32_t id;            // 记录ID
    uint32_t size;          // 数据大小
    uint32_t next;          // 下一条记录偏移
    char data[];            // 变长数据
};

// 数据库类
class SimpleDB {
private:
    int fd;                 // 文件描述符
    void* mapped_addr;      // 映射地址
    size_t file_size;       // 文件大小
    DBHeader* header;       // 数据库头部
    
public:
    SimpleDB(const char* filename, size_t initial_size = 1024*1024) {
        bool new_file = (access(filename, F_OK) == -1);
        
        // 打开或创建数据库文件
        fd = open(filename, O_RDWR | O_CREAT, 0666);
        if (fd == -1) {
            throw "Cannot open database file";
        }

        // 设置文件大小
        file_size = new_file ? initial_size : lseek(fd, 0, SEEK_END);
        if (new_file) {
            if (ftruncate(fd, file_size) == -1) {
                close(fd);
                throw "Cannot set file size";
            }
        }

        // 建立内存映射
        mapped_addr = mmap(NULL, file_size, 
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
        if (mapped_addr == MAP_FAILED) {
            close(fd);
            throw "Cannot map file";
        }

        header = (DBHeader*)mapped_addr;

        // 初始化新数据库
        if (new_file) {
            header->magic = 0x4244534D;    // "MSDB"
            header->version = 1;
            header->record_count = 0;
            header->free_list = 0;
            header->data_size = sizeof(DBHeader);
        }
    }

    ~SimpleDB() {
        if (mapped_addr != MAP_FAILED) {
            msync(mapped_addr, file_size, MS_SYNC);
            munmap(mapped_addr, file_size);
        }
        if (fd != -1) {
            close(fd);
        }
    }

    // 插入记录
    uint32_t insert(const void* data, size_t size) {
        // 计算需要的空间
        size_t total_size = sizeof(Record) + size;
        
        // 查找空闲空间或在文件末尾添加
        uint64_t offset;
        if (header->free_list && 
            get_record(header->free_list)->size >= total_size) {
            offset = header->free_list;
            header->free_list = get_record(offset)->next;
        } else {
            offset = header->data_size;
            header->data_size += total_size;
            
            // 检查是否需要扩展文件
            if (header->data_size > file_size) {
                size_t new_size = file_size * 2;
                while (new_size < header->data_size) {
                    new_size *= 2;
                }
                resize_file(new_size);
            }
        }

        // 写入记录
        Record* record = get_record(offset);
        record->id = header->record_count++;
        record->size = size;
        record->next = 0;
        memcpy(record->data, data, size);

        return record->id;
    }

    // 读取记录
    bool read(uint32_t id, void* buffer, size_t* size) {
        uint64_t offset = sizeof(DBHeader);
        while (offset < header->data_size) {
            Record* record = get_record(offset);
            if (record->id == id) {
                *size = record->size;
                memcpy(buffer, record->data, record->size);
                return true;
            }
            offset += sizeof(Record) + record->size;
        }
        return false;
    }

    // 删除记录
    bool remove(uint32_t id) {
        uint64_t offset = sizeof(DBHeader);
        while (offset < header->data_size) {
            Record* record = get_record(offset);
            if (record->id == id) {
                // 添加到空闲列表
                record->next = header->free_list;
                header->free_list = offset;
                return true;
            }
            offset += sizeof(Record) + record->size;
        }
        return false;
    }

private:
    // 获取指定偏移的记录
    Record* get_record(uint64_t offset) {
        return (Record*)((char*)mapped_addr + offset);
    }

    // 调整文件大小
    void resize_file(size_t new_size) {
        // 解除当前映射
        munmap(mapped_addr, file_size);
        
        // 调整文件大小
        ftruncate(fd, new_size);
        
        // 重新映射
        mapped_addr = mmap(NULL, new_size, 
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
        if (mapped_addr == MAP_FAILED) {
            throw "Cannot remap file";
        }
        
        header = (DBHeader*)mapped_addr;
        file_size = new_size;
    }
};