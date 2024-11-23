#include "simple_db.h"
#include <stdio.h>

int main() {
    try {
        SimpleDB db("test.db");

        // 写入测试
        const char* test_data = "Hello, Memory Mapped Database!";
        uint64_t pos = db.write(test_data, strlen(test_data) + 1);
        printf("Written at position: %lu\n", pos);

        // 读取测试
        char buffer[1024];
        size_t size;
        if (db.read(pos, buffer, &size)) {
            printf("Read data: %s\n", buffer);
        }

        // 删除测试
        if (db.remove(pos)) {
            printf("Record deleted\n");
        }

    } catch (const char* err) {
        printf("Error: %s\n", err);
        return 1;
    }

    return 0;
}