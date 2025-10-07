#pragma once

/**
 * @file ConcurrencyAlloc.h
 * @brief 高并发内存分配器对外接口
 * @details 提供统一的内存分配和释放接口，自动选择合适的分配策略
 */

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

/**
 * @brief 高并发内存分配函数
 * @param size 需要分配的内存大小
 * @return 分配的内存指针
 * @details 分配策略：
 *          - 小于等于256KB：使用ThreadCache分配（快速、无锁）
 *          - 大于256KB：直接从PageCache分配（大对象直接分配）
 */
static void *ConcurrencyAlloc(size_t size)
{
	if (size > MAX_MEMORYSIZE)
	{
		// 大对象直接从PageCache分配
		size_t alignSize = SizeClass::RoundUp(size);
		size_t npages = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->GetMutex().lock();
		Span *span = PageCache::GetInstance()->NewSpan(npages);
		span->_objSize = size;
		PageCache::GetInstance()->GetMutex().unlock();

		void *ptr = (void *)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		// 小对象通过ThreadCache分配
		if (pTLSThreadCache == nullptr)
		{
			// 首次使用时创建线程本地的ThreadCache
			static ObjectPool<ThreadCache> tcPool;
			pTLSThreadCache = tcPool.New();
		}
		return pTLSThreadCache->Allocate(size);
	}
}

/**
 * @brief 高并发内存释放函数
 * @param ptr 要释放的内存指针
 * @details 释放策略：
 *          - 小对象：归还到ThreadCache
 *          - 大对象：直接归还到PageCache
 */
static void ConcurrencyFree(void *ptr)
{
	Span *span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	if (span->_objSize > MAX_MEMORYSIZE)
	{
		// 大对象直接归还到PageCache
		PageCache::GetInstance()->GetMutex().lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->GetMutex().unlock();
	}
	else
	{
		// 小对象归还到ThreadCache
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, span->_objSize);
	}
}