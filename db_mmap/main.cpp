#include "optimized_db.h"
#include <vector>
#include <string>

int main() {
    try {
        OptimizedDB db("test.db");

        // 单条写入
        const char* data1 = "Hello, Optimized DB!";
        uint64_t pos1 = db.write(data1, strlen(data1) + 1);

        // 批量写入
        std::vector<std::pair<const void*, size_t>> records;
        std::vector<std::string> strings = {
            "Record 1",
            "Record 2",
            "Record 3"
        };
        
        for (const auto& s : strings) {
            records.push_back({s.c_str(), s.length() + 1});
        }

        std::vector<uint64_t> positions;
        db.batch_write(records, positions);

        // 读取测试
        char buffer[1024];
        size_t size;
        
        if (db.read(pos1, buffer, &size)) {
            printf("Read first record: %s\n", buffer);
        }

        for (uint64_t pos : positions) {
            if (db.read(pos, buffer, &size)) {
                printf("Read batch record: %s\n", buffer);
            }
        }

    } catch (const char* err) {
        printf("Error: %s\n", err);
        return 1;
    }

    return 0;
}