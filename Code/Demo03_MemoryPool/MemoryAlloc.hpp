#ifndef	MEMORYALLOC__HPP__
#define MEMORYALLOC__HPP__

#include "MemoryPool.hpp"

void* operator new(size_t nSize)
{
	return CMemoryManagement::Instance().Alloc(nSize);
}

void operator delete(void* pMem)
{
	CMemoryManagement::Instance().Free(pMem);
	return;
}

void* operator new[](size_t nSize)
{
	return CMemoryManagement::Instance().Alloc(nSize);
}

void operator delete[](void* pMem)
{
	CMemoryManagement::Instance().Free(pMem);
	return;
}

#endif // !OPERATOR__HPP__
