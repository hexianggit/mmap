#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <stdexcept>
#include <algorithm>

class BigFileProcessor {
private:
    static const size_t CHUNK_SIZE = 64 * 1024 * 1024;  // 64MB chunks
    static const size_t MAX_THREADS;                     // 最大线程数

    int fd;                     // 文件描述符
    void* mapped_addr;          // 映射地址
    size_t file_size;          // 文件大小
    std::vector<std::thread> workers;  // 工作线程
    std::mutex mtx;            // 互斥锁
    std::atomic<size_t> processed_chunks{0};  // 已处理块数
    
public:
    // 构造函数
    BigFileProcessor(const char* filename, bool create = false) 
        : fd(-1), mapped_addr(nullptr), file_size(0) {
        
        // 打开或创建文件
        fd = open(filename, 
                 create ? (O_RDWR | O_CREAT) : O_RDWR, 
                 0666);
        
        if (fd == -1) {
            throw std::runtime_error("Failed to open file");
        }

        try {
            // 获取文件大小
            struct stat sb;
            if (fstat(fd, &sb) == -1) {
                throw std::runtime_error("Failed to get file size");
            }
            file_size = sb.st_size;

            // 建立内存映射
            mapped_addr = mmap(NULL, file_size, 
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
                             
            if (mapped_addr == MAP_FAILED) {
                throw std::runtime_error("Failed to map file");
            }

            // 设置内存访问模式
            madvise(mapped_addr, file_size, MADV_RANDOM);
        } catch (...) {
            if (fd != -1) {
                close(fd);
            }
            throw;
        }
    }

    // 析构函数
    ~BigFileProcessor() {
        if (mapped_addr != MAP_FAILED && mapped_addr != nullptr) {
            munmap(mapped_addr, file_size);
        }
        if (fd != -1) {
            close(fd);
        }
    }

    // 并行处理文件
    template<typename Func>
    void process_parallel(Func processor) {
        size_t chunk_count = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
        size_t thread_count = std::min(
            MAX_THREADS, 
            static_cast<size_t>((chunk_count + 1) / 2)
        );

        // 创建工作线程
        for (size_t i = 0; i < thread_count; ++i) {
            workers.emplace_back(&BigFileProcessor::worker_thread<Func>, 
                               this, processor, chunk_count);
        }

        // 等待所有线程完成
        for (auto& worker : workers) {
            worker.join();
        }
        workers.clear();
    }

    // 创建大文件
    static void create_big_file(const char* filename, size_t size) {
        int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (fd == -1) {
            throw std::runtime_error("Failed to create file");
        }

        // 设置文件大小
        if (ftruncate(fd, size) == -1) {
            close(fd);
            throw std::runtime_error("Failed to set file size");
        }

        close(fd);
    }

private:
    // 工作线程函数
    template<typename Func>
    void worker_thread(Func processor, size_t chunk_count) {
        while (true) {
            // 获取下一个要处理的块
            size_t current_chunk = processed_chunks++;
            if (current_chunk >= chunk_count) {
                break;
            }

            // 计算块的范围
            size_t start = current_chunk * CHUNK_SIZE;
            size_t end = std::min(start + CHUNK_SIZE, file_size);
            
            // 处理数据块
            char* chunk_start = static_cast<char*>(mapped_addr) + start;
            processor(chunk_start, end - start);
        }
    }
};

// 在类外定义静态成员变量
const size_t BigFileProcessor::MAX_THREADS = 8;


// 示例3：文件搜索
void search_in_file(const char* filename, const char* pattern) {
    BigFileProcessor processor(filename);
    std::mutex print_mutex;
    std::atomic<size_t> match_count{0};
    
    processor.process_parallel([&](char* data, size_t size) {
        std::string chunk(data, size);
        size_t pos = 0;
        while ((pos = chunk.find(pattern, pos)) != std::string::npos) {
            match_count++;
            std::lock_guard<std::mutex> lock(print_mutex);
            printf("Found match at position: %zu\n", pos);
            pos++;
        }
    });
    
    printf("Total matches found: %zu\n", match_count.load());
}

// 示例4：文件压缩
void compress_file(const char* filename) {
    BigFileProcessor processor(filename);
    std::vector<std::vector<char>> compressed_chunks;
    std::mutex chunks_mutex;
    
    processor.process_parallel([&](char* data, size_t size) {
        // 压缩数据块
        std::vector<char> compressed = compress_data(data, size);
        
        // 保存压缩后的数据
        std::lock_guard<std::mutex> lock(chunks_mutex);
        compressed_chunks.push_back(std::move(compressed));
    });
    
    // 合并压缩后的数据
    write_compressed_file(filename, compressed_chunks);
}

int main() {
    try {
        const char* filename = "bigfile.dat";
        const size_t FILE_SIZE = 1ULL * 1024 * 1024 * 1024;  // 1GB
        
        // 创建测试文件
        printf("Creating test file...\n");
        BigFileProcessor::create_big_file(filename, FILE_SIZE);
        
        // 打开文件进行处理
        BigFileProcessor processor(filename);
        
        // 示例1：计算文件中非零字节的数量
        std::atomic<size_t> nonzero_count{0};
        
        printf("Processing file...\n");
        processor.process_parallel([&nonzero_count](char* data, size_t size) {
            size_t local_count = 0;
            for (size_t i = 0; i < size; ++i) {
                if (data[i] != 0) {
                    local_count++;
                }
            }
            nonzero_count += local_count;
        });
        
        printf("Non-zero bytes: %zu\n", nonzero_count.load());
        
        // 示例2：将所有字节加1
        printf("Modifying file...\n");
        processor.process_parallel([](char* data, size_t size) {
            for (size_t i = 0; i < size; ++i) {
                data[i]++;
            }
        });
        
        printf("File processing completed.\n");
        
    } catch (const std::exception& e) {
        printf("Error: %s\n", e.what());
        return 1;
    }
    
    return 0;
}