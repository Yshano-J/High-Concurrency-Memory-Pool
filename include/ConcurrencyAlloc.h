#pragma once

/**
 * @file ConcurrencyAlloc.h
 * @brief �߲����ڴ����������ӿ�
 * @details �ṩͳһ���ڴ������ͷŽӿڣ��Զ�ѡ����ʵķ������
 */

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

/**
 * @brief �߲����ڴ���亯��
 * @param size ��Ҫ������ڴ��С
 * @return ������ڴ�ָ��
 * @details ������ԣ�
 *          - С�ڵ���256KB��ʹ��ThreadCache���䣨���١�������
 *          - ����256KB��ֱ�Ӵ�PageCache���䣨�����ֱ�ӷ��䣩
 */
static void *ConcurrencyAlloc(size_t size)
{
	if (size > MAX_MEMORYSIZE)
	{
		// �����ֱ�Ӵ�PageCache����
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
		// С����ͨ��ThreadCache����
		if (pTLSThreadCache == nullptr)
		{
			// �״�ʹ��ʱ�����̱߳��ص�ThreadCache
			static ObjectPool<ThreadCache> tcPool;
			pTLSThreadCache = tcPool.New();
		}
		return pTLSThreadCache->Allocate(size);
	}
}

/**
 * @brief �߲����ڴ��ͷź���
 * @param ptr Ҫ�ͷŵ��ڴ�ָ��
 * @details �ͷŲ��ԣ�
 *          - С���󣺹黹��ThreadCache
 *          - �����ֱ�ӹ黹��PageCache
 */
static void ConcurrencyFree(void *ptr)
{
	Span *span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	if (span->_objSize > MAX_MEMORYSIZE)
	{
		// �����ֱ�ӹ黹��PageCache
		PageCache::GetInstance()->GetMutex().lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->GetMutex().unlock();
	}
	else
	{
		// С����黹��ThreadCache
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, span->_objSize);
	}
}