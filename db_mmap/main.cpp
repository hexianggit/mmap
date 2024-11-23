#include "simple_db.h"
#include "optimized_db.h"
#include "indexed_db.h"
#include <cstring>

void print_usage(const char* program) {
    printf("Usage: %s <db_type> [command] [args...]\n\n", program);
    printf("Database Types:\n");
    printf("  simple     - Simple memory mapped database\n");
    printf("  optimized  - Optimized database with caching\n");
    printf("  indexed    - B+ tree indexed database\n\n");
    printf("Commands:\n");
    printf("  write <data>           - Write data to database\n");
    printf("  read <id>              - Read data by ID\n");
    printf("  delete <id>            - Delete data by ID\n");
    printf("  range <start> <end>    - Range query (indexed only)\n");
    printf("  batch <count> <prefix> - Batch write test\n\n");
    printf("Example:\n");
    printf("  %s indexed write \"Hello World\"\n", program);
    printf("  %s optimized batch 1000 \"Record-\"\n", program);
}

// 基类包装器
class DBWrapper {
public:
    virtual ~DBWrapper() = default;
    virtual uint64_t write(const void* data, size_t size) = 0;
    virtual bool read(uint64_t pos, void* buffer, size_t* size) = 0;
    virtual bool remove(uint64_t pos) = 0;
    virtual bool read_by_id(uint32_t id, void* buffer, size_t* size) { return false; }
    virtual void batch_write(int count, const char* prefix) {}
    virtual void range_query(uint32_t start, uint32_t end) {}
};

// SimpleDB包装器
class SimpleDBWrapper : public DBWrapper {
    SimpleDB db;
public:
    SimpleDBWrapper() : db("simple.db") {}
    uint64_t write(const void* data, size_t size) override {
        return db.write(data, size);
    }
    bool read(uint64_t pos, void* buffer, size_t* size) override {
        return db.read(pos, buffer, size);
    }
    bool remove(uint64_t pos) override {
        return db.remove(pos);
    }
};

// OptimizedDB包装器
class OptimizedDBWrapper : public DBWrapper {
    OptimizedDB db;
public:
    OptimizedDBWrapper() : db("optimized.db") {}
    uint64_t write(const void* data, size_t size) override {
        return db.write(data, size);
    }
    bool read(uint64_t pos, void* buffer, size_t* size) override {
        return db.read(pos, buffer, size);
    }
    bool remove(uint64_t pos) override {
        return db.remove(pos);
    }
    void batch_write(int count, const char* prefix) override {
        std::vector<std::pair<const void*, size_t>> records;
        std::vector<uint64_t> positions;
        char buffer[1024];
        
        for (int i = 0; i < count; i++) {
            snprintf(buffer, sizeof(buffer), "%s%d", prefix, i);
            size_t len = strlen(buffer) + 1;
            void* data = malloc(len);
            memcpy(data, buffer, len);
            records.push_back({data, len});
        }
        
        db.batch_write(records, positions);
        
        // 清理内存
        for (const auto& record : records) {
            free((void*)record.first);
        }
    }
};

// IndexedDB包装器
class IndexedDBWrapper : public DBWrapper {
    IndexedDB db;
public:
    IndexedDBWrapper() : db("indexed.db") {}
    uint64_t write(const void* data, size_t size) override {
        return db.write(data, size);
    }
    bool read(uint64_t pos, void* buffer, size_t* size) override {
        return db.read(pos, buffer, size);
    }
    bool remove(uint64_t pos) override {
        return db.remove(pos);
    }
    bool read_by_id(uint32_t id, void* buffer, size_t* size) override {
        return db.read_by_id(id, buffer, size);
    }
    void range_query(uint32_t start, uint32_t end) override {
        auto results = db.range_query(start, end);
        char buffer[1024];
        size_t size;
        
        for (const auto& result : results) {
            if (db.read(result.second, buffer, &size)) {
                printf("ID=%u: %s\n", result.first, buffer);
            }
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        // 创建数据库实例
        std::unique_ptr<DBWrapper> db;
        const char* db_type = argv[1];
        const char* command = argv[2];

        if (strcmp(db_type, "simple") == 0) {
            db = std::make_unique<SimpleDBWrapper>();
        } else if (strcmp(db_type, "optimized") == 0) {
            db = std::make_unique<OptimizedDBWrapper>();
        } else if (strcmp(db_type, "indexed") == 0) {
            db = std::make_unique<IndexedDBWrapper>();
        } else {
            printf("Unknown database type: %s\n", db_type);
            print_usage(argv[0]);
            return 1;
        }

        // 执行命令
        if (strcmp(command, "write") == 0) {
            if (argc < 4) {
                printf("Write command requires data argument\n");
                return 1;
            }
            uint64_t pos = db->write(argv[3], strlen(argv[3]) + 1);
            printf("Written at position: %lu\n", pos);

        } else if (strcmp(command, "read") == 0) {
            if (argc < 4) {
                printf("Read command requires ID argument\n");
                return 1;
            }
            char buffer[1024];
            size_t size;
            uint32_t id = atoi(argv[3]);
            
            if (db->read_by_id(id, buffer, &size)) {
                printf("Read by ID %u: %s\n", id, buffer);
            } else {
                printf("Record not found\n");
            }

        } else if (strcmp(command, "delete") == 0) {
            if (argc < 4) {
                printf("Delete command requires position argument\n");
                return 1;
            }
            uint64_t pos = strtoull(argv[3], NULL, 10);
            if (db->remove(pos)) {
                printf("Record deleted\n");
            } else {
                printf("Delete failed\n");
            }

        } else if (strcmp(command, "range") == 0) {
            if (argc < 5) {
                printf("Range command requires start and end arguments\n");
                return 1;
            }
            uint32_t start = atoi(argv[3]);
            uint32_t end = atoi(argv[4]);
            db->range_query(start, end);

        } else if (strcmp(command, "batch") == 0) {
            if (argc < 5) {
                printf("Batch command requires count and prefix arguments\n");
                return 1;
            }
            int count = atoi(argv[3]);
            db->batch_write(count, argv[4]);
            printf("Batch write completed\n");

        } else {
            printf("Unknown command: %s\n", command);
            print_usage(argv[0]);
            return 1;
        }

    } catch (const char* err) {
        printf("Error: %s\n", err);
        return 1;
    }

    return 0;
}