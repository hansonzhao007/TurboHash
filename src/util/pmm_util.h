// MIT License

// Copyright (c) [2020] [Xingsheng Zhao]

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <immintrin.h>
#include <xmmintrin.h>

#include <cassert>
#include <regex>
#include <sstream>
#include <thread>

#include "cpucounters.h"
#include "time.h"
#define force_inline __attribute__ ((always_inline)) inline

#define _mm_clwb(addr) asm volatile("clwb %0" : "+m"(*(volatile char*)(addr)));

namespace util {

enum {
    DimmID,
    MediaReads,
    MediaWrites,
    ReadRequests,
    WriteRequests,
    TotalMediaReads,
    TotalMediaWrites,
    TotalReadRequests,
    TotalWriteRequests
};

class DimmAttribute128b {
public:
    uint64_t l_u64b, h_u64b;
};

class DimmDataContainer {
public:
    std::string dimm_id_;
    DimmAttribute128b stat_[8];
};

class PMMData {
    const std::string filename;

public:
    explicit PMMData (const std::string& file) : filename (file) {}

    DimmDataContainer PMM_[6];
    std::vector<DimmDataContainer> pmm_dimms_;
    void get_pmm_data () {
        util::Execute ("ipmctl show -performance > " + filename);
        std::ifstream ipmctl_stat;
        ipmctl_stat.open (filename);  // open the input file

        std::vector<std::string> reg_init_set = {R"(DimmID=0x([0-9a-f]*))",
                                                 R"(^MediaReads=0x([0-9a-f]*))",
                                                 R"(^MediaWrites=0x([0-9a-f]*))",
                                                 R"(^ReadRequests=0x([0-9a-f]*))",
                                                 R"(^WriteRequests=0x([0-9a-f]*))",
                                                 R"(^TotalMediaReads=0x([0-9a-f]*))",
                                                 R"(^TotalMediaWrites=0x([0-9a-f]*))",
                                                 R"(^TotalReadRequests=0x([0-9a-f]*))",
                                                 R"(^TotalWriteRequests=0x([0-9a-f]*))"};
        std::vector<std::regex> reg_set;
        std::regex stat_bit_convert_reg (R"(^([0-9a-f]{16})([0-9a-f]{16}))");
        for (auto i : reg_init_set) {
            reg_set.push_back (std::regex (i));
        }

        std::string str_line;
        std::smatch matched_data;
        std::smatch matched_num;
        int index = 0;
        while (ipmctl_stat >> str_line) {
            for (size_t i = 0; i < reg_set.size (); i++) {
                if (std::regex_search (str_line, matched_data, reg_set.at (i))) {
                    index = i;
                    break;
                }
            }
            if (index == DimmID) {
                pmm_dimms_.push_back (DimmDataContainer ());
                pmm_dimms_.back ().dimm_id_ = matched_data[1];
            } else {
                uint64_t h64, l64;
                std::string str128b = matched_data[1];
                if (std::regex_search (str128b, matched_num, stat_bit_convert_reg)) {
                    pmm_dimms_.back ().stat_[index - 1].h_u64b =
                        std::stoull (matched_num[1], nullptr, 16);
                    pmm_dimms_.back ().stat_[index - 1].l_u64b =
                        std::stoull (matched_num[2], nullptr, 16);
                } else {
                    perror ("parse dimm stat");
                    exit (EXIT_FAILURE);
                }
            }
        }
    }
};

class IPMWatcher {
public:
    std::string file_name_;

    PMMData *start, *end;
    float *outer_imc_read_addr = nullptr, *outer_imc_write_addr = nullptr,
          *outer_media_read_addr = nullptr, *outer_media_write_addr = nullptr;
    std::chrono::_V2::system_clock::time_point start_timer, end_timer;

    IPMWatcher (const std::string& name) : file_name_ ("ipm_" + name + ".txt") {
        printf ("\033[32mStart IPMWatcher for %s\n\033[0m", name.c_str ());
        start = new PMMData (file_name_);
        end = new PMMData (file_name_);
        start_timer = std::chrono::system_clock::now ();
        start->get_pmm_data ();
    }

    ~IPMWatcher () { Report (); }

    void Report () {
        end->get_pmm_data ();
        end_timer = std::chrono::system_clock::now ();
        int dimm_num = end->pmm_dimms_.size ();
        float media_read_size_MB = 0, imc_read_size_MB = 0, imc_write_size_MB = 0,
              media_write_size_MB = 0;

        setlocale (LC_NUMERIC, "");
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds> (end_timer - start_timer);

        std::vector<float> TotalMediaReads_ (dimm_num), TotalMediaWrites_ (dimm_num),
            TotalReadRequests_ (dimm_num), TotalWriteRequests_ (dimm_num);
        for (int i = 0; i < dimm_num; i++) {
            for (int j = 0; j < 8; j++)
                assert (start->pmm_dimms_.at (i).stat_[j].h_u64b ==
                        end->pmm_dimms_.at (i).stat_[j].h_u64b);
            TotalMediaReads_.at (i) = (end->pmm_dimms_.at (i).stat_[TotalMediaReads - 1].l_u64b -
                                       start->pmm_dimms_.at (i).stat_[TotalMediaReads - 1].l_u64b) /
                                      16384.0;
            TotalMediaWrites_.at (i) =
                (end->pmm_dimms_.at (i).stat_[TotalMediaWrites - 1].l_u64b -
                 start->pmm_dimms_.at (i).stat_[TotalMediaWrites - 1].l_u64b) /
                16384.0;
            TotalReadRequests_.at (i) =
                (end->pmm_dimms_.at (i).stat_[TotalReadRequests - 1].l_u64b -
                 start->pmm_dimms_.at (i).stat_[TotalReadRequests - 1].l_u64b) /
                16384.0;
            TotalWriteRequests_.at (i) =
                (end->pmm_dimms_.at (i).stat_[TotalWriteRequests - 1].l_u64b -
                 start->pmm_dimms_.at (i).stat_[TotalWriteRequests - 1].l_u64b) /
                16384.0;

            TotalMediaReads_.at (i) = TotalMediaReads_.at (i) - TotalMediaWrites_.at (i);

            imc_read_size_MB += TotalReadRequests_.at (i);
            media_read_size_MB += TotalMediaReads_.at (i);
            imc_write_size_MB += TotalWriteRequests_.at (i);
            media_write_size_MB += TotalMediaWrites_.at (i);
        }
        if (outer_imc_read_addr) *outer_imc_read_addr = imc_read_size_MB;
        if (outer_imc_write_addr) *outer_imc_write_addr = imc_write_size_MB;
        if (outer_media_read_addr) *outer_media_read_addr = media_read_size_MB;
        if (outer_media_write_addr) *outer_media_write_addr = media_write_size_MB;
        for (int i = 0; i < dimm_num; i++) {
            printf (
                "DIMM%d | Read from IMC | Write from IMC |  Read DIMM  |  Write "
                "DIMM  |   "
                "RA   |   WA   |\n",
                i);
            printf ("  MB  | %13.2f | %14.2f | %11.2f | %12.2f | %6.2f | %6.2f |\n",
                    TotalReadRequests_.at (i), TotalWriteRequests_.at (i), TotalMediaReads_.at (i),
                    TotalMediaWrites_.at (i), (TotalMediaReads_.at (i) / TotalReadRequests_.at (i)),
                    (TotalMediaWrites_.at (i) / TotalWriteRequests_.at (i)));
        }

        double seconds = duration.count () / 1000.0;
        printf (
            "*SUM* | DIMM-R: %7.1f MB/s. User-R: %7.1f MB/s   | DIMM-W: %7.1f "
            "MB/s, "
            "User-W: %7.1f MB/s. Time: %6.2fs\n",
            media_read_size_MB / seconds, imc_read_size_MB / seconds, media_write_size_MB / seconds,
            imc_write_size_MB / seconds, seconds);

        delete start;
        delete end;

        printf ("\033[32mDestroy IPMWatcher.\n\033[0m\n");
    }
};

class PCMMetric {
public:
    PCMMetric (const std::string name) : name_ (name) {
        before_sstate_ = getSystemCounterState ();
        start_time_ = util::NowMicros ();
    }
    ~PCMMetric () {
        double duration = (util::NowMicros () - start_time_) / 1000000.0;
        after_sstate_ = getSystemCounterState ();
        std::cout
            << "PCM Metrics: " << name_
            << "\n"
            // << "\tL3 misses: " << getL3CacheMisses(*before_sstate,
            // *after_sstate) << "\n"
            << "\tDRAM Read  (bytes): " << getBytesReadFromMC (before_sstate_, after_sstate_)
            << "\n"
            << "\tDRAM Write (bytes): " << getBytesWrittenToMC (before_sstate_, after_sstate_)
            << "\n"
            << "\tPMEM Read  (bytes): " << getBytesReadFromPMM (before_sstate_, after_sstate_)
            << "\n"
            << "\tPMEM Write (bytes): " << getBytesWrittenToPMM (before_sstate_, after_sstate_)
            << "\n"
            << "\tDRAM Read    Speed: "
            << getBytesReadFromMC (before_sstate_, after_sstate_) / 1024.0 / 1024.0 / duration
            << " MB/s"
            << "\n"
            << "\tDRAM Write   Speed: "
            << getBytesWrittenToMC (before_sstate_, after_sstate_) / 1024.0 / 1024.0 / duration
            << " MB/s"
            << "\n"
            << "\tPMEM Read    Speed: "
            << getBytesReadFromPMM (before_sstate_, after_sstate_) / 1024.0 / 1024.0 / duration
            << " MB/s"
            << "\n"
            << "\tPMEM Write   Speed: "
            << getBytesWrittenToPMM (before_sstate_, after_sstate_) / 1024.0 / 1024.0 / duration
            << " MB/s"
            << "\n"
            << "\tDuration: " << duration << " s" << std::endl;
    }

private:
    SystemCounterState before_sstate_;
    SystemCounterState after_sstate_;
    const std::string name_;
    uint64_t start_time_;
};

}  // namespace util