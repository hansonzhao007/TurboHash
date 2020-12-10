#pragma once
#include <cstdint>
#include <atomic>
#include <immintrin.h>
#include <pthread.h>
#include <error.h>
#include <stdio.h>
#include <cstring>
#define likely(x)       (__builtin_expect(false || (x), true))
#define unlikely(x)     (__builtin_expect(x, 0))

// http://www.cs.utexas.edu/~pingali/CS378/2015sp/lectures/Spinlocks%20and%20Read-Write%20Locks.htm
/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")
/* Pause instruction to prevent excess processor bus usage */ 
#define cpu_relax() asm volatile("pause\n": : :"memory")

/** https://stackoverflow.com/questions/30467638/cheapest-least-intrusive-way-to-atomically-update-a-bit
 * \brief Atomically tests and sets a bit (INTEL only)
 * \details Sets bit \p bit of *\p ptr and returns its previous value.
 * The function is atomic and acts as a read-write memory barrier.
 * \param[in] ptr a pointer to an unsigned integer
 * \param[in] bit index of the bit to set in *\p ptr
 * \return the previous value of bit \p bit
 */
inline char turbo_atomic_bittestandset_x86(volatile unsigned int* ptr, unsigned int bit) {
    char out;
#if defined(__x86_64)
    __asm__ __volatile__ (
        "lock; bts %2,%1\n"  // set carry flag if bit %2 (bit) of %1 (ptr) is set
                             //   then set bit %2 of %1
        "sbb %0,%0\n"        // set %0 (out) if carry flag is set
        : "=r" (out), "=m" (*ptr)
        : "Ir" (bit)
        : "memory"
    );
#else
    __asm__ __volatile__ (
        "lock; bts %2,%1\n"  // set carry flag if bit %2 (bit) of %1 (ptr) is set
                             //   then set bit %2 of %1
        "sbb %0,%0\n"        // set %0 (out) if carry flag is set
        : "=q" (out), "=m" (*ptr)
        : "Ir" (bit)
        : "memory"
    );
#endif
    return out;
}
/**
 * \brief Atomically tests and resets a bit (INTEL only)
 * \details Resets bit \p bit of *\p ptr and returns its previous value.
 * The function is atomic and acts as a read-write memory barrier
 * \param[in] ptr a pointer to an unsigned integer
 * \param[in] bit index of the bit to reset in \p ptr
 * \return the previous value of bit \p bit
 */
inline char turbo_atomic_bittestandreset_x86(volatile unsigned int* ptr, unsigned int bit) {
    char out;
#if defined(__x86_64)
    __asm__ __volatile__ (
        "lock; btr %2,%1\n"  // set carry flag if bit %2 (bit) of %1 (ptr) is set
                             //   then reset bit %2 of %1
        "sbb %0,%0\n"        // set %0 (out) if carry flag is set
        : "=r" (out), "=m" (*ptr)
        : "Ir" (bit)
        : "memory"
    );
#else
    __asm__ __volatile__ (
        "lock; btr %2,%1\n"  // set carry flag if bit %2 (bit) of %1 (ptr) is set
                             //   then reset bit %2 of %1
        "sbb %0,%0\n"        // set %0 (out) if carry flag is set
        : "=q" (out), "=m" (*ptr)
        : "Ir" (bit)
        : "memory"
    );
#endif
    return out;
}

#define SPINLOCK_FREE 0
typedef unsigned int turbo_bitspinlock;

inline bool turbo_lockbusy(turbo_bitspinlock *lock, int bit_pos) {
    return (*lock) & (1 << bit_pos);
}

inline void turbo_bit_spin_lock(turbo_bitspinlock *lock, int bit_pos)
{
    
    while(1) {
        // test & set return 0 if success
        if (turbo_atomic_bittestandset_x86(lock, bit_pos) == SPINLOCK_FREE) {
            return;
        }
        while ((*lock) & (1 << bit_pos)) __builtin_ia32_pause();
    }
    
}

inline void turbo_bit_spin_unlock(turbo_bitspinlock *lock, int bit_pos)
{
    barrier();
    *lock &= ~(1 << bit_pos);
}

template<int kBitLockPosition>
class SpinLockScope {
public:
    SpinLockScope(turbo_bitspinlock *lock):
        lock_(lock) {
        turbo_bit_spin_lock(lock, kBitLockPosition);
    }
    ~SpinLockScope() {
        // release the bit lock
        turbo_bit_spin_unlock(lock_, kBitLockPosition);
    }
    turbo_bitspinlock *lock_;
};

typedef struct __spinlock {
union {
    pthread_spinlock_t lock;
    uint64_t padding[8];
};
} SpinLock;

class ShardLock {
public:
    const int kShard = 1 << 10;
    ShardLock() {
        int res = posix_memalign((void**)&locks_, 64, 64 * kShard);
        if (res != 0) {
            perror("ShardLock posix memalign fail");
            exit(1);
        }
        for (int i = 0; i < kShard; i++) {
            pthread_spin_init(&(locks_[i].lock), PTHREAD_PROCESS_SHARED);
        }
    }

    inline void Lock(uint32_t i) {
        const int r = pthread_spin_lock(&(locks_[i & lock_mask_].lock));
        if (unlikely(r != 0)) {
            perror("lock fail");
            exit(1);
        }
    }

    inline void Unlock(uint32_t i) {
        const int r = pthread_spin_unlock(&(locks_[i & lock_mask_].lock));
        if (unlikely(r != 0)) {
            perror("unlock fail");
            exit(1);
        }
    }

private:
    uint32_t lock_mask_ = kShard - 1;
    SpinLock* locks_;
};


class ShardLockScope {
public:
    ShardLockScope(ShardLock* lock, uint32_t x):
        lock_(lock),
        x_(x) {
        lock_->Lock(x_);
    }

    ~ShardLockScope() {
        lock_->Unlock(x_);
    }
private:
    ShardLock* lock_;
    uint32_t x_;
};

class ShardSpinLock {
public:
    ShardSpinLock(int left_shift_ccount) {
        uint32_t count = 1 << left_shift_ccount;
        mask_ = count - 1;
        locks_ = (turbo_bitspinlock*)aligned_alloc(64, count*64);
        memset(locks_, 0, count);
    }
    ~ShardSpinLock() {
        free(locks_);
    }
    inline void Lock(int x) {
        turbo_bit_spin_lock(&locks_[x & mask_], 0);
    }

    inline void Unlock(int x) {
        turbo_bit_spin_unlock(&locks_[x & mask_], 0);
    }
private:
    turbo_bitspinlock* locks_;
    uint32_t mask_;
};

// https://rigtorp.se/spinlock/
class AtomicSpinLock {
public:
  std::atomic<bool> lock_ = {0};

  void lock() noexcept {
    for (;;) {
      // Optimistically assume the lock is free on the first try
      if (!lock_.exchange(true, std::memory_order_acquire)) {
        return;
      }
      // Wait for lock to be released without generating cache misses
      while (lock_.load(std::memory_order_relaxed)) {
        // Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
        // hyper-threads
        __builtin_ia32_pause();
      }
    }
  }

  bool try_lock() noexcept {
    // First do a relaxed load to check if lock is free in order to prevent
    // unnecessary cache misses if someone does while(!try_lock())
    return !lock_.load(std::memory_order_relaxed) &&
           !lock_.exchange(true, std::memory_order_acquire);
  }

  void unlock() noexcept {
    lock_.store(false, std::memory_order_release);
  }
};

