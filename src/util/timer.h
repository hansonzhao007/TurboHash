#pragma once

#include "env.h"

namespace util {

class TimerMS {
public:
    TimerMS(const std::string& name):
        name_(name) {
        start_ = Env::Default()->NowMicros();
    }
    ~TimerMS() {
        printf("%s: duration %lu ms\n", name_.c_str(), Env::Default()->NowMicros() - start_);
    }
private:
    std::string name_;
    uint64_t start_;
};
}