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


//���������������ĳ���
const size_t NLISTS=240;   //�ĸ����䳤�����
const size_t MAXBYTES = 64 * 1024;//64k
const size_t PAGE_SHIFT = 12;
const size_t NPAGES = 129;


inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	//��Ҫ��ϵͳ�����ڴ�
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
	//��Ҫ��ϵͳ�����ڴ�
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


//��ҳΪ��λ
typedef size_t PageID;
//Span��ȣ���С�������кϲ�                           32λϵͳ 4G�����ڴ棬��һҳԼ4k
struct Span   
{
	PageID _pageid=0;			//ҳ��
	size_t _npage=0;		//ҳ������
	
	Span* _next=nullptr;
	Span* _prev=nullptr;

	void* _objlist=nullptr;		//������������
	size_t _objsize = 0;		//�����С
	size_t _usecount=0;			//ʹ�ü���

};
struct SpanList
{
	//��Ƴɴ�ͷ˫��ѭ����������
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

	void PushRange(void*start, void* end, size_t num)//��ʣ��Ĺҵ����������ϣ�����
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




//�������ӳ��
class ClasssSize
{
	// ������12%���ҵ�����Ƭ�˷�
	// [1,128]				8byte���� freelist[0,16)
	// [129,1024]			16byte���� freelist[16,72)
	// [1025,8*1024]		128byte���� freelist[72,128)
	// [8*1024+1,64*1024]	512byte���� freelist[128,240)
	
#if 0
	(1024-129)/16=56, 56+16=72
	7*1024/128=56,    56+72=128
#endif

public:
	static inline size_t _Roundup(size_t size,size_t align)   //align ������
	{
		//��8���� (size+7)&~7        ��16���� (size+15)&~15

		return (size + align - 1)&(~(align - 1));
	}
	//����ȡ��
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
		//λ����  8����1������λ������
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;//����  (size+7)/8-1
	}
	static inline size_t Index(size_t bytes)
	{
		assert(bytes < MAXBYTES);

		// ÿ�������ж��ٸ���
		static int group_array[4] = { 16, 56, 56, 112 };

		//������Ǹ����䣡���������ֳ���4������
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
		int num = (int)(MAXBYTES / size);           //һ���ƶ����ٸ�������/��������Ĵ�С
		if (num < 2)
			num = 2;								//2~512 ֮��
		if (num > 512)
			num = 512;
		return num;
	}


	// ����һ����ϵͳ��ȡ����ҳ
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num*size;
		npage >>= 12;         //����1ҳ��4096 
		if (npage == 0)
			npage = 1;
		return npage;
	}

};