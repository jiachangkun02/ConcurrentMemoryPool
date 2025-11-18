#pragma once

#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"


//单例模式
class PageCache
{
public:
	//提供一个全局访问点
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	//获取一个k页的span
	Span* NewSpan(size_t k);

	//获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);
	
	//手动或定期调用此函数释放多余内存
	void Trim();

	//释放空闲的span回到PageCache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

	std::mutex _pageMtx; //大锁
	
	// 获取当前内存池的统计信息
	struct Stats {
		size_t totalSystemAllocBytes = 0; // 向系统申请的总字节
		size_t cachedSpanCount = 0;       // PageCache 中缓存的 Span 总数
		size_t cachedPageCount = 0;       // PageCache 中缓存的 Page 总数
	};

	Stats GetStatistics();

	// 配合 SystemAlloc 统计，需要修改 PageCache 记录 SystemAlloc 的调用
	std::atomic<size_t> _totalAllocBytes = { 0 };
private:
	SpanList _spanLists[NPAGES];
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	ObjectPool<Span> _spanPool;
	
	PageCache() //构造函数私有
	{}
	PageCache(const PageCache&) = delete; //防拷贝
	

	static PageCache _sInst;
};
