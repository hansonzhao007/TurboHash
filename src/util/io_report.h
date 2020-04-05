// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef __IO_REPORT_H__
#define __IO_REPORT_H__

#include "env.h"
#include <inttypes.h>
#include <atomic>

namespace util {


class Stats {
public:
    Stats();
    Stats(int id);
    void Start();
    void Merge(const Stats& other);
    void Stop();
    void AddMessage(Slice msg);
    void AppendWithSpace(std::string* str, Slice msg);
    void PrintSpeed(int64_t done, uint64_t now);
    void FinishedSingleOp();
    void AddBytes(int64_t n);
    void Report(const Slice& name);

private:
    int                 id_;
    double              start_;
    double              finish_;
    double              seconds_;
    uint64_t            last_report_done_;
    uint64_t            last_report_finish_;
    uint64_t            next_report_;
    uint64_t            next_report_time_;

    std::atomic<int>        report_flag_;
    std::atomic<uint64_t>   done_;
    std::atomic<int64_t>    bytes_;
    uint64_t                last_op_finish_;
    std::string             message_;
};


}

#endif