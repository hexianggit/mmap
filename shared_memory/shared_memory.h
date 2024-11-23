// shared_memory.h
#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 共享内存结构体
typedef struct {
    int message_count;     // 消息计数
    char message[1024];    // 消息内容
    int ready;            // 数据就绪标志
} SharedData;

// 共享内存类
class SharedMemory {
private:
    SharedData* data;     // 共享内存指针
    int fd;              // 文件描述符
    const char* name;    // 共享内存名称

public:
    SharedMemory(const char* shm_name) : name(shm_name) {
        fd = shm_open(name, O_CREAT | O_RDWR, 0666);
        if (fd == -1) {
            perror("shm_open failed");
            exit(1);
        }

        // 设置共享内存大小
        if (ftruncate(fd, sizeof(SharedData)) == -1) {
            perror("ftruncate failed");
            exit(1);
        }

        // 映射共享内存
        data = (SharedData*)mmap(NULL, sizeof(SharedData), 
                                PROT_READ | PROT_WRITE, 
                                MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) {
            perror("mmap failed");
            exit(1);
        }
    }

    ~SharedMemory() {
        munmap(data, sizeof(SharedData));
        close(fd);
        shm_unlink(name);
    }

    SharedData* get_data() { return data; }
};

#endif