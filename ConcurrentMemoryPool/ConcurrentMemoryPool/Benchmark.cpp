//基准测试

#include "ConcurrentAlloc.h"
#include "ObjectPool.h"
#include "PageCache.h"

//ntimes：单轮次申请和释放内存的次数
//nworks：线程数
//rounds：轮次（跑多少轮）
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					//v.push_back(malloc(16));
					v.push_back(malloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
		});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime.load());
	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime.load());
	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n",
		nworks, nworks*rounds*ntimes, malloc_costtime.load() + free_costtime.load());
}

void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					//v.push_back(ConcurrentAlloc(16));
					v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
		});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%u个线程并发执行%u轮次，每轮次concurrent alloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime.load());
	printf("%u个线程并发执行%u轮次，每轮次concurrent dealloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime.load());
	printf("%u个线程并发concurrent alloc&dealloc %u次，总计花费：%u ms\n",
		nworks, nworks*rounds*ntimes, malloc_costtime.load() + free_costtime.load());
}

// 辅助函数：打印当前状态
void PrintStats(const char* tag)
{
	auto stats = PageCache::GetInstance()->GetStatistics();
	printf("[%s] SystemAlloc: %.2f MB | Cached Spans: %zu | Cached Pages: %zu\n",
		tag,
		stats.totalSystemAllocBytes / (1024.0 * 1024.0),
		stats.cachedSpanCount,
		stats.cachedPageCount);
}

void TestDecayAndTelemetry()
{
	printf("\n================ 内存归还与遥测功能测试 ================\n");

	// 1. 初始状态
	PrintStats("Step 1: 初始化");

	// 2. 制造高内存压力
	// 申请 2000 个 8KB 的对象（正好 1 页），这将迫使 PageCache 向系统申请大量内存
	// 同时也测试了 Telemetry 是否能正确统计 totalSystemAllocBytes
	const size_t ALLOC_COUNT = 2000;
	std::vector<void*> ptrs;
	ptrs.reserve(ALLOC_COUNT);

	printf(">>> 正再申请 %zu 个 8KB 对象...\n", ALLOC_COUNT);
	for (size_t i = 0; i < ALLOC_COUNT; ++i)
	{
		// 8KB 通常直接对齐为 1 页，或者根据你的 SizeClass 规则分配
		ptrs.push_back(ConcurrentAlloc(8 * 1024));
	}

	PrintStats("Step 2: 高负载中");

	// 3. 释放所有对象
	// 此时内存会回收到 ThreadCache -> CentralCache -> PageCache
	// PageCache 中的 Cached Spans 应该会飙升，但 SystemAlloc 不变（因为还没还给 OS）
	printf(">>> 正在释放所有对象...\n");
	for (auto p : ptrs)
	{
		ConcurrentFree(p);
	}
	ptrs.clear();

	PrintStats("Step 3: 释放后 (PageCache 缓存高峰)");

	// ---------------------------------------------------------
	// 此时请观察任务管理器：内存占用应该很高
	// ---------------------------------------------------------
	printf(">>> [请观察任务管理器] 此时内存占用应处于高位。\n");
	printf(">>> 准备执行 Trim (3秒后)...\n");
	std::this_thread::sleep_for(std::chrono::seconds(3));

	// 4. 执行内存归还 (Decay)
	// 这会遍历 PageCache，把多余的 Span 释放回操作系统
	PageCache::GetInstance()->Trim();
	printf(">>> Trim() 执行完毕。\n");

	// 5. 验证结果
	// Cached Spans 应该大幅减少
	PrintStats("Step 4: Trim 后 (应大幅下降)");

	printf(">>> [请观察任务管理器] 此时内存占用应显著下降。\n");
	printf("======================================================\n\n");
}


//无法百万并发  因为win32平台限制   

int main()
{
	size_t n = 10000;//最好保持在500000以下   若要追求更高性能  需要全部重写
	cout << "==========================================================" <<
		endl;
	BenchmarkConcurrentMalloc(n, 4, 10);
	cout << endl << endl;
	BenchmarkMalloc(n, 4, 10);
	cout << "==========================================================" <<
		endl;
	TestDecayAndTelemetry();
	
	return 0;
}