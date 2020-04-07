#pragma once
#include"Common.h"
#include"ThreadCache.h"
#include"PageCaceh.h"

void* ConcurrentAlloc(size_t size)
{
	if (size > MAXBYTES)//  >64k           ����64K��4K���ж��룡������
	{
		size_t roudsize = ClasssSize::_Roundup(size,PAGE_SHIFT);  //4k�Ķ�������12!!!
		//��ҳΪ��λ����
		size_t npage = roudsize >> PAGE_SHIFT;//�������ҳ
		Span* span=PageCache::GetInstance()->NewSpan(npage);
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		return ptr;
	}
	else{
		//ͨ��tls����ȡ�߳��Լ���threadcache
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