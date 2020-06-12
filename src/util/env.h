// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// An Env is an interface used by the leveldb implementation to access
// operating system functionality like the filesystem etc.  Callers
// may wish to provide a custom Env object when opening a database to
// get fine gain control; e.g., to rate limit file system operations.
//
// All Env implementations are safe for concurrent access from
// multiple threads without any external synchronization.

#ifndef __ENV_H__
#define __ENV_H__

#include "slice.h"
#include "status.h"
#include "posix_logger.h"

#include <string.h>
#include <signal.h>
#include <cstdio>
#include <atomic>

#include <vector>
#include <string>
#include <pthread.h>

#define likely(x)       (__builtin_expect(false || (x), true))
#define unlikely(x)     (__builtin_expect(x, 0))

#define __FILENAME__ ((strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__))

#define INFO(M, ...)\
do {\
  char buffer[1024] = "[INFO] ";\
  sprintf(buffer + strlen(buffer), "[%s] %s:%d ", __FILENAME__, __FUNCTION__, __LINE__);\
  sprintf(buffer + strlen(buffer), M, ##__VA_ARGS__);\
  util::Log(Env::Default()->LOG(), "%s", buffer);\
} while(0);

#ifndef NDEBUG
#define DEBUG(M, ...)\
do {\
  char buffer[1024] = "[DEBUG] ";\
  sprintf(buffer + strlen(buffer), "[%s] %s:%d ", __FILENAME__, __FUNCTION__, __LINE__);\
  sprintf(buffer + strlen(buffer), M, ##__VA_ARGS__);\
  util::Log(Env::Default()->LOG(), "%s", buffer);\
} while(0);
#else
#define DEBUG(M, ...)\
  do {\
  } while(0);
#endif

#define ERROR(M, ...)\
do {\
  char buffer[1024] = "[ERROR] ";\
  sprintf(buffer + strlen(buffer), "[%s] %s:%d ", __FILENAME__, __FUNCTION__, __LINE__);\
  sprintf(buffer + strlen(buffer), M, ##__VA_ARGS__);\
  util::Log(Env::Default()->LOG(), "%s", buffer);\
} while(0);


#define WARNING(M, ...)\
do {\
  char buffer[1024] = "[WARN] ";\
  sprintf(buffer + strlen(buffer), "[%s] %s:%d ", __FILENAME__, __FUNCTION__, __LINE__);\
  sprintf(buffer + strlen(buffer), M, ##__VA_ARGS__);\
  util::Log(Env::Default()->LOG(), "%s", buffer);\
} while(0);

namespace util {


// void INFO(const char* format, ...);
// void DEBUG(const char* format, ...);
// void ERROR(const char* format, ...);

void Log(PosixLogger* info_log, const char* format, ...);

class ThreadPoolImpl;

class Env {
 public:
  Env();
  ~Env();

  std::string Execute(const std::string& cmd);
  static void PinCore(int i);
  uint32_t NextID() {return next_id_.fetch_add(1, std::memory_order_relaxed); }
  // Priority for scheduling job in thread pool
  enum Priority { BOTTOM, LOW, HIGH, USER, TOTAL };

  static std::string PriorityToString(Priority priority);

  // Priority for requesting bytes in rate limiter scheduler
  enum IOPriority { IO_LOW = 0, IO_HIGH = 1, IO_TOTAL = 2 };

  // Return a default environment suitable for the current operating
  // system.  Sophisticated users may wish to provide their own Env
  // implementation instead of relying on this default environment.
  //
  // The result of Default() belongs to leveldb and must never be deleted.
  static Env* Default();


  // Returns true iff the named file exists.
   bool FileExists(const std::string& fname);

   bool FileValid(int fd);
  // Store in *result the names of the children of the specified directory.
  // The names are relative to "dir".
  // Original contents of *results are dropped.
   Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result);

  // Delete the named file.
   Status DeleteFile(const std::string& fname);

  // Create the specified directory.
   Status CreateDir(const std::string& dirname);

  // Create the specified File.
   Status CreateFile(const std::string& filename, size_t size = 0);
  
  // Delete the specified directory.
   Status DeleteDir(const std::string& dirname);

  // Store the size of fname in *file_size.
   Status GetFileSize(const std::string& fname, int64_t* file_size);

  // Rename file src to target.
   Status RenameFile(const std::string& src,
                            const std::string& target);

  // Returns the number of micro-seconds since some fixed point in time. Only
  // useful for computing deltas of time.
  inline uint64_t NowMicros() {
    static constexpr uint64_t kUsecondsPerSecond = 1000000;
    struct ::timeval tv;
    ::gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * kUsecondsPerSecond + tv.tv_usec;
  }
  // Returns the number of nano-seconds since some fixed point in time. Only
  // useful for computing deltas of time in one run.
  // Default implementation simply relies on NowMicros.
  // In platform-specific implementations, NowNanos() should return time points
  // that are MONOTONIC.
  inline uint64_t NowNanos() {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      return static_cast<uint64_t>(ts.tv_sec) * 1000000000L + ts.tv_nsec;
  }
  
  // In assembler language, the RDTSC instruction returns the value of the TSC directly in registers 
  // edx:eax. However, since modern CPU’s support out-of-order execution, it has been common 
  // practice to insert a serializing instruction (such as CPUID) prior to the RDTSC instruction in order 
  // to ensure that the execution of RDTSC is not reordered by the processor.
  
  // More recent CPU’s include the RDTSCP instruction, which does any necessary serialization itself. 
  // This avoids the overhead of the CPUID instruction, which can be considerable (and variable). If 
  // your CPU supports RDTSCP, use that instead of the CPUID/RDTSC combination.
  inline unsigned long long NowTick() {
      unsigned int lo, hi;
      asm volatile (
        "rdtscp"
      : "=a"(lo), "=d"(hi) /* outputs */
      : "a"(0)             /* inputs */
      : "%ebx", "%ecx");     /* clobbers*/
      return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
  }

  // Converts seconds-since-Jan-01-1970 to a printable string
   std::string TimeToString(uint64_t time);
  
  // Sleep/delay the thread for the prescribed number of micro-seconds.
   void SleepForMicroseconds(int micros);

  // Returns the ID of the current thread.
   uint64_t GetThreadID() const;

  void IncBackgroundThreadsIfNeeded(int number, Priority pri);
  
  void SetBackgroundThreads(int num, Priority pri);

  void Schedule(void (*function)(void* arg1), void* arg, Priority pri = LOW,
                void* tag = nullptr,
                void (*unschedFunction)(void* arg) = nullptr);

  int UnSchedule(void* arg, Priority pri);

  unsigned int GetThreadPoolQueueLen(Priority pri = LOW) const;

  void WaitAllJobs();
  void WaitAllJobs(Priority pri);
  static uint64_t gettid(pthread_t tid) {
    uint64_t thread_id = 0;
    memcpy(&thread_id, &tid, std::min(sizeof(thread_id), sizeof(tid)));
    return thread_id;
  }

  static uint64_t gettid() {
    pthread_t tid = pthread_self();
    return gettid(tid);
  }

  void StartThread(void (*function)(void* arg), void* arg);
  void WaitForJoin();
  int GetCoreNum() {
    return std::thread::hardware_concurrency();
  }
  Status NewLogger(const std::string& fname, PosixLogger** result);

  util::PosixLogger* LOG() {return logger_; };


private:
  std::vector<ThreadPoolImpl> thread_pools_;
  pthread_mutex_t mu_;
  std::vector<pthread_t> threads_to_join_;

  util::PosixLogger* logger_;
  std::atomic<uint32_t> next_id_;

};



}

#endif