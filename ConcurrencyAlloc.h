#pragma once

#include "Common.h"
#include "ThreadCache.h"

void* ConcurrencyAlloc(size_t size)
{
	if (pTLSThreadCache == nullptr)
	{
		pTLSThreadCache = new ThreadCache();
	}
	return pTLSThreadCache->Allocate(size);
}

void ConcurrencyFree(void* ptr, size_t size)
{
	pTLSThreadCache->Deallocate(ptr, size);
}