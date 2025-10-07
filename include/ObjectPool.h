#pragma once

/**
 * @file ObjectPool.h
 * @brief ���������ʵ��
 * @details ��Ч�Ķ����ڴ��������Ƶ����new/delete����
 */

#include "Common.h"

static const size_t FIXED_BLOCK_SIZE = 128 * 1024; // �̶��ڴ���С��128KB

/**
 * @class ObjectPool
 * @brief ���������ģ����
 * @tparam T ��������
 * @details ʹ���ڴ�ؼ����������Ĵ��������٣�����ڴ����Ч��
 *          �����������������յĶ��󣬼����ڴ���Ƭ
 */
template<class T>
class ObjectPool
{
public:
	/**
	 * @brief �Ӷ�����л�ȡһ������
	 * @return ����ָ��
	 * @details ���ȴ����������и����ѻ��յĶ���
	 *          ���û�пɸ��ö�������ڴ���з����¶���
	 */
	T* New()
	{
		T* obj = nullptr;
		// ���ȸ��û��յĶ���
		if (_freeList)
		{
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
			return obj;
		}
		else
		{
			// ��ǰ�ڴ�鲻������һ������ʱ�������µ��ڴ��
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

			// ʹ��placement new��ʽ���ù��캯����ʼ������
			new(obj)T;
			return obj;
		}
	}

	/**
	 * @brief �黹���󵽶����
	 * @param obj Ҫ�黹�Ķ���ָ��
	 * @details ��ʽ�������������������Ȼ�󽫶��������������
	 *          ע�⣺���ﲢ�������ͷ��ڴ棬���ǽ�������Ϊ�ɸ���
	 */
	void Delete(T* obj)
	{
		// ��ʽ�������������������
		obj->~T();

		// �����������������ͷ����ͷ�巨��
		*(void**)obj = _freeList;
		_freeList = obj;
	}

private:
	char* _memory = nullptr;    // ָ��ǰ�ڴ���ָ��
	size_t _leftBytes = 0;      // ��ǰ�ڴ��ʣ���ֽ���
	void* _freeList = nullptr;  // ��������ͷָ�룬�����ѻ��յĶ���
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