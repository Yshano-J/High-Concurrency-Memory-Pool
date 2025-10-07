#pragma once

/**
 * @file ThreadCache.h
 * @brief 线程缓存类定义
 * @details 每个线程私有的内存缓存，提供无锁的快速内存分配和释放
 */

#include "Common.h"

/**
 * @class ThreadCache
 * @brief 线程本地内存缓存类
 * @details 每个线程拥有一个ThreadCache实例，通过哈希桶管理不同大小的内存块
 *          实现线程本地的快速内存分配，避免锁竞争
 */
class ThreadCache
{
public:
	/**
	 * @brief 从线程缓存分配指定大小的内存
	 * @param size 需要分配的内存大小
	 * @return 分配的内存指针
	 */
	void *Allocate(size_t size);

	/**
	 * @brief 归还内存到线程缓存
	 * @param ptr 要归还的内存指针
	 * @param size 内存块大小
	 */
	void Deallocate(void *ptr, size_t size);

	/**
	 * @brief 从中央缓存获取内存对象
	 * @param index 桶索引
	 * @param size 对象大小
	 * @return 获取到的内存指针
	 */
	void *FetchFromCentralCache(size_t index, size_t size);

	/**
	 * @brief 处理自由链表过长的情况
	 * @param list 过长的自由链表
	 * @param size 对象大小
	 */
	void ListTooLong(FreeList &list, size_t size);

private:
	FreeList _freeList[MAX_BUCKETSIZE]; // 哈希桶数组，每个桶管理一种大小的内存块
};

// ================================ 线程本地存储 ================================

/**
 * @brief 线程本地存储的ThreadCache指针
 * @details 使用C++11的thread_local关键字，每个线程拥有独立的ThreadCache实例
 */
thread_local static ThreadCache *pTLSThreadCache = nullptr;