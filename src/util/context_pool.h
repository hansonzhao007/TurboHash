#pragma once
#include "util/blockingconcurrentqueue.h"

namespace util {

template <class T>
class ContextPool {
public:
    ContextPool(int pool_size) {
        for (int i = 0; i < pool_size; ++i) {
            T* context = new T();
            context_pool_.enqueue(context);
        }
    }
    ~ContextPool(){}

    inline T* Allocate() {
        T* context = nullptr;
        context_pool_.wait_dequeue(context);
        return context;
    }

    inline T* AllocateNoWait() {
        // context may be nullptr
        T* context = nullptr;
        context_pool_.try_dequeue(context);
        return context;
    }

    inline bool Release(T* context) {
        return context_pool_.enqueue(context);
    }

    size_t Size() { return context_pool_.size_approx(); }
private:
    moodycamel::BlockingConcurrentQueue<T*> context_pool_;
};

}