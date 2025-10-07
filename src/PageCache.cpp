/**
 * @file PageCache.cpp
 * @brief PageCache���ʵ��
 * @details ʵ��ҳ������ڴ���䡢���պ�ҳ�ϲ�����
 */

#include "PageCache.h"

// ����������
PageCache PageCache::_sInst;

/**
 * @brief ����kҳ�������ڴ�
 * @param k ��Ҫ�����ҳ��
 * @return �����Spanָ��
 * @details ������ԣ�
 *          1. ���k�������ҳ����ֱ�Ӵ�ϵͳ����
 *          2. �����ӦͰ����Span��ֱ�ӷ���
 *          3. ���û�У��Ӹ����Ͱ���з�
 *          4. �����û�У���ϵͳ�������ڴ��ݹ����
 */
Span *PageCache::NewSpan(size_t k)
{
    assert(k > 0);

    // �����ڴ����룬ֱ�Ӵ�ϵͳ����
    if (k > MAX_PAGESIZE - 1)
    {
        void *ptr = SystemAlloc(k);
        // Span* span = new Span;
        Span *span = _spanPool.New();
        span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
        span->_n = k;
        _idSpanMap.insert(span->_pageId, span);

        return span;
    }

    // ����ӦͰ���Ƿ��п��õ�Span
    if (!_pageList[k].empty())
    {
        Span *partSpan = _pageList[k].pop_front();

        // ����ҳ�ŵ�Span��ӳ���ϵ�����ں����ĵ�ַ����
        for (PAGE_ID i = 0; i < partSpan->_n; i++)
        {
            _idSpanMap.insert(partSpan->_pageId + i, partSpan);
        }
        return partSpan;
    }

    // ��ǰͰΪ�գ��Ӹ����Ͱ���з�Span
    for (size_t i = k + 1; i < MAX_PAGESIZE; i++)
    {
        if (!_pageList[i].empty())
        {
            // �Ӵ�Span���зֳ�kҳ
            Span *span = _pageList[i].pop_front();
            // Span* partSpan = new Span;
            Span *partSpan = _spanPool.New();
            // ����Span��ǰkҳ�ָ�partSpan
            partSpan->_pageId = span->_pageId;
            partSpan->_n = k;
            // ����ԭSpan����Ϣ��ʣ�ಿ�֣�
            span->_pageId += k;
            span->_n -= k;

            // ��ʣ���Span�Żض�Ӧ��Ͱ��
            _pageList[span->_n].push_front(span);

            // �洢ʣ��Span����βҳ�ŵ�ӳ����У���������ϲ�����
            _idSpanMap.insert(span->_pageId, span);
            _idSpanMap.insert(span->_pageId + span->_n - 1, span);

            // �����·���Span��ҳ��ӳ���ϵ
            for (PAGE_ID i = 0; i < partSpan->_n; i++)
            {
                _idSpanMap.insert(partSpan->_pageId + i, partSpan);
            }

            return partSpan;
        }
    }

    // ����Ͱ��Ϊ�գ���ϵͳ�������ڴ�
    // Span* bigSpan = new Span;
    Span *bigSpan = _spanPool.New();

    void *ptr = SystemAlloc(MAX_PAGESIZE - 1); // ����128ҳ
    bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
    bigSpan->_n = MAX_PAGESIZE - 1;
    _pageList[bigSpan->_n].push_front(bigSpan);

    // �ݹ�����Լ�����ʱ�϶��ܳɹ�����
    return NewSpan(k);
}

/**
 * @brief �����ڴ��ַӳ�䵽��Ӧ��Span
 * @param obj �ڴ����ָ��
 * @return ��Ӧ��Spanָ��
 * @details ͨ��ҳ����ӳ����в��Ҷ�Ӧ��Span��
 *          �����ڴ����ʱȷ������Span�Ĺؼ�����
 */
Span *PageCache::MapObjectToSpan(void *obj)
{
    PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;
    std::lock_guard<std::mutex> guard(_pageMtx);
    
    Span* span = _idSpanMap.lookup(id);
    if (span)
    {
        return span;
    }
    else
    {
        assert(false); // ��Ӧ���Ҳ�����Ӧ��Span
        return nullptr;
    }
}

/**
 * @brief �ͷ�Span��PageCache�������Ժϲ�����ҳ
 * @param span Ҫ�ͷŵ�Spanָ��
 * @details �ͷ����̣�
 *          1. ����ǳ���Span��ֱ���ͷŸ�ϵͳ
 *          2. ������ǰ�ϲ����ڵĿ���ҳ
 *          3. �������ϲ����ڵĿ���ҳ
 *          4. ���ϲ����Span�����Ӧ��Ͱ��
 *          5. ����ҳ��ӳ���
 */
void PageCache::ReleaseSpanToPageCache(Span *span)
{
    assert(span);
    
    // ����Spanֱ���ͷŸ�ϵͳ
    if (span->_n > MAX_PAGESIZE - 1)
    {
        void *ptr = (void *)(span->_pageId << PAGE_SHIFT);
        SystemFree(ptr);
        _spanPool.Delete(span);
        return;
    }

    // ��ǰ�ϲ����ڵĿ���ҳ�������ڴ���Ƭ
    while (1)
    {
        PAGE_ID prevId = span->_pageId - 1;
        Span *prevSpan = _idSpanMap.lookup(prevId);

        if (!prevSpan)
            break; // û���ҵ�ǰһ��ҳ�������ϲ�
        if (prevSpan->_isUse)
            break; // ǰһ��ҳ����ʹ���У����ܺϲ�
        if (prevSpan->_n + span->_n > MAX_PAGESIZE - 1)
            break; // �ϲ���ᳬ�����ҳ������

        // ִ����ǰ�ϲ�
        span->_pageId = prevSpan->_pageId;
        span->_n += prevSpan->_n;

        // �ӻ�������ɾ�����ϲ�Span��ӳ��
        _idSpanMap.remove(prevSpan->_pageId);
        _idSpanMap.remove(prevSpan->_pageId + prevSpan->_n - 1);

        // �Ӷ�ӦͰ���Ƴ����ϲ���Span
        _pageList[prevSpan->_n].erase(prevSpan);
        _spanPool.Delete(prevSpan);
    }

    // ���ϲ����ڵĿ���ҳ
    while (1)
    {
        PAGE_ID nextId = span->_pageId + span->_n;
        Span *nextSpan = _idSpanMap.lookup(nextId);
        if (!nextSpan)
            break; // û���ҵ���һ��ҳ�������ϲ�
        if (nextSpan->_isUse)
            break; // ��һ��ҳ����ʹ���У����ܺϲ�
        if (nextSpan->_n + span->_n > MAX_PAGESIZE - 1)
            break; // �ϲ���ᳬ�����ҳ������

        // ִ�����ϲ�
        span->_n += nextSpan->_n;
        
        // �ӻ�������ɾ�����ϲ�Span��ӳ��
        _idSpanMap.remove(nextSpan->_pageId);
        _idSpanMap.remove(nextSpan->_pageId + nextSpan->_n - 1);
        
        // �Ӷ�ӦͰ���Ƴ����ϲ���Span
        _pageList[nextSpan->_n].erase(nextSpan);
        _spanPool.Delete(nextSpan);
    }

    // ���ϲ����Span�����Ӧ��Ͱ��
    _pageList[span->_n].push_front(span);
    span->_isUse = false; // ���Ϊδʹ��״̬
    
    // ����ҳ��ӳ���ֻ��Ҫ�洢��βҳ�ţ�
    _idSpanMap.insert(span->_pageId, span);
    _idSpanMap.insert(span->_pageId + span->_n - 1, span);
}