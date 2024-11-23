#include "simple_db.h"
#include "optimized_db.h"
#include "indexed_db.h"
#include <string>
#include <memory>

void print_usage(const char* program) {
    printf("Usage: %s <db_type> [db_file]\n", program);
    printf("  db_type: simple, optimized, or indexed\n");
    printf("  db_file: database file path (default: test.db)\n");
}

// 基类指针包装器，用于统一接口
class DBWrapper {
public:
    virtual ~DBWrapper() = default;
    virtual uint32_t insert(const char* data, size_t size) = 0;
    virtual bool read(uint32_t id, char* buffer, size_t* size) = 0;
    virtual bool remove(uint32_t id) = 0;
};

// SimpleDB包装器
class SimpleDBWrapper : public DBWrapper {
    SimpleDB db;
public:
    SimpleDBWrapper(const char* filename) : db(filename) {}
    uint32_t insert(const char* data, size_t size) override { return db.insert(data, size); }
    bool read(uint32_t id, char* buffer, size_t* size) override { return db.read(id, buffer, size); }
    bool remove(uint32_t id) override { return db.remove(id); }
};

// OptimizedDB包装器
class OptimizedDBWrapper : public DBWrapper {
    OptimizedDB db;
public:
    OptimizedDBWrapper(const char* filename) : db(filename) {}
    uint32_t insert(const char* data, size_t size) override { return db.insert(data, size); }
    bool read(uint32_t id, char* buffer, size_t* size) override { return db.read(id, buffer, size); }
    bool remove(uint32_t id) override { return db.remove(id); }
};

// IndexedDB包装器
class IndexedDBWrapper : public DBWrapper {
    IndexedDB db;
public:
    IndexedDBWrapper(const char* filename) : db(filename) {}
    uint32_t insert(const char* data, size_t size) override { return db.insert(data, size); }
    bool read(uint32_t id, char* buffer, size_t* size) override { return db.read(id, buffer, size); }
    bool remove(uint32_t id) override { return db.remove(id); }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string db_type = argv[1];
    std::string db_file = (argc > 2) ? argv[2] : "test.db";
    
    try {
        // 根据参数创建相应的数据库实例
        std::unique_ptr<DBWrapper> db;
        
        if (db_type == "simple") {
            db = std::make_unique<SimpleDBWrapper>(db_file.c_str());
        } else if (db_type == "optimized") {
            db = std::make_unique<OptimizedDBWrapper>(db_file.c_str());
        } else if (db_type == "indexed") {
            db = std::make_unique<IndexedDBWrapper>(db_file.c_str());
        } else {
            printf("Unknown database type: %s\n", db_type.c_str());
            print_usage(argv[0]);
            return 1;
        }

        // 测试数据库操作
        const char* data1 = "Hello, Database!";
        uint32_t id1 = db->insert(data1, strlen(data1) + 1);
        printf("Inserted record with id: %u\n", id1);

        char buffer[1024];
        size_t size;
        if (db->read(id1, buffer, &size)) {
            printf("Read record: %s\n", buffer);
        }

        if (db->remove(id1)) {
            printf("Record deleted\n");
        }

    } catch (const char* err) {
        printf("Error: %s\n", err);
        return 1;
    }

    return 0;
}