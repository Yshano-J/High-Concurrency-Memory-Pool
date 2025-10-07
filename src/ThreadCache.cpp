/**
 * @file ThreadCache.cpp
 * @brief ThreadCache类的实现
 * @details 实现线程本地内存缓存的分配、释放和与中央缓存的交互
 */

#include "ThreadCache.h"
#include "CentralCache.h"

/**
 * @brief 从CentralCache中批量获取内存对象
 * @param index 桶索引
 * @param size 对象大小
 * @return 获取到的内存对象指针
 * @details 使用慢启动反馈调节算法动态调整批量获取数量，
 *          提高内存分配效率并减少锁竞争
 */
void *ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 慢启动反馈调节算法：动态调整批量获取数量
	size_t batchNum = std::min(SizeClass::NumMoveSize(size), _freeList[index].maxSize());
	if (_freeList[index].maxSize() == batchNum)
		_freeList[index].maxSize() += 2; // 逐步增加批量数量

	void *start = nullptr, *end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0);

	if (actualNum == 1)
	{
		// 只获取到一个对象，直接返回
		assert(start == end);
		return start;
	}
	else
	{
		// 获取到多个对象，将除第一个外的其他对象加入自由链表
		_freeList[index].PushRange(NextObj(start), end, actualNum - 1);
		return start;
	}
}

/**
 * @brief 从线程缓存分配指定大小的内存
 * @param size 需要分配的内存大小
 * @return 分配的内存指针
 * @details 分配流程：
 *          1. 将大小向上对齐到合适的边界
 *          2. 计算对应的桶索引
 *          3. 如果桶中有空闲对象则直接返回
 *          4. 否则从CentralCache批量获取对象
 */
void *ThreadCache::Allocate(size_t size)
{
	assert(size < MAX_MEMORYSIZE);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t freeListPos = SizeClass::Index(size);
	
	if (!_freeList[freeListPos].isEmpty())
	{
		// 桶中有空闲对象，直接分配
		return _freeList[freeListPos].pop();
	}
	else
	{
		// 桶为空，从CentralCache获取对象
		return FetchFromCentralCache(freeListPos, alignSize);
	}
}

/**
 * @brief 归还内存到线程缓存
 * @param ptr 要归还的内存指针
 * @param size 内存块大小
 * @details 归还流程：
 *          1. 计算对应的桶索引
 *          2. 将内存块加入对应的自由链表
 *          3. 检查链表长度，如果过长则归还部分给CentralCache
 */
void ThreadCache::Deallocate(void *ptr, size_t size)
{
	assert(size < MAX_MEMORYSIZE);
	size_t freeListPos = SizeClass::Index(size);
	_freeList[freeListPos].push(ptr);

	// 当链表长度达到批量传输阈值时，归还一批给CentralCache
	// 这样可以避免单个线程占用过多内存，保持内存在线程间的平衡分布
	if (_freeList[freeListPos].size() >= _freeList[freeListPos].maxSize())
	{
		ListTooLong(_freeList[freeListPos], size);
	}
}

/**
 * @brief 处理自由链表过长的情况
 * @param list 过长的自由链表
 * @param size 对象大小
 * @details 将一批对象归还给CentralCache，减少ThreadCache的内存占用
 */
void ThreadCache::ListTooLong(FreeList &list, size_t size)
{
	void *start = nullptr, *end = nullptr;
	list.PopRange(start, end, list.maxSize());
	CentralCache::GetInstance()->ReleaseListToSpan(start, size);
}
