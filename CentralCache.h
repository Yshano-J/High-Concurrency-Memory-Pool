#pragma once

#include "Common.h"

// 单例模式 饿汉模式
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	// 从CentralCache中获取n个obj, 返回实际获取的数量
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);
	// 获取一个非空的Span
	Span* GetOneSpan(SpanList& list, size_t size);

private:
	SpanList _spanList[MAX_BUCKETSIZE];
private:
	CentralCache(){}
	CentralCache(const CentralCache&) = delete;
	CentralCache operator=(const CentralCache&) = delete;

	static CentralCache _sInst;
};