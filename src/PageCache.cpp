/**
 * @file PageCache.cpp
 * @brief PageCache类的实现
 * @details 实现页缓存的内存分配、回收和页合并功能
 */

#include "PageCache.h"

// 单例对象定义
PageCache PageCache::_sInst;

/**
 * @brief 分配k页的连续内存
 * @param k 需要分配的页数
 * @return 分配的Span指针
 * @details 分配策略：
 *          1. 如果k超过最大页数，直接从系统申请
 *          2. 如果对应桶中有Span，直接返回
 *          3. 如果没有，从更大的桶中切分
 *          4. 如果都没有，向系统申请大块内存后递归调用
 */
Span *PageCache::NewSpan(size_t k)
{
    assert(k > 0);

    // 超大内存申请，直接从系统分配
    if (k > MAX_PAGESIZE - 1)
    {
        void *ptr = SystemAlloc(k);
        // Span* span = new Span;
        Span *span = _spanPool.New();
        span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
        span->_n = k;
        _idSpanMap.insert(span->_pageId, span);

        return span;
    }

    // 检查对应桶中是否有可用的Span
    if (!_pageList[k].empty())
    {
        Span *partSpan = _pageList[k].pop_front();

        // 建立页号到Span的映射关系，用于后续的地址查找
        for (PAGE_ID i = 0; i < partSpan->_n; i++)
        {
            _idSpanMap.insert(partSpan->_pageId + i, partSpan);
        }
        return partSpan;
    }

    // 当前桶为空，从更大的桶中切分Span
    for (size_t i = k + 1; i < MAX_PAGESIZE; i++)
    {
        if (!_pageList[i].empty())
        {
            // 从大Span中切分出k页
            Span *span = _pageList[i].pop_front();
            // Span* partSpan = new Span;
            Span *partSpan = _spanPool.New();
            // 将大Span的前k页分给partSpan
            partSpan->_pageId = span->_pageId;
            partSpan->_n = k;
            // 更新原Span的信息（剩余部分）
            span->_pageId += k;
            span->_n -= k;

            // 将剩余的Span放回对应的桶中
            _pageList[span->_n].push_front(span);

            // 存储剩余Span的首尾页号到映射表中，方便后续合并查找
            _idSpanMap.insert(span->_pageId, span);
            _idSpanMap.insert(span->_pageId + span->_n - 1, span);

            // 建立新分配Span的页号映射关系
            for (PAGE_ID i = 0; i < partSpan->_n; i++)
            {
                _idSpanMap.insert(partSpan->_pageId + i, partSpan);
            }

            return partSpan;
        }
    }

    // 所有桶都为空，向系统申请大块内存
    // Span* bigSpan = new Span;
    Span *bigSpan = _spanPool.New();

    void *ptr = SystemAlloc(MAX_PAGESIZE - 1); // 申请128页
    bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
    bigSpan->_n = MAX_PAGESIZE - 1;
    _pageList[bigSpan->_n].push_front(bigSpan);

    // 递归调用自己，此时肯定能成功分配
    return NewSpan(k);
}

/**
 * @brief 根据内存地址映射到对应的Span
 * @param obj 内存对象指针
 * @return 对应的Span指针
 * @details 通过页号在映射表中查找对应的Span，
 *          这是内存回收时确定归属Span的关键方法
 */
Span *PageCache::MapObjectToSpan(void *obj)
{
    PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;
    std::lock_guard<std::mutex> guard(_pageMtx);
    
    Span* span = _idSpanMap.lookup(id);
    if (span)
    {
        return span;
    }
    else
    {
        assert(false); // 不应该找不到对应的Span
        return nullptr;
    }
}

/**
 * @brief 释放Span回PageCache，并尝试合并相邻页
 * @param span 要释放的Span指针
 * @details 释放流程：
 *          1. 如果是超大Span，直接释放给系统
 *          2. 尝试向前合并相邻的空闲页
 *          3. 尝试向后合并相邻的空闲页
 *          4. 将合并后的Span放入对应的桶中
 *          5. 更新页号映射表
 */
void PageCache::ReleaseSpanToPageCache(Span *span)
{
    assert(span);
    
    // 超大Span直接释放给系统
    if (span->_n > MAX_PAGESIZE - 1)
    {
        void *ptr = (void *)(span->_pageId << PAGE_SHIFT);
        SystemFree(ptr);
        _spanPool.Delete(span);
        return;
    }

    // 向前合并相邻的空闲页，减少内存碎片
    while (1)
    {
        PAGE_ID prevId = span->_pageId - 1;
        Span *prevSpan = _idSpanMap.lookup(prevId);

        if (!prevSpan)
            break; // 没有找到前一个页，结束合并
        if (prevSpan->_isUse)
            break; // 前一个页正在使用中，不能合并
        if (prevSpan->_n + span->_n > MAX_PAGESIZE - 1)
            break; // 合并后会超过最大页数限制

        // 执行向前合并
        span->_pageId = prevSpan->_pageId;
        span->_n += prevSpan->_n;

        // 从基数树中删除被合并Span的映射
        _idSpanMap.remove(prevSpan->_pageId);
        _idSpanMap.remove(prevSpan->_pageId + prevSpan->_n - 1);

        // 从对应桶中移除被合并的Span
        _pageList[prevSpan->_n].erase(prevSpan);
        _spanPool.Delete(prevSpan);
    }

    // 向后合并相邻的空闲页
    while (1)
    {
        PAGE_ID nextId = span->_pageId + span->_n;
        Span *nextSpan = _idSpanMap.lookup(nextId);
        if (!nextSpan)
            break; // 没有找到后一个页，结束合并
        if (nextSpan->_isUse)
            break; // 后一个页正在使用中，不能合并
        if (nextSpan->_n + span->_n > MAX_PAGESIZE - 1)
            break; // 合并后会超过最大页数限制

        // 执行向后合并
        span->_n += nextSpan->_n;
        
        // 从基数树中删除被合并Span的映射
        _idSpanMap.remove(nextSpan->_pageId);
        _idSpanMap.remove(nextSpan->_pageId + nextSpan->_n - 1);
        
        // 从对应桶中移除被合并的Span
        _pageList[nextSpan->_n].erase(nextSpan);
        _spanPool.Delete(nextSpan);
    }

    // 将合并后的Span放入对应的桶中
    _pageList[span->_n].push_front(span);
    span->_isUse = false; // 标记为未使用状态
    
    // 更新页号映射表（只需要存储首尾页号）
    _idSpanMap.insert(span->_pageId, span);
    _idSpanMap.insert(span->_pageId + span->_n - 1, span);
}