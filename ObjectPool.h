#pragma once
#include "Common.h"

static const size_t FIXED_BLOCK_SIZE = 128 * 1024; // �̶���С�ڴ�ط���128KB

// �������ڴ��
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		// ��Ϊ�գ��ظ�����
		if (_freeList)
		{
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
			return obj;
		}
		else
		{
			if (_leftBytes < sizeof(T)) // ʣ���ڴ治��һ�������Сʱ�����·���һ����ڴ�
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

			// ��λnew , ��ʾ����T�Ĺ��캯�� ��ʼ��
			new(obj)T;
			return obj;
		}
	}

	void Delete(T* obj)
	{
		// ��ʾ����T����������
		obj->~T();

		*(void**)obj = _freeList;
		_freeList = obj;

		//// ����û�������ͷ��ڴ棬ֻ�ǽ���������������
		//if (_freeList == nullptr)
		//{
		//	_freeList = obj;
		//	// 32λϵͳ�£�ָ�룺4Byte
		//	// 64λϵͳ�£�ָ�룺8Byte
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
	char* _memory = nullptr;    // ָ�����ڴ��ָ��
	size_t _leftBytes = 0;      // ʣ���ڴ��ֽ���
	void* _freeList = nullptr;  // ���������ͷָ��
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
	//�����ͷŵ��ִ�
	const size_t Rounds = 5;
	//ÿ�������ͷŶ��ٴ�
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