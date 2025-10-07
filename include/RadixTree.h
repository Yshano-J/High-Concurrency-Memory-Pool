#pragma once

/**
 * @file RadixTree.h
 * @brief 基数树实现，用于高效的页号到Span映射
 * @details 基数树（Radix Tree）是一种压缩的前缀树，特别适合用于稀疏的整数键值映射
 *          相比哈希表，基数树在内存使用和缓存友好性方面有显著优势
 */

#include "Common.h"
#include "ObjectPool.h"
#include <array>

// ================================ 基数树配置 ================================

// 64位系统下，页号最多使用52位（物理地址限制）
static const int RADIX_TREE_MAP_SHIFT = 6;  // 每层6位，64个子节点
static const int RADIX_TREE_MAP_SIZE = 1 << RADIX_TREE_MAP_SHIFT;  // 64
static const int RADIX_TREE_MAP_MASK = RADIX_TREE_MAP_SIZE - 1;    // 63
static const int RADIX_TREE_MAX_PATH = (64 + RADIX_TREE_MAP_SHIFT - 1) / RADIX_TREE_MAP_SHIFT;  // 最大深度

// ================================ 基数树节点 ================================

/**
 * @struct RadixTreeNode
 * @brief 基数树节点结构
 * @details 每个节点包含64个子节点指针，使用稀疏存储优化内存使用
 */
struct RadixTreeNode
{
    void* slots[RADIX_TREE_MAP_SIZE];  // 子节点或叶子值的指针数组
    unsigned long tags;                 // 位图标记，指示哪些槽位被使用
    int count;                         // 当前节点的子节点数量
    int shift;                         // 当前节点在树中的层级（位移量）

    RadixTreeNode(int level_shift = 0) 
        : tags(0), count(0), shift(level_shift)
    {
        for (int i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
            slots[i] = nullptr;
        }
    }

    ~RadixTreeNode() = default;
};

// ================================ 基数树类 ================================

/**
 * @class RadixTree
 * @brief 基数树实现类
 * @details 提供高效的插入、查找、删除操作，专门优化用于页号到Span的映射
 */
template<typename T>
class RadixTree
{
public:
    /**
     * @brief 构造函数
     */
    RadixTree() : _root(nullptr), _height(0), _count(0) {}

    /**
     * @brief 析构函数
     */
    ~RadixTree() 
    {
        if (_root) {
            _destroyNode(_root, _height);
        }
    }

    /**
     * @brief 插入键值对
     * @param key 页号键
     * @param value 对应的值（Span指针）
     * @return 插入是否成功
     */
    bool insert(PAGE_ID key, T* value);

    /**
     * @brief 查找键对应的值
     * @param key 页号键
     * @return 对应的值指针，未找到返回nullptr
     */
    T* lookup(PAGE_ID key) const;

    /**
     * @brief 删除键值对
     * @param key 页号键
     * @return 被删除的值指针，未找到返回nullptr
     */
    T* remove(PAGE_ID key);

    /**
     * @brief 检查树是否为空
     * @return true表示为空
     */
    bool empty() const { return _count == 0; }

    /**
     * @brief 获取树中元素数量
     * @return 元素数量
     */
    size_t size() const { return _count; }

    /**
     * @brief 清空整个树
     */
    void clear();

    /**
     * @brief 获取树的高度
     * @return 树的高度
     */
    int height() const { return _height; }

private:
    RadixTreeNode* _root;                    // 根节点
    int _height;                             // 树的高度
    size_t _count;                           // 元素数量
    ObjectPool<RadixTreeNode> _nodePool;     // 节点对象池，优化内存分配

    /**
     * @brief 扩展树的高度以容纳新的键
     * @param key 要插入的键
     * @return 扩展后的新高度
     */
    int _extendTree(PAGE_ID key);

    /**
     * @brief 计算键在指定高度下的索引
     * @param key 键值
     * @param height 树的高度
     * @param shift 位移量
     * @return 索引值
     */
    static unsigned int _getIndex(PAGE_ID key, int height, int shift)
    {
        return (key >> shift) & RADIX_TREE_MAP_MASK;
    }

    /**
     * @brief 计算指定高度对应的位移量
     * @param height 高度
     * @return 位移量
     */
    static int _getShift(int height)
    {
        return height * RADIX_TREE_MAP_SHIFT;
    }

    /**
     * @brief 递归销毁节点
     * @param node 要销毁的节点
     * @param height 节点高度
     */
    void _destroyNode(RadixTreeNode* node, int height);

    /**
     * @brief 检查节点是否为空
     * @param node 要检查的节点
     * @return true表示节点为空
     */
    bool _isNodeEmpty(RadixTreeNode* node) const
    {
        return node->count == 0;
    }

    /**
     * @brief 分配新节点
     * @param shift 节点的位移量
     * @return 新分配的节点指针
     */
    RadixTreeNode* _allocNode(int shift)
    {
        RadixTreeNode* node = _nodePool.New();
        node->shift = shift;
        node->tags = 0;
        node->count = 0;
        for (int i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
            node->slots[i] = nullptr;
        }
        return node;
    }

    /**
     * @brief 释放节点
     * @param node 要释放的节点
     */
    void _freeNode(RadixTreeNode* node)
    {
        _nodePool.Delete(node);
    }
};

// ================================ 模板实现 ================================

template<typename T>
bool RadixTree<T>::insert(PAGE_ID key, T* value)
{
    if (!value) return false;

    // 如果树为空或需要扩展高度
    if (!_root || key >= (1ULL << _getShift(_height + 1))) {
        _height = _extendTree(key);
    }

    RadixTreeNode* node = _root;
    int shift = _getShift(_height);

    // 从根节点向下遍历到叶子节点
    for (int level = _height; level > 0; level--) {
        unsigned int index = _getIndex(key, level, shift);
        
        if (!node->slots[index]) {
            // 创建新的中间节点
            RadixTreeNode* newNode = _allocNode(shift - RADIX_TREE_MAP_SHIFT);
            node->slots[index] = newNode;
            node->tags |= (1UL << index);
            node->count++;
        }
        
        node = static_cast<RadixTreeNode*>(node->slots[index]);
        shift -= RADIX_TREE_MAP_SHIFT;
    }

    // 在叶子节点插入值
    unsigned int index = _getIndex(key, 0, 0);
    if (!node->slots[index]) {
        node->count++;
        _count++;
    }
    
    node->slots[index] = value;
    node->tags |= (1UL << index);
    
    return true;
}

template<typename T>
T* RadixTree<T>::lookup(PAGE_ID key) const
{
    if (!_root || key >= (1ULL << _getShift(_height + 1))) {
        return nullptr;
    }

    RadixTreeNode* node = _root;
    int shift = _getShift(_height);

    // 优化的查找路径：减少分支预测失败
    for (int level = _height; level > 0; level--) {
        unsigned int index = _getIndex(key, level, shift);
        unsigned long mask = 1UL << index;
        
        // 使用位运算快速检查，减少分支
        if (!(node->tags & mask)) {
            return nullptr;  // 路径不存在
        }
        
        node = static_cast<RadixTreeNode*>(node->slots[index]);
        // 预取下一级节点，提高缓存命中率
        if (level > 1 && node) {
            __builtin_prefetch(node, 0, 3);
        }
        
        shift -= RADIX_TREE_MAP_SHIFT;
    }

    // 在叶子节点查找值
    unsigned int index = _getIndex(key, 0, 0);
    unsigned long mask = 1UL << index;
    
    // 使用条件移动减少分支
    return (node->tags & mask) ? static_cast<T*>(node->slots[index]) : nullptr;
}

template<typename T>
T* RadixTree<T>::remove(PAGE_ID key)
{
    if (!_root || key >= (1ULL << _getShift(_height + 1))) {
        return nullptr;
    }

    // 记录路径以便回溯清理空节点
    RadixTreeNode* path[RADIX_TREE_MAX_PATH];
    unsigned int indices[RADIX_TREE_MAX_PATH];
    int pathLen = 0;

    RadixTreeNode* node = _root;
    int shift = _getShift(_height);

    // 记录查找路径
    for (int level = _height; level > 0; level--) {
        unsigned int index = _getIndex(key, level, shift);
        
        if (!(node->tags & (1UL << index))) {
            return nullptr;  // 路径不存在
        }
        
        path[pathLen] = node;
        indices[pathLen] = index;
        pathLen++;
        
        node = static_cast<RadixTreeNode*>(node->slots[index]);
        if (!node) return nullptr;
        
        shift -= RADIX_TREE_MAP_SHIFT;
    }

    // 在叶子节点删除值
    unsigned int index = _getIndex(key, 0, 0);
    if (!(node->tags & (1UL << index))) {
        return nullptr;  // 值不存在
    }

    T* value = static_cast<T*>(node->slots[index]);
    node->slots[index] = nullptr;
    node->tags &= ~(1UL << index);
    node->count--;
    _count--;

    // 回溯清理空节点
    if (_isNodeEmpty(node) && pathLen > 0) {
        _freeNode(node);
        
        for (int i = pathLen - 1; i >= 0; i--) {
            RadixTreeNode* parent = path[i];
            unsigned int idx = indices[i];
            
            parent->slots[idx] = nullptr;
            parent->tags &= ~(1UL << idx);
            parent->count--;
            
            if (!_isNodeEmpty(parent) || i == 0) {
                break;  // 父节点不为空或已到根节点
            }
            
            _freeNode(parent);
        }
    }

    return value;
}

template<typename T>
int RadixTree<T>::_extendTree(PAGE_ID key)
{
    int newHeight = 0;
    
    // 计算需要的最小高度
    PAGE_ID maxKey = key;
    while (maxKey >= (1ULL << _getShift(newHeight + 1))) {
        newHeight++;
    }

    // 如果需要增加高度
    while (_height < newHeight) {
        RadixTreeNode* newRoot = _allocNode(_getShift(_height + 1));
        
        if (_root) {
            newRoot->slots[0] = _root;
            newRoot->tags |= 1;
            newRoot->count = 1;
        }
        
        _root = newRoot;
        _height++;
    }

    // 如果树为空，创建根节点
    if (!_root) {
        _root = _allocNode(_getShift(newHeight));
        _height = newHeight;
    }

    return _height;
}

template<typename T>
void RadixTree<T>::_destroyNode(RadixTreeNode* node, int height)
{
    if (!node) return;

    if (height > 0) {
        // 递归销毁子节点
        for (int i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
            if (node->tags & (1UL << i)) {
                _destroyNode(static_cast<RadixTreeNode*>(node->slots[i]), height - 1);
            }
        }
    }
    
    _freeNode(node);
}

template<typename T>
void RadixTree<T>::clear()
{
    if (_root) {
        _destroyNode(_root, _height);
        _root = nullptr;
        _height = 0;
        _count = 0;
    }
}

// 为Span特化的基数树类型定义
using SpanRadixTree = RadixTree<Span>;