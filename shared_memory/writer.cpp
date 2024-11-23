// writer.cpp
#include "shared_memory.h"
#include <unistd.h>

int main() {
    printf("Starting writer process...\n");
    
    // 创建共享内存
    SharedMemory shm("/my_shared_memory");
    SharedData* shared_data = shm.get_data();
    
    // 初始化共享数据
    shared_data->message_count = 0;
    shared_data->ready = 0;

    // 循环写入消息
    for (int i = 1; i <= 5; i++) {
        // 等待读取进程处理完前一条消息
        while (shared_data->ready == 1) {
            usleep(1000); // 短暂休眠
        }

        // 写入新消息
        snprintf(shared_data->message, sizeof(shared_data->message), 
                "Message %d from writer", i);
        shared_data->message_count = i;
        
        // 设置就绪标志
        shared_data->ready = 1;
        
        printf("Wrote message: %s\n", shared_data->message);
        
        sleep(1); // 等待1秒
    }

    printf("Writer finished.\n");
    return 0;
}