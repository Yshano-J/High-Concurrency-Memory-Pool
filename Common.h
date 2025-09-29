#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <ctime>
#include <cstdint>
#include <thread>
#include <mutex>
using std::cout;
using std::endl;

static const size_t MAX_MEMORYSIZE = 256 * 1024; // threadcache最大的内存大小
static const size_t MAX_BUCKETSIZE = 208; // threadcache CentralCache 最大桶数

#ifdef _WIN64
	typedef uint64_t PAGE_TD;
#elif _WIN32
	typedef uint32_t PAGE_TD;
#else // Linux
	typedef uint64_t PAGE_TD;
#endif


// 获取obj的下一个对象指针
static inline void*& NextObj(void* obj)
{
	return *(void**)obj;
}

// 自由链表类
class FreeList
{
public:
	// 归还内存块到自由链表
	void push(void* obj)
	{
		NextObj(obj) = _freelist;
		_freelist = obj;
	}

	void PushRange(void* start, void* end)
	{
		assert(start);
		assert(end);
		NextObj(end) = _freelist;
		_freelist = start;
	}

	// 从自由链表获取内存块
	void* pop()
	{
		void* obj = _freelist;
		_freelist = NextObj(_freelist);
		return obj;
	}

	bool isEmpty()
	{
		return _freelist == nullptr;
	}

	size_t& maxSize() 
	{
		return _maxSize; 
	}

private:
	void* _freelist = nullptr;
	size_t _maxSize = 1; // 
};

class SizeClass
{
public:
	// Bytes：[1, 128]                  对齐到8              index范围[0, 16)
	// Bytes：[128+1, 1024]             对齐到16             index范围[16,72)
	// Bytes：[1024+1, 8*1024]          对齐到128            index范围[72,128)
	// Bytes：[8*1024+1, 64*1024]       对齐到1024           index范围[128,184)
	// Bytes：[64*1024+1, 256*1024]     对齐到8*1024         index范围[184,208)
	static size_t _RoundUp(size_t size, size_t alignNum)
	{
		return (size + alignNum - 1) & ~(alignNum - 1);
	}
	// 获得对齐之后的内存大小
	static size_t RoundUp(size_t size)
	{
		assert(size < MAX_MEMORYSIZE);
		if (size <= 8)
			return _RoundUp(size, 8);
		else if (size > 8 && size <= 16)
			return _RoundUp(size, 16);
		else if (size > 16 && size <= 128)
			return _RoundUp(size, 128);
		else if (size > 128 && size <= 1024)
			return _RoundUp(size, 1024);
		else if (size > 1024 && size <= 8 * 1024)
			return _RoundUp(size, 8 * 1024);
		else assert(false);

		return -1;
	}

	// 找到“桶”的位置：数组下标
	static size_t _Index(size_t size, size_t alignShifted)
	{
		return ((size + ((long long)1 << alignShifted) - 1) >> alignShifted) - 1;
	}

	static size_t Index(size_t size)
	{
		assert(size < MAX_MEMORYSIZE);
		static int CountArray[] = { 16,56,56,56 };

		if (size <= 128)
			return _Index(size, 3);
		else if (size > 128 && size <= 1024)
			return _Index(size - 128, 4) + CountArray[0];
		else if (size > 1024 && size <= 8 * 1024)
			return _Index(size - 1024, 7) + CountArray[0] + CountArray[1];
		else if (size > 8 * 1024 && size <= 64 * 1024)
			return _Index(size - 8 * 1024, 10) + CountArray[0] + CountArray[1] + CountArray[2];
		else if (size > 64 * 1024 && size <= 256 * 1024)
			return _Index(size - 64 * 1024, 13) + CountArray[0] + CountArray[1] + CountArray[2] + CountArray[3];
		else assert(false);

		return -1;
	}

	// 一次ThreadCache向CentralCache申请多少个
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		size_t num = MAX_MEMORYSIZE / size;
		if (num < 2) num = 2;  // 大对象申请少
		if (num > 512) num = 512;  // 小对象申请多
		return num;
	}

};

struct Span 
{
	PAGE_TD _pageId = 0;        // 起始页号
	size_t _n = 0;              // 页的数量

	Span* _prev = nullptr;    // 上一个span
	Span* _next = nullptr;    // 下一个span

	size_t _useCount = 0;          // 使用计数
	void* _freeList = nullptr; // 切好的小块内存的自由链表
};

class SpanList
{
public:
	std::mutex _mtx; // 桶锁

	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	void insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);
		Span* prev = pos->_prev;
		newSpan->_next = pos;
		newSpan->_prev = prev;
		prev->_next = newSpan;
		pos->_prev = newSpan;
	}

	void erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);
		Span* prev = pos->_prev;
		Span* next = pos->_next;
		prev->_next = next;
		next->_prev = prev;
	}

private:
	Span* _head = nullptr;
};