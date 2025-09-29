#include "ThreadCache.h"
#include "CentralCache.h"

// ��CentralCache�л�ȡ�ڴ�
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// ����ʼ���������㷨
	size_t batchNum = std::min(SizeClass::NumMoveSize(size), _freeList[index].maxSize());
	if (_freeList[index].maxSize() == batchNum)
		_freeList[index].maxSize() += 2;

	void* start = nullptr, * end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size); // ���ܲ���batchNum
	assert(actualNum > 1);

	if (actualNum == 1)
	{
		assert(start = end);
		return start;
	}
	else
	{
		_freeList[index].PushRange(NextObj(start), end);
		return start;
	}
}

// ��threadcache�����ڴ�
void* ThreadCache::Allocate(size_t size)
{
	assert(size < MAX_MEMORYSIZE);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t freeListPos = SizeClass::Index(size);
	if (!_freeList[freeListPos].isEmpty())
	{
		return _freeList[freeListPos].pop();
	}
	else
		return FetchFromCentralCache(freeListPos, alignSize);
}

// �黹�ڴ浽threadcache
void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(size < MAX_MEMORYSIZE);
	size_t freeListPos = SizeClass::Index(size);
	_freeList[freeListPos].push(ptr);
}
