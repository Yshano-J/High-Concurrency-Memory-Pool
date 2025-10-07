#pragma once

/**
 * @file ObjectPool.h
 * @brief 定长对象池实现
 * @details 高效的对象内存管理，避免频繁的new/delete操作
 */

#include "Common.h"

static const size_t FIXED_BLOCK_SIZE = 128 * 1024; // 固定内存块大小：128KB

/**
 * @class ObjectPool
 * @brief 定长对象池模板类
 * @tparam T 对象类型
 * @details 使用内存池技术管理对象的创建和销毁，提高内存分配效率
 *          采用自由链表管理回收的对象，减少内存碎片
 */
template<class T>
class ObjectPool
{
public:
	/**
	 * @brief 从对象池中获取一个对象
	 * @return 对象指针
	 * @details 优先从自由链表中复用已回收的对象，
	 *          如果没有可复用对象则从内存块中分配新对象
	 */
	T* New()
	{
		T* obj = nullptr;
		// 优先复用回收的对象
		if (_freeList)
		{
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
			return obj;
		}
		else
		{
			// 当前内存块不够分配一个对象时，申请新的内存块
			if (_leftBytes < sizeof(T))
			{
				_leftBytes = FIXED_BLOCK_SIZE;
				_memory = (char*)malloc(_leftBytes);
				if (_memory == nullptr)
					throw std::bad_alloc();
			}

			obj = (T*)_memory;
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_leftBytes -= objSize;

			// 使用placement new显式调用构造函数初始化对象
			new(obj)T;
			return obj;
		}
	}

	/**
	 * @brief 归还对象到对象池
	 * @param obj 要归还的对象指针
	 * @details 显式调用析构函数清理对象，然后将对象加入自由链表
	 *          注意：这里并不真正释放内存，而是将对象标记为可复用
	 */
	void Delete(T* obj)
	{
		// 显式调用析构函数清理对象
		obj->~T();

		// 将对象加入自由链表头部（头插法）
		*(void**)obj = _freeList;
		_freeList = obj;
	}

private:
	char* _memory = nullptr;    // 指向当前内存块的指针
	size_t _leftBytes = 0;      // 当前内存块剩余字节数
	void* _freeList = nullptr;  // 自由链表头指针，管理已回收的对象
};

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;
	TreeNode()
		: _val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

inline void TestObjectPool()
{
	//申请释放的轮次
	const size_t Rounds = 5;
	//每轮申请释放多少次
	const size_t N = 1000000;
	size_t begin1 = clock();
	std::vector<TreeNode*> v1;
	v1.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (int i = 0; i < N; ++i)
		{
			delete v1[i];
		}

		v1.clear();
	}
	size_t end1 = clock();

	ObjectPool<TreeNode>TNPool;
	size_t begin2 = clock();
	std::vector<TreeNode*> v2;
	v2.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v2.push_back(TNPool.New());
		}
		for (int i = 0; i < N; ++i)
		{
			TNPool.Delete(v2[i]);
		}
		v2.clear();
	}
	size_t end2 = clock();

	cout << "new cost time:" << end1 - begin1 << endl;
	cout << "object pol cost time:" << end2 - begin2 << endl;
}