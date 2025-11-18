#include "PageCache.h"
//#include "CentralCache.h"

PageCache PageCache::_sInst;

//获取一个k页的span
Span* PageCache::NewSpan(size_t k)
{
	//assert(k > 0 && k < NPAGES);
	assert(k > 0);
	if (k > NPAGES - 1) //大于128页直接找堆申请
	{
		void* ptr = SystemAlloc(k);
		
		_totalAllocBytes += (k << PAGE_SHIFT);
		
		//Span* span = new Span;
		Span* span = _spanPool.New();

		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = k;
		//建立页号与span之间的映射
		//_idSpanMap[span->_pageId] = span;
		_idSpanMap.set(span->_pageId, span);

		return span;
	}
	//先检查第k个桶里面有没有span
	if (!_spanLists[k].Empty())
	{
		Span* kSpan = _spanLists[k].PopFront();

		//建立页号与span的映射，方便central cache回收小块内存时查找对应的span
		for (PAGE_ID i = 0; i < kSpan->_n; i++)
		{
			//_idSpanMap[kSpan->_pageId + i] = kSpan;
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}

		return kSpan;
	}
	//检查一下后面的桶里面有没有span，如果有可以将其进行切分
	for (size_t i = k + 1; i < NPAGES; i++)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			//Span* kSpan = new Span;
			Span* kSpan = _spanPool.New();

			//在nSpan的头部切k页下来
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;
			//将剩下的挂到对应映射的位置
			_spanLists[nSpan->_n].PushFront(nSpan);
			//存储nSpan的首尾页号与nSpan之间的映射，方便page cache合并span时进行前后页的查找
			//_idSpanMap[nSpan->_pageId] = nSpan;
			//_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;
			_idSpanMap.set(nSpan->_pageId, nSpan);
			_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

			//建立页号与span的映射，方便central cache回收小块内存时查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; i++)
			{
				//_idSpanMap[kSpan->_pageId + i] = kSpan;
				_idSpanMap.set(kSpan->_pageId + i, kSpan);
			}

			//cout << "dargon" << endl; //for test
			return kSpan;
		}
	}
	//走到这里说明后面没有大页的span了，这时就向堆申请一个128页的span
	//Span* bigSpan = new Span;
	Span* bigSpan = _spanPool.New();

	void* ptr = SystemAlloc(NPAGES - 1);
	//收集信息
	_totalAllocBytes += ((NPAGES - 1) << PAGE_SHIFT);
	
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);

	//尽量避免代码重复，递归调用自己
	return NewSpan(k);
}

//获取从对象到span的映射
Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT; //页号

	//std::unique_lock<std::mutex> lock(_pageMtx); //构造时加锁，析构时自动解锁
	//auto ret = _idSpanMap.find(id);
	//if (ret != _idSpanMap.end())
	//{
	//	return ret->second;
	//}
	//else
	//{
	//	assert(false);
	//	return nullptr;
	//}

	Span* ret = (Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}

//释放空闲的span回到PageCache，并合并相邻的span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	if (span->_n > NPAGES - 1) //大于128页直接释放给堆
	{
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr,0,SystemFreeMode::Release);
		//delete span;
		_spanPool.Delete(span);

		return;
	}
	//对span的前后页，尝试进行合并，缓解内存碎片问题
	//1、向前合并
	while (1)
	{
		PAGE_ID prevId = span->_pageId - 1;
		//auto ret = _idSpanMap.find(prevId);
		////前面的页号没有（还未向系统申请），停止向前合并
		//if (ret == _idSpanMap.end())
		//{
		//	break;
		//}
		Span* ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr)
		{
			break;
		}
		//前面的页号对应的span正在被使用，停止向前合并
		//Span* prevSpan = ret->second;
		Span* prevSpan = ret;
		if (prevSpan->_isUse == true)
		{
			break;
		}
		//合并出超过128页的span无法进行管理，停止向前合并
		if (prevSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		//进行向前合并
		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		//将prevSpan从对应的双链表中移除
		_spanLists[prevSpan->_n].Erase(prevSpan);

		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}
	//2、向后合并
	while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;
		//auto ret = _idSpanMap.find(nextId);
		////后面的页号没有（还未向系统申请），停止向后合并
		//if (ret == _idSpanMap.end())
		//{
		//	break;
		//}
		Span* ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr)
		{
			break;
		}
		//后面的页号对应的span正在被使用，停止向后合并
		//Span* nextSpan = ret->second;
		Span* nextSpan = ret;
		if (nextSpan->_isUse == true)
		{
			break;
		}
		//合并出超过128页的span无法进行管理，停止向后合并
		if (nextSpan->_n + span->_n > NPAGES - 1)
		{
			break;
		}
		//进行向后合并
		span->_n += nextSpan->_n;

		//将nextSpan从对应的双链表中移除
		_spanLists[nextSpan->_n].Erase(nextSpan);

		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}
	//将合并后的span挂到对应的双链表当中
	_spanLists[span->_n].PushFront(span);
	//建立该span与其首尾页的映射
	//_idSpanMap[span->_pageId] = span;
	//_idSpanMap[span->_pageId + span->_n - 1] = span;
	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);

	//将该span设置为未被使用的状态
	span->_isUse = false;
}


// 文件: ConcurrentMemoryPool/PageCache.cpp

PageCache::Stats PageCache::GetStatistics()
{
	Stats stats;
	stats.totalSystemAllocBytes = _totalAllocBytes.load();

	std::unique_lock<std::mutex> lock(_pageMtx);

	for (size_t i = 1; i < NPAGES; ++i)
	{
		if (_spanLists[i].Empty()) continue;

		Span* cur = _spanLists[i].Begin();
		while (cur != _spanLists[i].End())
		{
			stats.cachedSpanCount++;
			stats.cachedPageCount += cur->_n;
			cur = cur->_next;
		}
	}
	return stats;
}


// 【新增】内存归还机制：释放长时间闲置的内存
void PageCache::Trim()
{
	std::unique_lock<std::mutex> lock(_pageMtx);

	// 遍历所有 PageCache 的桶
	for (size_t i = 1; i < NPAGES; ++i)
	{
		SpanList& list = _spanLists[i];
		if (list.Empty()) continue;

		// 策略：每个桶只保留最多 4 个 Span，多余的释放
		// 这是一个简单的启发式策略，更高级的策略可以结合 LRU
		const size_t KEEP_LIMIT = 4;
		size_t currentSize = 0;

		// 由于 SpanList 没有 size 计数，我们需要遍历
		// 注意：为了安全删除，不能直接在遍历中 Erase，
		// 除非我们小心处理 next 指针。

		Span* cur = list.Begin();
		while (cur != list.End())
		{
			// 保留前几个，跳过
			if (currentSize < KEEP_LIMIT) {
				currentSize++;
				cur = cur->_next;
				continue;
			}

			// 超过保留限制，开始回收
			Span* spanToRemove = cur;
			cur = cur->_next; // 先保存下一个节点

			// 1. 从链表移除
			list.Erase(spanToRemove);

			// 2. 清除 PageMap 映射 (防止 dangling pointer)
			// 必须清除该 span 覆盖的所有页的映射，否则 SystemFree 后
			// 如果有人通过 MapObjectToSpan 访问这些页会崩溃
			for (PAGE_ID k = 0; k < spanToRemove->_n; ++k) {
				_idSpanMap.set(spanToRemove->_pageId + k, nullptr);
			}

			// 3. 归还物理内存给操作系统 (Release 模式)
			void* ptr = (void*)(spanToRemove->_pageId << PAGE_SHIFT);
			size_t bytes = spanToRemove->_n << PAGE_SHIFT;

			// 这里使用我们在 Common.h 定义的增强版 SystemFree
			SystemFree(ptr, bytes, SystemFreeMode::Release);

			// 4. 销毁 Span 对象
			_spanPool.Delete(spanToRemove);
		}
	}
}