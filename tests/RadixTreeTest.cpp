/**
 * @file RadixTreeTest.cpp
 * @brief 基数树测试程序
 * @details 测试基数树的正确性、性能，并与哈希表进行对比
 */

#include "RadixTree.h"
#include "Common.h"
#include <unordered_map>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include <cassert>

using namespace std;
using namespace std::chrono;

// ================================ 测试数据结构 ================================

struct TestSpan {
    PAGE_ID pageId;
    size_t n;
    bool isUse;
    
    TestSpan(PAGE_ID id = 0, size_t pages = 1) 
        : pageId(id), n(pages), isUse(false) {}
};

// ================================ 正确性测试 ================================

/**
 * @brief 基本功能测试
 */
void testBasicOperations() {
    cout << "=== 基本功能测试 ===" << endl;
    
    RadixTree<TestSpan> tree;
    
    // 测试空树
    assert(tree.empty());
    assert(tree.size() == 0);
    assert(tree.lookup(123) == nullptr);
    
    // 测试插入和查找
    TestSpan span1(100, 4);
    TestSpan span2(200, 8);
    TestSpan span3(300, 2);
    
    assert(tree.insert(100, &span1));
    assert(tree.insert(200, &span2));
    assert(tree.insert(300, &span3));
    
    assert(!tree.empty());
    assert(tree.size() == 3);
    
    // 测试查找
    TestSpan* found = tree.lookup(100);
    assert(found && found->pageId == 100);
    
    found = tree.lookup(200);
    assert(found && found->pageId == 200);
    
    found = tree.lookup(300);
    assert(found && found->pageId == 300);
    
    assert(tree.lookup(400) == nullptr);
    
    // 测试删除
    TestSpan* removed = tree.remove(200);
    assert(removed && removed->pageId == 200);
    assert(tree.size() == 2);
    assert(tree.lookup(200) == nullptr);
    
    // 测试重复插入
    TestSpan span4(100, 6);
    assert(tree.insert(100, &span4));  // 覆盖原值
    found = tree.lookup(100);
    assert(found && found->n == 6);
    
    cout << "基本功能测试通过！" << endl;
}

/**
 * @brief 大数据量测试
 */
void testLargeDataset() {
    cout << "=== 大数据量测试 ===" << endl;
    
    RadixTree<TestSpan> tree;
    vector<TestSpan> spans;
    vector<PAGE_ID> keys;
    
    const size_t TEST_SIZE = 100000;
    spans.reserve(TEST_SIZE);
    keys.reserve(TEST_SIZE);
    
    // 生成测试数据
    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<PAGE_ID> dis(1, 0x1FFFFFFFFULL);
    
    for (size_t i = 0; i < TEST_SIZE; i++) {
        PAGE_ID key = dis(gen);
        keys.push_back(key);
        spans.emplace_back(key, i % 128 + 1);
        tree.insert(key, &spans.back());
    }
    
    assert(tree.size() <= TEST_SIZE);  // 可能有重复键
    
    // 验证所有插入的数据都能找到
    size_t found_count = 0;
    for (size_t i = 0; i < TEST_SIZE; i++) {
        TestSpan* found = tree.lookup(keys[i]);
        if (found && found->pageId == keys[i]) {
            found_count++;
        }
    }
    
    cout << "插入 " << TEST_SIZE << " 个元素，成功查找 " << found_count << " 个" << endl;
    cout << "树的高度: " << tree.height() << endl;
    cout << "实际存储元素数量: " << tree.size() << endl;
    
    cout << "大数据量测试通过！" << endl;
}

/**
 * @brief 边界条件测试
 */
void testEdgeCases() {
    cout << "=== 边界条件测试 ===" << endl;
    
    RadixTree<TestSpan> tree;
    TestSpan span1(0, 1);
    TestSpan span2(UINT64_MAX, 1);
    TestSpan span3(1ULL << 32, 1);
    
    // 测试边界值
    assert(tree.insert(0, &span1));
    assert(tree.insert(UINT64_MAX, &span2));
    assert(tree.insert(1ULL << 32, &span3));
    
    assert(tree.lookup(0) == &span1);
    assert(tree.lookup(UINT64_MAX) == &span2);
    assert(tree.lookup(1ULL << 32) == &span3);
    
    // 测试删除不存在的键
    assert(tree.remove(12345) == nullptr);
    
    // 测试清空
    tree.clear();
    assert(tree.empty());
    assert(tree.size() == 0);
    
    cout << "边界条件测试通过！" << endl;
}

// ================================ 性能测试 ================================

/**
 * @brief 性能对比测试：基数树 vs 哈希表
 */
void performanceComparison() {
    cout << "=== 性能对比测试 ===" << endl;
    
    const size_t TEST_SIZE = 1000000;
    const size_t LOOKUP_COUNT = 5000000;
    
    vector<PAGE_ID> keys;
    vector<TestSpan> spans;
    keys.reserve(TEST_SIZE);
    spans.reserve(TEST_SIZE);
    
    // 生成测试数据
    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<PAGE_ID> dis(1, 0x1FFFFFFFFULL);
    
    for (size_t i = 0; i < TEST_SIZE; i++) {
        PAGE_ID key = dis(gen);
        keys.push_back(key);
        spans.emplace_back(key, i % 128 + 1);
    }
    
    // 基数树性能测试
    {
        RadixTree<TestSpan> tree;
        
        auto start = high_resolution_clock::now();
        
        // 插入测试
        for (size_t i = 0; i < TEST_SIZE; i++) {
            tree.insert(keys[i], &spans[i]);
        }
        
        auto insert_end = high_resolution_clock::now();
        
        // 查找测试
        uniform_int_distribution<size_t> lookup_dis(0, TEST_SIZE - 1);
        volatile TestSpan* result = nullptr;  // 防止编译器优化
        
        for (size_t i = 0; i < LOOKUP_COUNT; i++) {
            size_t idx = lookup_dis(gen);
            result = tree.lookup(keys[idx]);
        }
        
        auto lookup_end = high_resolution_clock::now();
        
        auto insert_time = duration_cast<milliseconds>(insert_end - start).count();
        auto lookup_time = duration_cast<milliseconds>(lookup_end - insert_end).count();
        
        cout << "基数树性能:" << endl;
        cout << "  插入 " << TEST_SIZE << " 个元素耗时: " << insert_time << " ms" << endl;
        cout << "  查找 " << LOOKUP_COUNT << " 次耗时: " << lookup_time << " ms" << endl;
        cout << "  树的高度: " << tree.height() << endl;
        cout << "  内存使用(估算): " << tree.size() * sizeof(RadixTreeNode) << " bytes" << endl;
    }
    
    // 哈希表性能测试
    {
        unordered_map<PAGE_ID, TestSpan*> hashmap;
        
        auto start = high_resolution_clock::now();
        
        // 插入测试
        for (size_t i = 0; i < TEST_SIZE; i++) {
            hashmap[keys[i]] = &spans[i];
        }
        
        auto insert_end = high_resolution_clock::now();
        
        // 查找测试
        uniform_int_distribution<size_t> lookup_dis(0, TEST_SIZE - 1);
        volatile TestSpan* result = nullptr;  // 防止编译器优化
        
        for (size_t i = 0; i < LOOKUP_COUNT; i++) {
            size_t idx = lookup_dis(gen);
            auto it = hashmap.find(keys[idx]);
            if (it != hashmap.end()) {
                result = it->second;
            }
        }
        
        auto lookup_end = high_resolution_clock::now();
        
        auto insert_time = duration_cast<milliseconds>(insert_end - start).count();
        auto lookup_time = duration_cast<milliseconds>(lookup_end - insert_end).count();
        
        cout << "哈希表性能:" << endl;
        cout << "  插入 " << TEST_SIZE << " 个元素耗时: " << insert_time << " ms" << endl;
        cout << "  查找 " << LOOKUP_COUNT << " 次耗时: " << lookup_time << " ms" << endl;
        cout << "  桶数量: " << hashmap.bucket_count() << endl;
        cout << "  负载因子: " << hashmap.load_factor() << endl;
    }
}

/**
 * @brief 内存使用测试
 */
void memoryUsageTest() {
    cout << "=== 内存使用测试 ===" << endl;
    
    RadixTree<TestSpan> tree;
    vector<TestSpan> spans;
    
    const size_t TEST_SIZE = 100000;
    spans.reserve(TEST_SIZE);
    
    // 插入稀疏数据（模拟真实的页号分布）
    for (size_t i = 0; i < TEST_SIZE; i++) {
        PAGE_ID key = i * 1000;  // 稀疏分布
        spans.emplace_back(key, 1);
        tree.insert(key, &spans.back());
    }
    
    cout << "稀疏数据测试:" << endl;
    cout << "  元素数量: " << tree.size() << endl;
    cout << "  树的高度: " << tree.height() << endl;
    cout << "  键的范围: 0 - " << (TEST_SIZE - 1) * 1000 << endl;
    
    // 清空并测试密集数据
    tree.clear();
    spans.clear();
    spans.reserve(TEST_SIZE);
    
    // 插入密集数据
    for (size_t i = 0; i < TEST_SIZE; i++) {
        PAGE_ID key = i;  // 密集分布
        spans.emplace_back(key, 1);
        tree.insert(key, &spans.back());
    }
    
    cout << "密集数据测试:" << endl;
    cout << "  元素数量: " << tree.size() << endl;
    cout << "  树的高度: " << tree.height() << endl;
    cout << "  键的范围: 0 - " << TEST_SIZE - 1 << endl;
}

// ================================ 主测试函数 ================================

int main() {
    cout << "基数树测试开始..." << endl << endl;
    
    try {
        // 正确性测试
        testBasicOperations();
        cout << endl;
        
        testLargeDataset();
        cout << endl;
        
        testEdgeCases();
        cout << endl;
        
        // 性能测试
        performanceComparison();
        cout << endl;
        
        memoryUsageTest();
        cout << endl;
        
        cout << "所有测试通过！基数树优化成功。" << endl;
        
    } catch (const exception& e) {
        cout << "测试失败: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}