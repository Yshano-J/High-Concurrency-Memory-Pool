#pragma once
#include "Common.h"

class ThreadCache
{
public:
	// ��threadcache�����ڴ�
	void* Allocate(size_t size);
	// �黹�ڴ浽threadcache
	void Deallocate(void* ptr, size_t size);
	// ��CentralCache�л�ȡ�ڴ�
	void* FetchFromCentralCache(size_t index, size_t size);
private:
	FreeList _freeList[MAX_BUCKETSIZE]; // ��ϣͰ, ÿ��Ͱһ����������
};

// C++11 TLS Thread-Local Storage 
thread_local ThreadCache* pTLSThreadCache = nullptr; // �߳�˽�е�ThreadCacheָ��