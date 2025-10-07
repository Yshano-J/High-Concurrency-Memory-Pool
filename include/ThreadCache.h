#pragma once

/**
 * @file ThreadCache.h
 * @brief �̻߳����ඨ��
 * @details ÿ���߳�˽�е��ڴ滺�棬�ṩ�����Ŀ����ڴ������ͷ�
 */

#include "Common.h"

/**
 * @class ThreadCache
 * @brief �̱߳����ڴ滺����
 * @details ÿ���߳�ӵ��һ��ThreadCacheʵ����ͨ����ϣͰ����ͬ��С���ڴ��
 *          ʵ���̱߳��صĿ����ڴ���䣬����������
 */
class ThreadCache
{
public:
	/**
	 * @brief ���̻߳������ָ����С���ڴ�
	 * @param size ��Ҫ������ڴ��С
	 * @return ������ڴ�ָ��
	 */
	void *Allocate(size_t size);

	/**
	 * @brief �黹�ڴ浽�̻߳���
	 * @param ptr Ҫ�黹���ڴ�ָ��
	 * @param size �ڴ���С
	 */
	void Deallocate(void *ptr, size_t size);

	/**
	 * @brief �����뻺���ȡ�ڴ����
	 * @param index Ͱ����
	 * @param size �����С
	 * @return ��ȡ�����ڴ�ָ��
	 */
	void *FetchFromCentralCache(size_t index, size_t size);

	/**
	 * @brief ��������������������
	 * @param list ��������������
	 * @param size �����С
	 */
	void ListTooLong(FreeList &list, size_t size);

private:
	FreeList _freeList[MAX_BUCKETSIZE]; // ��ϣͰ���飬ÿ��Ͱ����һ�ִ�С���ڴ��
};

// ================================ �̱߳��ش洢 ================================

/**
 * @brief �̱߳��ش洢��ThreadCacheָ��
 * @details ʹ��C++11��thread_local�ؼ��֣�ÿ���߳�ӵ�ж�����ThreadCacheʵ��
 */
thread_local static ThreadCache *pTLSThreadCache = nullptr;