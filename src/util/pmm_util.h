// MIT License

// Copyright (c) [2020] [Xingsheng Zhao]

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

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
#include <sstream>
#include <thread>
#include "time.h"
#include "cpucounters.h"
#define force_inline __attribute__((always_inline)) inline

#define _mm_clwb(addr)\
    asm volatile("clwb %0" : "+m" (*(volatile char *)(addr)));


namespace util {

uint64_t PMMMask(size_t length) {
    if (64 == length) {
        return 0xFFFFFFFFFFFFFFC0;
    } else if (128 == length) {
        return 0xFFFFFFFFFFFFFF80;
    } else if (256 == length) {
        return 0xFFFFFFFFFFFFFF00;
    } else if (512 == length) {
        return 0xFFFFFFFFFFFFFE00;
    } else if (
        length % 1024 == 0 &&
        length > 0) {
        double tmp = std::log2(length / 1024);
        int left_shift = tmp;
        return (0xFFFFFFFFFFFFFC00 << left_shift);
    }
    else 
        return 0xFFFFFFFFFFFFFFFF;
}

force_inline void 
MFence() {
    __asm__ __volatile__ ("mfence" ::: "memory");
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
void __attribute__((optimize("O0")))
ClearCache() {
    static const int size = 20*1024*1024; // Allocate 20M. Set much larger then L3
    static char *c = (char *)malloc(size);
    for (int i = 0; i < 1; i++)
        memset(c, 0, size);
    _mm_mfence();
}

inline void // __attribute__((optimize("O0"),always_inline))
Load64_NT(char* addr, int N = 0) {
    volatile __m512i zmm0 = _mm512_stream_load_si512((__m512i *)addr + 0);
    // __m512i zmm00 = _mm512_stream_load_si512((__m512i *)addr + 0); // 64
    // _mm_lfence();
}


inline void // __attribute__((optimize("O0"),always_inline))
Load128_NT(char* addr, int N = 0) {
    volatile __m512i zmm0 = _mm512_stream_load_si512((__m512i *)addr + 0);
    volatile __m512i zmm1 = _mm512_stream_load_si512((__m512i *)addr + 1);

    // __m512i zmm0 = _mm512_stream_load_si512((__m512i *)addr + 0);
	// __m512i zmm1 = _mm512_stream_load_si512((__m512i *)addr + 1);
    // _mm_lfence();
}


inline void // __attribute__((optimize("O0"),always_inline))
Load256_NT(char* addr, int N = 0) {
    volatile __m512i zmm00 = _mm512_stream_load_si512((__m512i *)addr + 0);
    volatile __m512i zmm01 = _mm512_stream_load_si512((__m512i *)addr + 1);
    volatile __m512i zmm02 = _mm512_stream_load_si512((__m512i *)addr + 2);
    volatile __m512i zmm03 = _mm512_stream_load_si512((__m512i *)addr + 3);

    // __m512i zmm00 = _mm512_stream_load_si512((__m512i *)addr + 0); // 64
    // __m512i zmm01 = _mm512_stream_load_si512((__m512i *)addr + 1); // 128
    // __m512i zmm02 = _mm512_stream_load_si512((__m512i *)addr + 2); // 192
    // __m512i zmm03 = _mm512_stream_load_si512((__m512i *)addr + 3); // 256
    // _mm_lfence();
}

inline void // __attribute__((optimize("O0"),always_inline))
Load512_NT(char* addr, int N = 0) {
    volatile __m512i zmm00 = _mm512_stream_load_si512((__m512i *)addr + 0);
    volatile __m512i zmm01 = _mm512_stream_load_si512((__m512i *)addr + 1);
    volatile __m512i zmm02 = _mm512_stream_load_si512((__m512i *)addr + 2);
    volatile __m512i zmm03 = _mm512_stream_load_si512((__m512i *)addr + 3);
    volatile __m512i zmm04 = _mm512_stream_load_si512((__m512i *)addr + 4);
    volatile __m512i zmm05 = _mm512_stream_load_si512((__m512i *)addr + 5);
    volatile __m512i zmm06 = _mm512_stream_load_si512((__m512i *)addr + 6);
    volatile __m512i zmm07 = _mm512_stream_load_si512((__m512i *)addr + 7);
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


inline void // __attribute__((optimize("O0"),always_inline))
Load1024_NT(char* addr, int N = 0) {
    volatile __m512i zmm00 = _mm512_stream_load_si512((__m512i *)addr + 0);
    volatile __m512i zmm01 = _mm512_stream_load_si512((__m512i *)addr + 1);
    volatile __m512i zmm02 = _mm512_stream_load_si512((__m512i *)addr + 2);
    volatile __m512i zmm03 = _mm512_stream_load_si512((__m512i *)addr + 3);
    volatile __m512i zmm04 = _mm512_stream_load_si512((__m512i *)addr + 4);
    volatile __m512i zmm05 = _mm512_stream_load_si512((__m512i *)addr + 5);
    volatile __m512i zmm06 = _mm512_stream_load_si512((__m512i *)addr + 6);
    volatile __m512i zmm07 = _mm512_stream_load_si512((__m512i *)addr + 7);
    volatile __m512i zmm08 = _mm512_stream_load_si512((__m512i *)addr + 8);
    volatile __m512i zmm09 = _mm512_stream_load_si512((__m512i *)addr + 9);
    volatile __m512i zmm10 = _mm512_stream_load_si512((__m512i *)addr + 10);
    volatile __m512i zmm11 = _mm512_stream_load_si512((__m512i *)addr + 11);
    volatile __m512i zmm12 = _mm512_stream_load_si512((__m512i *)addr + 12);
    volatile __m512i zmm13 = _mm512_stream_load_si512((__m512i *)addr + 13);
    volatile __m512i zmm14 = _mm512_stream_load_si512((__m512i *)addr + 14);
    volatile __m512i zmm15 = _mm512_stream_load_si512((__m512i *)addr + 15);
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

inline void
LoadNKB_NT(char* addr, int N) {
    char* tmp = addr;
	// Load N KB from
	for (int i = 0; i < N; ++i) {
		Load1024_NT(tmp);
		tmp += 1024;
	}
}


static __m512i zmm = _mm512_set1_epi8((char)(31));

force_inline void
Store64_NT(char* dest, int N = 0) {
    _mm512_stream_si512((__m512i *)dest + 0, zmm);
    _mm_sfence();
}

force_inline void
Store128_NT(char* dest, int N = 0) {
    _mm512_stream_si512((__m512i *)dest + 0, zmm);
	_mm512_stream_si512((__m512i *)dest + 1, zmm);
    _mm_sfence();
}

force_inline void
Store256_NT(char* dest, int N = 0) {
    _mm512_stream_si512((__m512i *)dest + 0, zmm);
	_mm512_stream_si512((__m512i *)dest + 1, zmm);
	_mm512_stream_si512((__m512i *)dest + 2, zmm);
	_mm512_stream_si512((__m512i *)dest + 3, zmm);
    _mm_sfence();
}

force_inline void
Store512_NT(char* dest, int N = 0) {
    _mm512_stream_si512((__m512i *)dest + 0, zmm);
	_mm512_stream_si512((__m512i *)dest + 1, zmm);
	_mm512_stream_si512((__m512i *)dest + 2, zmm);
	_mm512_stream_si512((__m512i *)dest + 3, zmm);
    #ifdef FENCE_ALL
    _mm_sfence();
    #endif
	_mm512_stream_si512((__m512i *)dest + 4, zmm);
	_mm512_stream_si512((__m512i *)dest + 5, zmm);
	_mm512_stream_si512((__m512i *)dest + 6, zmm);
	_mm512_stream_si512((__m512i *)dest + 7, zmm);
    _mm_sfence();
}

force_inline void
Store1024_NT(char* dest, int N = 0) {
    _mm512_stream_si512((__m512i *)dest + 0, zmm);
	_mm512_stream_si512((__m512i *)dest + 1, zmm);
	_mm512_stream_si512((__m512i *)dest + 2, zmm);
	_mm512_stream_si512((__m512i *)dest + 3, zmm);
    #ifdef FENCE_ALL
    _mm_sfence();
    #endif
	_mm512_stream_si512((__m512i *)dest + 4, zmm);
	_mm512_stream_si512((__m512i *)dest + 5, zmm);
	_mm512_stream_si512((__m512i *)dest + 6, zmm);
	_mm512_stream_si512((__m512i *)dest + 7, zmm);
    #ifdef FENCE_ALL
    _mm_sfence();
    #endif
	_mm512_stream_si512((__m512i *)dest + 8, zmm);
	_mm512_stream_si512((__m512i *)dest + 9, zmm);
	_mm512_stream_si512((__m512i *)dest + 10, zmm);
	_mm512_stream_si512((__m512i *)dest + 11, zmm);
    #ifdef FENCE_ALL
    _mm_sfence();
    #endif
	_mm512_stream_si512((__m512i *)dest + 12, zmm);
	_mm512_stream_si512((__m512i *)dest + 13, zmm);
	_mm512_stream_si512((__m512i *)dest + 14, zmm);
	_mm512_stream_si512((__m512i *)dest + 15, zmm);
    _mm_sfence();
}

force_inline void
StoreNKB_NT(char* dest, int N) {
	// store N KB to dest
    // char* old = dest;
	for (int i = 0; i < N; ++i) {
		Store1024_NT(dest);
		dest += 1024;
	}
}

force_inline void
Store64_WB(char* dest, int N = 0) {
    _mm512_store_epi64((__m512i *)dest + 0, zmm);
    _mm_clwb((__m512i *)dest + 0);
    _mm_sfence();
}


force_inline void
Store128_WB(char* dest, int N = 0) {
    _mm512_store_epi64((__m512i *)dest + 0, zmm);
    _mm_clwb((__m512i *)dest + 0);
	_mm512_store_epi64((__m512i *)dest + 1, zmm);
    _mm_clwb((__m512i *)dest + 1);
    _mm_sfence();
}

force_inline void
Store256_WB(char* dest, int N = 0) {
    _mm512_store_epi64((__m512i *)dest + 0, zmm);
    _mm_clwb((__m512i *)dest + 0);
	_mm512_store_epi64((__m512i *)dest + 1, zmm);
    _mm_clwb((__m512i *)dest + 1);
	_mm512_store_epi64((__m512i *)dest + 2, zmm);
    _mm_clwb((__m512i *)dest + 2);
	_mm512_store_epi64((__m512i *)dest + 3, zmm);
    _mm_clwb((__m512i *)dest + 3);
    _mm_sfence();
}

force_inline void
Store512_WB(char* dest, int N = 0) {
    _mm512_store_epi64((__m512i *)dest + 0, zmm);
    _mm_clwb((__m512i *)dest + 0);
	_mm512_store_epi64((__m512i *)dest + 1, zmm);
    _mm_clwb((__m512i *)dest + 1);
	_mm512_store_epi64((__m512i *)dest + 2, zmm);
    _mm_clwb((__m512i *)dest + 2);
	_mm512_store_epi64((__m512i *)dest + 3, zmm);
    _mm_clwb((__m512i *)dest + 3);
	_mm512_store_epi64((__m512i *)dest + 4, zmm);
    _mm_clwb((__m512i *)dest + 4);
	_mm512_store_epi64((__m512i *)dest + 5, zmm);
    _mm_clwb((__m512i *)dest + 5);
	_mm512_store_epi64((__m512i *)dest + 6, zmm);
    _mm_clwb((__m512i *)dest + 6);
	_mm512_store_epi64((__m512i *)dest + 7, zmm);
    _mm_clwb((__m512i *)dest + 7);
    _mm_sfence();
}

force_inline void
Store1024_WB(char* dest, int N = 0) {
    _mm512_store_epi64((__m512i *)dest + 0, zmm);
    _mm_clwb((__m512i *)dest + 0);
	_mm512_store_epi64((__m512i *)dest + 1, zmm);
    _mm_clwb((__m512i *)dest + 1);
	_mm512_store_epi64((__m512i *)dest + 2, zmm);
    _mm_clwb((__m512i *)dest + 2);
	_mm512_store_epi64((__m512i *)dest + 3, zmm);
    _mm_clwb((__m512i *)dest + 3);
	_mm512_store_epi64((__m512i *)dest + 4, zmm);
    _mm_clwb((__m512i *)dest + 4);
	_mm512_store_epi64((__m512i *)dest + 5, zmm);
    _mm_clwb((__m512i *)dest + 5);
	_mm512_store_epi64((__m512i *)dest + 6, zmm);
    _mm_clwb((__m512i *)dest + 6);
	_mm512_store_epi64((__m512i *)dest + 7, zmm);
    _mm_clwb((__m512i *)dest + 7);
	_mm512_store_epi64((__m512i *)dest + 8, zmm);
    _mm_clwb((__m512i *)dest + 8);
	_mm512_store_epi64((__m512i *)dest + 9, zmm);
    _mm_clwb((__m512i *)dest + 9);
	_mm512_store_epi64((__m512i *)dest + 10, zmm);
    _mm_clwb((__m512i *)dest + 10);
	_mm512_store_epi64((__m512i *)dest + 11, zmm);
    _mm_clwb((__m512i *)dest + 11);
	_mm512_store_epi64((__m512i *)dest + 12, zmm);
    _mm_clwb((__m512i *)dest + 12);
	_mm512_store_epi64((__m512i *)dest + 13, zmm);
    _mm_clwb((__m512i *)dest + 13);
	_mm512_store_epi64((__m512i *)dest + 14, zmm);
    _mm_clwb((__m512i *)dest + 14);
	_mm512_store_epi64((__m512i *)dest + 15, zmm);
    _mm_clwb((__m512i *)dest + 15);
    _mm_sfence();
}

#pragma GCC diagnostic pop

force_inline void
StoreNKB_WB(char* dest, int N) {
	// store N KB to dest
    // char* old = dest;
	for (int i = 0; i < N; ++i) {
		Store1024_WB(dest);
		dest += 1024;
	}
}

struct IPMInfo{
    std::string dimm_name;
    // Number of read and write operations performed from/to the physical media. 
    // Each operation transacts a 64byte operation. These operations includes 
    // commands transacted for maintenance as well as the commands transacted 
    // by the CPU.
    uint64_t    read_64B_ops_received  = 0;
    uint64_t    write_64B_ops_received = 0;

    // Number of read and write operations received from the CPU (memory controller)
    // unit: 64 byte
    uint64_t    ddrt_read_ops   = 0;      
    uint64_t    ddrt_write_ops  = 0;

    // actual byte write/read to DIMM media
    // bytes_read (derived)   : number of bytes transacted by the read operations
    // bytes_written (derived): number of bytes transacted by the write operations
    // Note: The total number of bytes transacted in any sample is computed as 
    // bytes_read (derived) + 2 * bytes_written (derived).
    // 
    // Formula:
    // bytes_read   : (read_64B_ops_received - write_64B_ops_received) * 64
    // bytes_written: write_64B_ops_received * 64
    uint64_t    bytes_read      = 0;
    uint64_t    bytes_written   = 0;

    // Number of bytes received from the CPU (memory controller)
    // ddrt_read_bytes  : (ddrt_read_ops * 64)
    // ddrt_write_bytes : (ddrt_write_ops * 64)
    uint64_t    ddrt_read_bytes   = 0;      
    uint64_t    ddrt_write_bytes  = 0;

    // DIMM ; read_64B_ops_received  ; write_64B_ops_received ; ddrt_read_ops  ; ddrt_write_ops;
    // DIMM0; 1292533678412          ; 643726680616           ; 508664958085   ; 560639638344;
    void Parse(const std::string& result) {
        // printf("Parse: %s\n", result.c_str());
        std::vector<std::string> infos = split(result, ";");
        dimm_name = infos[0];
        read_64B_ops_received  = stol(infos[1]);
        write_64B_ops_received = stol(infos[2]);
        ddrt_read_ops          = stol(infos[3]);
        ddrt_write_ops         = stol(infos[4]);
        ddrt_read_bytes        = ddrt_read_ops * 64;
        ddrt_write_bytes       = ddrt_write_ops * 64; 
        bytes_read             = (read_64B_ops_received - write_64B_ops_received) * 64;
        bytes_written          = write_64B_ops_received * 64;
        // printf("%s", ToString().c_str());
    }

    std::string ToString() {
        std::string res;
        char buffer[1024];
        sprintf(buffer, "\033[34m%s | Read from IMC | Write from IMC | Read DIMM | Write DIMM |\n", dimm_name.c_str());
        res += buffer;
        sprintf(buffer, "  MB  | %13.0f | %14.0f | %9.0f | %10.0f |", 
            ddrt_read_bytes/1024.0/1024.0,
            ddrt_write_bytes/1024.0/1024.0,
            read_64B_ops_received*64/1024.0/1024.0,
            write_64B_ops_received*64/1024.0/1024.0);
        res += buffer;
        res += "\033[0m\n";
        return res;
    }

    std::vector<std::string> split(std::string str, std::string token){
        std::vector<std::string>result;
        while(str.size()){
            size_t index = str.find(token);
            if(index != std::string::npos){
                result.push_back(str.substr(0,index));
                str = str.substr(index+token.size());
                if(str.size()==0)result.push_back(str);
            }else{
                result.push_back(str);
                str = "";
            }
        }
        return result;
    }
};

class IPMMetric {
public:
    IPMMetric(){}
    IPMMetric(const IPMInfo& before, const IPMInfo& after) {
        metric_.bytes_read = after.bytes_read - before.bytes_read;
        metric_.bytes_written = after.bytes_written - before.bytes_written;
        metric_.ddrt_read_bytes = after.ddrt_read_bytes - before.ddrt_read_bytes;
        metric_.ddrt_write_bytes = after.ddrt_write_bytes - before.ddrt_write_bytes;
    }

    void Merge(const IPMMetric& metric) {
        metric_.bytes_read += metric.metric_.bytes_read;
        metric_.bytes_written += metric.metric_.bytes_written;
        metric_.ddrt_read_bytes += metric.metric_.ddrt_read_bytes;
        metric_.ddrt_write_bytes += metric.metric_.ddrt_write_bytes;
    }

    uint64_t GetByteReadToDIMM() {
        return metric_.bytes_read;
    }

    uint64_t GetByteWriteToDIMM() {
        return metric_.bytes_written;
    }

    uint64_t GetByteReadFromIMC() {
        return metric_.ddrt_read_bytes;
    }

    uint64_t GetByteWriteFromIMC() {
        return metric_.ddrt_write_bytes;
    }

    
private:
    IPMInfo metric_;
};

class IPMWatcher {
public:
    
    IPMWatcher(const std::string& name): file_name_("ipm_" + name + ".txt") {
        printf("\033[32mStart IPMWatcher for %s\n\033[0m", name.c_str());
        metrics_before_ = Profiler();
        start_time_ = util::NowMicros();
    }

    ~IPMWatcher() {
        if (!finished_) {
            Report();
        }
    }

    void Report() {
        finished_ = true;
        duration_ = (util::NowMicros() - start_time_) / 1000000.0;
        metrics_after_ = Profiler();
        IPMMetric metric_merge;
        for (size_t i = 0; i < metrics_before_.size(); ++i) {
            auto& info_before = metrics_before_[i];
            auto& info_after  = metrics_after_[i];
            IPMMetric metric(info_before, info_after);
            metric_merge.Merge(metric);
            std::string res;
            char buffer[1024];
            sprintf(buffer, "\033[34m%s | Read from IMC | Write from IMC |  Read DIMM  |  Write DIMM  |   RA   |   WA   |\n", info_before.dimm_name.c_str());
            res += buffer;
            // double duration = (end_time_ - start_time_) / 1000000.0;
            // printf("duration: %f s", duration);
            // double read_throughput = metric.GetByteReadToDIMM() / 1024.0 / 1024.0 / duration;
            // double write_throughput = metric.GetByteWriteToDIMM() / 1024.0 / 1024.0 / duration;
            sprintf(buffer, "  MB  | %13.2f | %14.2f | %11.2f | %12.2f | %6.2f | %6.2f |", // Read: %6.2f MB/s, Write: %6.2f MB/s", 
                    metric.GetByteReadFromIMC()/1024.0/1024.0,
                    metric.GetByteWriteFromIMC() /1024.0/1024.0,
                    metric.GetByteReadToDIMM() /1024.0/1024.0,
                    metric.GetByteWriteToDIMM() /1024.0/1024.0,
                    (double) metric.GetByteReadToDIMM() / metric.GetByteReadFromIMC(),
                    (double) metric.GetByteWriteToDIMM() / metric.GetByteWriteFromIMC()
                    // write_throughput
                    );
            res += buffer;
            res += "\033[0m\n";
            printf("%s", res.c_str());
        }           
        dimm_read_  = metric_merge.GetByteReadToDIMM() / 1024.0/1024.0/ (duration_);
        dimm_write_ = metric_merge.GetByteWriteToDIMM() / 1024.0/1024.0/ (duration_);
        app_read_   = metric_merge.GetByteReadFromIMC() / 1024.0/1024.0/ (duration_);
        app_write_  = metric_merge.GetByteWriteFromIMC() / 1024.0/1024.0/ (duration_);
        printf("\033[34m*SUM* | DIMM-R: %7.1f MB/s. User-R: %7.1f MB/s   | DIMM-W: %7.1f MB/s, User-W: %7.1f MB/s. Time: %6.2fs.\033[0m\n", 
            dimm_read_,            
            app_read_,
            dimm_write_,
            app_write_,
            duration_);  
        printf("\033[32mDestroy IPMWatcher.\n\033[0m\n");
        fflush(nullptr);
    }

    std::vector<IPMInfo> Profiler() const {
        std::vector<IPMInfo> infos;
        util::Execute("/opt/intel/ipmwatch/bin64/ipmwatch -l >" + file_name_);
        std::string results = util::Execute("grep -w \'DIMM.\' " + file_name_);
        std::stringstream ss(results);
        while (!ss.eof()) {
            std::string res;
            ss >> res;
            if (res.empty()) break;
            IPMInfo tmp;
            tmp.Parse(res);
            infos.push_back(tmp);
        }
        return infos;
    }

    const std::string file_name_;
    std::vector<IPMInfo> metrics_before_;
    std::vector<IPMInfo> metrics_after_;
    double dimm_read_   = 0;
    double dimm_write_  = 0;
    double app_read_    = 0;
    double app_write_   = 0;
    double start_time_  = 0;
    double duration_    = 0;
    bool   finished_    = false;
};

class WriteAmplificationWatcher {
public:
    WriteAmplificationWatcher(const IPMWatcher& watcher): watcher_(watcher) {
        auto tmp = watcher_.Profiler();
        before_ = watcher_.Profiler();
        for (size_t i = 0; i < before_.size(); ++i) {
            IPMMetric metric(tmp[i], before_[i]);
            dimm_bias_.push_back(metric.GetByteWriteToDIMM());
            imc_bias_.push_back(metric.GetByteWriteFromIMC());
        }
        
    }

    ~WriteAmplificationWatcher() {
        after_ = watcher_.Profiler();
        for (size_t i = 0; i < before_.size(); ++i) {
            IPMMetric metric(before_[i], after_[i]);
            printf("%s. WA(Bias): %2.4f. Write to DIMM: %10lu. Write from IMC: %10lu \n%s. WA(Fix) : %2.4f. Write to DIMM: %10lu. Write from IMC: %10lu \n", 
                before_[i].dimm_name.c_str(),
                (double)metric.GetByteWriteToDIMM() / metric.GetByteWriteFromIMC(),
                metric.GetByteWriteToDIMM(),
                metric.GetByteWriteFromIMC(),
                before_[i].dimm_name.c_str(),
                ((double)metric.GetByteWriteToDIMM() - dimm_bias_[i]) / (metric.GetByteWriteFromIMC() - imc_bias_[i]),
                metric.GetByteWriteToDIMM() - dimm_bias_[i],
                metric.GetByteWriteFromIMC() - imc_bias_[i]
                );
        }
    }

private:
    const IPMWatcher& watcher_;
    std::vector<IPMInfo> before_;
    std::vector<IPMInfo> after_;
    std::vector<uint64_t> dimm_bias_;
    std::vector<uint64_t> imc_bias_;
};

class PCMMetric {
public:
    PCMMetric(const std::string name): name_(name) {
        before_sstate_ = getSystemCounterState();
        start_time_ = util::NowMicros();
    }
    ~PCMMetric() {
        double duration = (util::NowMicros() - start_time_) / 1000000.0;
        after_sstate_ = getSystemCounterState();
        std::cout 
                << "PCM Metrics: " << name_
                << "\n"
                // << "\tL3 misses: " << getL3CacheMisses(*before_sstate, *after_sstate) << "\n"
                << "\tDRAM Read  (bytes): " << getBytesReadFromMC(before_sstate_, after_sstate_) << "\n"
                << "\tDRAM Write (bytes): " << getBytesWrittenToMC(before_sstate_, after_sstate_) << "\n"
                << "\tPMEM Read  (bytes): " << getBytesReadFromPMM(before_sstate_, after_sstate_) << "\n"
                << "\tPMEM Write (bytes): " << getBytesWrittenToPMM(before_sstate_, after_sstate_) << "\n"
                << "\tDRAM Read    Speed: " << getBytesReadFromMC(before_sstate_, after_sstate_) / 1024.0 / 1024.0 / duration << " MB/s" << "\n"
                << "\tDRAM Write   Speed: " << getBytesWrittenToMC(before_sstate_, after_sstate_) / 1024.0 / 1024.0 / duration << " MB/s" << "\n"
                << "\tPMEM Read    Speed: " << getBytesReadFromPMM(before_sstate_, after_sstate_) / 1024.0 / 1024.0 / duration << " MB/s" << "\n"
                << "\tPMEM Write   Speed: " << getBytesWrittenToPMM(before_sstate_, after_sstate_) / 1024.0 / 1024.0 / duration << " MB/s" << "\n"
                << "\tDuration: " << duration << " s" << std::endl;
    }
private:
    SystemCounterState before_sstate_;
    SystemCounterState after_sstate_;
    const std::string name_;
    uint64_t start_time_;
};


}