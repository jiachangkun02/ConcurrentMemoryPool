#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES) //大于256KB的内存申请
	{
		//计算出对齐后需要申请的页数
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kPage = alignSize >> PAGE_SHIFT;

		//向page cache申请kPage页的span
		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kPage);
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		//通过TLS，每个线程无锁的获取自己专属的ThreadCache对象
		if (pTLSThreadCache == nullptr)
		{
			static std::mutex tcMtx;
			//cout << std::this_thread::get_id() <<"  "<<&tcMtx<< endl;
			static ObjectPool<ThreadCache> tcPool;
			tcMtx.lock();
			//pTLSThreadCache = new ThreadCache;
			pTLSThreadCache = tcPool.New();
			tcMtx.unlock();
		}
		//cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;

		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	if (size > MAX_BYTES) //大于256KB的内存释放
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}

inline static void* ConcurrentAllocAligned(size_t alignment, size_t size)
{
	//默认的对齐就是8字节
	if (alignment <= 8)
	{
		return ConcurrentAlloc(size);
	}

	// 检查对齐是否是 2 的幂 如果不是返回空指针 这是非法的对齐要求 
	if ((alignment & (alignment - 1)) != 0)
	{
		return nullptr;
	}


	//计算想要额外分配的空间
	size_t extra_need = alignment + sizeof(void*);
	size_t total_size = size + extra_need;

	void* original_ptr = ConcurrentAlloc(total_size);

	if (original_ptr == nullptr)
	{
		return nullptr;
	}


	void* aligned_ptr = (void*)
		(
			((uintptr_t)original_ptr + sizeof(void*) + (alignment - 1))
			& ~(alignment - 1)
			);
	//stashing
	((void**)aligned_ptr)[-1] = original_ptr;

	return aligned_ptr;
}


inline static void ConcurrentFreeAligned(void* aligned_ptr)
{
	if (aligned_ptr == nullptr)
	{
		return;
	}
	//unstashing  
	void* original_ptr = ((void**)aligned_ptr)[-1];

	ConcurrentFree(original_ptr);

}




