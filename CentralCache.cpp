#include "CentralCache.h"

CentralCache CentralCache::_sInst; // 类内静态成员变量类外初始化

// 获取一个非空的Span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	return nullptr;
}

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);

	_spanList[index]._mtx.lock();  // 解锁

	Span* span = GetOneSpan(_spanList[index], size);
	assert(span);
	assert(span->_freeList);

	// 从span中切batchNum个对象
	// 如果不够， 有多少拿多少
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	while ((i < batchNum - 1) && NextObj(end))
	{
		end = NextObj(end);
		++i, ++actualNum;
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr; // 截断

	_spanList[index]._mtx.unlock(); // 释放锁

	return actualNum;
}
