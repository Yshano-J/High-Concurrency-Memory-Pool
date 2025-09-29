#include "ThreadCache.h"
#include "CentralCache.h"

// 从CentralCache中获取内存
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 慢开始反馈调节算法
	size_t batchNum = std::min(SizeClass::NumMoveSize(size), _freeList[index].maxSize());
	if (_freeList[index].maxSize() == batchNum)
		_freeList[index].maxSize() += 2;

	void* start = nullptr, * end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size); // 可能不够batchNum
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

// 从threadcache分配内存
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

// 归还内存到threadcache
void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(size < MAX_MEMORYSIZE);
	size_t freeListPos = SizeClass::Index(size);
	_freeList[freeListPos].push(ptr);
}
