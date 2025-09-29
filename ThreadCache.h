#pragma once
#include "Common.h"

class ThreadCache
{
public:
	// 从threadcache分配内存
	void* Allocate(size_t size);
	// 归还内存到threadcache
	void Deallocate(void* ptr, size_t size);
	// 从CentralCache中获取内存
	void* FetchFromCentralCache(size_t index, size_t size);
private:
	FreeList _freeList[MAX_BUCKETSIZE]; // 哈希桶, 每个桶一个自由链表
};

// C++11 TLS Thread-Local Storage 
thread_local ThreadCache* pTLSThreadCache = nullptr; // 线程私有的ThreadCache指针