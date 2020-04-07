#include"ThreadCache.h"
#include"CentralCache.h"

void* ThreadCache::FetchFromCentralCache(size_t index,size_t byte)
{
	FreeList* freelist = &_freelist[index];
	
	//
	//批量获取，返回一个，将剩余的挂到自由链表上！！！
	//size_t num = 10;
	//

	size_t num_to_move =min(ClasssSize::NumMoveSize(byte),freelist->MaxSize());//NumMoveSize决定上限！！，最大513
	
	void*start, *end;
	size_t fetchnum = CentralCache::GetInstance()->FetchRangeObj(start, end, num_to_move, byte);
	if (fetchnum > 1)
	{
		freelist->PushRange(NEXT_OBJ(start), end, fetchnum - 1);
	}
	////假设获取10个，返回1 个，将后9个挂到自由链表上
	//freelist->PushRange(NEXT_OBJ(start),end,fetchnum-1);
	
	if (num_to_move == freelist->MaxSize())
	{
		freelist->SetMaxSize(num_to_move + 1);
	}
	
	return start;
}


void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAXBYTES);
	//重新对齐取整
	size = ClasssSize::Roundup(size);
	size_t index = ClasssSize::Index(size);//计算下标
	//对应的自由链表
	FreeList* freelist = &_freelist[index];

	if (!freelist->Empty())
	{
		return freelist->Pop();
	}
	else{
		//中心cache去取
		return FetchFromCentralCache(index,size);
	}

}

void ThreadCache::Deallocate(void* ptr, size_t byte)//解除分配
{
	assert(byte <= MAXBYTES);
	byte = ClasssSize::Roundup(byte);
	size_t index = ClasssSize::Index(byte);
	//对应的自由链表
	FreeList* freelist = &_freelist[index];
	freelist->Push(ptr);

	//ListToolong();

	//当自由链表对象数量超过一次批量从中心缓存移动的数量时
	//开始回收对象到中心缓存
	if (freelist->Size() >= freelist->MaxSize())
	{
		ListTooLong(freelist,byte);
	}

	//thread cache总的字节数超过2M，则开始释放


}

void ThreadCache::ListTooLong(FreeList* freelist,size_t size)
{
	void* start = freelist->Clear();
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}