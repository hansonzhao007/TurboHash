#pragma once

#include <functional>
#include <unistd.h>

class WaitableCallback {
public:
    WaitableCallback():
        fired_(false),
        func_(std::move([](){})) {
    }
    WaitableCallback(std::function<void()> func) :
        fired_(false),
        func_(std::move(func)) {

    }
    
    // copy constructor
    WaitableCallback(const WaitableCallback& cb) {
        fired_ = false;
        func_  = cb.func_;
    }

    void Wait() {
        while (!fired_) {
            usleep(10);
        }
    }

    void operator()() {
        func_();
        fired_ = true;
    }

    void Reset() {
        fired_ = false;
    }
private:
    std::function<void()> func_;
    volatile bool fired_;
};
