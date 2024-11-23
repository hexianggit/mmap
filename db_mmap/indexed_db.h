#pragma once
#include "optimized_db.h"
#include <algorithm>

// B+树节点结构
struct IndexNode {
    static const int MAX_KEYS = 64;
    uint32_t keys[MAX_KEYS];     // 键值数组
    uint64_t children[MAX_KEYS + 1]; // 子节点或数据指针
    uint32_t count;        // 当前键值数量
    bool is_leaf;          // 是否是叶子节点
    uint64_t next;         // 叶子节点链表（用于范围查询）
};

class IndexedDB : public OptimizedDB {
private:
    IndexNode* root;
    uint64_t root_offset;  // 根节点在文件中的偏移
    
protected:
    using OptimizedDB::addr;
    using OptimizedDB::header;
    using OptimizedDB::get_record;
    
public:
    IndexedDB(const char* filename) : OptimizedDB(filename), root(nullptr) {
        if (header->version == 1) {
            // 新数据库，创建索引
            create_index();
        } else {
            // 加载已有索引
            root_offset = header->free_start;  // 使用 free_start 存储根节点位置
            root = get_node(root_offset);
        }
    }

    // 重写写入方法，维护索引
    uint64_t write(const void* data, size_t size) override {
        uint64_t pos = OptimizedDB::write(data, size);
        if (pos) {
            // 使用记录ID作为索引键
            RecordHeader* rec = get_record(pos);
            insert_index(rec->id, pos);
        }
        return pos;
    }

    // 使用索引进行查找
    bool read_by_id(uint32_t id, void* buffer, size_t* size) {
        uint64_t pos = find_by_index(id);
        if (pos == 0) return false;
        return read(pos, buffer, size);
    }

    // 范围查询
    std::vector<std::pair<uint32_t, uint64_t>> range_query(uint32_t start_key, uint32_t end_key) {
        std::vector<std::pair<uint32_t, uint64_t>> results;
        
        // 找到起始叶子节点
        IndexNode* leaf = find_leaf(start_key);
        while (leaf) {
            // 遍历叶子节点
            for (uint32_t i = 0; i < leaf->count; i++) {
                if (leaf->keys[i] >= start_key && leaf->keys[i] <= end_key) {
                    results.push_back({leaf->keys[i], leaf->children[i]});
                } else if (leaf->keys[i] > end_key) {
                    return results;
                }
            }
            
            // 移动到下一个叶子节点
            if (leaf->next == 0) break;
            leaf = get_node(leaf->next);
        }
        
        return results;
    }

private:
    // 创建索引
    void create_index() {
        // 分配根节点空间
        root_offset = allocate_node();
        header->free_start = root_offset;  // 存储根节点位置
        root = get_node(root_offset);
        
        // 初始化根节点
        root->count = 0;
        root->is_leaf = true;
        root->next = 0;
        
        // 更新版本号表示已创建索引
        header->version = 2;
    }

    // 分配新节点
    uint64_t allocate_node() {
        uint64_t offset = header->data_size;
        header->data_size += sizeof(IndexNode);
        return offset;
    }

    // 获取节点指针
    IndexNode* get_node(uint64_t offset) {
        return reinterpret_cast<IndexNode*>(static_cast<char*>(addr) + offset);
    }

    // 插入索引项
    void insert_index(uint32_t key, uint64_t value) {
        if (root->count == 0) {
            // 首次插入
            root->keys[0] = key;
            root->children[0] = value;
            root->count = 1;
            return;
        }

        // 检查是否需要分裂根节点
        if (root->count == IndexNode::MAX_KEYS) {
            uint64_t new_root_offset = allocate_node();
            IndexNode* new_root = get_node(new_root_offset);
            
            // 初始化新根节点
            new_root->is_leaf = false;
            new_root->count = 0;
            
            // 分裂旧根节点
            uint64_t new_node_offset = split_node(root_offset);
            
            // 设置新根节点的子节点
            new_root->children[0] = root_offset;
            new_root->children[1] = new_node_offset;
            new_root->keys[0] = get_node(new_node_offset)->keys[0];
            new_root->count = 1;
            
            // 更新根节点
            root_offset = new_root_offset;
            root = new_root;
            header->free_start = root_offset;
        }

        // 执行插入
        insert_non_full(root_offset, key, value);
    }

    // 向非满节点插入
    void insert_non_full(uint64_t node_offset, uint32_t key, uint64_t value) {
        IndexNode* node = get_node(node_offset);
        
        if (node->is_leaf) {
            // 在叶子节点中插入
            int i = node->count - 1;
            while (i >= 0 && key < node->keys[i]) {
                node->keys[i + 1] = node->keys[i];
                node->children[i + 1] = node->children[i];
                i--;
            }
            
            node->keys[i + 1] = key;
            node->children[i + 1] = value;
            node->count++;
            
        } else {
            // 在内部节点中查找子节点
            int i = node->count - 1;
            while (i >= 0 && key < node->keys[i]) {
                i--;
            }
            i++;
            
            uint64_t child_offset = node->children[i];
            IndexNode* child = get_node(child_offset);
            
            // 如果子节点已满，需要分裂
            if (child->count == IndexNode::MAX_KEYS) {
                uint64_t new_child_offset = split_node(child_offset);
                IndexNode* new_child = get_node(new_child_offset);
                
                if (key >= new_child->keys[0]) {
                    child_offset = new_child_offset;
                    child = new_child;
                }
            }
            
            insert_non_full(child_offset, key, value);
        }
    }

    // 分裂节点
    uint64_t split_node(uint64_t node_offset) {
        IndexNode* old_node = get_node(node_offset);
        uint64_t new_node_offset = allocate_node();
        IndexNode* new_node = get_node(new_node_offset);
        
        // 复制后半部分到新节点
        int mid = IndexNode::MAX_KEYS / 2;
        new_node->is_leaf = old_node->is_leaf;
        new_node->count = IndexNode::MAX_KEYS - mid;
        
        for (int i = 0; i < new_node->count; i++) {
            new_node->keys[i] = old_node->keys[mid + i];
            new_node->children[i] = old_node->children[mid + i];
        }
        
        // 更新旧节点
        old_node->count = mid;
        
        // 维护叶子节点链表
        if (old_node->is_leaf) {
            new_node->next = old_node->next;
            old_node->next = new_node_offset;
        }
        
        return new_node_offset;
    }

    // 查找叶子节点
    IndexNode* find_leaf(uint32_t key) {
        IndexNode* node = root;
        while (!node->is_leaf) {
            int i;
            for (i = 0; i < node->count; i++) {
                if (key < node->keys[i]) break;
            }
            node = get_node(node->children[i]);
        }
        return node;
    }

    // 通过索引查找记录位置
    uint64_t find_by_index(uint32_t key) {
        IndexNode* leaf = find_leaf(key);
        for (uint32_t i = 0; i < leaf->count; i++) {
            if (leaf->keys[i] == key) {
                return leaf->children[i];
            }
        }
        return 0;
    }
};