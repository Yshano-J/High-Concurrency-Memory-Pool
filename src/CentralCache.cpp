/**
 * @file CentralCache.cpp
 * @brief CentralCache类的实现
 * @details 实现中央缓存的Span管理、对象分配和回收功能
 */

#include "CentralCache.h"
#include "PageCache.h"

// 单例对象定义
CentralCache CentralCache::_sInst;

/**
 * @brief 获取一个包含空闲对象的Span
 * @param list 对应大小的SpanList
 * @param size 对象大小
 * @return 包含空闲对象的Span指针
 * @details 获取流程：
 *          1. 遍历SpanList查找有空闲对象的Span
 *          2. 如果没有找到，从PageCache申请新的Span
 *          3. 将新Span切分成指定大小的对象链表
 *          4. 将切分好的Span加入SpanList
 */
Span *CentralCache::GetOneSpan(SpanList &list, size_t size)
{
	// 遍历SpanList查找有空闲对象的Span
	Span *it = list.begin();
	while (it != list.end())
	{
		if (it->_freeList)
			return it;
		else
			it = it->_next;
	}

	// 没有找到空闲Span，需要从PageCache申请新的Span
	list._mtx.unlock(); // 先释放桶锁，避免死锁

	// 向PageCache申请新的Span
	PageCache::GetInstance()->GetMutex().lock();
	Span *span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true; // 标记Span为使用状态
	span->_objSize = size;
	PageCache::GetInstance()->GetMutex().unlock();

	// 将Span切分成指定大小的对象链表
	// 此时不需要加锁，因为其他线程还访问不到这个新Span
	
	// 计算Span内存的起始地址和结束地址
	char *start = (char *)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char *end = start + bytes;

	// 将Span内存切分成size大小的对象，并用自由链表串联起来
	span->_freeList = start;
	start += size;
	void *tail = span->_freeList;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = start;
		start += size;
	}
	NextObj(tail) = nullptr; // 链表尾部置空

	// 将切分好的Span加入SpanList
	list._mtx.lock(); // 重新加锁
	list.push_front(span);
	return span;
}

/**
 * @brief 从CentralCache中批量获取内存对象
 * @param start 返回的对象链表起始指针
 * @param end 返回的对象链表结束指针
 * @param batchNum 期望获取的对象数量
 * @param size 对象大小
 * @return 实际获取到的对象数量
 * @details 获取流程：
 *          1. 根据对象大小计算桶索引
 *          2. 获取包含空闲对象的Span
 *          3. 从Span中取出指定数量的对象（如果不够则有多少取多少）
 *          4. 更新Span的使用计数
 */
size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);

	_spanList[index]._mtx.lock();

	Span *span = GetOneSpan(_spanList[index], size);
	assert(span);
	assert(span->_freeList);

	// 从Span的自由链表中获取batchNum个对象
	// 如果Span中的对象不够batchNum个，则有多少取多少
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	while ((i < batchNum - 1) && NextObj(end))
	{
		end = NextObj(end);
		++i, ++actualNum;
	}
	
	// 更新Span的自由链表头指针
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr; // 截断链表
	
	// 更新Span的使用计数
	span->_useCount += actualNum;

	_spanList[index]._mtx.unlock();

	return actualNum;
}

/**
 * @brief 将内存对象链表归还到CentralCache的对应Span
 * @param start 内存对象链表的起始指针
 * @param bytes_size 内存对象的大小
 * @details 归还流程：
 *          1. 根据对象大小计算桶索引
 *          2. 遍历对象链表，将每个对象归还到对应的Span
 *          3. 更新Span的使用计数
 *          4. 如果Span的所有对象都归还了，将Span归还给PageCache
 */
void CentralCache::ReleaseListToSpan(void *start, size_t bytes_size)
{
	size_t index = SizeClass::Index(bytes_size);
	_spanList[index]._mtx.lock();

	while (start)
	{
		void *next = NextObj(start);

		// 根据内存地址找到对应的Span
		Span *span = PageCache::GetInstance()->MapObjectToSpan(start);
		
		// 将对象归还到Span的自由链表
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		
		// 如果Span的所有对象都归还了，将整个Span归还给PageCache
		if (span->_useCount == 0)
		{
			// 从SpanList中移除该Span
			_spanList[index].erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// 将Span归还给PageCache（需要先释放桶锁，再获取PageCache锁）
			_spanList[index]._mtx.unlock();
			PageCache::GetInstance()->GetMutex().lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->GetMutex().unlock();
			_spanList[index]._mtx.lock(); // 重新获取桶锁
		}

		start = next;
	}

	_spanList[index]._mtx.unlock();
}