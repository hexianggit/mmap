#include "simple_db.h"

SimpleDB::SimpleDB(const char* filename) {
    // 打开或创建数据库文件
    fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        throw "Cannot open database file";
    }

    // 获取文件大小
    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        throw "Cannot get file size";
    }

    // 新文件初始化
    bool is_new = (st.st_size == 0);
    mapped_size = is_new ? 4096 : st.st_size;
    
    if (is_new) {
        if (ftruncate(fd, mapped_size) == -1) {
            close(fd);
            throw "Cannot set initial file size";
        }
    }

    // 建立内存映射
    addr = mmap(NULL, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        throw "Cannot map file";
    }

    header = (DBHeader*)addr;
    if (is_new) {
        init_header();
    } else if (memcmp(header->magic, "MMDB", 4) != 0) {
        munmap(addr, mapped_size);
        close(fd);
        throw "Invalid database file";
    }
}

SimpleDB::~SimpleDB() {
    if (addr != MAP_FAILED) {
        msync(addr, mapped_size, MS_SYNC);
        munmap(addr, mapped_size);
    }
    if (fd != -1) {
        close(fd);
    }
}

void SimpleDB::init_header() {
    memcpy(header->magic, "MMDB", 4);
    header->version = 1;
    header->size = mapped_size;
    header->data_start = sizeof(DBHeader);
    header->free_start = 0;
}

bool SimpleDB::extend_mapping(size_t new_size) {
    // 解除旧映射
    munmap(addr, mapped_size);
    
    // 扩展文件
    if (ftruncate(fd, new_size) == -1) {
        return false;
    }

    // 建立新映射
    addr = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        return false;
    }

    header = (DBHeader*)addr;
    header->size = new_size;
    mapped_size = new_size;
    return true;
}

uint64_t SimpleDB::write(const void* data, size_t size) {
    size_t total_size = sizeof(RecordHeader) + size;
    
    // 检查是否需要扩展文件
    if (header->data_start + total_size > mapped_size) {
        size_t new_size = mapped_size * 2;
        while (new_size < header->data_start + total_size) {
            new_size *= 2;
        }
        if (!extend_mapping(new_size)) {
            return 0;
        }
    }

    // 写入数据
    uint64_t pos = header->data_start;
    RecordHeader* rec = get_record(pos);
    rec->size = size;
    rec->flags = 0;
    rec->next = header->data_start + total_size;
    memcpy(rec + 1, data, size);

    header->data_start += total_size;
    return pos;
}

bool SimpleDB::read(uint64_t pos, void* buffer, size_t* size) {
    if (pos >= header->data_start) {
        return false;
    }

    RecordHeader* rec = get_record(pos);
    if (rec->flags & 1) {  // 已删除
        return false;
    }

    *size = rec->size;
    memcpy(buffer, rec + 1, rec->size);
    return true;
}

bool SimpleDB::remove(uint64_t pos) {
    if (pos >= header->data_start) {
        return false;
    }

    RecordHeader* rec = get_record(pos);
    if (rec->flags & 1) {  // 已删除
        return false;
    }

    rec->flags |= 1;  // 标记为已删除
    return true;
}

RecordHeader* SimpleDB::get_record(uint64_t pos) {
    return (RecordHeader*)((char*)addr + pos);
} 