//����ϵͳ
#pragma once
#include"Common.h"


//�̻߳��壬Ч�ʸߣ���Ϊû����
class ThreadCache
{
public:
	void* Allocate(size_t size);//�����ڴ�
	void Deallocate(void* ptr,size_t size);//�������
	// �����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);
	//�����ж���̫�࣬��ʼ���ա�
	void ListTooLong(FreeList* freelist,size_t byte);
private:
	//����������С�ɿأ�
	FreeList _freelist[NLISTS];
	//int _tid;
	//ThreadCache* _next;
};

//��̬tsl�����������̶߳���һ��������
static _declspec(thread) ThreadCache* tls_threadcache=nullptr;

