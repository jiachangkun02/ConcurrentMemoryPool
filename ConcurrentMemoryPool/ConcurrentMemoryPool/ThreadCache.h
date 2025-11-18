#pragma once

#include "Common.h"

class alignas(64) ThreadCache
{
public:
	//申请内存对象
	void* Allocate(size_t size);

	//释放内存对象
	void Deallocate(void* ptr, size_t size);

	//从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);

	//释放对象导致链表过长，回收内存到中心缓存
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELISTS]; //哈希桶
};

//TLS - Thread Local Storage
//static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
// TLS - Thread Local Storage（仅声明，不定义）
#if defined(_WIN32)
static __declspec(thread) ThreadCache* pTLSThreadCache=nullptr;
#else
static thread_local ThreadCache* pTLSThreadCache=nullptr;
#endif

