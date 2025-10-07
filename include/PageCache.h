#pragma once

/**
 * @file PageCache.h
 * @brief 页缓存类定义
 * @details 管理大块内存页的分配、回收和合并，与系统内存直接交互
 *          使用基数树优化页号到Span的映射，提升查找性能和内存效率
 */

#include "Common.h"
#include "ObjectPool.h"
#include "RadixTree.h"

/**
 * @class PageCache
 * @brief 页缓存管理类（单例模式）
 * @details 负责管理大块内存页，支持页的分配、回收和相邻页的合并
 *          使用全局锁保证线程安全，是内存池的最底层
 */
class PageCache
{
public:
    /**
     * @brief 获取PageCache单例实例
     * @return PageCache单例指针
     */
    static PageCache *GetInstance()
    {
        return &_sInst;
    }

    /**
     * @brief 分配k页的连续内存
     * @param k 需要分配的页数
     * @return 分配的Span指针
     */
    Span *NewSpan(size_t k);

    /**
     * @brief 根据内存地址映射到对应的Span
     * @param obj 内存对象指针
     * @return 对应的Span指针
     */
    Span *MapObjectToSpan(void *obj);

    /**
     * @brief 释放Span回PageCache，并尝试合并相邻页
     * @param span 要释放的Span指针
     */
    void ReleaseSpanToPageCache(Span *span);

    /**
     * @brief 获取PageCache的互斥锁引用
     * @return 互斥锁引用
     */
    std::mutex &GetMutex()
    {
        return _pageMtx;
    }

private:
    SpanList _pageList[MAX_PAGESIZE];               // 按页数分类的Span链表数组
    ObjectPool<Span> _spanPool;                     // Span对象池，避免频繁new/delete

    SpanRadixTree _idSpanMap;                       // 基数树：页号到Span的映射，优化查找性能

    std::mutex _pageMtx;                            // 全局锁，保证PageCache线程安全

private:
    // 单例模式：禁止外部构造和拷贝
    PageCache() {}
    PageCache(const PageCache &) = delete;

    static PageCache _sInst;                        // 静态单例实例
};