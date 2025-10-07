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

static const size_t MAX_MEMORYSIZE = 256 * 1024; // threadcache�����ڴ��С
static const size_t MAX_BUCKETSIZE = 208;		 // threadcache CentralCache ���Ͱ��
static const size_t MAX_PAGESIZE = 129;			 // PageCache���ҳ��: 128
static const size_t PAGE_SHIFT = 13;			 // 8Kһҳ

#ifdef _WIN64
typedef uint64_t PAGE_ID;
#elif _WIN32
typedef uint32_t PAGE_ID;
#else // Linux
typedef uint64_t PAGE_ID;
#endif

// ��ȡobj����һ������ָ��
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

// ����������
class FreeList
{
public:
	// �黹�ڴ�鵽��������
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
		NextObj(end) = nullptr; // �ض�
		_size -= n;
	}

	// �����������ȡ�ڴ��
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
	// Bytes��[1, 128]                  ���뵽8              index��Χ[0, 16)
	// Bytes��[128+1, 1024]             ���뵽16             index��Χ[16,72)
	// Bytes��[1024+1, 8*1024]          ���뵽128            index��Χ[72,128)
	// Bytes��[8*1024+1, 64*1024]       ���뵽1024           index��Χ[128,184)
	// Bytes��[64*1024+1, 256*1024]     ���뵽8*1024         index��Χ[184,208)
	static size_t _RoundUp(size_t size, size_t alignNum)
	{
		return (size + alignNum - 1) & ~(alignNum - 1);
	}
	// ��ö���֮����ڴ��С
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
	 * @brief �ڲ��������㺯��
	 * @param size �ڴ��С
	 * @param alignShifted ����λ����
	 * @return �����������
	 */
	static size_t _Index(size_t size, size_t alignShifted)
	{
		return ((size + ((long long)1 << alignShifted) - 1) >> alignShifted) - 1;
	}

	/**
	 * @brief �����ڴ��С�����Ӧ��Ͱ����
	 * @param size �ڴ��С
	 * @return ��Ӧ��Ͱ����
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
	 * @brief ����ThreadCache��CentralCacheһ�λ�ȡ�Ķ�������
	 * @param size �����С
	 * @return һ�λ�ȡ�Ķ�������
	 */
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		size_t num = MAX_MEMORYSIZE / size;
		if (num < 2)
			num = 2; // ���ٻ�ȡ��������
		if (num > 512)
			num = 512; // С�����ȡ��һЩ��������ȡ��һЩ
		return num;
	}

	/**
	 * @brief ����CentralCache��PageCacheһ�λ�ȡ��ҳ��
	 * @param size �����С
	 * @return ��Ҫ��ȡ��ҳ��
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

// ================================ Span�ṹ�� ================================

/**
 * @struct Span
 * @brief ҳ��Ƚṹ�壬�����������ڴ�ҳ
 * @details Span���ڴ�صĻ�������Ԫ�������������ڴ�ҳ����صĹ�����Ϣ
 */
struct Span
{
	PAGE_ID _pageId = 0;       // ��ʼҳ��
	size_t _n = 0;             // ����ҳ������

	Span *_prev = nullptr;     // ˫�������е�ǰһ��Span
	Span *_next = nullptr;     // ˫�������еĺ�һ��Span

	size_t _useCount = 0;      // �ѷ����ȥ��С���ڴ�����
	void *_freeList = nullptr; // �зֺõ�С���ڴ����������ͷָ��

	bool _isUse = false;       // ��Ǹ�Span�Ƿ����ڱ�ʹ�ã�����ҳ�ϲ��жϣ�

	size_t _objSize = 0;       // ��Span��ÿ��С����Ĵ�С
};

// ================================ Span������ ================================

/**
 * @class SpanList
 * @brief Span˫��ѭ�����������
 * @details ʹ�ô�ͷ�ڵ��˫��ѭ���������Span��֧���̰߳�ȫ����
 */
class SpanList
{
public:
	std::mutex _mtx; // Ͱ������֤�̰߳�ȫ

	/**
	 * @brief ���캯������ʼ����ͷ�ڵ��˫��ѭ������
	 */
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	/**
	 * @brief ��ȡ����ĵ�һ����Ч�ڵ�
	 * @return ��һ��Spanָ��
	 */
	Span *begin()
	{
		return _head->_next;
	}

	/**
	 * @brief ��ȡ����Ľ�����ǣ�ͷ�ڵ㣩
	 * @return ͷ�ڵ�ָ��
	 */
	Span *end()
	{
		return _head;
	}

	/**
	 * @brief ��������Ƿ�Ϊ��
	 * @return true��ʾΪ�գ�false��ʾ�ǿ�
	 */
	bool empty()
	{
		return begin() == end();
	}

	/**
	 * @brief ��������ͷ���ĵ�һ��Span
	 * @return ��������Spanָ��
	 */
	Span *pop_front()
	{
		assert(!empty());
		Span *first = begin();
		erase(first);
		return first;
	}

	/**
	 * @brief ������ͷ�������µ�Span
	 * @param newSpan Ҫ�����Spanָ��
	 */
	void push_front(Span *newSpan)
	{
		insert(begin(), newSpan);
	}

	/**
	 * @brief ��ָ��λ��ǰ�����µ�Span
	 * @param pos ����λ��
	 * @param newSpan Ҫ�����Spanָ��
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
	 * @brief ��������ɾ��ָ����Span
	 * @param pos Ҫɾ����Spanָ��
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