/**
 * @file ThreadCache.cpp
 * @brief ThreadCache���ʵ��
 * @details ʵ���̱߳����ڴ滺��ķ��䡢�ͷź������뻺��Ľ���
 */

#include "ThreadCache.h"
#include "CentralCache.h"

/**
 * @brief ��CentralCache��������ȡ�ڴ����
 * @param index Ͱ����
 * @param size �����С
 * @return ��ȡ�����ڴ����ָ��
 * @details ʹ�����������������㷨��̬����������ȡ������
 *          ����ڴ����Ч�ʲ�����������
 */
void *ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// ���������������㷨����̬����������ȡ����
	size_t batchNum = std::min(SizeClass::NumMoveSize(size), _freeList[index].maxSize());
	if (_freeList[index].maxSize() == batchNum)
		_freeList[index].maxSize() += 2; // ��������������

	void *start = nullptr, *end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0);

	if (actualNum == 1)
	{
		// ֻ��ȡ��һ������ֱ�ӷ���
		assert(start == end);
		return start;
	}
	else
	{
		// ��ȡ��������󣬽�����һ������������������������
		_freeList[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
}

/**
 * @brief ���̻߳������ָ����С���ڴ�
 * @param size ��Ҫ������ڴ��С
 * @return ������ڴ�ָ��
 * @details �������̣�
 *          1. ����С���϶��뵽���ʵı߽�
 *          2. �����Ӧ��Ͱ����
 *          3. ���Ͱ���п��ж�����ֱ�ӷ���
 *          4. �����CentralCache������ȡ����
 */
void *ThreadCache::Allocate(size_t size)
{
	assert(size < MAX_MEMORYSIZE);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t freeListPos = SizeClass::Index(size);
	
	if (!_freeList[freeListPos].isEmpty())
	{
		// Ͱ���п��ж���ֱ�ӷ���
		return _freeList[freeListPos].pop();
	}
	else
	{
		// ͰΪ�գ���CentralCache��ȡ����
		return FetchFromCentralCache(freeListPos, alignSize);
	}
}

/**
 * @brief �黹�ڴ浽�̻߳���
 * @param ptr Ҫ�黹���ڴ�ָ��
 * @param size �ڴ���С
 * @details �黹���̣�
 *          1. �����Ӧ��Ͱ����
 *          2. ���ڴ������Ӧ����������
 *          3. ��������ȣ����������黹���ָ�CentralCache
 */
void ThreadCache::Deallocate(void *ptr, size_t size)
{
	assert(size < MAX_MEMORYSIZE);
	size_t freeListPos = SizeClass::Index(size);
	_freeList[freeListPos].push(ptr);

	// �������ȴﵽ����������ֵʱ���黹һ����CentralCache
	// �������Ա��ⵥ���߳�ռ�ù����ڴ棬�����ڴ����̼߳��ƽ��ֲ�
	if (_freeList[freeListPos].size() >= _freeList[freeListPos].maxSize())
	{
		ListTooLong(_freeList[freeListPos], size);
	}
}

/**
 * @brief ��������������������
 * @param list ��������������
 * @param size �����С
 * @details ��һ������黹��CentralCache������ThreadCache���ڴ�ռ��
 */
void ThreadCache::ListTooLong(FreeList &list, size_t size)
{
	void *start = nullptr, *end = nullptr;
	list.PopRange(start, end, list.maxSize());
	CentralCache::GetInstance()->ReleaseListToSpan(start, size);
}
