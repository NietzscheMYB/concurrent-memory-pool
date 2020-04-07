# concurrent-memory-pool
并发内存池(concurrent memory pool)由thread cache，central cache，page cache

## thread cache
线程缓存是每个线程独有的，用于小于64k的内存分配，线程从这里申请内存不需要加锁，每个线程独享一个cache，这也就是这个并发内存池高效的地方。

## central cache
中心缓存是所有线程所共享，thread cache是按需从central cache中获取对象。central cache周期性的回收thread cache中的对象，避免一个线程占用了太多内存，而其他线程的内存吃紧。达到内存分配在多个线程中更均衡的按需调度的目的。

## page cache
页缓存是在central cache缓存上的一层缓存，存储的内存是按页为单位存储及分配的，central cache没有内存对象时，从page cache会回收central cahce满足条件的span对象，并且合并相邻的页，组成更大的页，缓解内存碎片的问题。
