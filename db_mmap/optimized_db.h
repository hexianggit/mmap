#pragma once
#include "simple_db.h"
#include <unordered_map>
#include <ctime>
#include <sys/mman.h>
#include <cstdlib>
#include <cstring>

#define PAGE_SIZE 4096

class OptimizedDB : public SimpleDB {
private:
    // 页面缓存
    struct Page {
        void* data;
        bool dirty;
        uint64_t last_access;
    };
    std::unordered_map<size_t, Page> page_cache;
    
protected:
    // 使基类的protected成员在此类中可访问
    using SimpleDB::mapped_addr;
    using SimpleDB::file_size;
    using SimpleDB::header;
    
public:
    OptimizedDB(const char* filename) : SimpleDB(filename) {
        // 使用大页面
        #ifdef MADV_HUGEPAGE
        madvise(mapped_addr, file_size, MADV_HUGEPAGE);
        #endif
        
        // 预读取
        madvise(mapped_addr, file_size, MADV_WILLNEED);
        
        // 随机访问模式
        madvise(mapped_addr, file_size, MADV_RANDOM);
    }

    ~OptimizedDB() {
        // 清理页面缓存
        for (auto& pair : page_cache) {
            free(pair.second.data);
        }
    }

    // 实现页面缓存
    void* get_page(size_t page_num) {
        auto it = page_cache.find(page_num);
        if (it != page_cache.end()) {
            it->second.last_access = std::time(nullptr);
            return it->second.data;
        }

        // 加载新页面
        void* page_data = std::malloc(PAGE_SIZE);
        std::memcpy(page_data, 
                   static_cast<char*>(mapped_addr) + page_num * PAGE_SIZE, 
                   PAGE_SIZE);
        
        page_cache[page_num] = {page_data, false, std::time(nullptr)};
        return page_data;
    }

    // 实现批量操作
    void batch_insert(const std::vector<std::pair<void*, size_t>>& records) {
        // 开始事务
        for (const auto& record : records) {
            insert(static_cast<const char*>(record.first), record.second);
        }
        // 提交事务
        msync(mapped_addr, file_size, MS_ASYNC);
    }
};