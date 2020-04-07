#pragma once
#include<iostream>
#include<assert.h>
#include<thread>
#include<mutex>
#include<unordered_map>
#include<map>



#ifdef _WIN32
#include <windows.h>
#endif  //_WIN32


using namespace std;


//管理对象自由链表的长度
const size_t NLISTS=240;   //四个区间长度相加
const size_t MAXBYTES = 64 * 1024;//64k
const size_t PAGE_SHIFT = 12;
const size_t NPAGES = 129;


inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	//需要向系统申请内存
	VirtualFree(ptr,0,MEM_RELEASE);
	if (ptr == nullptr)
	{
		throw std::bad_alloc();
	}
#else
	//brk  mumap

#endif //WIN32

}

inline static void* SystemAlloc(size_t npage)
{
#ifdef _WIN32
	//需要向系统申请内存
	void* ptr = VirtualAlloc(NULL, (npage) << PAGE_SHIFT, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (ptr == nullptr)
	{
		throw std::bad_alloc();
	}
#else
	//brk  mmap

#endif //WIN32

	return ptr;
}

static inline void*& NEXT_OBJ(void* obj)
{
	return *((void**)obj);
}


//以页为单位
typedef size_t PageID;
//Span跨度，将小东西进行合并                           32位系统 4G虚拟内存，，一页约4k
struct Span   
{
	PageID _pageid=0;			//页号
	size_t _npage=0;		//页的数量
	
	Span* _next=nullptr;
	Span* _prev=nullptr;

	void* _objlist=nullptr;		//对象自由链表
	size_t _objsize = 0;		//对象大小
	size_t _usecount=0;			//使用计数

};
struct SpanList
{
	//设计成带头双向循环链表！！！
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}
	Span* begin()
	{
		return _head->_next;
	}

	Span* end()
	{
		return _head;
	}

	void Insert(Span* cur,Span* newspan)
	{
		assert(cur);
		Span* prev = cur->_prev;
		//prev newspan cur
		prev->_next = newspan;
		newspan->_prev = prev;
		newspan->_next = cur;
		cur->_prev = newspan;
	}

	void Erase(Span* cur)
	{
		assert(cur!=nullptr&&cur != _head);
		Span* prev = cur->_prev;
		Span* next = cur->_next;

		prev->_next = next;
		next->_prev = prev;

	}

	void PushBack(Span* span)
	{
		Insert(end(), span);
	}

	Span* PopBack()
	{
		Span* tail = _head->_prev;
		Erase(tail);
		return tail;
	}

	void PushFront(Span* span)
	{
		Insert(begin(), span);
	}

	Span* PopFront()
	{
		Span* span = begin();
		Erase(span);
		return span;
	}
	bool Empty()
	{
		return _head->_next == _head;
	}
public:
	std::mutex _mtx;
private:
	Span* _head = nullptr;
};


class FreeList
{
public:
	
	bool Empty()
	{
		return _list == nullptr;
	}

	void PushRange(void*start, void* end, size_t num)//将剩余的挂到自由链表上！！！
	{
		NEXT_OBJ(end) = _list;
		_list = start;
		_size += num;
	}
	void* Clear()
	{
		_size = 0;
		void* list = _list;
		_list = nullptr;
		return list;
	}

	void* Pop()
	{
		void* obj = _list;
		_list = NEXT_OBJ(obj);
		--_size;
		return obj;
	}
	void Push(void* obj)
	{
		NEXT_OBJ(obj) = _list;
		_list = obj;
		++_size;
	}
	size_t Size()
	{
		return _size;
	}

	void SetMaxSize(size_t maxsize)
	{
		_maxsize = maxsize;
	}

	size_t MaxSize()
	{
		return _maxsize;
	}

private:
	void* _list=nullptr;
	size_t _size=0;
	size_t _maxsize=1;
};




//管理对齐映射
class ClasssSize
{
	// 控制在12%左右的内碎片浪费
	// [1,128]				8byte对齐 freelist[0,16)
	// [129,1024]			16byte对齐 freelist[16,72)
	// [1025,8*1024]		128byte对齐 freelist[72,128)
	// [8*1024+1,64*1024]	512byte对齐 freelist[128,240)
	
#if 0
	(1024-129)/16=56, 56+16=72
	7*1024/128=56,    56+72=128
#endif

public:
	static inline size_t _Roundup(size_t size,size_t align)   //align 对齐数
	{
		//跟8对齐 (size+7)&~7        跟16对齐 (size+15)&~15

		return (size + align - 1)&(~(align - 1));
	}
	//向上取整
	static inline size_t Roundup(size_t size)
	{
		assert(size < MAXBYTES);

		if (size <= 128){
			return _Roundup(size, 8);
		}
		else if (size <= 1024){
			return _Roundup(size, 16);
		}
		else if (size <= 8192){
			return _Roundup(size, 128);
		}
		else if (size <= 65536){
			return _Roundup(size, 512);
		}
		return -1;
	}

	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		//位操作  8就是1左移三位！！！
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;//类似  (size+7)/8-1
	}
	static inline size_t Index(size_t bytes)
	{
		assert(bytes < MAXBYTES);

		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 112 };

		//算出在那个区间！！！！，分成了4个区间
		if (bytes <= 128){
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024){
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8192){
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 65536){
			return _Index(bytes - 8192, 9) + group_array[2] + group_array[1] +
				group_array[0];
		}
		return -1;
	}

	static size_t NumMoveSize(size_t size)
	{
		if (size == 0)
			return 0;
		int num = (int)(MAXBYTES / size);           //一次移动多少个，最大的/单个对象的大小
		if (num < 2)
			num = 2;								//2~512 之间
		if (num > 512)
			num = 512;
		return num;
	}


	// 计算一次向系统获取几个页
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num*size;
		npage >>= 12;         //除以1页，4096 
		if (npage == 0)
			npage = 1;
		return npage;
	}

};