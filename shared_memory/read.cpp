// reader.cpp
#include "shared_memory.h"
#include <unistd.h>

int main() {
    printf("Starting reader process...\n");
    
    // 打开共享内存
    SharedMemory shm("/my_shared_memory");
    SharedData* shared_data = shm.get_data();
    
    // 循环读取消息
    int last_count = 0;
    while (last_count < 5) { // 读取5条消息后退出
        // 检查是否有新消息
        if (shared_data->ready == 1) {
            printf("Read message %d: %s\n", 
                   shared_data->message_count, 
                   shared_data->message);
            
            // 更新计数
            last_count = shared_data->message_count;
            
            // 清除就绪标志
            shared_data->ready = 0;
        }
        
        usleep(100000); // 短暂休眠
    }

    printf("Reader finished.\n");
    return 0;
}