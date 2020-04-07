//不分系统
#pragma once
#include"Common.h"


//线程缓冲，效率高，因为没有锁
class ThreadCache
{
public:
	void* Allocate(size_t size);//分配内存
	void Deallocate(void* ptr,size_t size);//解除分配
	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);
	//链表中对象太多，开始回收。
	void ListTooLong(FreeList* freelist,size_t byte);
private:
	//自由链表，大小可控，
	FreeList _freelist[NLISTS];
	//int _tid;
	//ThreadCache* _next;
};

//静态tsl变量，所有线程都有一个！！！
static _declspec(thread) ThreadCache* tls_threadcache=nullptr;

