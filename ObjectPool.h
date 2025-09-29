#pragma once
#include "Common.h"

static const size_t FIXED_BLOCK_SIZE = 128 * 1024; // 固定大小内存池分配128KB

// 定长的内存池
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		// 不为空，重复利用
		if (_freeList)
		{
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
			return obj;
		}
		else
		{
			if (_leftBytes < sizeof(T)) // 剩余内存不够一个对象大小时，重新分配一块大内存
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

			// 定位new , 显示调用T的构造函数 初始化
			new(obj)T;
			return obj;
		}
	}

	void Delete(T* obj)
	{
		// 显示调用T的析构函数
		obj->~T();

		*(void**)obj = _freeList;
		_freeList = obj;

		//// 这里没有真正释放内存，只是将对象放入空闲链表
		//if (_freeList == nullptr)
		//{
		//	_freeList = obj;
		//	// 32位系统下，指针：4Byte
		//	// 64位系统下，指针：8Byte
		//	// *(void**)obj
		//	*(void**)obj = nullptr;
		//}
		//else
		//{
		//	*(void**)obj = _freeList;
		//	_freeList = obj;
		//}
	}

private:
	char* _memory = nullptr;    // 指向大块内存的指针
	size_t _leftBytes = 0;      // 剩余内存字节数
	void* _freeList = nullptr;  // 空闲链表的头指针
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

void TestObjectPool()
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