/**
 * @file CentralCache.cpp
 * @brief CentralCache���ʵ��
 * @details ʵ�����뻺���Span�����������ͻ��չ���
 */

#include "CentralCache.h"
#include "PageCache.h"

// ����������
CentralCache CentralCache::_sInst;

/**
 * @brief ��ȡһ���������ж����Span
 * @param list ��Ӧ��С��SpanList
 * @param size �����С
 * @return �������ж����Spanָ��
 * @details ��ȡ���̣�
 *          1. ����SpanList�����п��ж����Span
 *          2. ���û���ҵ�����PageCache�����µ�Span
 *          3. ����Span�зֳ�ָ����С�Ķ�������
 *          4. ���зֺõ�Span����SpanList
 */
Span *CentralCache::GetOneSpan(SpanList &list, size_t size)
{
	// ����SpanList�����п��ж����Span
	Span *it = list.begin();
	while (it != list.end())
	{
		if (it->_freeList)
			return it;
		else
			it = it->_next;
	}

	// û���ҵ�����Span����Ҫ��PageCache�����µ�Span
	list._mtx.unlock(); // ���ͷ�Ͱ������������

	// ��PageCache�����µ�Span
	PageCache::GetInstance()->GetMutex().lock();
	Span *span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true; // ���SpanΪʹ��״̬
	span->_objSize = size;
	PageCache::GetInstance()->GetMutex().unlock();

	// ��Span�зֳ�ָ����С�Ķ�������
	// ��ʱ����Ҫ��������Ϊ�����̻߳����ʲ��������Span
	
	// ����Span�ڴ����ʼ��ַ�ͽ�����ַ
	char *start = (char *)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char *end = start + bytes;

	// ��Span�ڴ��зֳ�size��С�Ķ��󣬲�����������������
	span->_freeList = start;
	start += size;
	void *tail = span->_freeList;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = start;
		start += size;
	}
	NextObj(tail) = nullptr; // ����β���ÿ�

	// ���зֺõ�Span����SpanList
	list._mtx.lock(); // ���¼���
	list.push_front(span);
	return span;
}

/**
 * @brief ��CentralCache��������ȡ�ڴ����
 * @param start ���صĶ���������ʼָ��
 * @param end ���صĶ����������ָ��
 * @param batchNum ������ȡ�Ķ�������
 * @param size �����С
 * @return ʵ�ʻ�ȡ���Ķ�������
 * @details ��ȡ���̣�
 *          1. ���ݶ����С����Ͱ����
 *          2. ��ȡ�������ж����Span
 *          3. ��Span��ȡ��ָ�������Ķ�������������ж���ȡ���٣�
 *          4. ����Span��ʹ�ü���
 */
size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);

	_spanList[index]._mtx.lock();

	Span *span = GetOneSpan(_spanList[index], size);
	assert(span);
	assert(span->_freeList);

	// ��Span�����������л�ȡbatchNum������
	// ���Span�еĶ��󲻹�batchNum�������ж���ȡ����
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	while ((i < batchNum - 1) && NextObj(end))
	{
		end = NextObj(end);
		++i, ++actualNum;
	}
	
	// ����Span����������ͷָ��
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr; // �ض�����
	
	// ����Span��ʹ�ü���
	span->_useCount += actualNum;

	_spanList[index]._mtx.unlock();

	return actualNum;
}

/**
 * @brief ���ڴ��������黹��CentralCache�Ķ�ӦSpan
 * @param start �ڴ�����������ʼָ��
 * @param bytes_size �ڴ����Ĵ�С
 * @details �黹���̣�
 *          1. ���ݶ����С����Ͱ����
 *          2. ��������������ÿ������黹����Ӧ��Span
 *          3. ����Span��ʹ�ü���
 *          4. ���Span�����ж��󶼹黹�ˣ���Span�黹��PageCache
 */
void CentralCache::ReleaseListToSpan(void *start, size_t bytes_size)
{
	size_t index = SizeClass::Index(bytes_size);
	_spanList[index]._mtx.lock();

	while (start)
	{
		void *next = NextObj(start);

		// �����ڴ��ַ�ҵ���Ӧ��Span
		Span *span = PageCache::GetInstance()->MapObjectToSpan(start);
		
		// ������黹��Span����������
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		
		// ���Span�����ж��󶼹黹�ˣ�������Span�黹��PageCache
		if (span->_useCount == 0)
		{
			// ��SpanList���Ƴ���Span
			_spanList[index].erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// ��Span�黹��PageCache����Ҫ���ͷ�Ͱ�����ٻ�ȡPageCache����
			_spanList[index]._mtx.unlock();
			PageCache::GetInstance()->GetMutex().lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->GetMutex().unlock();
			_spanList[index]._mtx.lock(); // ���»�ȡͰ��
		}

		start = next;
	}

	_spanList[index]._mtx.unlock();
}