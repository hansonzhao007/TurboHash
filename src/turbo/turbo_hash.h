//     ________  ______  ____  ____     __  _____   _____ __  __ 
//    /_  __/ / / / __ \/ __ )/ __ \   / / / /   | / ___// / / / 
//     / / / / / / /_/ / __  / / / /  / /_/ / /| | \__ \/ /_/ /  
//    / / / /_/ / _, _/ /_/ / /_/ /  / __  / ___ |___/ / __  /   
//   /_/  \____/_/ |_/_____/\____/  /_/ /_/_/  |_/____/_/ /_/    
// 
//  Fast concurrent hashtable for c++11
//  version 1.0.0 
// 
// MIT License
// 
// Copyright (c) 2020 Xingsheng Zhao <xingshengzhao@gmail.com>
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef TURBO_HASH_H_
#define TURBO_HASH_H_

#include <deque>
#include <string>
#include <vector>
#include <unordered_map>

#include <thread>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <type_traits>

#include <error.h>
#include <stdio.h>
#include <pthread.h>
#include <immintrin.h>

#include <sys/mman.h>
#include <sys/time.h>

#include <jemalloc/jemalloc.h>

#define TURBO_LIKELY(x)     (__builtin_expect(false || (x), true))
#define TURBO_UNLIKELY(x)   (__builtin_expect(x, 0))

#define TURBO_BARRIER()     asm volatile("": : :"memory")           /* Compile read-write barrier */
#define TURBO_CPU_RELAX()   asm volatile("pause\n": : :"memory")    /* Pause instruction to prevent excess processor bus usage */ 

#define TURBO_SPINLOCK_FREE ((0))

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000 /* arch specific */
#endif
#define TURBO_HUGEPAGE_SIZE     (2UL*1024*1024)
#define TURBO_HUGE_PROTECTION   (PROT_READ | PROT_WRITE)
#ifdef __ia64__  /* Only ia64 requires this */
#define TURBO_HUGE_ADDR (void *)(0x8000000000000000UL)
#define TURBO_HUGE_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_FIXED)
#else
#define TURBO_HUGE_ADDR (void *)(0x0UL)
#define TURBO_HUGE_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)
#endif

inline void TURBO_COMPILER_FENCE() {
    asm volatile("" : : : "memory"); /* Compiler fence. */
}

// Linear probing setting
static const int kTurboMaxProbeLen = 17;
static const int kTurboProbeStep   = 1;        

namespace turbo {

namespace util {

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

/** BitSet
 *  @note: used for bitmap testing
 *  @example: 
 *  BitSet bitset(0x05); 
 *  for (int i : bitset) {
 *      printf("i: %d\n", i);
 *  }
 *  This will print out 0, 2 for little endian
*/
class BitSet {
public:
    BitSet():
        bits_(0) {}

    explicit BitSet(uint32_t bits): bits_(bits) {}

    BitSet(const BitSet& b) {
        bits_ = b.bits_;
    }
    
    inline BitSet& operator++() {
        // remove the lowest 1-bit
        bits_ &= (bits_ - 1);
        return *this;
    }

    inline explicit operator bool() const { return bits_ != 0; }

    inline int operator*() const { 
        // count the tailing zero bit
        return __builtin_ctz(bits_); 
    }

    inline BitSet begin() const { return *this; }

    inline BitSet end() const { return BitSet(0); }

    inline uint32_t bit() {
        return bits_;
    }

private:
    friend bool operator==(const BitSet& a, const BitSet& b) {
        return a.bits_ == b.bits_;
    }
    friend bool operator!=(const BitSet& a, const BitSet& b) {
        return a.bits_ != b.bits_;
    }
    uint32_t bits_;
}; // end of class BitSet

/** Slice
 *  @note: Derived from LevelDB. the data is stored in the *data_
*/
class Slice {
public:
    // operator <
    bool operator < (const Slice& b) const {
        return compare(b) < 0 ;
    }

    bool operator > (const Slice& b) const {
        return compare(b) > 0 ;
    }
    // Create an empty slice.
    Slice() : data_(""), size_(0) { }

    // Create a slice that refers to d[0,n-1].
    Slice(const char* d, size_t n) : data_(d), size_(n) { }

    // Create a slice that refers to the contents of "s"
    Slice(const std::string& s) : data_(s.data()), size_(s.size()) { }

    // Create a slice that refers to s[0,strlen(s)-1]
    Slice(const char* s) : 
        data_(s), 
        size_((s == nullptr) ? 0 : strlen(s)) {
    }

    // Return a pointer to the beginning of the referenced data
    inline const char* data() const { return data_; }

    // Return the length (in bytes) of the referenced data
    inline size_t size() const { return size_; }

    // Return true iff the length of the referenced data is zero
    inline bool empty() const { return size_ == 0; }

    // Return the ith byte in the referenced data.
    // REQUIRES: n < size()
    inline char operator[](size_t n) const {
        assert(n < size());
        return data_[n];
    }

    // Change this slice to refer to an empty array
    inline void clear() { data_ = ""; size_ = 0; }

    inline std::string ToString() const {
        std::string res;
        res.assign(data_, size_);
        return res;
    }

    // Three-way comparison.  Returns value:
    //   <  0 iff "*this" <  "b",
    //   == 0 iff "*this" == "b",
    //   >  0 iff "*this" >  "b"
    inline int compare(const Slice& b) const {
        assert(data_ != nullptr && b.data_ != nullptr);
        const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
        int r = memcmp(data_, b.data_, min_len);
        if (r == 0) {
            if (size_ < b.size_) r = -1;
            else if (size_ > b.size_) r = +1;
        }
        return r;
    }
    const char* data_;
    size_t size_;
}; // end of class Slice

inline bool operator==(const Slice& x, const Slice& y) {
    return ((x.size() == y.size()) &&
            (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const Slice& x, const Slice& y) {
    return !(x == y);
}


/** Hasher
 *  @note: provide hash function for string
*/
class Hasher {
public:

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    using uint128_t = unsigned __int128;
#pragma GCC diagnostic pop
#endif

    static inline uint64_t umul128(uint64_t a, uint64_t b, uint64_t* high) noexcept {
        auto result = static_cast<uint128_t>(a) * static_cast<uint128_t>(b);
        *high = static_cast<uint64_t>(result >> 64U);
        return static_cast<uint64_t>(result);
    }

    static inline size_t hash ( const void * key, int len)
    {
        return ((MurmurHash64A(key, len)));
    }

    static inline size_t hash_int(uint64_t obj) noexcept {
        // 167079903232 masksum, 120428523 ops best: 0xde5fb9d2630458e9
        static constexpr uint64_t k = UINT64_C(0xde5fb9d2630458e9);
        uint64_t h;
        uint64_t l = umul128(obj, k, &h);
        return h + l;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
    static inline uint64_t MurmurHash64A ( const void * key, int len)
    {
        const uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
        const uint64_t seed = UINT64_C(0xe17a1465);
        const int r = 47;

        uint64_t h = seed ^ (len * m);

        const uint64_t * data = (const uint64_t *)key;
        const uint64_t * end = data + (len/8);

        while(data != end)
        {
            uint64_t k = *data++;

            k *= m;
            k ^= k >> r;
            k *= m;

            h ^= k;
            h *= m;
        }

        const unsigned char * data2 = (const unsigned char*)data;

        switch(len & 7)
        {
        case 7: h ^= ((uint64_t)data2[6]) << 48;
        case 6: h ^= ((uint64_t)data2[5]) << 40;
        case 5: h ^= ((uint64_t)data2[4]) << 32;
        case 4: h ^= ((uint64_t)data2[3]) << 24;
        case 3: h ^= ((uint64_t)data2[2]) << 16;
        case 2: h ^= ((uint64_t)data2[1]) << 8; 
        case 1: h ^= ((uint64_t)data2[0]);
            h *= m;
        };

        h ^= h >> r;
        h *= m;
        h ^= h >> r;

        return h;
    }
#pragma GCC diagnostic pop

}; // end fo class Hasher

/** AtomicBitOps
 *  @note: provide atomic bit test-and-set, test-and-reset
*/
class AtomicBitOps {
public:

    /** https://stackoverflow.com/questions/30467638/cheapest-least-intrusive-way-to-atomically-update-a-bit
     * \brief Atomically tests and sets a bit (INTEL only)
     * \details Sets bit \p bit of *\p ptr and returns its previous value.
     * The function is atomic and acts as a read-write memory barrier.
     * \param[in] ptr a pointer to an unsigned integer
     * \param[in] bit index of the bit to set in *\p ptr
     * \return the previous value of bit \p bit
     */
    static inline char BitTestAndSet(volatile unsigned int* ptr, unsigned int bit) {
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
    static inline char BitTestAndReset(volatile unsigned int* ptr, unsigned int bit) {
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

}; // end of class AtomicBitOps


static inline bool turbo_lockbusy(uint32_t *lock, int bit_pos) {
    return (*lock) & (1 << bit_pos);
}

static inline void turbo_bit_spin_lock(uint32_t *lock, int bit_pos)
{
    while(1) {
        // test & set return 0 if success
        if (AtomicBitOps::BitTestAndSet(lock, bit_pos) == TURBO_SPINLOCK_FREE) {
            return;
        }
        while (turbo_lockbusy(lock, bit_pos)) __builtin_ia32_pause();
    }
}

static inline void turbo_bit_spin_unlock(uint32_t *lock, int bit_pos)
{
    TURBO_BARRIER();
    *lock &= ~(1 << bit_pos);
}
/** SpinLockScope
 *  @note: a spinlock monitor, lock when initialized, unlock then deconstructed.
*/
template<int kBitLockPosition>
class SpinLockScope {
public:
    SpinLockScope(uint32_t *lock):
        lock_(lock) {
        // lock the bit lock
        turbo_bit_spin_lock(lock, kBitLockPosition);
    }
    ~SpinLockScope() {
        // release the bit lock
        turbo_bit_spin_unlock(lock_, kBitLockPosition);
    }
private:
    uint32_t *lock_;
}; // end of class SpinLockScope


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
}; // end of class AtomicSpinLock

}; // end of namespace turbo::util


// A thin wrapper around std::hash, performing an additional simple mixing step of the result.
// from https://github.com/martinus/robin-hood-hashing
template <typename T>
struct hash : public std::hash<T> {
    size_t operator()(T const& obj) const
        noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<T const&>()))) {
        // call base hash
        auto result = std::hash<T>::operator()(obj);
        // return mixed of that, to be save against identity has
        return util::Hasher::hash_int(static_cast<uint64_t>(result));
    }
};
template <>
struct hash<std::string> {
    size_t operator()(std::string const& str) const noexcept {
        return util::Hasher::hash(str.data(), str.size());
    }
};
template <class T>
struct hash<T*> {
    size_t operator()(T* ptr) const noexcept {
        return util::Hasher::hash_int(reinterpret_cast<size_t>(ptr));
    }
};
#define TURBO_HASH_INT(T)                                               \
    template <>                                                         \
    struct hash<T> {                                                    \
        size_t operator()(T obj) const noexcept {                       \
            return util::Hasher::hash_int(static_cast<uint64_t>(obj));  \
        }                                                               \
    }

#if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
// see https://en.cppreference.com/w/cpp/utility/hash
TURBO_HASH_INT(bool);
TURBO_HASH_INT(char);
TURBO_HASH_INT(signed char);
TURBO_HASH_INT(unsigned char);
TURBO_HASH_INT(char16_t);
TURBO_HASH_INT(char32_t);
TURBO_HASH_INT(wchar_t);
TURBO_HASH_INT(short);
TURBO_HASH_INT(unsigned short);
TURBO_HASH_INT(int);
TURBO_HASH_INT(unsigned int);
TURBO_HASH_INT(long);
TURBO_HASH_INT(long long);
TURBO_HASH_INT(unsigned long);
TURBO_HASH_INT(unsigned long long);
#if defined(__GNUC__) && !defined(__clang__)
#    pragma GCC diagnostic pop
#endif

namespace detail {

/** CellMeta128
 *  @note:  Hash cell whose size is 128 byte. There are 14 slots in the cell.
 *  @format:
 *  | ----------------------- meta ------------------------| ----- slots ---- |
 *  | 2 byte bitmap | 14 byte: one byte hash for each slot | 8 byte * 14 slot |
 * 
 *  |- bitmap: 
 *            0 bit: used as a bitlock
 *            1 bit: not in use
 *      2  - 15 bit: indicate which slot is empty, 0: empty or deleted, 1: occupied
 * 
 *  |- one byte hash:
 *      8 bit hash for the slot
 * 
 *  |- slot:
 *      0  -  5 byte: the pointer used to point to DIMM where store the actual kv value
 *      6  -  7 byte: two hash byte for this slot
*/ 
class CellMeta128 {
public:
    static const uint16_t BitMapMask    = 0xFFFC;
    static const int CellSizeLeftShift  = 7;

    explicit CellMeta128(char* rep) {
        meta_   = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rep));
        bitmap_ = *(uint32_t*)(rep);    // the lowest 32bit is used as bitmap
        bitmap_ &= BitMapMask;          // hide the 0, 1 bit in bitmap
    }

    ~CellMeta128() {
    }

    // return a bitset, the position that matches the hash is set to 1
    inline util::BitSet MatchBitSet(uint8_t hash) {
        auto bitset = _mm_set1_epi8(hash);
        uint16_t mask = _mm_cmpeq_epi8_mask(bitset, meta_);
        return util::BitSet(mask & bitmap_);        
    }

    // return a bitset, the position that is empty for insertion
    inline util::BitSet EmptyBitSet() {
        return turbo::util::BitSet((~bitmap_) & BitMapMask);
    }

    inline util::BitSet OccupyBitSet() {
        return util::BitSet(bitmap_ & BitMapMask);
    }

    inline bool Full() {
        return (bitmap_ & BitMapMask) == BitMapMask;
    }

    inline bool Occupy(int slot_index) {
        return bitmap_ & (1 << slot_index);
    }
    
    inline int OccupyCount() {
        return __builtin_popcount(bitmap_);
    }

    inline static uint8_t StartSlotPos() {
        return 2;
    }

    inline static uint32_t CellSize() {
        // cell size (include meta) in byte
        return 128;
    }

    inline static uint32_t SlotMaxRange() {
        return 16;
    }

    inline static uint32_t SlotSize() {
        // slot count
        return 14;
    }

    inline static uint16_t BitMapType() {
        return 0;
    }

    inline static size_t size() {
        // the meta size in byte in current cell
        return 16;
    }

    inline uint16_t bitmap() {
        return bitmap_;
    }

    std::string BitMapToString() {
        std::string res;
        char buffer[1024];
        uint64_t H2s[2];
        memcpy(H2s, &meta_, 16);
        sprintf(buffer, "bitmap: 0b%s - H2: 0x%016lx%016lx", print_binary(bitmap_).c_str(), H2s[1], H2s[0]);
        return buffer;
    }

    std::string print_binary(uint16_t bitmap)
    {
        char buffer[1024];
        static std::string bit_rep[16] = {
            "0000", "0001", "0010", "0011",
            "0100", "0101", "0110", "0111",
            "1000", "1001", "1010", "1011",
            "1100", "1101", "1110", "1111"
        };
        sprintf(buffer, "%s%s%s%s", 
            bit_rep[(bitmap >> 12) & 0x0F].c_str(),
            bit_rep[(bitmap >>  8) & 0x0F].c_str(),
            bit_rep[(bitmap >>  4) & 0x0F].c_str(),
            bit_rep[(bitmap >>  0) & 0x0F].c_str()
        );
        return buffer;
    }


private:
    __m128i     meta_;          // 16 byte integer vector
    uint16_t    bitmap_;        // 1: occupied, 0: empty or deleted
}; // end of class CellMeta128

/** CellMeta256
 *  @note: Hash cell whose size is 256 byte. There are 28 slots in the cell.
 *  @format:
 *  | ----------------------- meta ------------------------| ----- slots ---- |
 *  | 4 byte bitmap | 28 byte: one byte hash for each slot | 8 byte * 28 slot |
 * 
 *  |- bitmap: 
 *           0  bit: used as a bitlock
 *      1  - 3  bit: not in use
 *      4  - 31 bit: indicate which slot is empty, 0: empty or deleted, 1: occupied
 * 
 *  |- one byte hash:
 *      8 bit hash for the slot
 * 
 *  |- slot:
 *      0  -  5 byte: the pointer used to point to DIMM where store the actual kv value
 *      6  -  7 byte: two hash byte for this slot (totally 3 byte is used as the hash)
*/ 
class CellMeta256 {
public:
    static const uint32_t BitMapMask = 0x0FFFFFFF0;
    static const int CellSizeLeftShift = 8;

    explicit CellMeta256(char* rep) {
        meta_   = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rep));
        bitmap_ = *(uint32_t*)(rep);              // the lowest 32bit is used as bitmap
        bitmap_ &= BitMapMask;                   // hidden the 0 - 3 bit in bitmap
    }

    ~CellMeta256() {
    }

    // return a bitset, the slot that matches the hash is set to 1
    inline util::BitSet MatchBitSet(uint8_t hash) {
        auto bitset = _mm256_set1_epi8(hash);
        uint32_t mask = _mm256_cmpeq_epi8_mask(bitset, meta_);
        return util::BitSet(mask & bitmap_);
    }

    // return a bitset, indicating the availabe slot
    inline util::BitSet EmptyBitSet() {
        return util::BitSet((~bitmap_) & BitMapMask);
    }

    inline util::BitSet OccupyBitSet() {
        return util::BitSet(bitmap_ & BitMapMask);
    }

    inline bool Full() {
        return (bitmap_ & BitMapMask) == BitMapMask;
    }

    inline bool Occupy(int slot_index) {
        return bitmap_ & (1 << slot_index);
    }

    inline int OccupyCount() {
        return __builtin_popcount(bitmap_);
    }

    inline static uint8_t StartSlotPos() {
        return 4;
    }

    inline static uint32_t CellSize() {
        // cell size (include meta) in byte
        return 256;
    }

    inline static uint32_t SlotMaxRange() {
        return 32;
    }
    
    inline static uint32_t SlotSize() {
        // slot count
        return 28;
    }

    inline static uint32_t BitMapType() {
        return 0;
    }

    inline static size_t size() {
        // the meta size in byte in current cell
        return 32;
    }

    inline uint32_t bitmap() {
        return bitmap_;
    }

    std::string BitMapToString() {
        std::string res;
        char buffer[1024];
        uint64_t H2s[4];
        memcpy(H2s, &meta_, 32);
        sprintf(buffer, "bitmap: 0b%s - H2: 0x%016lx%016lx%016lx%016lx", print_binary(bitmap_).c_str(), H2s[3], H2s[2], H2s[1], H2s[0]);
        return buffer;
    }

    std::string print_binary(uint32_t bitmap)
    {
        char buffer[1024];
        static std::string bit_rep[16] = {
            "0000", "0001", "0010", "0011",
            "0100", "0101", "0110", "0111",
            "1000", "1001", "1010", "1011",
            "1100", "1101", "1110", "1111"
        };
        sprintf(buffer, "%s%s%s%s%s%s%s%s", 
            bit_rep[(bitmap >> 28) & 0x0F].c_str(),
            bit_rep[(bitmap >> 24) & 0x0F].c_str(),
            bit_rep[(bitmap >> 20) & 0x0F].c_str(),
            bit_rep[(bitmap >> 16) & 0x0F].c_str(),
            bit_rep[(bitmap >> 12) & 0x0F].c_str(),
            bit_rep[(bitmap >>  8) & 0x0F].c_str(),
            bit_rep[(bitmap >>  4) & 0x0F].c_str(),
            bit_rep[(bitmap >>  0) & 0x0F].c_str()
        );
        return buffer;
    }
    
private:
    __m256i     meta_;          // 32 byte integer vector
    uint32_t    bitmap_;        // 1: occupied, 0: empty or deleted
}; // end of class CellMeta256

/** MemBlock
 *  @note: a memory block that can allocate at most 'count' Cell (128-byte Cell or 256-byte Cell)
*/
template <typename CellMeta = CellMeta128>
class MemBlock { 
public:
    MemBlock(int block_id, size_t count):
        start_addr_(nullptr),
        cur_addr_(nullptr),
        ref_(0),
        size_(count),
        remaining_(count),
        is_hugepage_(true),
        block_id_(block_id) {
        // Allocate memory space for this MemBlock
        size_t space = size_ * CellMeta::CellSize();
        if ((space & 0xfff) != 0) {
            // space is not several times of 4KB
            printf("MemBlock size is not 4KB aligned. %lu", space);
            exit(1);
        }
        // printf("Add %.2f MB MemBlock\n", space/1024.0/1024.0 );
        start_addr_ = (char*) mmap(TURBO_HUGE_ADDR, space, TURBO_HUGE_PROTECTION, TURBO_HUGE_FLAGS, -1, 0);
        if (start_addr_ == MAP_FAILED) {
            fprintf(stderr, "mmap %lu hugepage fail.\n", space);
            is_hugepage_ = false;
            start_addr_ = (char* ) aligned_alloc(CellMeta::CellSize(), space);
            if (start_addr_ == nullptr) {
                fprintf(stderr, "malloc %lu space fail.\n", space);
                exit(1);
            }
        }
        cur_addr_ = start_addr_;
    }

    ~MemBlock() {
        /* munmap() size_ of MAP_HUGETLB memory must be hugepage aligned */
        size_t space = size_ * CellMeta::CellSize();
        if (munmap(start_addr_, space)) {
            fprintf(stderr, "munmap %lu hugepage fail.\n", space);
            exit(1);
        }
    }
    
    inline size_t Remaining() {
        return remaining_;
    }

    inline char* Allocate(size_t request_count) {
        // Allocate 'request_count' cells 
        if (request_count > remaining_) {
            // There isn't enough space for allocation
            return nullptr;
        }

        // allocate count space.
        ++ref_;
        remaining_ -= request_count; 
        char* return_addr = cur_addr_;
        cur_addr_ += (request_count * CellMeta::CellSize());
        
        return return_addr;
    }

    inline bool Release() {
        // reduce reference counter
        // return true if reference is 0, meaning this MemBlock can be added to free list.
         --ref_;
        if (ref_ == 0) {
            remaining_ = size_;
            cur_addr_ = start_addr_;
            return true;
        }
        else {
            return false;
        }
    }

    inline int Reference() {
        return ref_;
    }

    inline bool IsHugePage() {
        return is_hugepage_;
    }

    inline int ID() {
        return block_id_;
    }

    std::string ToString() {
        char buffer[1024];
        sprintf(buffer, "ID: %d, size: %lu, remain: %lu, ref: %d\n", block_id_, size_, remaining_, ref_);
        return buffer;
    }
private:
    char*   start_addr_;    // start address of MemBlock
    char*   cur_addr_;      // current position for space allocation
    int     ref_;           // reference of this MemBlock
    size_t  size_;          // number of cells
    size_t  remaining_;     // remaining cells for allocation
    bool    is_hugepage_;   // This MemBlock uses hugepage or not
    int     block_id_;          
};// end of class MemBlock

/** CellAllocator
 *  @note: used to allocate N Cell space
*/
template <typename CellMeta = CellMeta128, size_t kBucketCellCount = 32768>
class CellAllocator {
public:
    CellAllocator(int initial_blocks = 1):
        cur_mem_block_(nullptr),
        next_id_(0),
        spin_lock_(0) {
        AddMemBlock(initial_blocks);
        cur_mem_block_ = GetMemBlock();
    }

    // Allocate memory space for a Bucket with 'count' cells
    // Return:
    // int: the MemBlock ID
    // char*: address in MemBlock
    inline std::pair<int, char*> AllocateNoSafe(size_t count) {
        if (cur_mem_block_->Remaining() < count) {
            // if remaining space is not enough, allocate a new MemBlock
            cur_mem_block_ = GetMemBlock();
        }
        char* addr = cur_mem_block_->Allocate(count);
        if (addr == nullptr) {
            fprintf(stderr, "CellAllocator::Allocate addr is nullptr\n");
            exit(1);
        }
        
        return {cur_mem_block_->ID(), addr};
    }

    inline std::pair<int, char*> AllocateSafe(size_t count) {
        util::SpinLockScope<0> lock(&spin_lock_);
        if (cur_mem_block_->Remaining() < count) {
            // if remaining space is not enough, allocate a new MemBlock
            cur_mem_block_ = GetMemBlock();
        }
        char* addr = cur_mem_block_->Allocate(count);
        if (addr == nullptr) {
            fprintf(stderr, "CellAllocator::Allocate addr is nullptr\n");
            exit(1);
        }
        
        return {cur_mem_block_->ID(), addr};
    }

    inline void ReleaseNoSafe(int id) {
        auto iter = mem_block_map_.find(id);
        if (iter != mem_block_map_.end()) {
            bool should_recycle = iter->second->Release();
            if (should_recycle) {
                RecycleMemBlock(iter->second);
            }
        }
    }

    inline void ReleaseSafe(int id) {
        util::SpinLockScope<0> lock(&spin_lock_);
        auto iter = mem_block_map_.find(id);
        if (iter != mem_block_map_.end()) {
            bool should_recycle = iter->second->Release();
            if (should_recycle) {
                RecycleMemBlock(iter->second);
            }
        }
    }

    std::string ToString() {
        std::string res;
        for (auto& mb : mem_block_map_) {
            res.append(mb.second->ToString());
        }
        return res;
    }

private:
    inline void AddMemBlock(int n) {
        // add n MemBlock to free_mem_block_list_
        for (int i = 0; i < n; i++) {
            MemBlock<CellMeta>* mem_block = new MemBlock<CellMeta>(next_id_++, kBucketCellCount);
            free_mem_block_list_.push_back(mem_block);
            mem_block_map_[mem_block->ID()] = mem_block;
        }
    }

    inline MemBlock<CellMeta>* GetMemBlock() {
        if (free_mem_block_list_.empty()) {
            AddMemBlock(1);
        }
        auto res = free_mem_block_list_.front();
        free_mem_block_list_.pop_front();
        return res;
    }

    inline void RecycleMemBlock(MemBlock<CellMeta>* mem_block) {
        assert(mem_block->Reference() == 0);
        free_mem_block_list_.push_back(mem_block);
    }

    std::deque<MemBlock<CellMeta>*> free_mem_block_list_;
    std::unordered_map<int, MemBlock<CellMeta>*> mem_block_map_;
    MemBlock<CellMeta>* cur_mem_block_;
    int         next_id_;
    uint32_t    spin_lock_;
}; // end of class CellAllocator

/** HashSlot
 *  @node: highest 2 byte is used as hash-tag to reduce unnecessary touch key-value
*/
struct HashSlot{
    uint64_t entry:48;      // pointer to key-value record
    uint64_t H1:16;         // hash-tag for the key
};

/** PartialHash
 *  @note: a 64-bit hash is used to locate the cell location, and provide hash-tag.
 *  @format:
 *  | MSB    - - - - - - - - - - - - - - - - - - LSB |
 *  |     32 bit     |   16 bit  |  8 bit  |  8 bit  |
 *  |    bucket_hash |     H1    |         |    H2   |
*/
struct PartialHash {
    PartialHash(uint64_t hash) :
        bucket_hash_( hash >> 32 ),
        H1_( ( hash >> 16 ) & 0xFFFF ),
        H2_( hash & 0xFF )
        { };

    // used to locate in the bucket directory
    uint32_t  bucket_hash_;

    // H1: 2 byte hash tag 
    uint16_t H1_;

    // H2: 1 byte hash tag in CellMeta for parallel comparison using SIMD cmd
    uint8_t  H2_;

}; // end of class PartialHash

/** SlotInfo
 *  @note: use to store the target slot location info
*/
class SlotInfo {
public:
    uint32_t bucket;        // bucket index
    uint16_t cell;          // cell index
    uint8_t  slot;          // slot index
    uint8_t  H2;            // hash-tag in CellMeta
    uint16_t H1;            // hash-tag in HashSlot
    bool equal_key;         // If we find a equal key in this slot
    SlotInfo(uint32_t b, uint32_t a, int s, uint16_t h1, uint8_t h2, bool euqal):
        bucket(b),
        cell(a),
        slot(s),        
        H2(h2),
        H1(h1),
        equal_key(euqal) {}
    SlotInfo():
        bucket(0),
        cell(0),
        slot(0),        
        H2(0),
        H1(0),
        equal_key(false) {}
    std::string ToString() {
        char buffer[128];
        sprintf(buffer, "b: %4u, c: %4u, s: %2u, H2: 0x%02x, H1: 0x%04x",
            bucket,
            cell,
            slot,
            H2,
            H1);
        return buffer;
    }
}; // end of class SlotInfo

/** BucketMeta
 *  @note: a 8-byte 
*/
class BucketMeta {
public:
    explicit BucketMeta(char* addr, uint16_t cell_count) {
        data_ = (((uint64_t) addr) << 16) | ((cell_count - 1) << 1);
    }

    BucketMeta():
        data_(0) {}
    
    BucketMeta(const BucketMeta& a) {
        data_ = a.data_;
    }

    inline char* Address() {
        return (char*)(data_ >> 16);
    }

    inline uint16_t CellCountMask() {
        // extract bit [1, 15]
        return ((data_ & 0xFFFF) >> 1);
    }
    inline uint16_t CellCount() {
        return  (1 << __builtin_popcount(data_ & 0xFFFE));
    }

    inline void SetAddress(char* addr) {
        data_ = (data_ & 0xFFFF) | (((uint64_t)addr) << 16);
    }

    inline void SetCellCount(uint16_t size) {
        data_ = (data_ & 0xFFFFFFFFFFFF0001) | ((size - 1) << 1);
    }

    inline void Reset(char* addr, uint16_t cell_count) {
        data_ = (((uint64_t) addr) << 16) | ((cell_count - 1) << 1);
    }

    // lowest -> highest
    // | 1 bit lock | 15 bit cell mask | 48 bit address |
    uint64_t data_;
};

/** ProbeWithinCell
 *  @note: probe within each cell
*/
class ProbeWithinCell {
public:
    static const int MAX_PROBE_LEN  = kTurboMaxProbeLen;
    static const int PROBE_STEP     = kTurboProbeStep;

    ProbeWithinCell(uint64_t initial_hash, uint32_t cell_count_mask, uint32_t bucket_i) {
        h_               = initial_hash;
        cell_count_mask_  = cell_count_mask;
        cell_index_ = h_ & cell_count_mask_;
        bucket_i_        = bucket_i;
        probe_count_     = 0;
    }

    inline void reset() {
        cell_index_ = h_ & cell_count_mask_;
        probe_count_     = 0;
    }
    // indicate whether we have already probed all the assocaite cells
    inline operator bool() const {
        return probe_count_ < 1;
    }

    inline void next() {
        cell_index_ += PROBE_STEP;
        // CellCountMask should be like 0b11
        cell_index_ &= cell_count_mask_;
        probe_count_++;
    }

    inline std::pair<uint32_t, uint32_t> offset() {
        return {bucket_i_, cell_index_};
    }

    static std::string name() {
        return "ProbeWithinCell";
    }
private:
    uint64_t  h_;
    uint32_t  cell_count_mask_;
    uint32_t  cell_index_;
    uint32_t  bucket_i_;
    uint32_t  probe_count_;
}; // end of class ProbeWithinCell

/** ProbeWithinBucket
 *  @note: probe within a bucket
*/
class ProbeWithinBucket {
public:
    static const int MAX_PROBE_LEN  = kTurboMaxProbeLen;
    static const int PROBE_STEP     = kTurboProbeStep;
    ProbeWithinBucket(uint64_t initial_hash, uint32_t cell_count_mask, uint32_t bucket_i) {
        h_               = initial_hash;
        cell_count_mask_  = cell_count_mask;
        cell_index_ = h_ & cell_count_mask_;
        bucket_i_        = bucket_i;
        probe_count_     = 0;
    }

    inline void reset() {
        cell_index_ = h_ & cell_count_mask_;
        probe_count_     = 0;
    }
    
    // indicate whether we have already probed all the assocaite cells
    inline operator bool() const {
        return probe_count_ <= cell_count_mask_;
    }

    inline void next() {
        cell_index_ += PROBE_STEP;
        // CellCountMask should be like 0b11
        cell_index_ &= cell_count_mask_;
        probe_count_++;
    }

    inline std::pair<uint32_t, uint32_t> offset() {
        return {bucket_i_, cell_index_};
    }

    static std::string name() {
        return "ProbeWithinBucket";
    }
private:
    uint64_t  h_;
    uint32_t  cell_count_mask_;
    uint32_t  cell_index_;
    uint32_t  bucket_i_;
    uint32_t  probe_count_;
};

/** ValueType
 *  @note: for key-value record type
*/
enum ValueType: unsigned char {
    kTypeDeletion = 0x2A,   // 0b 0_0101010
    kTypeValue    = 0xAA,   // 0b 1_0101010
    kTypeMask     = 0x7F    // 0b 0_1111111
};

/** Record
 *  @note: key-value record
*/
struct Record {
    ValueType type;
    util::Slice key;
    util::Slice value;
};

/** DramMedia
 *  Dram Record Format: 
 *  | ValueType | key size | key | value size |  value  |
 *  |    1B     |   4B     | ... |    4B      |   ...
*/
class DramMedia {
public:

    static inline void* Store(ValueType type, const util::Slice& key, const util::Slice& value) {
        size_t key_len = key.size();
        size_t value_len = value.size();
        size_t encode_len = key_len + value_len;
        if (type == kTypeValue) {
            // has both key and value
            encode_len += 9;
        } else if (type == kTypeDeletion) {
            encode_len += 5;
        }

        char* buffer = (char*)malloc(encode_len);
        
        // store value type
        memcpy(buffer, &type, 1);
        // store key len
        memcpy(buffer + 1, &key_len, 4);
        // store key
        memcpy(buffer + 5, key.data(), key_len);

        if (type == kTypeDeletion) {
            return buffer;
        }

         // store value len
        memcpy(buffer + 5 + key_len, &value_len, 4);
        // store value
        memcpy(buffer + 9 + key_len, value.data(), value_len);
        return buffer;
    }


    static inline util::Slice ParseKey(const void* _addr) {
        char* addr = (char*) _addr;
        uint32_t key_len = 0;
        memcpy(&key_len, addr + 1, 4);
        return util::Slice(addr + 5, key_len);
    }


    static inline Record ParseData(uint64_t offset) {
        char* addr = (char*) offset;
        ValueType type = kTypeValue;
        uint32_t key_len = 0;
        uint32_t value_len = 0;
        memcpy(&type, addr, 1);
        memcpy(&key_len, addr + 1, 4);
        if (type == kTypeValue) {
            memcpy(&value_len, addr + 5 + key_len, 4);
            return {
                type, 
                util::Slice(addr + 5, key_len), 
                util::Slice(addr + 9 + key_len, value_len)};
        } else if (type == kTypeDeletion) {
            return {
                type, 
                util::Slice(addr + 5, key_len), 
                "" };
        } else {
            printf("Prase type incorrect: %d\n", type);
            exit(1);
        }
    }

};


/** Usage: iterator every slot in the bucket, return the pointer in the slot
 *  BucketIterator<CellMeta> iter(bucket_addr, cell_count_);
 *  while (iter.valid()) {
 *      ...
 *      ++iter;
 *  }
*/        
template<typename CellMeta>
class BucketIterator {
public:
typedef std::pair<SlotInfo, HashSlot> value_type;
    BucketIterator(uint32_t bi, char* bucket_addr, size_t cell_count, size_t cell_i = 0):
        bi_(bi),
        cell_count_(cell_count),
        cell_i_(cell_i),
        bitmap_(0),
        bucket_addr_(bucket_addr) {

        assert(bucket_addr != 0);
        CellMeta meta(bucket_addr);
        bitmap_ = meta.OccupyBitSet();
        if(!bitmap_) toNextValidBitMap();
        // printf("Initial Bucket iter at ai: %u, si: %u\n", cell_i_, *bitmap_);
    }

    explicit operator bool() const {
        return (cell_i_ < cell_count_) ||
               (cell_count_ == cell_count_ && bitmap_);
    }

    // ++iterator
    inline BucketIterator& operator++() {
        ++bitmap_;
        if (!bitmap_) {
            toNextValidBitMap();
        }
        return *this;
    }

    inline value_type operator*() const {
        // return the cell index, slot index and its slot content
        uint8_t slot_index = *bitmap_;
        char* cell_addr = bucket_addr_ + cell_i_ * CellMeta::CellSize();
        HashSlot* slot = (HashSlot*)(cell_addr + (slot_index << 3));
        uint8_t H2 = *(uint8_t*)(cell_addr + slot_index);
        return  { {bi_ /* ignore bucket index */, cell_i_ /* cell index */, *bitmap_ /* slot index*/, (uint16_t)slot->H1, H2, false}, 
                *slot};
    }

    inline bool valid() {
        return cell_i_ < cell_count_ && (bitmap_ ? true : false);
    }

    std::string ToString() {
        char buffer[128];
        sprintf(buffer, "cell: %8d, slot: %2d", cell_i_, *bitmap_);
        return buffer;
    }

private:

    inline void toNextValidBitMap() {
        while(!bitmap_ && cell_i_ < cell_count_) {
            cell_i_++;
            if (cell_i_ == cell_count_) return;
            char* cell_addr = bucket_addr_ + ( cell_i_ << CellMeta::CellSizeLeftShift );
            bitmap_ = util::BitSet(*(uint32_t*)(cell_addr) & CellMeta::BitMapMask); 
        }
    }

    friend bool operator==(const BucketIterator& a, const BucketIterator& b) {
        return  a.cell_i_ == b.cell_i_ &&
                a.bitmap_ == b.bitmap_;
                
    }

    friend bool operator!=(const BucketIterator& a, const BucketIterator& b) {
        return  a.cell_i_ != b.cell_i_ ||
                a.bitmap_ != b.bitmap_;
    }

    uint32_t        bi_;
    uint32_t        cell_count_;
    uint32_t        cell_i_; 
    util::BitSet    bitmap_;
    char*           bucket_addr_;
};

// using wrapper classes for hash and key_equal prevents the diamond problem when the same type is
// used. see https://stackoverflow.com/a/28771920/48181
// from robinhood hash. https://github.com/martinus/robin-hood-hashing
template <typename T>
struct WrapHash : public T {
    WrapHash() = default;
    explicit WrapHash(T const& o) noexcept(noexcept(T(std::declval<T const&>())))
        : T(o) {}
};
template <typename T>
struct WrapKeyEqual : public T {
    WrapKeyEqual() = default;
    explicit WrapKeyEqual(T const& o) noexcept(noexcept(T(std::declval<T const&>())))
        : T(o) {}
};

/** TurboHashTable
 *  @format:
 *           | bucket 0 | bucket 1 | ... | bucket n |
 *           |  cell 0  |  cell 0  |     |          |
 *           |  cell 1  |  cell 1  |     |          |
 *           |  cell 2  |  cell 2  |     |          |
 *           |    ...   |    ...   |     |          |
 * 
 *  Cell:
 *  |    meta   |       slots        |
 *  meta: store the meta info, such as which slot is empty
 *  slots: each slot is a 8-byte value, that stores the pointer to kv location.
 *         the higher 2-byte is used as partial hash
 *  Description:
 *  TurboHashTable has bucket_count * cell_count cells
*/
template <  typename Key, typename T, typename Hash, typename KeyEqual,
            typename CellMeta = CellMeta128, typename ProbeStrategy = ProbeWithinBucket, typename Media = DramMedia, int kCellCountLimit = 32768>
class TurboHashTable 
    : public WrapHash<Hash>, 
      public WrapKeyEqual<KeyEqual> {
public:
    static constexpr bool is_map = !std::is_void<T>::value;
    static constexpr bool is_set = !is_map;

    using key_type = Key;
    using mapped_type = T;
    using value_type = typename std::conditional<is_set, Key, std::pair<Key, T>>::type;
    using size_type = size_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using Self = TurboHashTable<key_type, mapped_type, hasher, key_equal, CellMeta, ProbeStrategy, Media, kCellCountLimit>;

    using Hasher = util::Hasher;
    using Slice  = util::Slice;

private:
    static_assert(kCellCountLimit <= 32768, "kCellCountLimit needs to be <= 32768");
    using WHash = WrapHash<Hash>;
    using WKeyEqual = WrapKeyEqual<KeyEqual>;

public:
    explicit TurboHashTable(uint32_t bucket_count = 128 << 10, uint32_t cell_count = 64):
        bucket_count_(bucket_count),
        bucket_mask_(bucket_count - 1),
        capacity_(bucket_count * cell_count * CellMeta::SlotSize()),
        size_(0) {
        if (!isPowerOfTwo(bucket_count) ||
            !isPowerOfTwo(cell_count)) {
            printf("the hash table size setting is wrong. bucket: %u, cell: %u\n", bucket_count, cell_count);
            exit(1);
        }

        size_t bucket_meta_space = bucket_count * sizeof(BucketMeta);
        size_t bucket_meta_space_huge = (bucket_meta_space + TURBO_HUGEPAGE_SIZE - 1) / TURBO_HUGEPAGE_SIZE * TURBO_HUGEPAGE_SIZE;
        printf("BucketMeta space required: %lu. ", bucket_meta_space);
        
        BucketMeta* buckets_addr = (BucketMeta*) mmap(TURBO_HUGE_ADDR, bucket_meta_space_huge, TURBO_HUGE_PROTECTION, TURBO_HUGE_FLAGS, -1, 0);
        if (buckets_addr == MAP_FAILED) {
            buckets_addr = (BucketMeta* ) aligned_alloc(sizeof(BucketMeta), bucket_meta_space);            
            if (buckets_addr == nullptr) {
                fprintf(stderr, "malloc %lu space fail.\n", bucket_meta_space);
                exit(1);
            }
            printf(" Allocated: %lu\n", bucket_meta_space);
            memset(buckets_addr, 0, bucket_meta_space);
        }
        else {
            printf(" Allocated: %lu\n", bucket_meta_space_huge);
            memset(buckets_addr, 0, bucket_meta_space_huge);
        }
        
        buckets_ = buckets_addr;
        buckets_mem_block_ids_ = new int[bucket_count];
        for (size_t i = 0; i < bucket_count; ++i) {
            auto res = mem_allocator_.AllocateNoSafe(cell_count);
            memset(res.second, 0, cell_count * kCellSize);
            buckets_[i].Reset(res.second, cell_count);
            buckets_mem_block_ids_[i] = res.first;
        }
        
    }

    void DebugInfo()  {
        printf("%s\n", PrintMemAllocator().c_str());
    }
    std::string PrintMemAllocator() {
        return mem_allocator_.ToString();
    }

    void MinorReHashAll()  {
        // allocate new mem space together
        int* old_mem_block_ids = (int*)malloc(bucket_count_ * sizeof(int));
        char** new_mem_block_addr = (char**)malloc(bucket_count_ * sizeof(char*));
        for (size_t i = 0; i < bucket_count_; ++i) {
            auto& bucket_meta = locateBucket(i);
            uint32_t new_cell_count = bucket_meta.CellCount() << 1;
            // if current bucket cannot enlarge any more, continue
            if (TURBO_UNLIKELY(new_cell_count > kCellCountLimit)) {
                printf("Cannot rehash bucket %lu\n", i);
                continue;
            }
            auto res = mem_allocator_.AllocateNoSafe(new_cell_count);
            if (res.second == nullptr) {
                printf("Error\n");
                exit(1);
            }
            old_mem_block_ids[i] = buckets_mem_block_ids_[i];
            buckets_mem_block_ids_[i] = res.first;
            new_mem_block_addr[i] = res.second;
            capacity_.fetch_add(bucket_meta.CellCount() * CellMeta::SlotSize());
            // printf("bucket %lu. Allocate address: 0x%lx. size: %lu\n", i, res.second, new_cell_count);
        }

        // rehash for all the buckets
        int rehash_thread = 4;
        printf("Rehash threads: %d\n", rehash_thread);
        std::vector<std::thread> workers(rehash_thread);
        std::vector<size_t> add_capacity(rehash_thread, 0);
        std::atomic<size_t> rehash_count(0);
        auto rehash_start = util::NowMicros();
        for (int t = 0; t < rehash_thread; t++) {
            workers[t] = std::thread([&, t]
            {
                size_t start_b = bucket_count_ / rehash_thread * t;
                size_t end_b   = start_b + bucket_count_ / rehash_thread;
                size_t counts  = 0;
                // printf("Rehash bucket [%lu, %lu)\n", start_b, end_b);
                for (size_t i = start_b; i < end_b; ++i) {
                    counts += MinorRehash(i, new_mem_block_addr[i]);
                }
                rehash_count.fetch_add(counts, std::memory_order_relaxed);
            });
        }
        std::for_each(workers.begin(), workers.end(), [](std::thread &t) 
        {
            t.join();
        });
        auto rehash_end = util::NowMicros();
        printf("Real rehash speed: %f Mops/s\n", (double)rehash_count / (rehash_end - rehash_start));
        // release the old mem block space
        for (size_t i = 0; i < bucket_count_; ++i) {
            mem_allocator_.ReleaseNoSafe(old_mem_block_ids[i]);
        }

        free(old_mem_block_ids);
        free(new_mem_block_addr);
        printf("Rehash %lu entries\n", rehash_count.load());
    }

    // return the cell index and slot index
    inline std::pair<uint16_t, uint8_t> findNextSlotInRehash(uint8_t* slot_vec, uint16_t h1, uint16_t cell_count_mask) {
        uint16_t ai = h1 & cell_count_mask;
        int loop_count = 0;

        // find next cell that is not full yet
        uint32_t SLOT_MAX_RANGE = CellMeta::SlotMaxRange(); 
        while (slot_vec[ai] >= SLOT_MAX_RANGE) {
            // because we use linear probe, if this cell is full, we go to next cell
            ai += ProbeStrategy::PROBE_STEP; 
            loop_count++;
            if (TURBO_UNLIKELY(loop_count > ProbeStrategy::MAX_PROBE_LEN)) {
                printf("ERROR!!! Even we rehash this bucket, we cannot find a valid slot within %d probe\n", ProbeStrategy::MAX_PROBE_LEN);
                exit(1);
            }
            if (ai > cell_count_mask) {
                ai &= cell_count_mask;
            }
        }
        return {ai, slot_vec[ai]++};
    }

    size_t MinorRehash(int bi, char* new_bucket_addr) {
        size_t count = 0;
        auto& bucket_meta = locateBucket(bi);

        // create new bucket and initialize its meta
        uint32_t old_cell_count = bucket_meta.CellCount();
        uint32_t new_cell_count      = old_cell_count << 1;
        uint32_t new_cell_count_mask = new_cell_count - 1;
        if (!isPowerOfTwo(new_cell_count)) {
            printf("rehash bucket is not power of 2. %u\n", new_cell_count);
            exit(1);
        }
        if (new_bucket_addr == nullptr) {
            perror("rehash alloc memory fail\n");
            exit(1);
        }
        BucketMeta new_bucket_meta(new_bucket_addr, new_cell_count);
     
        // reset all cell's meta data
        for (size_t i = 0; i < new_cell_count; ++i) {
            char* des_cell_addr = new_bucket_addr + (i << kCellSizeLeftShift);
            memset(des_cell_addr, 0, CellMeta::size());
        }

        // iterator old bucket and insert slots info to new bucket
        // old: |11111111|22222222|33333333|44444444|   
        //       ========>
        // new: |1111    |22222   |333     |4444    |1111    |222     |33333   |4444    |
        
        // record next avaliable slot position of each cell within new bucket for rehash
        uint8_t* slot_vec = (uint8_t*)malloc(new_cell_count);
        memset(slot_vec, CellMeta::StartSlotPos(), new_cell_count);
        // std::vector<uint8_t> slot_vec(new_cell_count, CellMeta::StartSlotPos());   
        BucketIterator<CellMeta> iter(bi, bucket_meta.Address(), bucket_meta.CellCount()); 

        while (iter.valid()) { // iterator every slot in this bucket
            count++;
            // obtain old slot info and slot content
            auto res = *iter;

            // update bitmap, H2, H1 and slot pointer in new bucket
            // 1. find valid slot in new bucket
            auto valid_slot = findNextSlotInRehash(slot_vec, res.first.H1, new_cell_count_mask);
            
            // 2. move the slot from old bucket to new bucket
            char* des_cell_addr = new_bucket_addr + (valid_slot.first << kCellSizeLeftShift);
            res.first.slot      = valid_slot.second;
            res.first.cell      = valid_slot.first;
            if (res.first.slot >= CellMeta::SlotMaxRange() || res.first.cell >= new_cell_count) {
                printf("rehash fail: %s\n", res.first.ToString().c_str());
                exit(1);
            }
            moveSlot(des_cell_addr, res.first, res.second);            
            ++iter;
        }

        // replace old bucket meta in buckets_
        buckets_[bi].data_ = new_bucket_meta.data_;
        
        free(slot_vec);
        return count;
    }

    ~TurboHashTable() {

    }

    bool Put(const util::Slice& key, const util::Slice& value)  {
        // calculate hash value of the key
        size_t hash_value = Hasher::hash(key.data(), key.size());

        // store the kv pair to media
        void* media_offset = Media::Store(kTypeValue, key, value);

        // update DRAM index, thread safe
        return insertSlot(key, hash_value, media_offset);
    }

    // Return the entry if key exists
    bool Get(const std::string& key, std::string* value)  {
        size_t hash_value = Hasher::hash(key.data(), key.size());
        auto res = findSlot(key, hash_value);
        if (res.second) {
            // find a key in hash table
            Record record = Media::ParseData(res.first.entry);
            if (record.type  == kTypeValue) {
                value->assign(record.value.data(), record.value.size());
                return true;
            }
            else {
                // this key has been deleted
                return false;
            }
        }
        return false;
    }

    bool Find(const std::string& key, uint64_t& data_offset)  {
        size_t hash_value = Hasher::hash(key.data(), key.size());
        auto res = findSlot(key, hash_value);
        data_offset = res.first.entry;
        return res.second;
    }

    void Delete(const Slice& key) {
        assert(key.size() != 0);
        printf("%s\n", key.ToString().c_str());
    }

    double LoadFactor()  {
        return (double) size_.load(std::memory_order_relaxed) / capacity_.load(std::memory_order_relaxed);
    }

    size_t Size()  { return size_.load(std::memory_order_relaxed);}

    void IterateValidBucket() {
        printf("Iterate Valid Bucket\n");
        for (size_t i = 0; i < bucket_count_; ++i) {
            auto& bucket_meta = locateBucket(i);
            BucketIterator<CellMeta> iter(i, bucket_meta.Address(), bucket_meta.info.cell_count);
            if (iter.valid()) {
                printf("%s\n", PrintBucketMeta(i).c_str());
            }
        }
    }

    void IterateBucket(uint32_t i) {
        auto& bucket_meta = locateBucket(i);
        BucketIterator<CellMeta> iter(i, bucket_meta.Address(), bucket_meta.CellCount());
        while (iter.valid()) {
            auto res = (*iter);
            SlotInfo& info = res.first;
            info.bucket = i;
            HashSlot& slot = res.second;
            Record record = Media::ParseData(slot.entry);
            printf("%s, addr: %16lx. key: %.8s, value: %s\n", 
                info.ToString().c_str(),
                slot.entry, 
                record.key.ToString().c_str(),
                record.value.ToString().c_str());
            ++iter;
        }
    }

    void IterateAll() {
        size_t count = 0;
        for (size_t i = 0; i < bucket_count_; ++i) {
            auto& bucket_meta = locateBucket(i);
            BucketIterator<CellMeta> iter(i, bucket_meta.Address(), bucket_meta.CellCount());
            while (iter.valid()) {
                auto res = (*iter);
                SlotInfo& info = res.first;
                HashSlot& slot = res.second;
                Record record = Media::ParseData(slot.entry);
                printf("%s, addr: %16lx. key: %.8s, value: %s\n", 
                    info.ToString().c_str(),
                    slot.entry, 
                    record.key.ToString().c_str(),
                    record.value.ToString().c_str());
                ++iter;
                count++;
            }
        }
        printf("iterato %lu entries. total size: %lu\n", count, size_.load(std::memory_order_relaxed));
    }

    std::string ProbeStrategyName()  {
        return ProbeStrategy::name();
    }

    std::string PrintBucketMeta(uint32_t bucket_i) {
        std::string res;
        char buffer[1024];
        BucketMeta& meta = locateBucket(bucket_i);
        sprintf(buffer, "----- bucket %10u -----\n", bucket_i);
        res += buffer;
        ProbeStrategy probe(0, meta.CellCountMask(), bucket_i);
        uint32_t i = 0;
        int count_sum = 0;
        while (probe) {
            char* cell_addr = locateCell(probe.offset());
            CellMeta meta(cell_addr);
            int count = meta.OccupyCount();
            sprintf(buffer, "\t%4u - 0x%12lx: %s. Cell valid slot count: %d\n", i++, (uint64_t)cell_addr, meta.BitMapToString().c_str(), count);
            res += buffer;
            probe.next();
            count_sum += count;
        }
        sprintf(buffer, "\tBucket %u: valid slot count: %d. Load factor: %f\n", bucket_i, count_sum, (double)count_sum / (CellMeta::SlotSize() * meta.CellCount()));
        res += buffer;
        return res;
    }
    
    void PrintAllMeta() {
        for (size_t b = 0; b < bucket_count_; ++b) {
            printf("%s\n", PrintBucketMeta(b).c_str());
        }
    }

    void PrintHashTable() {
        for (int b = 0; b < bucket_count_; ++b) {
            printf("%s\n", PrintBucketMeta(b).c_str());
        }
    }

private:
    inline uint32_t bucketIndex(uint64_t hash) {
        return hash & bucket_mask_;
    }

    inline BucketMeta& locateBucket(uint32_t bi) const {
        return buckets_[bi];
    }

    // offset.first: bucket index
    // offset.second: cell index
    inline char* locateCell(const std::pair<size_t, size_t>& offset) {
        return  buckets_[offset.first].Address() +      // locate the bucket
                (offset.second << kCellSizeLeftShift);  // locate the cell cell
    }
  
    inline HashSlot* locateSlot(const char* cell_addr, int slot_i) {
        return (HashSlot*)(cell_addr + (slot_i << 3));
    }

    // used in rehash function, move slot to new cell_addr
    inline void moveSlot(char* des_cell_addr, const SlotInfo& des_info, const HashSlot& src_slot) {
        // move slot content, including H1 and pointer
        HashSlot* des_slot = locateSlot(des_cell_addr, des_info.slot);
        *des_slot = src_slot;
        // obtain bitmap
        decltype(CellMeta::BitMapType())* bitmap = (decltype(CellMeta::BitMapType())*)des_cell_addr;
        // set H2
        des_cell_addr += des_info.slot; // move to H2 address
        *des_cell_addr = des_info.H2;   // update  H2
        // set bitmap
        *bitmap = (*bitmap) | (1 << des_info.slot);
    }
    
    inline void updateSlotAndMeta(char* cell_addr, const SlotInfo& info, void* media_offset) {
        // set bitmap, 1 byte H2, 2 byte H1, 6 byte pointer

        // set 2 byte H1 and 6 byte pointer
        HashSlot* slot_pos = locateSlot(cell_addr, info.slot);
        slot_pos->entry = (uint64_t)media_offset;
        slot_pos->H1 = info.H1;
       
        // obtain bitmap
        decltype(CellMeta::BitMapType())* bitmap = (decltype(CellMeta::BitMapType())*)cell_addr;

        // set H2
        cell_addr += info.slot; // move cell_addr one byte hash position
        *cell_addr = info.H2;   // update the one byte hash

        // add a fence here. 
        // Make sure the bitmap is updated after H2
        // https://www.modernescpp.com/index.php/fences-as-memory-barriers
        // https://preshing.com/20130922/acquire-and-release-fences/
        TURBO_COMPILER_FENCE();

        // set bitmap
        *bitmap = (*bitmap) | (1 << info.slot);
    }


    inline bool insertSlot(const Slice& key, size_t hash_value, void* media_offset) {
        bool retry_find = false;
        do { // concurrent insertion may find same position for insertion, retry insertion if neccessary
            PartialHash partial_hash(hash_value);            

            auto res = findSlotForInsert(key, partial_hash);

            if (res.second) { // find a valid slot
                char* cell_addr = locateCell({res.first.bucket, res.first.cell});
                
                util::SpinLockScope<0> lock_scope((uint32_t*)cell_addr); // Lock current cell
                
                CellMeta meta(cell_addr);   // obtain the meta part
                
                if (TURBO_LIKELY(!meta.Occupy(res.first.slot) ||  // if the slot is not occupied or
                    res.first.equal_key)) {                 // the slot has same key, update the slot

                    // update slot content (including pointer and H1), H2 and bitmap
                    updateSlotAndMeta(cell_addr, res.first, media_offset);
                    if (!res.first.equal_key) size_.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
                else { // current slot has been occupied by another concurrent thread, retry.
                    // #ifdef LTHASH_DEBUG_OUT
                    // INFO("retry find slot. %s\n", key.ToString().c_str());
                    // #endif
                    retry_find = true;
                }
            }
            else { // cannot find a valid slot for insertion, rehash current bucket then retry
                printf("=========== Need Rehash ==========\n");
                printf("%s\n", PrintBucketMeta(res.first.bucket).c_str());
                
                break;
            }
        } while (retry_find);
        
        return false;
    }

    inline bool slotKeyEqual(const HashSlot& slot, const Slice& key) {
        Slice res = Media::ParseKey(reinterpret_cast<void*>(slot.entry));
        return res == key;
    }

    inline const Slice extractSlice(const HashSlot& slot) {
        return  Media::ParseKey(reinterpret_cast<void*>(slot.entry));
    }

    // Find a valid slot for insertion
    // Return: std::pair
    //      first: the slot info that should insert the key
    //      second: whether we can find a valid(empty or belong to the same key) slot to insert
    // Node: Only when the second value is true, can we insert this key
    inline std::pair<SlotInfo, bool> findSlotForInsert(const Slice& key, const PartialHash& partial_hash) {
        uint32_t bucket_i = bucketIndex(partial_hash.bucket_hash_);
        auto& bucket_meta = locateBucket(bucket_i);
        ProbeStrategy probe(partial_hash.H1_, bucket_meta.CellCountMask(), bucket_i);

        int probe_count = 0; // limit probe times
        while (probe && (probe_count++ < ProbeStrategy::MAX_PROBE_LEN)) {
            auto offset = probe.offset();
            char* cell_addr = locateCell(offset);
            CellMeta meta(cell_addr);

            // if this is a update request, we overwrite existing slot
            for (int i : meta.MatchBitSet(partial_hash.H2_)) {  // if there is any H2 match in this cell (H2 is 8-byte)
                                                                // i is the slot index in current cell, each slot occupies 8-byte
                // locate the slot reference
                const HashSlot& slot = *locateSlot(cell_addr, i);

                if (TURBO_LIKELY(slot.H1 == partial_hash.H1_)) 
                {  // compare if the H1 partial hash is equal (H1 is 16-byte)
                    if (TURBO_LIKELY(slotKeyEqual(slot, key))) {  // compare if the slot key is equal
                        return {{   offset.first,           /* bucket */
                                    offset.second,          /* cell */
                                    i,                      /* slot */
                                    partial_hash.H1_,       /* H1 */ 
                                    partial_hash.H2_,       /* H2 */
                                    true},                  /* equal_key */
                                true};
                    }
                    else {                                  // two key is not equal, go to next slot
                        #ifdef LTHASH_DEBUG_OUT
                        Slice slot_key =  extractSlice(slot);
                        printf("H1 conflict. Slot (%8u, %3u, %2d) bitmap: %s. Insert key: %15s, 0x%016lx, Slot key: %15s, 0x%016lx\n", 
                            offset.first, 
                            offset.second, 
                            i, 
                            meta.BitMapToString().c_str(),
                            key.ToString().c_str(),
                            hash_value,
                            slot_key.ToString().c_str(),
                            Hasher::hash(slot_key.data(), slot_key.size())
                            );
                        #endif
                    }
                }
            }

            // return an empty slot for new insertion
            auto empty_bitset = meta.EmptyBitSet(); 
            if (TURBO_LIKELY(empty_bitset)) {
                // for (int i : empty_bitset) { // there is empty slot, return its meta

                    return {{   offset.first,           /* bucket */
                                offset.second,          /* cell */
                                *empty_bitset,          /* pick a slot, empty_bitset.pickOne() */ 
                                partial_hash.H1_,       /* H1 */
                                partial_hash.H2_,       /* H2 */
                                false /* a new slot */}, 
                            true};
                // }
            }
            
            // probe the next cell in the same bucket
            probe.next(); 
        }

        #if 0
        #ifdef LTHASH_DEBUG_OUT
        printf("Fail to find one empty slot\n");
        printf("%s\n", PrintBucketMeta(bucket_i).c_str());
        #endif
        #endif
        // only when all the probes fail and there is no empty slot
        // exists in this bucket. 
        return {{
                    bucket_i,
                    0,
                    0, 
                    partial_hash.H1_, 
                    partial_hash.H2_, 
                    false /* a new slot */},
                false};
    }
    
    inline std::pair<HashSlot, bool> findSlot(const Slice& key, size_t hash_value) {
        PartialHash partial_hash(hash_value);
        uint32_t bucket_i = bucketIndex(partial_hash.bucket_hash_);
        auto& bucket_meta = locateBucket(bucket_i);
        ProbeStrategy probe(partial_hash.H1_,  bucket_meta.CellCountMask(), bucket_i);

        int probe_count = 0; // limit probe times
        while (probe && (probe_count++ < ProbeStrategy::MAX_PROBE_LEN)) {
            auto offset = probe.offset();
            char* cell_addr = locateCell(offset);
            CellMeta meta(cell_addr);
            
            for (int i : meta.MatchBitSet(partial_hash.H2_)) {  // Locate if there is any H2 match in this cell
                                                                // i is the slot index in current cell, each slot occupies 8-byte
                // locate the slot reference
                const HashSlot& slot = *locateSlot(cell_addr, i);

                if (TURBO_LIKELY(slot.H1 == partial_hash.H1_))  // Compare if the H1 partial hash is equal.
                {
                    if (TURBO_LIKELY(slotKeyEqual(slot, key))) {      // If the slot key is equal to search key. SlotKeyEqual is very expensive 
                        return {slot, true};
                    }
                    else {
                        #ifdef LTHASH_DEBUG_OUT
                        // printf("H1 conflict. Slot (%8u, %3u, %2d) bitmap: %s. Search key: %15s, H2: 0x%2x, H1: 0x%4x, Slot key: %15s, H1: 0x%4x\n", 
                        //     probe.offset().first, 
                        //     probe.offset().second, 
                        //     i, 
                        //     meta.BitMapToString().c_str(),
                        //     key.ToString().c_str(),
                        //     partial_hash.H2_,
                        //     partial_hash.H1_,
                        //     extraceSlice(slot, key.size()).ToString().c_str(),
                        //     slot.H1
                        //     );
                        
                        Slice slot_key =  extractSlice(slot);
                        printf("H1 conflict. Slot (%8u, %3u, %2d) bitmap: %s. Search key: %15s, 0x%016lx, Slot key: %15s, 0x%016lx\n", 
                            offset.first, 
                            offset.second, 
                            i, 
                            meta.BitMapToString().c_str(),
                            key.ToString().c_str(),
                            hash_value,
                            slot_key.ToString().c_str(),
                            Hasher::hash(slot_key.data(), slot_key.size())
                            );
                        #endif
                    }
                    
                }
            }

            // if this cell still has empty slot, then it means the key does't exist.
            if (TURBO_LIKELY(meta.EmptyBitSet())) {
                return {{0, 0}, false};
            }
            
            probe.next();
        }
        // after all the probe, no key exist
        return {{0, 0}, false};
    }

    inline bool isPowerOfTwo(uint32_t n) {
        return (n != 0 && __builtin_popcount(n) == 1);
    }

private:
    CellAllocator<CellMeta, kCellCountLimit> mem_allocator_;
    BucketMeta*   buckets_;
    int*          buckets_mem_block_ids_;
    const size_t  bucket_count_ = 0;
    const size_t  bucket_mask_  = 0;
    std::atomic<size_t> capacity_;
    std::atomic<size_t> size_;

    const int       kCellSize = CellMeta::CellSize();
    const int       kCellSizeLeftShift = CellMeta::CellSizeLeftShift;
};

}; // end of namespace turbo::detail

template <typename Key, typename T, typename Hash = hash<Key>,
          typename KeyEqual = std::equal_to<Key> >
using unordered_map = detail::TurboHashTable<Key, T, Hash, KeyEqual,
                detail::CellMeta128, detail::ProbeWithinBucket>;

}; // end of namespace turbo

#endif
                                                              