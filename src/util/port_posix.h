//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// See port_example.h for documentation for the following types/functions.

#pragma once

#include <thread>

// size_t printf formatting named in the manner of C99 standard formatting
// strings such as PRIu64
// in fact, we could use that one
#define ROCKSDB_PRIszt "zu"

#define __declspec(S)

#define ROCKSDB_NOEXCEPT noexcept

#include <endian.h>

#include <pthread.h>

#include <stdint.h>
#include <string.h>
#include <limits>
#include <string>

namespace util {

extern const bool kDefaultToAdaptiveMutex;

namespace port {

// For use at db/file_indexer.h kLevelMaxIndex
const uint32_t kMaxUint32 = std::numeric_limits<uint32_t>::max();
const int kMaxInt32 = std::numeric_limits<int32_t>::max();
const int kMinInt32 = std::numeric_limits<int32_t>::min();
const uint64_t kMaxUint64 = std::numeric_limits<uint64_t>::max();
const int64_t kMaxInt64 = std::numeric_limits<int64_t>::max();
const int64_t kMinInt64 = std::numeric_limits<int64_t>::min();
const size_t kMaxSizet = std::numeric_limits<size_t>::max();

constexpr bool kLittleEndian = true;



// static inline bool atomic_compare_exchange(int* ptr, int compare, int exchange) {
//     return __atomic_compare_exchange_n(ptr, &compare, exchange,
//             0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
// }

// static inline void atomic_store(int* ptr, int value) {
//     __atomic_store_n(ptr, 0, __ATOMIC_SEQ_CST);
// }

// static inline int atomic_add_fetch(int* ptr, int d) {
//     return __atomic_add_fetch(ptr, d, __ATOMIC_SEQ_CST);
// }


// class SpinLock {
// public:
//   SpinLock() {
//     locked_ = 0;
//   }
//   inline void Lock() {
//       while (!atomic_compare_exchange(&locked_, 0, 1)) {
//       }
//   }
//   inline void Unlock() {
//       atomic_store(&locked_, 0);
//   }

// private:
//   int locked_;
// };

class CondVar;

class Mutex {
 public:
  explicit Mutex(bool adaptive = kDefaultToAdaptiveMutex);
  // No copying
  Mutex(const Mutex&) = delete;
  void operator=(const Mutex&) = delete;

  ~Mutex();

  void Lock();
  void Unlock();
  // this will assert if the mutex is not locked
  // it does NOT verify that mutex is held by a calling thread
  void AssertHeld();

 private:
  friend class CondVar;
  pthread_mutex_t mu_;
#ifndef NDEBUG
  bool locked_;
#endif
};

class MutexLock {
public:
    explicit MutexLock(port::Mutex *mu)
        : mu_(mu)  {
      this->mu_->Lock();
    }
    ~MutexLock() { this->mu_->Unlock(); }

    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;

private:
    port::Mutex *const mu_;
};

class RWMutex {
 public:
  RWMutex();
  // No copying allowed
  RWMutex(const RWMutex&) = delete;
  void operator=(const RWMutex&) = delete;

  ~RWMutex();

  void ReadLock();
  void WriteLock();
  void ReadUnlock();
  void WriteUnlock();
  void AssertHeld() { }

 private:
  pthread_rwlock_t mu_; // the underlying platform mutex
};

class CondVar {
 public:
  explicit CondVar(Mutex* mu);
  ~CondVar();
  void Wait();
  // Timed condition wait.  Returns true if timeout occurred.
  bool TimedWait(uint64_t abs_time_us);
  void Signal();
  void SignalAll();
 private:
  pthread_cond_t cv_;
  Mutex* mu_;
};

using Thread = std::thread;

static inline void AsmVolatilePause() {
#if defined(__i386__) || defined(__x86_64__)
  asm volatile("pause");
#elif defined(__aarch64__)
  asm volatile("wfe");
#elif defined(__powerpc64__)
  asm volatile("or 27,27,27");
#endif
  // it's okay for other platforms to be no-ops
}

// Returns -1 if not available on this platform
extern int PhysicalCoreID();

typedef pthread_once_t OnceType;
#define LEVELDB_ONCE_INIT PTHREAD_ONCE_INIT
extern void InitOnce(OnceType* once, void (*initializer)());

#ifndef CACHE_LINE_SIZE
// To test behavior with non-native cache line size, e.g. for
// Bloom filters, set TEST_CACHE_LINE_SIZE to the desired test size.
// This disables ALIGN_AS to keep it from failing compilation.
#ifdef TEST_CACHE_LINE_SIZE
#define CACHE_LINE_SIZE TEST_CACHE_LINE_SIZE
#define ALIGN_AS(n) /*empty*/
#else
#if defined(__s390__)
#define CACHE_LINE_SIZE 256U
#elif defined(__powerpc__) || defined(__aarch64__)
#define CACHE_LINE_SIZE 128U
#else
#define CACHE_LINE_SIZE 64U
#endif
#define ALIGN_AS(n) alignas(n)
#endif
#endif

static_assert((CACHE_LINE_SIZE & (CACHE_LINE_SIZE - 1)) == 0,
              "Cache line size must be a power of 2 number of bytes");

extern void *cacheline_aligned_alloc(size_t size);

extern void cacheline_aligned_free(void *memblock);

#define PREFETCH(addr, rw, locality) __builtin_prefetch(addr, rw, locality)

extern void Crash(const std::string& srcfile, int srcline);

extern int GetMaxOpenFiles();

extern const size_t kPageSize;



} // namespace port
}  // namespace ROCKSDB_NAMESPACE
