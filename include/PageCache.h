#pragma once

/**
 * @file PageCache.h
 * @brief ҳ�����ඨ��
 * @details �������ڴ�ҳ�ķ��䡢���պͺϲ�����ϵͳ�ڴ�ֱ�ӽ���
 *          ʹ�û������Ż�ҳ�ŵ�Span��ӳ�䣬�����������ܺ��ڴ�Ч��
 */

#include "Common.h"
#include "ObjectPool.h"
#include "RadixTree.h"

/**
 * @class PageCache
 * @brief ҳ��������ࣨ����ģʽ��
 * @details ����������ڴ�ҳ��֧��ҳ�ķ��䡢���պ�����ҳ�ĺϲ�
 *          ʹ��ȫ������֤�̰߳�ȫ�����ڴ�ص���ײ�
 */
class PageCache
{
public:
    /**
     * @brief ��ȡPageCache����ʵ��
     * @return PageCache����ָ��
     */
    static PageCache *GetInstance()
    {
        return &_sInst;
    }

    /**
     * @brief ����kҳ�������ڴ�
     * @param k ��Ҫ�����ҳ��
     * @return �����Spanָ��
     */
    Span *NewSpan(size_t k);

    /**
     * @brief �����ڴ��ַӳ�䵽��Ӧ��Span
     * @param obj �ڴ����ָ��
     * @return ��Ӧ��Spanָ��
     */
    Span *MapObjectToSpan(void *obj);

    /**
     * @brief �ͷ�Span��PageCache�������Ժϲ�����ҳ
     * @param span Ҫ�ͷŵ�Spanָ��
     */
    void ReleaseSpanToPageCache(Span *span);

    /**
     * @brief ��ȡPageCache�Ļ���������
     * @return ����������
     */
    std::mutex &GetMutex()
    {
        return _pageMtx;
    }

private:
    SpanList _pageList[MAX_PAGESIZE];               // ��ҳ�������Span��������
    ObjectPool<Span> _spanPool;                     // Span����أ�����Ƶ��new/delete

    SpanRadixTree _idSpanMap;                       // ��������ҳ�ŵ�Span��ӳ�䣬�Ż���������

    std::mutex _pageMtx;                            // ȫ��������֤PageCache�̰߳�ȫ

private:
    // ����ģʽ����ֹ�ⲿ����Ϳ���
    PageCache() {}
    PageCache(const PageCache &) = delete;

    static PageCache _sInst;                        // ��̬����ʵ��
};