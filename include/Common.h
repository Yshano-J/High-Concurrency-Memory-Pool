#pragma once

#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>
#include <ctime>
#include <cstdint>
#include <thread>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#include <Windows.h>
#else // Linux
#include <sys/mman.h>
#endif

using std::cout;
using std::endl;

static const size_t MAX_MEMORYSIZE = 256 * 1024; // threadcache最大的内存大小
static const size_t MAX_BUCKETSIZE = 208;		 // threadcache CentralCache 最大桶数
static const size_t MAX_PAGESIZE = 129;			 // PageCache最大页数: 128
static const size_t PAGE_SHIFT = 13;			 // 8K一页

#ifdef _WIN64
typedef uint64_t PAGE_ID;
#elif _WIN32
typedef uint32_t PAGE_ID;
#else // Linux
typedef uint64_t PAGE_ID;
#endif

// 获取obj的下一个对象指针
static inline void *&NextObj(void *obj)
{
	return *(void **)obj;
}

inline static void *SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void *ptr = VirtualAlloc(NULL, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	void *ptr = mmap(NULL, kpage << PAGE_SHIFT, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

inline static void SystemFree(void *ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else

#endif
}

// 自由链表类
class FreeList
{
public:
	// 归还内存块到自由链表
	void push(void *obj)
	{
		NextObj(obj) = _freelist;
		_freelist = obj;
		++_size;
	}

	void PushRange(void *start, void *end, size_t n)
	{
		assert(start);
		assert(end);
		NextObj(end) = _freelist;
		_freelist = start;

		_size += n;
	}

	void PopRange(void *&start, void *&end, size_t n)
	{
		assert(n > 0 && n <= _size);
		start = _freelist;
		end = start;
		for (size_t i = 0; i < n - 1; i++)
			end = NextObj(end);
		_freelist = NextObj(end);
		NextObj(end) = nullptr; // 截断
		_size -= n;
	}

	// 从自由链表获取内存块
	void *pop()
	{
		void *obj = _freelist;
		_freelist = NextObj(_freelist);
		--_size;
		return obj;
	}

	bool isEmpty()
	{
		return _freelist == nullptr;
	}

	size_t &maxSize()
	{
		return _maxSize;
	}

	size_t size()
	{
		return _size;
	}

private:
	void *_freelist = nullptr;
	size_t _maxSize = 1; //
	size_t _size = 0;
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
		assert(size > 0);
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
		else
			return _RoundUp(size, 1 << PAGE_SHIFT);

		return -1;
	}

	/**
	 * @brief 内部索引计算函数
	 * @param size 内存大小
	 * @param alignShifted 对齐位移量
	 * @return 计算出的索引
	 */
	static size_t _Index(size_t size, size_t alignShifted)
	{
		return ((size + ((long long)1 << alignShifted) - 1) >> alignShifted) - 1;
	}

	/**
	 * @brief 根据内存大小计算对应的桶索引
	 * @param size 内存大小
	 * @return 对应的桶索引
	 */
	static size_t Index(size_t size)
	{
		assert(size < MAX_MEMORYSIZE);
		static int CountArray[] = {16, 56, 56, 56};

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
		else
			assert(false);

		return -1;
	}

	/**
	 * @brief 计算ThreadCache从CentralCache一次获取的对象数量
	 * @param size 对象大小
	 * @return 一次获取的对象数量
	 */
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		size_t num = MAX_MEMORYSIZE / size;
		if (num < 2)
			num = 2; // 最少获取两个对象
		if (num > 512)
			num = 512; // 小对象获取多一些，大对象获取少一些
		return num;
	}

	/**
	 * @brief 计算CentralCache从PageCache一次获取的页数
	 * @param size 对象大小
	 * @return 需要获取的页数
	 */
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;

		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;
		return npage;
	}
};

// ================================ Span结构体 ================================

/**
 * @struct Span
 * @brief 页跨度结构体，管理连续的内存页
 * @details Span是内存池的基本管理单元，包含连续的内存页和相关的管理信息
 */
struct Span
{
	PAGE_ID _pageId = 0;       // 起始页号
	size_t _n = 0;             // 连续页的数量

	Span *_prev = nullptr;     // 双向链表中的前一个Span
	Span *_next = nullptr;     // 双向链表中的后一个Span

	size_t _useCount = 0;      // 已分配出去的小块内存数量
	void *_freeList = nullptr; // 切分好的小块内存的自由链表头指针

	bool _isUse = false;       // 标记该Span是否正在被使用（用于页合并判断）

	size_t _objSize = 0;       // 该Span中每个小对象的大小
};

// ================================ Span链表类 ================================

/**
 * @class SpanList
 * @brief Span双向循环链表管理类
 * @details 使用带头节点的双向循环链表管理Span，支持线程安全操作
 */
class SpanList
{
public:
	std::mutex _mtx; // 桶锁，保证线程安全

	/**
	 * @brief 构造函数，初始化带头节点的双向循环链表
	 */
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	/**
	 * @brief 获取链表的第一个有效节点
	 * @return 第一个Span指针
	 */
	Span *begin()
	{
		return _head->_next;
	}

	/**
	 * @brief 获取链表的结束标记（头节点）
	 * @return 头节点指针
	 */
	Span *end()
	{
		return _head;
	}

	/**
	 * @brief 检查链表是否为空
	 * @return true表示为空，false表示非空
	 */
	bool empty()
	{
		return begin() == end();
	}

	/**
	 * @brief 弹出链表头部的第一个Span
	 * @return 被弹出的Span指针
	 */
	Span *pop_front()
	{
		assert(!empty());
		Span *first = begin();
		erase(first);
		return first;
	}

	/**
	 * @brief 在链表头部插入新的Span
	 * @param newSpan 要插入的Span指针
	 */
	void push_front(Span *newSpan)
	{
		insert(begin(), newSpan);
	}

	/**
	 * @brief 在指定位置前插入新的Span
	 * @param pos 插入位置
	 * @param newSpan 要插入的Span指针
	 */
	void insert(Span *pos, Span *newSpan)
	{
		assert(pos);
		assert(newSpan);
		Span *prev = pos->_prev;
		newSpan->_next = pos;
		newSpan->_prev = prev;
		prev->_next = newSpan;
		pos->_prev = newSpan;
	}

	/**
	 * @brief 从链表中删除指定的Span
	 * @param pos 要删除的Span指针
	 */
	void erase(Span *pos)
	{
		assert(pos);
		assert(pos != _head);
		Span *prev = pos->_prev;
		Span *next = pos->_next;
		prev->_next = next;
		next->_prev = prev;
	}

private:
	Span *_head = nullptr;
};