#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <thread>
#include <algorithm>
#include <libpmem.h>
#include <libvmmalloc.h>
#include <random>
#include <numeric>
#include <linux/membarrier.h>

#include "util/env.h"
#include "util/io_report.h"
#include "util/test_util.h"
#include "util/trace.h"



/* using 1k of pmem for this example */
#define PMEM_LEN ((4ULL << 30))  // 4 GB file

// Maximum length of our buffer
std::vector<uint64_t> kBufferVector = 
    {64, 128, 256, 512, 1 << 10, 2 << 10, 4 << 10, 8 << 10, 16 << 10, 32 << 10};

using namespace util;

// Number of thread
int kThread = 2;

// Number of operations
const uint64_t kNum = 10000000;

static int threadIDs[16] = {0, 1, 2, 3, 4, 5, 6, 7,
                            16,17,18,19,20,21,22,23};

util::RandomGenerator gen;

volatile bool kRunning = true;

static __inline void MFence() {
    __asm__ __volatile__ ("mfence" ::: "memory");
  }

force_inline void 
LoadNT(const char *src, uint64_t MAX_BUF_LEN) {
    static thread_local char dest[2048<<10];
    memcpy(dest, src, MAX_BUF_LEN);
    // MFence();
}

static void sig_int(int sig)
{
	printf("Exiting on signal %d\n", sig);
	kRunning = false;
}
static void arm_sig_int(void)
{
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = sig_int;
	act.sa_flags = SA_RESTART;
	sigaction(SIGINT, &act, NULL);
}

force_inline void RandomWrite(
    std::vector<std::thread>& workers,
    std::vector<double>& results,
    uint64_t MAX_BUF_LEN,
    int i,
    char* pmemaddr,
    bool is_seq = false,
    uint32_t flag = PMEM_F_MEM_NONTEMPORAL) {
    
    // Create a cpu_set_t object representing a set of CPUs. Clear it and mark
    // only CPU i as set.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadIDs[i], &cpuset);
    int rc = pthread_setaffinity_np(workers[i].native_handle(),
                                    sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        fprintf(stderr,"Error calling pthread_setaffinity_np: %d \n", rc);
    }
    
    util::Slice value(gen.Generate(MAX_BUF_LEN));
    util::TraceUniform rnd(123321 + i);
    uint64_t*  offsets = new uint64_t [kNum];
    if (!is_seq) {
        for (size_t k = 0; k < kNum; ++k) {
            offsets[k] = (rnd.Next() % (PMEM_LEN - MAX_BUF_LEN)) / MAX_BUF_LEN * MAX_BUF_LEN;
        }
    }
    else {
        uint64_t off = (rnd.Next() % (PMEM_LEN - MAX_BUF_LEN)) / MAX_BUF_LEN * MAX_BUF_LEN;
        for (size_t k = 0; k < kNum; ++k) {
            offsets[k] = off;
            off += MAX_BUF_LEN;
            if (off >= PMEM_LEN - MAX_BUF_LEN) {
                off = 0;
            }
        }
    }
    
    printf("Start test\n");
    uint64_t k = 0;
    auto time_start = util::Env::Default()->NowMicros();
    auto start = time_start;
    uint64_t left_byte = PMEM_LEN;
    for (; k < kNum && kRunning && left_byte > 0; k++)
    {
        pmem_memmove(pmemaddr + offsets[k], value.data() + (k % 128), MAX_BUF_LEN, flag);
        DEBUG("write offset: %lu", offsets[k]);
        if (k != 0 && k % 1000000 == 0) {
            auto end = util::Env::Default()->NowMicros();
            double throughput = (MAX_BUF_LEN * 1000000)/ ((end - start) / 1000000.0) / 1024.0 / 1024.0;
            fprintf(stderr, "Write throughput (%2d): %.2f MB/s, latency(%.4f)\r", i, throughput, (end - start) / 1000000.0);
            fflush(stderr);
            start = end;
        }
        left_byte -= MAX_BUF_LEN;
    }
    auto time_end = util::Env::Default()->NowMicros();
    double throughput = (MAX_BUF_LEN * k)/ ((time_end - time_start) / 1000000.0) / 1024.0 / 1024.0;
    if (!is_seq) printf("Avg Random Write throughput (%2d - %lu byte): %.2f MB/s\n", i, MAX_BUF_LEN, throughput);
    else         printf("Avg Sequential Write throughput (%2d - %lu byte): %.2f MB/s\n", i, MAX_BUF_LEN, throughput);
    results[i] = throughput;
    delete[] offsets;
}

void RandomWriteBench(char* pmemaddr, bool is_seq = false, bool is_wb = false) {
    std::vector<uint64_t> buf_lens = kBufferVector;
    std::vector<std::vector<double> > result_table;
    

    // iterate to kThread
    for (int thread_num = 1; thread_num <= kThread; thread_num++) {
        std::vector<double> final_results(buf_lens.size());
        int results_len = final_results.size();
        uint32_t flag = is_wb ? PMEM_F_MEM_WB : PMEM_F_MEM_NONTEMPORAL;
        for (size_t i = 0; i < buf_lens.size(); ++i) {
            auto BUF_LEN = buf_lens[i];
            std::vector<std::thread> workers(thread_num);
            std::vector<double> results(thread_num);
            
            kRunning = true;
            for (int t = 0; t < thread_num; t++) {
                workers[t] = std::thread([&, t]
                {
                    RandomWrite(workers, results, BUF_LEN, t, pmemaddr, is_seq, flag);
                });
            }
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
            {
                t.join();
            });
            double final_throughput = std::accumulate(results.begin(), results.end(), 0);
            final_results[i] = final_throughput;
            if (!is_seq) printf("----------- Random Write Throughput (%lu): %.2f MB/s ------------\n", BUF_LEN, final_throughput);
            else         printf("----------- Sequential Write Throughput (%lu): %.2f MB/s ------------\n", BUF_LEN, final_throughput);
        }

        printf("\033[32mFinal Results (Thread: %2d, %s Write)\033[0m\n", thread_num, is_seq ? "Sequential" : "Random");
        printf("\033[34m");
        for (int i = 0; i < results_len; ++i) {
            printf("|--- %5lu B ---", buf_lens[i]);
        }
        printf("|\n");
        for (int i = 0; i < results_len; ++i) {
            printf("|%10.1f MB/s", final_results[i]);
        }
        printf("|\033[0m\n");
        fflush(nullptr);
        result_table.push_back(final_results);
    }

    printf("\033[32mFinal Results Table (%s Write. %s)\033[0m\n", is_seq ? "Sequential" : "Random", is_wb ? "WB" : "NT");
    printf("\033[34m");
    for (int i = 0; i < buf_lens.size(); ++i) {
        printf("|--- %5lu B ---", buf_lens[i]);
    }
    printf("|\n");
    int t = 1;
    for (auto& res : result_table) {
        for (int i = 0; i < res.size(); ++i) {
            printf("|%10.1f MB/s", res[i]);
        }
        printf("| %2d\n", t++);
    }
    
    printf("|\033[0m\n");
    fflush(nullptr);
}

force_inline void RandomRead(
    std::vector<std::thread>& workers,
    std::vector<double>& results,
    uint64_t MAX_BUF_LEN,
    int i,
    char* pmemaddr,
    bool is_seq = false) {
    
    // Create a cpu_set_t object representing a set of CPUs. Clear it and mark
    // only CPU i as set.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadIDs[i], &cpuset);
    int rc = pthread_setaffinity_np(workers[i].native_handle(),
                                    sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        fprintf(stderr,"Error calling pthread_setaffinity_np: %d \n", rc);
    }
    
    util::Slice value(gen.Generate(MAX_BUF_LEN));
    util::TraceUniform rnd(123321 + i * 333);
    uint64_t*  offsets = new uint64_t [kNum];
    if (!is_seq) {
        for (size_t k = 0; k < kNum; ++k) {
            offsets[k] = (rnd.Next() % (PMEM_LEN - MAX_BUF_LEN)) / MAX_BUF_LEN * MAX_BUF_LEN;
        }
    }
    else {
        uint64_t off = (rnd.Next() % (PMEM_LEN - MAX_BUF_LEN)) / MAX_BUF_LEN * MAX_BUF_LEN;
        for (size_t k = 0; k < kNum; ++k) {
            offsets[k] = off;
            off += MAX_BUF_LEN;
            if (off >= PMEM_LEN - MAX_BUF_LEN) {
                off = 0;
            }
        }
    }
    
    printf("Start test\n");
    uint64_t k = 0;
    auto time_start = util::Env::Default()->NowMicros();
    auto start = time_start;
    uint64_t left_byte = PMEM_LEN;
    for (; k < kNum && kRunning && left_byte > 0; k++)
    {
        LoadNT(pmemaddr + offsets[k], MAX_BUF_LEN);
        DEBUG("read offset: %lu", offsets[k]);
        if (k != 0 && k % 1000000 == 0) {
            auto end = util::Env::Default()->NowMicros();
            double throughput = (MAX_BUF_LEN * 1000000)/ ((end - start) / 1000000.0) / 1024.0 / 1024.0;
            fprintf(stderr, "Read throughput (%2d): %.2f MB/s, latency(%.4f)\r", i, throughput, (end - start) / 1000000.0);
            fflush(stderr);
            start = end;
        }
        left_byte -= MAX_BUF_LEN;
    }
    auto time_end = util::Env::Default()->NowMicros();
    double throughput = (MAX_BUF_LEN * k)/ ((time_end - time_start) / 1000000.0) / 1024.0 / 1024.0;
    if (!is_seq) printf("Avg Random Read throughput (%2d - %lu byte): %.2f MB/s\n", i, MAX_BUF_LEN, throughput);
    else         printf("Avg Sequential Read throughput (%2d - %lu byte): %.2f MB/s\n", i, MAX_BUF_LEN, throughput);
    results[i] = throughput;
    delete[] offsets;
}

void RandomReadBench(char* pmemaddr, bool is_seq = false) {
    std::vector<uint64_t> buf_lens = kBufferVector;
    std::vector<std::vector<double> > result_table;
    // iterate to kThread
    for (int thread_num = 1; thread_num <= kThread; thread_num++) {
        std::vector<double> final_results(buf_lens.size());
        int results_len = final_results.size();
        for (size_t i = 0; i < buf_lens.size(); ++i) {
            auto BUF_LEN = buf_lens[i];
            std::vector<std::thread> workers(thread_num);
            std::vector<double> results(thread_num);
        
            kRunning = true;
            for (int i = 0; i < thread_num; i++) {
                workers[i] = std::thread([&, i]
                {
                    RandomRead(workers, results, BUF_LEN, i, pmemaddr, is_seq);
                });
            }
            std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
            {
                t.join();
            });
            double final_throughput = std::accumulate(results.begin(), results.end(), 0);
            if (!is_seq) printf("----------- Random Read Throughput (%lu): %.2f MB/s ------------\n", BUF_LEN, final_throughput);
            else         printf("----------- Sequential Read Throughput (%lu): %.2f MB/s ------------\n", BUF_LEN, final_throughput);
            final_results[i] = final_throughput;
        }


        printf("\033[32mFinal Results (Thread: %2d, %s Read)\033[0m\n", thread_num, is_seq ? "Sequential" : "Random");
        printf("\033[34m");
        for (int i = 0; i < results_len; ++i) {
            printf("|--- %5lu B ---", buf_lens[i]);
        }
        printf("|\n");
        for (int i = 0; i < results_len; ++i) {
            printf("|%10.1f MB/s", final_results[i]);
        }
        printf("|\033[0m\n");
        fflush(nullptr);
        result_table.push_back(final_results);
    }
    
    printf("\033[32mFinal Results Table (%s Read)\033[0m\n", is_seq ? "Sequential" : "Random");
    printf("\033[34m");
    for (int i = 0; i < buf_lens.size(); ++i) {
        printf("|--- %5lu B ---", buf_lens[i]);
    }
    printf("|\n");
    int t = 1;
    for (auto& res : result_table) {
        for (int i = 0; i < res.size(); ++i) {
            printf("|%10.1f MB/s", res[i]);
        }
        printf("| %2d\n", t++);
    }
    
    printf("|\033[0m\n");
    fflush(nullptr);
}

int main(int argc, char *argv[])
{
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename> thread_num\n", argv[0]);
        exit(0);
    }
    // initial interrupt function 
    arm_sig_int();

    char *path = argv[1];
    char *pmemaddr;
    size_t mapped_len;
    int is_pmem;

    kThread = std::stoi(argv[2]);

    // size_t left_len = PMEM_LEN;
    // int fd = open(path, O_CREAT | O_DIRECT | O_TRUNC, 0666);
    // if (fd < 0) {
    //     perror(strerror(-fd));
    // }
    // while (left_len > 0) {
    //    int res = write(fd, (void*) gen.Generate(1 << 20).data(), 1 << 20);
    //    if (res < 0) {
    //        perror(strerror(-res));
    //        exit(1);
    //    }
    // }
    // close(fd);

    /* create a pmem file and memory map it */
    if ((pmemaddr = (char *)pmem_map_file(path, PMEM_LEN, PMEM_FILE_CREATE, 0666, &mapped_len, &is_pmem)) == NULL) {
        perror("pmem_map_file");
        exit(1);
    }


    // random write bench
    RandomWriteBench(pmemaddr, false, false);

    // sequential write bench
    RandomWriteBench(pmemaddr, true,  false);

    // random write bench
    RandomWriteBench(pmemaddr, false, true);

    // sequential write bench
    RandomWriteBench(pmemaddr, true, true);
    
    // random read bench
    RandomReadBench(pmemaddr);

    // sequential read bench
    RandomReadBench(pmemaddr, true);

    
    pmem_unmap(pmemaddr, PMEM_LEN);
    return 0;
}