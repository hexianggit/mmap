#include "indexed_db.h"
#include <vector>
#include <string>

int main() {
    try {
        IndexedDB db("test.db");

        // 写入测试数据
        const char* data1 = "Record One";
        const char* data2 = "Record Two";
        const char* data3 = "Record Three";

        uint64_t pos1 = db.write(data1, strlen(data1) + 1);
        uint64_t pos2 = db.write(data2, strlen(data2) + 1);
        uint64_t pos3 = db.write(data3, strlen(data3) + 1);

        // 按ID读取
        char buffer[1024];
        size_t size;
        
        if (db.read_by_id(1, buffer, &size)) {
            printf("Read record #1: %s\n", buffer);
        }

        // 范围查询
        auto results = db.range_query(1, 3);
        for (const auto& result : results) {
            if (db.read(result.second, buffer, &size)) {
                printf("Range query result [ID=%u]: %s\n", 
                       result.first, buffer);
            }
        }

    } catch (const char* err) {
        printf("Error: %s\n", err);
        return 1;
    }

    return 0;
}