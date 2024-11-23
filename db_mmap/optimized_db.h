#pragma once
#include "simple_db.h"
#include <unordered_map>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>

class OptimizedDB : public SimpleDB {
private:
    // 页面缓存结构
    struct CachePage {
        void* data;
        bool dirty;
        std::chrono::steady_clock::time_point last_access;
        std::mutex mutex;
    };

    // 缓存配置
    static const size_t PAGE_SIZE = 4096;
    static const size_t MAX_CACHE_PAGES = 1000;
    static const size_t BATCH_SIZE = 1024;
    
    std::unordered_map<uint64_t, CachePage> page_cache;
    std::mutex cache_mutex;
    std::vector<std::pair<uint64_t, size_t>> write_buffer;
    std::mutex buffer_mutex;

public:
    OptimizedDB(const char* filename) : SimpleDB(filename) {
        // 启用大页面支持
        #ifdef MADV_HUGEPAGE
        madvise(addr, mapped_size, MADV_HUGEPAGE);
        #endif
        
        // 预读取
        madvise(addr, mapped_size, MADV_WILLNEED);
        
        // 设置随机访问模式
        madvise(addr, mapped_size, MADV_RANDOM);

        // 启动后台刷新线程
        start_background_flush();
    }

    ~OptimizedDB() override {
        stop_background_flush();
        flush_all();
        clear_cache();
    }

    // 重写写入方法，使用写缓冲
    uint64_t write(const void* data, size_t size) override {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        
        uint64_t pos = SimpleDB::write(data, size);
        write_buffer.push_back({pos, size});

        // 如果缓冲区满，则刷新
        if (write_buffer.size() >= BATCH_SIZE) {
            flush_buffer();
        }

        return pos;
    }

    // 重写读取方法，使用页面缓存
    bool read(uint64_t pos, void* buffer, size_t* size) override {
        uint64_t page_num = pos / PAGE_SIZE;
        CachePage* page = get_cached_page(page_num);
        
        if (!page) return SimpleDB::read(pos, buffer, size);

        std::lock_guard<std::mutex> lock(page->mutex);
        return SimpleDB::read(pos, buffer, size);
    }

    // 批量写入接口
    void batch_write(const std::vector<std::pair<const void*, size_t>>& records, 
                    std::vector<uint64_t>& positions) {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        positions.reserve(records.size());

        for (const auto& record : records) {
            uint64_t pos = SimpleDB::write(record.first, record.second);
            positions.push_back(pos);
            write_buffer.push_back({pos, record.second});
        }

        if (write_buffer.size() >= BATCH_SIZE) {
            flush_buffer();
        }
    }

private:
    bool should_stop = false;
    std::thread flush_thread;

    // 获取缓存页面
    CachePage* get_cached_page(uint64_t page_num) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        
        auto it = page_cache.find(page_num);
        if (it != page_cache.end()) {
            it->second.last_access = std::chrono::steady_clock::now();
            return &it->second;
        }

        // 如果缓存已满，清理最旧的页面
        if (page_cache.size() >= MAX_CACHE_PAGES) {
            evict_oldest_page();
        }

        // 加载新页面
        void* page_data = malloc(PAGE_SIZE);
        memcpy(page_data, 
               (char*)addr + page_num * PAGE_SIZE, 
               PAGE_SIZE);

        CachePage& page = page_cache[page_num];
        page.data = page_data;
        page.dirty = false;
        page.last_access = std::chrono::steady_clock::now();
        
        return &page;
    }

    // 清理最旧的页面
    void evict_oldest_page() {
        auto oldest = page_cache.begin();
        for (auto it = page_cache.begin(); it != page_cache.end(); ++it) {
            if (it->second.last_access < oldest->second.last_access) {
                oldest = it;
            }
        }

        if (oldest->second.dirty) {
            flush_page(oldest->first, &oldest->second);
        }
        free(oldest->second.data);
        page_cache.erase(oldest);
    }

    // 刷新单个页面
    void flush_page(uint64_t page_num, CachePage* page) {
        std::lock_guard<std::mutex> lock(page->mutex);
        if (!page->dirty) return;

        memcpy((char*)addr + page_num * PAGE_SIZE,
               page->data,
               PAGE_SIZE);
        
        msync((char*)addr + page_num * PAGE_SIZE,
              PAGE_SIZE,
              MS_ASYNC);
        
        page->dirty = false;
    }

    // 刷新写缓冲
    void flush_buffer() {
        if (write_buffer.empty()) return;
        
        msync(addr, mapped_size, MS_ASYNC);
        write_buffer.clear();
    }

    // 刷新所有缓存
    void flush_all() {
        flush_buffer();
        
        std::lock_guard<std::mutex> lock(cache_mutex);
        for (auto& pair : page_cache) {
            flush_page(pair.first, &pair.second);
        }
        
        msync(addr, mapped_size, MS_SYNC);
    }

    // 清理缓存
    void clear_cache() {
        std::lock_guard<std::mutex> lock(cache_mutex);
        for (auto& pair : page_cache) {
            free(pair.second.data);
        }
        page_cache.clear();
    }

    // 启动后台刷新线程
    void start_background_flush() {
        flush_thread = std::thread([this]() {
            while (!should_stop) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                flush_buffer();
            }
        });
    }

    // 停止后台刷新线程
    void stop_background_flush() {
        should_stop = true;
        if (flush_thread.joinable()) {
            flush_thread.join();
        }
    }
};