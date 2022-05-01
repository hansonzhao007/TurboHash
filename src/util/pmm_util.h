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

        printf (
            "--------------------------------------------------------------------------------------"
            "\n");
        printf (
            "DIMM  | Read from IMC | Write from IMC |   Read DIMM |   Write DIMM |     RA |     WA "
            "|\n");
        for (int i = 0; i < dimm_num; i++) {
            printf ("  %-2d  | %13.2f | %14.2f | %11.2f | %12.2f | %6.2f | %6.2f |\n", i,
                    TotalReadRequests_.at (i), TotalWriteRequests_.at (i), TotalMediaReads_.at (i),
                    TotalMediaWrites_.at (i), (TotalMediaReads_.at (i) / TotalReadRequests_.at (i)),
                    (TotalMediaWrites_.at (i) / TotalWriteRequests_.at (i)));
        }

        double seconds = duration.count () / 1000.0;
        printf (
            "--------------------------------------------------------------------------------------"
            "\n");
        printf (
            " SUM:\n"
            " DIMM-R: %7.1f MB/s, %7.1f MB\n"
            " User-R: %7.1f MB/s, %7.1f MB\n"
            " DIMM-W: %7.1f MB/s, %7.1f MB\n"
            " User-W: %7.1f MB/s, %7.1f MB\n"
            "   Time: %7.1f s\n",
            media_read_size_MB / seconds, media_read_size_MB, imc_read_size_MB / seconds,
            imc_read_size_MB, media_write_size_MB / seconds, media_write_size_MB,
            imc_write_size_MB / seconds, imc_write_size_MB, seconds);

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

uint64_t PMMMask (size_t length) {
    if (64 == length) {
        return 0xFFFFFFFFFFFFFFC0;
    } else if (128 == length) {
        return 0xFFFFFFFFFFFFFF80;
    } else if (256 == length) {
        return 0xFFFFFFFFFFFFFF00;
    } else if (512 == length) {
        return 0xFFFFFFFFFFFFFE00;
    } else if (length % 1024 == 0 && length > 0) {
        double tmp = std::log2 (length / 1024);
        int left_shift = tmp;
        return (0xFFFFFFFFFFFFFC00 << left_shift);
    } else
        return 0xFFFFFFFFFFFFFFFF;
}

force_inline void MFence () { __asm__ __volatile__("mfence" ::: "memory"); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
void __attribute__ ((optimize ("O0"))) ClearCache () {
    static const int size = 20 * 1024 * 1024;  // Allocate 20M. Set much larger then L3
    static char* c = (char*)malloc (size);
    for (int i = 0; i < 1; i++) memset (c, 0, size);
    _mm_mfence ();
}

inline void  // __attribute__((optimize("O0"),always_inline))
Load64_NT (char* addr, int N = 0) {
    volatile __m512i zmm0 = _mm512_stream_load_si512 ((__m512i*)addr + 0);
    // __m512i zmm00 = _mm512_stream_load_si512((__m512i *)addr + 0); // 64
    // _mm_lfence();
}

inline void  // __attribute__((optimize("O0"),always_inline))
Load128_NT (char* addr, int N = 0) {
    volatile __m512i zmm0 = _mm512_stream_load_si512 ((__m512i*)addr + 0);
    volatile __m512i zmm1 = _mm512_stream_load_si512 ((__m512i*)addr + 1);

    // __m512i zmm0 = _mm512_stream_load_si512((__m512i *)addr + 0);
    // __m512i zmm1 = _mm512_stream_load_si512((__m512i *)addr + 1);
    // _mm_lfence();
}

inline void  // __attribute__((optimize("O0"),always_inline))
Load256_NT (char* addr, int N = 0) {
    volatile __m512i zmm00 = _mm512_stream_load_si512 ((__m512i*)addr + 0);
    volatile __m512i zmm01 = _mm512_stream_load_si512 ((__m512i*)addr + 1);
    volatile __m512i zmm02 = _mm512_stream_load_si512 ((__m512i*)addr + 2);
    volatile __m512i zmm03 = _mm512_stream_load_si512 ((__m512i*)addr + 3);

    // __m512i zmm00 = _mm512_stream_load_si512((__m512i *)addr + 0); // 64
    // __m512i zmm01 = _mm512_stream_load_si512((__m512i *)addr + 1); // 128
    // __m512i zmm02 = _mm512_stream_load_si512((__m512i *)addr + 2); // 192
    // __m512i zmm03 = _mm512_stream_load_si512((__m512i *)addr + 3); // 256
    // _mm_lfence();
}

inline void  // __attribute__((optimize("O0"),always_inline))
Load512_NT (char* addr, int N = 0) {
    volatile __m512i zmm00 = _mm512_stream_load_si512 ((__m512i*)addr + 0);
    volatile __m512i zmm01 = _mm512_stream_load_si512 ((__m512i*)addr + 1);
    volatile __m512i zmm02 = _mm512_stream_load_si512 ((__m512i*)addr + 2);
    volatile __m512i zmm03 = _mm512_stream_load_si512 ((__m512i*)addr + 3);
    volatile __m512i zmm04 = _mm512_stream_load_si512 ((__m512i*)addr + 4);
    volatile __m512i zmm05 = _mm512_stream_load_si512 ((__m512i*)addr + 5);
    volatile __m512i zmm06 = _mm512_stream_load_si512 ((__m512i*)addr + 6);
    volatile __m512i zmm07 = _mm512_stream_load_si512 ((__m512i*)addr + 7);
    // __m512i zmm00 = _mm512_stream_load_si512((__m512i *)addr + 0); // 64
    // __m512i zmm01 = _mm512_stream_load_si512((__m512i *)addr + 1); // 128
    // __m512i zmm02 = _mm512_stream_load_si512((__m512i *)addr + 2); // 192
    // __m512i zmm03 = _mm512_stream_load_si512((__m512i *)addr + 3); // 256
    // __m512i zmm04 = _mm512_stream_load_si512((__m512i *)addr + 4); //
    // __m512i zmm05 = _mm512_stream_load_si512((__m512i *)addr + 5); //
    // __m512i zmm06 = _mm512_stream_load_si512((__m512i *)addr + 6); //
    // __m512i zmm07 = _mm512_stream_load_si512((__m512i *)addr + 7); // 512
    // _mm_lfence();
}

inline void  // __attribute__((optimize("O0"),always_inline))
Load1024_NT (char* addr, int N = 0) {
    volatile __m512i zmm00 = _mm512_stream_load_si512 ((__m512i*)addr + 0);
    volatile __m512i zmm01 = _mm512_stream_load_si512 ((__m512i*)addr + 1);
    volatile __m512i zmm02 = _mm512_stream_load_si512 ((__m512i*)addr + 2);
    volatile __m512i zmm03 = _mm512_stream_load_si512 ((__m512i*)addr + 3);
    volatile __m512i zmm04 = _mm512_stream_load_si512 ((__m512i*)addr + 4);
    volatile __m512i zmm05 = _mm512_stream_load_si512 ((__m512i*)addr + 5);
    volatile __m512i zmm06 = _mm512_stream_load_si512 ((__m512i*)addr + 6);
    volatile __m512i zmm07 = _mm512_stream_load_si512 ((__m512i*)addr + 7);
    volatile __m512i zmm08 = _mm512_stream_load_si512 ((__m512i*)addr + 8);
    volatile __m512i zmm09 = _mm512_stream_load_si512 ((__m512i*)addr + 9);
    volatile __m512i zmm10 = _mm512_stream_load_si512 ((__m512i*)addr + 10);
    volatile __m512i zmm11 = _mm512_stream_load_si512 ((__m512i*)addr + 11);
    volatile __m512i zmm12 = _mm512_stream_load_si512 ((__m512i*)addr + 12);
    volatile __m512i zmm13 = _mm512_stream_load_si512 ((__m512i*)addr + 13);
    volatile __m512i zmm14 = _mm512_stream_load_si512 ((__m512i*)addr + 14);
    volatile __m512i zmm15 = _mm512_stream_load_si512 ((__m512i*)addr + 15);
    // __m512i zmm00 = _mm512_stream_load_si512((__m512i *)addr + 0); // 64
    // __m512i zmm01 = _mm512_stream_load_si512((__m512i *)addr + 1); // 128
    // __m512i zmm02 = _mm512_stream_load_si512((__m512i *)addr + 2); // 192
    // __m512i zmm03 = _mm512_stream_load_si512((__m512i *)addr + 3); // 256
    // __m512i zmm04 = _mm512_stream_load_si512((__m512i *)addr + 4); //
    // __m512i zmm05 = _mm512_stream_load_si512((__m512i *)addr + 5); //
    // __m512i zmm06 = _mm512_stream_load_si512((__m512i *)addr + 6); //
    // __m512i zmm07 = _mm512_stream_load_si512((__m512i *)addr + 7); // 512
    // __m512i zmm08 = _mm512_stream_load_si512((__m512i *)addr + 8);
    // __m512i zmm09 = _mm512_stream_load_si512((__m512i *)addr + 9);
    // __m512i zmm10 = _mm512_stream_load_si512((__m512i *)addr + 10);
    // __m512i zmm11 = _mm512_stream_load_si512((__m512i *)addr + 11);
    // __m512i zmm12 = _mm512_stream_load_si512((__m512i *)addr + 12);
    // __m512i zmm13 = _mm512_stream_load_si512((__m512i *)addr + 13);
    // __m512i zmm14 = _mm512_stream_load_si512((__m512i *)addr + 14);
    // __m512i zmm15 = _mm512_stream_load_si512((__m512i *)addr + 15); // 1024
    // _mm_lfence();
}

inline void LoadNKB_NT (char* addr, int N) {
    char* tmp = addr;
    // Load N KB from
    for (int i = 0; i < N; ++i) {
        Load1024_NT (tmp);
        tmp += 1024;
    }
}

static __m512i zmm = _mm512_set1_epi8 ((char)(31));

force_inline void Store64_NT (char* dest, int N = 0) {
    _mm512_stream_si512 ((__m512i*)dest + 0, zmm);
    _mm_sfence ();
}

force_inline void Store128_NT (char* dest, int N = 0) {
    _mm512_stream_si512 ((__m512i*)dest + 0, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 1, zmm);
    _mm_sfence ();
}

force_inline void Store256_NT (char* dest, int N = 0) {
    _mm512_stream_si512 ((__m512i*)dest + 0, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 1, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 2, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 3, zmm);
    _mm_sfence ();
}

force_inline void Store512_NT (char* dest, int N = 0) {
    _mm512_stream_si512 ((__m512i*)dest + 0, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 1, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 2, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 3, zmm);
#ifdef FENCE_ALL
    _mm_sfence ();
#endif
    _mm512_stream_si512 ((__m512i*)dest + 4, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 5, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 6, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 7, zmm);
    _mm_sfence ();
}

force_inline void Store1024_NT (char* dest, int N = 0) {
    _mm512_stream_si512 ((__m512i*)dest + 0, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 1, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 2, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 3, zmm);
#ifdef FENCE_ALL
    _mm_sfence ();
#endif
    _mm512_stream_si512 ((__m512i*)dest + 4, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 5, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 6, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 7, zmm);
#ifdef FENCE_ALL
    _mm_sfence ();
#endif
    _mm512_stream_si512 ((__m512i*)dest + 8, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 9, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 10, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 11, zmm);
#ifdef FENCE_ALL
    _mm_sfence ();
#endif
    _mm512_stream_si512 ((__m512i*)dest + 12, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 13, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 14, zmm);
    _mm512_stream_si512 ((__m512i*)dest + 15, zmm);
    _mm_sfence ();
}

force_inline void StoreNKB_NT (char* dest, int N) {
    // store N KB to dest
    // char* old = dest;
    for (int i = 0; i < N; ++i) {
        Store1024_NT (dest);
        dest += 1024;
    }
}

force_inline void Store64_WB (char* dest, int N = 0) {
    _mm512_store_epi64 ((__m512i*)dest + 0, zmm);
    _mm_clwb ((__m512i*)dest + 0);
    _mm_sfence ();
}

force_inline void Store128_WB (char* dest, int N = 0) {
    _mm512_store_epi64 ((__m512i*)dest + 0, zmm);
    _mm_clwb ((__m512i*)dest + 0);
    _mm512_store_epi64 ((__m512i*)dest + 1, zmm);
    _mm_clwb ((__m512i*)dest + 1);
    _mm_sfence ();
}

force_inline void Store256_WB (char* dest, int N = 0) {
    _mm512_store_epi64 ((__m512i*)dest + 0, zmm);
    _mm_clwb ((__m512i*)dest + 0);
    _mm512_store_epi64 ((__m512i*)dest + 1, zmm);
    _mm_clwb ((__m512i*)dest + 1);
    _mm512_store_epi64 ((__m512i*)dest + 2, zmm);
    _mm_clwb ((__m512i*)dest + 2);
    _mm512_store_epi64 ((__m512i*)dest + 3, zmm);
    _mm_clwb ((__m512i*)dest + 3);
    _mm_sfence ();
}

force_inline void Store512_WB (char* dest, int N = 0) {
    _mm512_store_epi64 ((__m512i*)dest + 0, zmm);
    _mm_clwb ((__m512i*)dest + 0);
    _mm512_store_epi64 ((__m512i*)dest + 1, zmm);
    _mm_clwb ((__m512i*)dest + 1);
    _mm512_store_epi64 ((__m512i*)dest + 2, zmm);
    _mm_clwb ((__m512i*)dest + 2);
    _mm512_store_epi64 ((__m512i*)dest + 3, zmm);
    _mm_clwb ((__m512i*)dest + 3);
    _mm512_store_epi64 ((__m512i*)dest + 4, zmm);
    _mm_clwb ((__m512i*)dest + 4);
    _mm512_store_epi64 ((__m512i*)dest + 5, zmm);
    _mm_clwb ((__m512i*)dest + 5);
    _mm512_store_epi64 ((__m512i*)dest + 6, zmm);
    _mm_clwb ((__m512i*)dest + 6);
    _mm512_store_epi64 ((__m512i*)dest + 7, zmm);
    _mm_clwb ((__m512i*)dest + 7);
    _mm_sfence ();
}

force_inline void Store1024_WB (char* dest, int N = 0) {
    _mm512_store_epi64 ((__m512i*)dest + 0, zmm);
    _mm_clwb ((__m512i*)dest + 0);
    _mm512_store_epi64 ((__m512i*)dest + 1, zmm);
    _mm_clwb ((__m512i*)dest + 1);
    _mm512_store_epi64 ((__m512i*)dest + 2, zmm);
    _mm_clwb ((__m512i*)dest + 2);
    _mm512_store_epi64 ((__m512i*)dest + 3, zmm);
    _mm_clwb ((__m512i*)dest + 3);
    _mm512_store_epi64 ((__m512i*)dest + 4, zmm);
    _mm_clwb ((__m512i*)dest + 4);
    _mm512_store_epi64 ((__m512i*)dest + 5, zmm);
    _mm_clwb ((__m512i*)dest + 5);
    _mm512_store_epi64 ((__m512i*)dest + 6, zmm);
    _mm_clwb ((__m512i*)dest + 6);
    _mm512_store_epi64 ((__m512i*)dest + 7, zmm);
    _mm_clwb ((__m512i*)dest + 7);
    _mm512_store_epi64 ((__m512i*)dest + 8, zmm);
    _mm_clwb ((__m512i*)dest + 8);
    _mm512_store_epi64 ((__m512i*)dest + 9, zmm);
    _mm_clwb ((__m512i*)dest + 9);
    _mm512_store_epi64 ((__m512i*)dest + 10, zmm);
    _mm_clwb ((__m512i*)dest + 10);
    _mm512_store_epi64 ((__m512i*)dest + 11, zmm);
    _mm_clwb ((__m512i*)dest + 11);
    _mm512_store_epi64 ((__m512i*)dest + 12, zmm);
    _mm_clwb ((__m512i*)dest + 12);
    _mm512_store_epi64 ((__m512i*)dest + 13, zmm);
    _mm_clwb ((__m512i*)dest + 13);
    _mm512_store_epi64 ((__m512i*)dest + 14, zmm);
    _mm_clwb ((__m512i*)dest + 14);
    _mm512_store_epi64 ((__m512i*)dest + 15, zmm);
    _mm_clwb ((__m512i*)dest + 15);
    _mm_sfence ();
}

#pragma GCC diagnostic pop

force_inline void StoreNKB_WB (char* dest, int N) {
    // store N KB to dest
    // char* old = dest;
    for (int i = 0; i < N; ++i) {
        Store1024_WB (dest);
        dest += 1024;
    }
}

}  // namespace util