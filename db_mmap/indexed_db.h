#pragma once
#include "optimized_db.h"

// B+树索引结构
struct IndexNode {
    uint32_t keys[64];     // 键值
    uint64_t children[65]; // 子节点或数据指针
    uint32_t count;        // 键值数量
    bool is_leaf;          // 是否是叶子节点
};

class IndexedDB : public OptimizedDB {
private:
    IndexNode* root;
    
protected:
    // 使基类的protected成员在此类中可访问
    using OptimizedDB::mapped_addr;
    using OptimizedDB::header;
    using OptimizedDB::get_record;
    
public:
    IndexedDB(const char* filename) : OptimizedDB(filename), root(nullptr) {
        create_index();
    }

    // 添加索引支持
    void create_index() {
        // 分配索引空间
        uint64_t index_offset = header->data_size;
        header->data_size += sizeof(IndexNode);
        root = reinterpret_cast<IndexNode*>(static_cast<char*>(mapped_addr) + index_offset);
        
        // 初始化根节点
        root->count = 0;
        root->is_leaf = true;
    }

    // 索引查找
    Record* find_by_index(uint32_t key) {
        IndexNode* node = root;
        while (!node->is_leaf) {
            int i;
            for (i = 0; i < node->count; i++) {
                if (key < node->keys[i]) break;
            }
            node = reinterpret_cast<IndexNode*>(static_cast<char*>(mapped_addr) + node->children[i]);
        }
        
        // 在叶子节点中查找
        for (uint32_t i = 0; i < node->count; i++) {
            if (node->keys[i] == key) {
                return get_record(node->children[i]);
            }
        }
        return nullptr;
    }
};