#pragma once

#include "Common.h"

// ����ģʽ ����ģʽ
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	// ��CentralCache�л�ȡn��obj, ����ʵ�ʻ�ȡ������
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);
	// ��ȡһ���ǿյ�Span
	Span* GetOneSpan(SpanList& list, size_t size);

private:
	SpanList _spanList[MAX_BUCKETSIZE];
private:
	CentralCache(){}
	CentralCache(const CentralCache&) = delete;
	CentralCache operator=(const CentralCache&) = delete;

	static CentralCache _sInst;
};