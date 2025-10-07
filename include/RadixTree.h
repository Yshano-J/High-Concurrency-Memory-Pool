#pragma once

/**
 * @file RadixTree.h
 * @brief ������ʵ�֣����ڸ�Ч��ҳ�ŵ�Spanӳ��
 * @details ��������Radix Tree����һ��ѹ����ǰ׺�����ر��ʺ�����ϡ���������ֵӳ��
 *          ��ȹ�ϣ�����������ڴ�ʹ�úͻ����Ѻ��Է�������������
 */

#include "Common.h"
#include "ObjectPool.h"
#include <array>

// ================================ ���������� ================================

// 64λϵͳ�£�ҳ�����ʹ��52λ�������ַ���ƣ�
static const int RADIX_TREE_MAP_SHIFT = 6;  // ÿ��6λ��64���ӽڵ�
static const int RADIX_TREE_MAP_SIZE = 1 << RADIX_TREE_MAP_SHIFT;  // 64
static const int RADIX_TREE_MAP_MASK = RADIX_TREE_MAP_SIZE - 1;    // 63
static const int RADIX_TREE_MAX_PATH = (64 + RADIX_TREE_MAP_SHIFT - 1) / RADIX_TREE_MAP_SHIFT;  // ������

// ================================ �������ڵ� ================================

/**
 * @struct RadixTreeNode
 * @brief �������ڵ�ṹ
 * @details ÿ���ڵ����64���ӽڵ�ָ�룬ʹ��ϡ��洢�Ż��ڴ�ʹ��
 */
struct RadixTreeNode
{
    void* slots[RADIX_TREE_MAP_SIZE];  // �ӽڵ��Ҷ��ֵ��ָ������
    unsigned long tags;                 // λͼ��ǣ�ָʾ��Щ��λ��ʹ��
    int count;                         // ��ǰ�ڵ���ӽڵ�����
    int shift;                         // ��ǰ�ڵ������еĲ㼶��λ������

    RadixTreeNode(int level_shift = 0) 
        : tags(0), count(0), shift(level_shift)
    {
        for (int i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
            slots[i] = nullptr;
        }
    }

    ~RadixTreeNode() = default;
};

// ================================ �������� ================================

/**
 * @class RadixTree
 * @brief ������ʵ����
 * @details �ṩ��Ч�Ĳ��롢���ҡ�ɾ��������ר���Ż�����ҳ�ŵ�Span��ӳ��
 */
template<typename T>
class RadixTree
{
public:
    /**
     * @brief ���캯��
     */
    RadixTree() : _root(nullptr), _height(0), _count(0) {}

    /**
     * @brief ��������
     */
    ~RadixTree() 
    {
        if (_root) {
            _destroyNode(_root, _height);
        }
    }

    /**
     * @brief �����ֵ��
     * @param key ҳ�ż�
     * @param value ��Ӧ��ֵ��Spanָ�룩
     * @return �����Ƿ�ɹ�
     */
    bool insert(PAGE_ID key, T* value);

    /**
     * @brief ���Ҽ���Ӧ��ֵ
     * @param key ҳ�ż�
     * @return ��Ӧ��ֵָ�룬δ�ҵ�����nullptr
     */
    T* lookup(PAGE_ID key) const;

    /**
     * @brief ɾ����ֵ��
     * @param key ҳ�ż�
     * @return ��ɾ����ֵָ�룬δ�ҵ�����nullptr
     */
    T* remove(PAGE_ID key);

    /**
     * @brief ������Ƿ�Ϊ��
     * @return true��ʾΪ��
     */
    bool empty() const { return _count == 0; }

    /**
     * @brief ��ȡ����Ԫ������
     * @return Ԫ������
     */
    size_t size() const { return _count; }

    /**
     * @brief ���������
     */
    void clear();

    /**
     * @brief ��ȡ���ĸ߶�
     * @return ���ĸ߶�
     */
    int height() const { return _height; }

private:
    RadixTreeNode* _root;                    // ���ڵ�
    int _height;                             // ���ĸ߶�
    size_t _count;                           // Ԫ������
    ObjectPool<RadixTreeNode> _nodePool;     // �ڵ����أ��Ż��ڴ����

    /**
     * @brief ��չ���ĸ߶��������µļ�
     * @param key Ҫ����ļ�
     * @return ��չ����¸߶�
     */
    int _extendTree(PAGE_ID key);

    /**
     * @brief �������ָ���߶��µ�����
     * @param key ��ֵ
     * @param height ���ĸ߶�
     * @param shift λ����
     * @return ����ֵ
     */
    static unsigned int _getIndex(PAGE_ID key, int height, int shift)
    {
        return (key >> shift) & RADIX_TREE_MAP_MASK;
    }

    /**
     * @brief ����ָ���߶ȶ�Ӧ��λ����
     * @param height �߶�
     * @return λ����
     */
    static int _getShift(int height)
    {
        return height * RADIX_TREE_MAP_SHIFT;
    }

    /**
     * @brief �ݹ����ٽڵ�
     * @param node Ҫ���ٵĽڵ�
     * @param height �ڵ�߶�
     */
    void _destroyNode(RadixTreeNode* node, int height);

    /**
     * @brief ���ڵ��Ƿ�Ϊ��
     * @param node Ҫ���Ľڵ�
     * @return true��ʾ�ڵ�Ϊ��
     */
    bool _isNodeEmpty(RadixTreeNode* node) const
    {
        return node->count == 0;
    }

    /**
     * @brief �����½ڵ�
     * @param shift �ڵ��λ����
     * @return �·���Ľڵ�ָ��
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
     * @brief �ͷŽڵ�
     * @param node Ҫ�ͷŵĽڵ�
     */
    void _freeNode(RadixTreeNode* node)
    {
        _nodePool.Delete(node);
    }
};

// ================================ ģ��ʵ�� ================================

template<typename T>
bool RadixTree<T>::insert(PAGE_ID key, T* value)
{
    if (!value) return false;

    // �����Ϊ�ջ���Ҫ��չ�߶�
    if (!_root || key >= (1ULL << _getShift(_height + 1))) {
        _height = _extendTree(key);
    }

    RadixTreeNode* node = _root;
    int shift = _getShift(_height);

    // �Ӹ��ڵ����±�����Ҷ�ӽڵ�
    for (int level = _height; level > 0; level--) {
        unsigned int index = _getIndex(key, level, shift);
        
        if (!node->slots[index]) {
            // �����µ��м�ڵ�
            RadixTreeNode* newNode = _allocNode(shift - RADIX_TREE_MAP_SHIFT);
            node->slots[index] = newNode;
            node->tags |= (1UL << index);
            node->count++;
        }
        
        node = static_cast<RadixTreeNode*>(node->slots[index]);
        shift -= RADIX_TREE_MAP_SHIFT;
    }

    // ��Ҷ�ӽڵ����ֵ
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

    // �Ż��Ĳ���·�������ٷ�֧Ԥ��ʧ��
    for (int level = _height; level > 0; level--) {
        unsigned int index = _getIndex(key, level, shift);
        unsigned long mask = 1UL << index;
        
        // ʹ��λ������ټ�飬���ٷ�֧
        if (!(node->tags & mask)) {
            return nullptr;  // ·��������
        }
        
        node = static_cast<RadixTreeNode*>(node->slots[index]);
        // Ԥȡ��һ���ڵ㣬��߻���������
        if (level > 1 && node) {
            __builtin_prefetch(node, 0, 3);
        }
        
        shift -= RADIX_TREE_MAP_SHIFT;
    }

    // ��Ҷ�ӽڵ����ֵ
    unsigned int index = _getIndex(key, 0, 0);
    unsigned long mask = 1UL << index;
    
    // ʹ�������ƶ����ٷ�֧
    return (node->tags & mask) ? static_cast<T*>(node->slots[index]) : nullptr;
}

template<typename T>
T* RadixTree<T>::remove(PAGE_ID key)
{
    if (!_root || key >= (1ULL << _getShift(_height + 1))) {
        return nullptr;
    }

    // ��¼·���Ա��������սڵ�
    RadixTreeNode* path[RADIX_TREE_MAX_PATH];
    unsigned int indices[RADIX_TREE_MAX_PATH];
    int pathLen = 0;

    RadixTreeNode* node = _root;
    int shift = _getShift(_height);

    // ��¼����·��
    for (int level = _height; level > 0; level--) {
        unsigned int index = _getIndex(key, level, shift);
        
        if (!(node->tags & (1UL << index))) {
            return nullptr;  // ·��������
        }
        
        path[pathLen] = node;
        indices[pathLen] = index;
        pathLen++;
        
        node = static_cast<RadixTreeNode*>(node->slots[index]);
        if (!node) return nullptr;
        
        shift -= RADIX_TREE_MAP_SHIFT;
    }

    // ��Ҷ�ӽڵ�ɾ��ֵ
    unsigned int index = _getIndex(key, 0, 0);
    if (!(node->tags & (1UL << index))) {
        return nullptr;  // ֵ������
    }

    T* value = static_cast<T*>(node->slots[index]);
    node->slots[index] = nullptr;
    node->tags &= ~(1UL << index);
    node->count--;
    _count--;

    // ��������սڵ�
    if (_isNodeEmpty(node) && pathLen > 0) {
        _freeNode(node);
        
        for (int i = pathLen - 1; i >= 0; i--) {
            RadixTreeNode* parent = path[i];
            unsigned int idx = indices[i];
            
            parent->slots[idx] = nullptr;
            parent->tags &= ~(1UL << idx);
            parent->count--;
            
            if (!_isNodeEmpty(parent) || i == 0) {
                break;  // ���ڵ㲻Ϊ�ջ��ѵ����ڵ�
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
    
    // ������Ҫ����С�߶�
    PAGE_ID maxKey = key;
    while (maxKey >= (1ULL << _getShift(newHeight + 1))) {
        newHeight++;
    }

    // �����Ҫ���Ӹ߶�
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

    // �����Ϊ�գ��������ڵ�
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
        // �ݹ������ӽڵ�
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

// ΪSpan�ػ��Ļ��������Ͷ���
using SpanRadixTree = RadixTree<Span>;