#include"ConcurrenrAlloc.h"
#include<vector>
using namespace std;

void TestThreadCache()
{
	//thread t1(ConcurrentAlloc, 10);
	//thread t2(ConcurrentAlloc, 10);
	//t1.join();
	//t2.join();

	//test1  观察流程！！
	//void* p1 = ConcurrentAlloc(10);
	//void* p2 = ConcurrentAlloc(10);


	//test2  观察前10个与第11个
	//for (size_t i = 0; i < 11; ++i)
	//{
	//	cout << ConcurrentAlloc(10) << endl;
	//}

	//test3 反复用10个！！
	vector<void*> v;
	for (size_t i = 0; i < 2; ++i)
	{
		v.push_back(ConcurrentAlloc(10));
		cout <<v.back() << endl;
	}
	cout << endl << endl;
	for (size_t i = 0; i < 2; i++)
	{
		ConcurrentFree(v[i]);
	}
	v.clear();

	for (size_t i = 0; i < 10; ++i)
	{
		v.push_back(ConcurrentAlloc(2));
		cout << v.back() << endl;
	}
	for (size_t i = 0; i < 2; i++)
	{
		ConcurrentFree(v[i]);
	}
	v.clear();
}

void TestPageCache()
{
	void* ptr = VirtualAlloc(NULL, (NPAGES - 1) << PAGE_SHIFT, MEM_RESERVE, PAGE_READWRITE);
	
	//malloc 不行 
	//void* ptr=malloc((NPAGES-1)<<PAGE_SHIFT);
	
	cout << ptr << endl;
	if (ptr == nullptr)
	{
		throw std::bad_alloc();
	}
	
	PageID _pageid = (PageID)ptr >> PAGE_SHIFT;
	cout << _pageid << endl;

	void* shiftptr = (void*)(_pageid << PAGE_SHIFT);
	cout << shiftptr << endl;
}

void TestConcurrentAlloc()
{
	//void* p1=ConcurrentAlloc(10);


	for (size_t i = 0; i < 10; ++i)
	{
		cout << ConcurrentAlloc(10) << endl;
	}

}
void TestLargeAlloc()
{
	void* ptr1 = ConcurrentAlloc(MAXBYTES * 2);
	void* ptr2 = ConcurrentAlloc(129 << PAGE_SHIFT);

	ConcurrentFree(ptr1);
	ConcurrentFree(ptr2);
}


//int main()
//{
//	TestThreadCache();
//	//TestPageCache();
//	//TestConcurrentAlloc();
//	return 0;
//}



#if 1
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t malloc_costtime = 0;
	size_t free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc(16));
				}
				size_t end1 = clock(); size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += end1 - begin1;
				free_costtime += end2 - begin2;
			}
		});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime);
	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime);
	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n",
		nworks, nworks*rounds*ntimes, malloc_costtime + free_costtime);
}
// 单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t malloc_costtime = 0;
	size_t free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAlloc(16));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear(); malloc_costtime += end1 - begin1;
				free_costtime += end2 - begin2;
			}
		});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%u个线程并发执行%u轮次，每轮次concurrent alloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime);
	printf("%u个线程并发执行%u轮次，每轮次concurrent dealloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime);
	printf("%u个线程并发concurrent alloc&dealloc %u次，总计花费：%u ms\n",
		nworks, nworks*rounds*ntimes, malloc_costtime + free_costtime);
}
int main()
{
	cout << "==========================================================" << endl;
	BenchmarkMalloc(50000,2, 5);
	cout << endl << endl;
	BenchmarkConcurrentMalloc(50000,2, 5);
	cout << "==========================================================" << endl;
	return 0;
}
#endif