#ifndef __OBJECT_POOL_H__
#define __OBJECT_POOL_H__

#include <stdlib.h>
#include <iostream>
#include "util/concurrentqueue.h"

namespace util {

template<typename T>
class ObjectPool
{

public:
	explicit ObjectPool(size_t capacity = 1000):
		capacity_(capacity) {
		objects_ = new T[capacity_];
		for (int32_t i = 0; i < capacity_; ++i) {
			pool_.enqueue(std::move(objects_+i));
		}
	}
	~ObjectPool()
	{
		delete[] objects_;
	}

	inline T*  Allocate() {
		T* object = nullptr;
		pool_.try_dequeue(object);
		return object;
	}
	inline void Release(T* object) {
		pool_.enqueue(object);
	}

	inline int32_t Capacity() {return capacity_; }
private:
	moodycamel::ConcurrentQueue<T*> pool_;
	T* objects_;
	const int32_t capacity_;
};

}

#endif
