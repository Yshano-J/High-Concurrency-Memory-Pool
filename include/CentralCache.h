#pragma once

/**
 * @file CentralCache.h
 * @brief 中央缓存类定义
 * @details 全局唯一的中央缓存，负责管理ThreadCache和PageCache之间的内存交换
 */

#include "Common.h"

/**
 * @class CentralCache
 * @brief 中央缓存类（单例模式 - 饿汉模式）
 * @details 作为ThreadCache和PageCache之间的中介层，管理Span的分配和回收
 *          使用桶锁机制保证线程安全，减少锁竞争
 */
class CentralCache
{
public:
	/**
	 * @brief 获取CentralCache单例实例
	 * @return CentralCache单例指针
	 */
	static CentralCache *GetInstance()
	{
		return &_sInst;
	}

	/**
	 * @brief 从CentralCache中批量获取内存对象
	 * @param start 返回的对象链表起始指针
	 * @param end 返回的对象链表结束指针
	 * @param n 期望获取的对象数量
	 * @param size 对象大小
	 * @return 实际获取到的对象数量
	 */
	size_t FetchRangeObj(void *&start, void *&end, size_t n, size_t size);

	/**
	 * @brief 获取一个非空的Span
	 * @param list 对应大小的SpanList
	 * @param size 对象大小
	 * @return 非空的Span指针
	 */
	Span *GetOneSpan(SpanList &list, size_t size);

	/**
	 * @brief 归还一段内存链表到CentralCache的对应Span
	 * @param start 内存链表起始指针
	 * @param bytes_size 内存块大小
	 */
	void ReleaseListToSpan(void *start, size_t bytes_size);

private:
	SpanList _spanList[MAX_BUCKETSIZE]; // Span链表数组，按对象大小分类管理

private:
	// 单例模式：禁止外部构造、拷贝和赋值
	CentralCache() {}
	CentralCache(const CentralCache &) = delete;
	CentralCache operator=(const CentralCache &) = delete;

	static CentralCache _sInst; // 静态单例实例
};