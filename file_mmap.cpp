#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

class MappedFile {
private:
    void* addr;      // 映射的内存地址
    size_t size;     // 文件大小
    bool writable;   // 是否可写

public:
    // 构造函数，filename: 文件名，write_mode: 是否以写模式打开
    MappedFile(const char* filename, bool write_mode = false) 
        : addr(nullptr), size(0), writable(write_mode) {
        
        // 打开文件
        int flags = write_mode ? O_RDWR : O_RDONLY;
        int fd = open(filename, flags);
        if (fd == -1) {
            throw std::runtime_error("Failed to open file");
        }

        try {
            // 获取文件信息
            struct stat sb;
            if (fstat(fd, &sb) == -1) {
                close(fd);
                throw std::runtime_error("Failed to get file size");
            }
            size = sb.st_size;

            // 设置映射保护标志
            int prot = PROT_READ;
            if (write_mode) {
                prot |= PROT_WRITE;
            }

            // 创建内存映射
            addr = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
            if (addr == MAP_FAILED) {
                close(fd);
                throw std::runtime_error("Failed to map file");
            }

            // 关闭文件描述符（映射仍然有效）
            close(fd);
        } catch (...) {
            close(fd);
            throw;
        }
    }

    // 禁用拷贝构造和赋值
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    // 移动构造函数
    MappedFile(MappedFile&& other) noexcept
        : addr(other.addr), size(other.size), writable(other.writable) {
        other.addr = nullptr;
        other.size = 0;
    }

    // 移动赋值运算符
    MappedFile& operator=(MappedFile&& other) noexcept {
        if (this != &other) {
            cleanup();
            addr = other.addr;
            size = other.size;
            writable = other.writable;
            other.addr = nullptr;
            other.size = 0;
        }
        return *this;
    }

    // 析构函数
    ~MappedFile() {
        cleanup();
    }

    // 获取映射的内存地址
    void* get_addr() const { return addr; }
    
    // 获取文件大小
    size_t get_size() const { return size; }

    // 读取指定位置的数据
    bool read_at(size_t offset, void* buffer, size_t length) const {
        if (offset + length > size) {
            return false;
        }
        memcpy(buffer, static_cast<char*>(addr) + offset, length);
        return true;
    }

    // 写入数据到指定位置
    bool write_at(size_t offset, const void* buffer, size_t length) {
        if (!writable || offset + length > size) {
            return false;
        }
        memcpy(static_cast<char*>(addr) + offset, buffer, length);
        return true;
    }

    // 同步内存到文件
    bool sync() {
        if (!writable) {
            return false;
        }
        return msync(addr, size, MS_SYNC) == 0;
    }

private:
    void cleanup() {
        if (addr) {
            munmap(addr, size);
            addr = nullptr;
        }
    }
};

// 使用示例
int main() {
    try {
        // 创建测试文件
        const char* test_filename = "test.txt";
        const char* test_data = "Hello, Memory Mapping!";
        
        // 写入初始数据到文件
        {
            FILE* fp = fopen(test_filename, "w");
            if (!fp) {
                throw std::runtime_error("Failed to create test file");
            }
            fputs(test_data, fp);
            fclose(fp);
        }

        // 以读写模式打开文件
        MappedFile file(test_filename, true);
        std::cout << "File size: " << file.get_size() << " bytes\n";

        // 读取数据
        char buffer[100];
        if (file.read_at(0, buffer, file.get_size())) {
            buffer[file.get_size()] = '\0';
            std::cout << "Read data: " << buffer << std::endl;
        }

        // 写入新数据
        const char* new_data = "Modified content";
        if (file.write_at(0, new_data, strlen(new_data))) {
            std::cout << "Data written successfully\n";
            file.sync();  // 同步到文件
        }

        // 再次读取验证
        if (file.read_at(0, buffer, strlen(new_data))) {
            buffer[strlen(new_data)] = '\0';
            std::cout << "New data: " << buffer << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}