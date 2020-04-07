#include"CentralCache.h"
#include"PageCaceh.h"

CentralCache CentralCache::_inst;

////打桩 测试代码
//// 从中心缓存获取一定数量的对象给thread cache
//size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t bytes)
//{
//	start = malloc(bytes*n);
//	end = (char*)start + bytes*(n-1);
//	void* cur = start;
//	while (cur <= end)
//	{
//		void* next = (char*)cur + bytes;
//		NEXT_OBJ(cur) = next;
//		cur = next;
//	}
//	NEXT_OBJ(end) = nullptr;
//	return n;
//}

Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t bytes)
{
	Span* span = spanlist->begin();
	while (span != spanlist->end())
	{
		if (span->_objlist != nullptr)
		{
			return span;
		}
		span = span->_next;
	}

	//向pagecache申请一个新的合适大小的span
	size_t npage = ClasssSize::NumMovePage(bytes);     //最大的大小是64k，以单个对象对齐到16字节为例，64k除以16得到获取多少个，但是有个上限是512个，然后算出有多少个对象的情况下，再乘以它的字节，在对齐到我们的页！！
	Span* newspan = PageCache::GetInstance()->NewSpan(npage);
	
	//将span内存切割成一个个bytes大小的对象挂起来
	char* start = (char*)(newspan->_pageid << PAGE_SHIFT);//乘以4k，算出它的地址
	char* end = start + (newspan->_npage << PAGE_SHIFT);
	char* cur = start;
	char* next = cur + bytes;

	while (next<end)
	{
		NEXT_OBJ(cur) = next;
		cur = next;
		next = cur + bytes;
	}
	NEXT_OBJ(cur) = nullptr;
	newspan->_objlist = start;
	newspan->_objsize = bytes;
	newspan->_usecount = 0;

	//将newspan插入到spanlist中
	spanlist->PushFront(newspan);
	
	return newspan;
}

//打桩
// 从中心缓存获取一定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t bytes)
{
	
	size_t index = ClasssSize::Index(bytes);
	SpanList* spanlist = &_spanlist[index];

	//对当前桶进行加锁 RAII思想            ///减少锁的密度！！！！放到这一行
	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	Span* span = GetOneSpan(spanlist,bytes);
	
	void* cur = span->_objlist;
	void* prev = cur;
	
	size_t fetchnum = 0;
	while (cur != nullptr&&fetchnum<num)       //一种情况cur跳出循环，第二种情况fetech退出循环
	{
		prev = cur;
		cur = NEXT_OBJ(cur);
		++fetchnum;
	}
	//从spanlist上拿
	start = span->_objlist;
	end = prev;
	NEXT_OBJ(end) = nullptr;

	//拿完后，进行修改！！
	span->_objlist = cur;
	span->_usecount += fetchnum;

	//当一个span为空，将span移动到尾上，提高查找效率
	if (span->_objlist == nullptr)
	{
		spanlist->Erase(span);
		spanlist->PushBack(span);
	}

	return fetchnum;
}


// 将一定数量的对象释放到span跨度
void CentralCache::ReleaseListToSpans(void* start, size_t byte)
{
	size_t index = ClasssSize::Index(byte);
	SpanList* spanlist = &_spanlist[index];

	std::unique_lock<std::mutex> lock(spanlist->_mtx);
	while (start)
	{
		void* next = NEXT_OBJ(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//当释放对象回到空的span，把空的span移动到头上，提高查找效率
		if (span->_objlist == nullptr)
		{
			spanlist->Erase(span);
			spanlist->PushFront(span);
		}
			
		//头插
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;
		//span usecount==0表示span切出去的对象都还回来了
		//释放span回到pagecache进行合并
		if (--span->_usecount == 0)
		{

			spanlist->Erase(span);
			
			span->_objlist = nullptr;
			span->_objsize = 0;
			span->_prev = nullptr;
			span->_next = nullptr;
		
			PageCache::GetInstance()->ReleaseSpanToPageCahce(span);
		}
		start = next;
	}
}