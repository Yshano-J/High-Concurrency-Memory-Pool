#pragma once

/**
 * @file CentralCache.h
 * @brief ���뻺���ඨ��
 * @details ȫ��Ψһ�����뻺�棬�������ThreadCache��PageCache֮����ڴ潻��
 */

#include "Common.h"

/**
 * @class CentralCache
 * @brief ���뻺���ࣨ����ģʽ - ����ģʽ��
 * @details ��ΪThreadCache��PageCache֮����н�㣬����Span�ķ���ͻ���
 *          ʹ��Ͱ�����Ʊ�֤�̰߳�ȫ������������
 */
class CentralCache
{
public:
	/**
	 * @brief ��ȡCentralCache����ʵ��
	 * @return CentralCache����ָ��
	 */
	static CentralCache *GetInstance()
	{
		return &_sInst;
	}

	/**
	 * @brief ��CentralCache��������ȡ�ڴ����
	 * @param start ���صĶ���������ʼָ��
	 * @param end ���صĶ����������ָ��
	 * @param n ������ȡ�Ķ�������
	 * @param size �����С
	 * @return ʵ�ʻ�ȡ���Ķ�������
	 */
	size_t FetchRangeObj(void *&start, void *&end, size_t n, size_t size);

	/**
	 * @brief ��ȡһ���ǿյ�Span
	 * @param list ��Ӧ��С��SpanList
	 * @param size �����С
	 * @return �ǿյ�Spanָ��
	 */
	Span *GetOneSpan(SpanList &list, size_t size);

	/**
	 * @brief �黹һ���ڴ�����CentralCache�Ķ�ӦSpan
	 * @param start �ڴ�������ʼָ��
	 * @param bytes_size �ڴ���С
	 */
	void ReleaseListToSpan(void *start, size_t bytes_size);

private:
	SpanList _spanList[MAX_BUCKETSIZE]; // Span�������飬�������С�������

private:
	// ����ģʽ����ֹ�ⲿ���졢�����͸�ֵ
	CentralCache() {}
	CentralCache(const CentralCache &) = delete;
	CentralCache operator=(const CentralCache &) = delete;

	static CentralCache _sInst; // ��̬����ʵ��
};