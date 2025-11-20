//#pragma once
//
//#include "Common.h"
//
////单层基数树
//template <int BITS>
//class TCMalloc_PageMap1
//{
//public:
//	typedef uintptr_t Number;
//	explicit TCMalloc_PageMap1()
//	{
//		size_t size = sizeof(void*) << BITS; //需要开辟数组的大小
//		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT); //按页对齐后的大小
//		array_ = (void**)SystemAlloc(alignSize >> PAGE_SHIFT); //向堆申请空间
//		memset(array_, 0, size); //对申请到的内存进行清理
//	}
//	void* get(Number k) const
//	{
//		if ((k >> BITS) > 0) //k的范围不在[0, 2^BITS-1]
//		{
//			return NULL;
//		}
//		return array_[k]; //返回该页号对应的span
//	}
//	void set(Number k, void* v)
//	{
//		assert((k >> BITS) == 0); //k的范围必须在[0, 2^BITS-1]
//		array_[k] = v; //建立映射
//	}
//private:
//	void** array_; //存储映射关系的数组
//	static const int LENGTH = 1 << BITS; //页的数目
//};
//
////二层基数树
//template <int BITS>
//class TCMalloc_PageMap2
//{
//private:
//	static const int ROOT_BITS = 5;                //第一层对应页号的前5个比特位
//	static const int ROOT_LENGTH = 1 << ROOT_BITS; //第一层存储元素的个数
//	static const int LEAF_BITS = BITS - ROOT_BITS; //第二层对应页号的其余比特位
//	static const int LEAF_LENGTH = 1 << LEAF_BITS; //第二层存储元素的个数
//	//第一层数组中存储的元素类型
//	struct Leaf
//	{
//		void* values[LEAF_LENGTH];
//	};
//	Leaf* root_[ROOT_LENGTH]; //第一层数组
//public:
//	typedef uintptr_t Number;
//	explicit TCMalloc_PageMap2()
//	{
//		memset(root_, 0, sizeof(root_)); //将第一层的空间进行清理
//		PreallocateMoreMemory(); //直接将第二层全部开辟
//	}
//	void* get(Number k) const
//	{
//		const Number i1 = k >> LEAF_BITS;        //第一层对应的下标
//		const Number i2 = k & (LEAF_LENGTH - 1); //第二层对应的下标
//		if ((k >> BITS) > 0 || root_[i1] == NULL) //页号值不在范围或没有建立过映射
//		{
//			return NULL;
//		}
//		return root_[i1]->values[i2]; //返回该页号对应span的指针
//	}
//	void set(Number k, void* v)
//	{
//		const Number i1 = k >> LEAF_BITS;        //第一层对应的下标
//		const Number i2 = k & (LEAF_LENGTH - 1); //第二层对应的下标
//		assert(i1 < ROOT_LENGTH);
//		root_[i1]->values[i2] = v; //建立该页号与对应span的映射
//	}
//	//确保映射[start,start_n-1]页号的空间是开辟好了的
//	bool Ensure(Number start, size_t n)
//	{
//		for (Number key = start; key <= start + n - 1;)
//		{
//			const Number i1 = key >> LEAF_BITS;
//			if (i1 >= ROOT_LENGTH) //页号超出范围
//				return false;
//			if (root_[i1] == NULL) //第一层i1下标指向的空间未开辟
//			{
//				//开辟对应空间
//				static ObjectPool<Leaf> leafPool;
//				Leaf* leaf = (Leaf*)leafPool.New();
//				memset(leaf, 0, sizeof(*leaf));
//				root_[i1] = leaf;
//			}
//			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS; //继续后续检查
//		}
//		return true;
//	}
//	void PreallocateMoreMemory()
//	{
//		Ensure(0, 1 << BITS); //将第二层的空间全部开辟好
//	}
//};
//
////三层基数树
//template <int BITS>
//class TCMalloc_PageMap3
//{
//private:
//	static const int INTERIOR_BITS = (BITS + 2) / 3;       //第一、二层对应页号的比特位个数
//	static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS; //第一、二层存储元素的个数
//	static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS; //第三层对应页号的比特位个数
//	static const int LEAF_LENGTH = 1 << LEAF_BITS;         //第三层存储元素的个数
//	struct Node
//	{
//		Node* ptrs[INTERIOR_LENGTH];
//	};
//	struct Leaf
//	{
//		void* values[LEAF_LENGTH];
//	};
//	Node* NewNode()
//	{
//		static ObjectPool<Node> nodePool;
//		Node* result = nodePool.New();
//		if (result != NULL)
//		{
//			memset(result, 0, sizeof(*result));
//		}
//		return result;
//	}
//	Node* root_;
//public:
//	typedef uintptr_t Number;
//	explicit TCMalloc_PageMap3()
//	{
//		root_ = NewNode();
//	}
//	void* get(Number k) const
//	{
//		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);         //第一层对应的下标
//		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1); //第二层对应的下标
//		const Number i3 = k & (LEAF_LENGTH - 1);                    //第三层对应的下标
//		//页号超出范围，或映射该页号的空间未开辟
//		if ((k >> BITS) > 0 || root_->ptrs[i1] == NULL || root_->ptrs[i1]->ptrs[i2] == NULL)
//		{
//			return NULL;
//		}
//		return reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3]; //返回该页号对应span的指针
//	}
//	void set(Number k, void* v)
//	{
//		assert(k >> BITS == 0);
//		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);         //第一层对应的下标
//		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1); //第二层对应的下标
//		const Number i3 = k & (LEAF_LENGTH - 1);                    //第三层对应的下标
//		Ensure(k, 1); //确保映射第k页页号的空间是开辟好了的
//		reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v; //建立该页号与对应span的映射
//	}
//	//确保映射[start,start+n-1]页号的空间是开辟好了的
//	bool Ensure(Number start, size_t n)
//	{
//		for (Number key = start; key <= start + n - 1;)
//		{
//			const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);         //第一层对应的下标
//			const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1); //第二层对应的下标
//			if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH) //下标值超出范围
//				return false;
//			if (root_->ptrs[i1] == NULL) //第一层i1下标指向的空间未开辟
//			{
//				//开辟对应空间
//				Node* n = NewNode();
//				if (n == NULL) return false;
//				root_->ptrs[i1] = n;
//			}
//			if (root_->ptrs[i1]->ptrs[i2] == NULL) //第二层i2下标指向的空间未开辟
//			{
//				//开辟对应空间
//				static ObjectPool<Leaf> leafPool;
//				Leaf* leaf = leafPool.New();
//				if (leaf == NULL) return false;
//				memset(leaf, 0, sizeof(*leaf));
//				root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
//			}
//			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS; //继续后续检查
//		}
//		return true;
//	}
//	void PreallocateMoreMemory()
//	{}
//};


#pragma once

#include "Common.h"

//单层基数树
template <int BITS>
class TCMalloc_PageMap1
{
public:
	typedef uintptr_t Number;
	explicit TCMalloc_PageMap1()
	{
		size_t size = sizeof(void*) << BITS; //需要开辟数组的大小
		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT); //按页对齐后的大小
		array_ = (void**)SystemAlloc(alignSize >> PAGE_SHIFT); //向堆申请空间（按页数或由 SystemAlloc 约定）
		assert(array_ != nullptr);
		memset(array_, 0, alignSize); //对申请到的内存进行清理（使用对齐后的实际字节数）
	}
	void* get(Number k) const
	{
		if ((k >> BITS) > 0) //k的范围不在[0, 2^BITS-1]
		{
			return NULL;
		}
		return array_[k]; //返回该页号对应的span
	}
	void set(Number k, void* v)
	{
		assert((k >> BITS) == 0); //k的范围必须在[0, 2^BITS-1]
		array_[k] = v; //建立映射
	}
private:
	void** array_; //存储映射关系的数组
	static const int LENGTH = 1 << BITS; //页的数目
};

//二层基数树
template <int BITS>
class TCMalloc_PageMap2
{
private:
	static const int ROOT_BITS = 5;                //第一层对应页号的前5个比特位
	static const int ROOT_LENGTH = 1 << ROOT_BITS; //第一层存储元素的个数
	static const int LEAF_BITS = BITS - ROOT_BITS; //第二层对应页号的其余比特位
	static const int LEAF_LENGTH = 1 << LEAF_BITS; //第二层存储元素的个数
	//第一层数组中存储的元素类型
	struct Leaf
	{
		void* values[LEAF_LENGTH];
	};
	Leaf* root_[ROOT_LENGTH]; //第一层数组
public:
	typedef uintptr_t Number;
	explicit TCMalloc_PageMap2()
	{
		memset(root_, 0, sizeof(root_)); //将第一层的空间进行清理
		PreallocateMoreMemory(); //直接将第二层全部开辟
	}
	void* get(Number k) const
	{
		const Number i1 = k >> LEAF_BITS;        //第一层对应的下标
		const Number i2 = k & (LEAF_LENGTH - 1); //第二层对应的下标
		if ((k >> BITS) > 0 || root_[i1] == NULL) //页号值不在范围或没有建立过映射
		{
			return NULL;
		}
		return root_[i1]->values[i2]; //返回该页号对应span的指针
	}
	void set(Number k, void* v)
	{
		const Number i1 = k >> LEAF_BITS;        //第一层对应的下标
		const Number i2 = k & (LEAF_LENGTH - 1); //第二层对应的下标
		assert(i1 < ROOT_LENGTH);
		// 确保目标叶节点已分配（修复：避免直接在可能为 NULL 的 root_[i1] 上写入）
		if (!Ensure(k, 1))
		{
			// 无法分配则显式失败（可替换为日志/错误返回）
			assert(false && "TCMalloc_PageMap2::set Ensure failed");
			return;
		}
		// 现在 root_[i1] 必定非空
		root_[i1]->values[i2] = v; //建立该页号与对应span的映射
	}
	//确保映射[start,start_n-1]页号的空间是开辟好了的
	bool Ensure(Number start, size_t n)
	{
		for (Number key = start; key <= start + n - 1;)
		{
			const Number i1 = key >> LEAF_BITS;
			if (i1 >= ROOT_LENGTH) //页号超出范围
				return false;
			if (root_[i1] == NULL) //第一层i1下标指向的空间未开辟
			{
				//开辟对应空间
				static ObjectPool<Leaf> leafPool;
				Leaf* leaf = (Leaf*)leafPool.New();
				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS; //继续后续检查
		}
		return true;
	}
	void PreallocateMoreMemory()
	{
		Ensure(0, 1 << BITS); //将第二层的空间全部开辟好
	}
};

//三层基数树
template <int BITS>
class TCMalloc_PageMap3
{
private:
	static const int INTERIOR_BITS = (BITS + 2) / 3;       //第一、二层对应页号的比特位个数
	static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS; //第一、二层存储元素的个数
	static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS; //第三层对应页号的比特比特个数
	static const int LEAF_LENGTH = 1 << LEAF_BITS;         //第三层存储元素的个数
	struct Node
	{
		Node* ptrs[INTERIOR_LENGTH];
	};
	struct Leaf
	{
		void* values[LEAF_LENGTH];
	};
	Node* NewNode()
	{
		static ObjectPool<Node> nodePool;
		Node* result = nodePool.New();
		if (result != NULL)
		{
			memset(result, 0, sizeof(*result));
		}
		return result;
	}
	Node* root_;
public:
	typedef uintptr_t Number;
	explicit TCMalloc_PageMap3()
	{
		root_ = NewNode();
	}
	void* get(Number k) const
	{
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);         //第一层对应的下标
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1); //第二层对应的下标
		const Number i3 = k & (LEAF_LENGTH - 1);                    //第三层对应的下标
		//页号超出范围，或映射该页号的空间未开辟
		if ((k >> BITS) > 0 || root_->ptrs[i1] == NULL || root_->ptrs[i1]->ptrs[i2] == NULL)
		{
			return NULL;
		}
		return reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3]; //返回该页号对应span的指针
	}
	void set(Number k, void* v)
	{
		assert(k >> BITS == 0);
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);         //第一层对应的下标
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1); //第二层对应的下标
		const Number i3 = k & (LEAF_LENGTH - 1);                    //第三层对应的下标
		Ensure(k, 1); //确保映射第k页页号的空间是开辟好了的
		reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v; //建立该页号与对应span的映射
	}
	//确保映射[start,start+n-1]页号的空间是开辟好了的
	bool Ensure(Number start, size_t n)
	{
		for (Number key = start; key <= start + n - 1;)
		{
			const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);         //第一层对应的下标
			const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1); //第二层对应的下标
			if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH) //下标值超出范围
				return false;
			if (root_->ptrs[i1] == NULL) //第一层i1下标指向的空间未开辟
			{
				//开辟对应空间
				Node* n = NewNode();
				if (n == nullptr) return false;
				root_->ptrs[i1] = n;
			}
			if (root_->ptrs[i1]->ptrs[i2] == NULL) //第二层i2下标指向的空间未开辟
			{
				//开辟对应空间
				static ObjectPool<Leaf> leafPool;
				Leaf* leaf = leafPool.New();
				if (leaf == NULL) return false;
				memset(leaf, 0, sizeof(*leaf));
				root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
			}
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS; //继续后续检查
		}
		return true;
	}
	void PreallocateMoreMemory()
	{
		    //1. 接口一致性(Interface Consistency) :
			//    本函数是为了满足上层模板代码（如 CentralCache）对 PageMap 类型的统一接口要求。
			//    即便在 64 位系统下我们不进行预分配，也必须保留此函数接口，否则会导致模板实例化失败。
			//
			// 2. 时空权衡 (Time-Space Trade-off):
			//    - PageMap2 (通常用于32位) 总是预分配，因为空间开销小(约4MB)，换取更快的访问速度。
			//    - PageMap3 (通常用于64位) 必须采用"按需分配"(Lazy Allocation)，因为空间开销巨大。
			//
			// 3. 安全性防范 (OOM Protection):
			//    在 64 位地址空间下 (BITS通常为48)，如果强行预分配所有节点：
			//    需要内存 = 2^48 页 * 指针大小 ≈ 281 TB。
			//    这将直接导致操作系统内存耗尽 (OOM) 并杀死进程。
		
		// 逻辑：只有在管理的内存范围非常小 (BITS <= 20, 即页表项 < 100万) 时，才启用预分配。
		// 这里的 20 是一个经验阈值 (Heuristic)，确保预分配内存控制在几十MB的安全范围内。
		if constexpr (BITS <= 20)
		{
			// 场景 A：小内存环境 (类似 32位 行为)
			// 策略：一次性分配所有节点，牺牲少量启动内存，换取运行时无锁、无检查的高速访问。

			// 注意：必须将 1 强转为 Number (uintptr_t)，防止 BITS=32 时发生整型溢出。
			const Number fullRange = Number(1) << BITS;
			Ensure(0, fullRange);
		}
		else
		{
			// 场景 B：大内存环境 (64位 典型场景)
			// 策略：什么都不做 (Do Nothing)。
			// 原因：保持 PageMap3 的稀疏 (Sparse) 特性。
			// 实际的内存分配将推迟到 set() 函数调用 Ensure() 时才发生 (Lazy Allocation)。
			// 这种“留空”是针对 64 位系统防止崩溃的防御性编程手段。
		}
		
	}
};
	
