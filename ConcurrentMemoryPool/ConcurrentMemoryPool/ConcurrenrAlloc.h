#pragma once
#include"Common.h"
#include"ThreadCache.h"
#include"PageCaceh.h"

void* ConcurrentAlloc(size_t size)
{
	if (size > MAXBYTES)//  >64k           大于64K安4K进行对齐！！！！
	{
		size_t roudsize = ClasssSize::_Roundup(size,PAGE_SHIFT);  //4k的对齐数是12!!!
		//以页为单位对齐
		size_t npage = roudsize >> PAGE_SHIFT;//算出多少页
		Span* span=PageCache::GetInstance()->NewSpan(npage);
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		return ptr;
	}
	else{
		//通过tls，获取线程自己的threadcache
		if (tls_threadcache == nullptr)
		{
			tls_threadcache = new ThreadCache;
			//cout << std::this_thread::get_id()<<"->" << endl;

		}

		return tls_threadcache->Allocate(size);
		//return nullptr;
	}
}


void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objsize;

	if (size > MAXBYTES)
	{
		PageCache::GetInstance()->ReleaseSpanToPageCahce(span);
	}
	else{
		tls_threadcache->Deallocate(ptr, size);
	}
}