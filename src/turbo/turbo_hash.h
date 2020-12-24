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
#include <iomanip>
#include <sstream>
#include <iostream>
#include <utility>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <error.h>
#include <stdio.h>
#include <pthread.h>
#include <immintrin.h>

#include <sys/mman.h>
#include <sys/time.h>

#include <jemalloc/jemalloc.h>

// un-comment this to disable logging, log is saved at robin_hood.log
#define TURBO_ENABLE_LOGGING

// #define LTHASH_DEBUG_OUT

#define TURBO_ENABLE_HUGEPAGE /* enable huge page or not */

// Linear probing setting
static const int kTurboMaxProbeLen = 18;
static const int kTurboProbeStep   = 1;    

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

// turbo_hash logging
namespace {

static inline std::string TURBO_CMD(const std::string& content) {
    std::array<char, 128> buffer;
    std::string result;
    auto cmd = "echo '" + content + "' >> turbo.log";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

#define TURBO_LOG(M, x)\
do {\
    std::ostringstream ss;\
    ss << M << x;\
    TURBO_CMD(ss.str());\
} while(0);

#define __FILENAME__ ((strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__))

#if defined TURBO_ENABLE_LOGGING
#define TURBO_INFO(x)\
do {\
    char buffer[1024] = "[ INFO] ";\
    sprintf(buffer + 8, "[%s %s:%d] ", __FILENAME__, __FUNCTION__, __LINE__);\
    TURBO_LOG(buffer, x);\
} while(0);
#else
#define TURBO_INFO(x)\
  do {\
  } while(0);
#endif

#if !defined NDEBUG && defined TURBO_ENABLE_LOGGING
#define TURBO_DEBUG(x)\
do {\
    char buffer[1024] = "[DEBUG] ";\
    sprintf(buffer + 8, "[%s %s:%d] ", __FILENAME__, __FUNCTION__, __LINE__);\
    TURBO_LOG(buffer, x);\
} while(0);
#else
#define TURBO_DEBUG(x)\
  do {\
  } while(0);
#endif

#if defined TURBO_ENABLE_LOGGING
#define TURBO_ERROR(x)\
do {\
    char buffer[1024] = "[ERROR] ";\
    sprintf(buffer + 8, "[%s %s:%d] ", __FILENAME__, __FUNCTION__, __LINE__);\
    TURBO_LOG(buffer, x);\
} while(0);
#endif 

#if defined TURBO_ENABLE_LOGGING
#define TURBO_WARNING(x)\
do {\
    char buffer[1024] = "[ WARN] ";\
    sprintf(buffer + strlen(buffer), "[%s %s:%d] ", __FILENAME__, __FUNCTION__, __LINE__);\
    TURBO_LOG(buffer, x);\
} while(0);
#endif

}; // end of namespace for logging

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
    
    inline int validCount(void) {
        return __builtin_popcount(bits_);
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
    using type = Slice;
    // operator <
    bool operator < (const Slice& b) const {
        return compare(b) < 0 ;
    }

    bool operator > (const Slice& b) const {
        return compare(b) > 0 ;
    }

    // explicit conversion
    inline operator std::string() const {
        return std::string(data_, size_);
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

    friend std::ostream& operator<<(std::ostream& os, const Slice& str) {
        os <<  str.ToString();
        return os;
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
    SpinLockScope(char* addr):
        lock_(reinterpret_cast<uint32_t*>(addr)) {
        // lock the bit lock
        turbo_bit_spin_lock(lock_, kBitLockPosition);
    }

    SpinLockScope(uint32_t *lock):
        lock_(lock) {
        // lock the bit lock
        turbo_bit_spin_lock(lock_, kBitLockPosition);
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

    void inline lock() noexcept {
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

    bool inline is_locked(void) noexcept {
        return lock_.load(std::memory_order_relaxed);
    }

    bool inline try_lock() noexcept {
        // First do a relaxed load to check if lock is free in order to prevent
        // unnecessary cache misses if someone does while(!try_lock())
        return !lock_.load(std::memory_order_relaxed) &&
            !lock_.exchange(true, std::memory_order_acquire);
    }

    void inline unlock() noexcept {
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
// dummy hash, unsed as mixer when turbo::hash is already used
template <typename T>
struct identity_hash {
    constexpr size_t operator()(T const& obj) const noexcept {
        return static_cast<size_t>(obj);
    }
};

namespace detail {

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
 * 
 *  @note: CellMetaType:
 *              0 - CellMeta128
 *              1 - CellMeta256
 *              2 - CellMeta256V2
 *         ProbeStrategyType:
 *              0 - ProbeWithinBucket
 *              1 - ProbeWithinCell
 *              
*/
template <  typename Key, typename T, typename Hash, typename KeyEqual,
            int CellMetaType = 0, int ProbeStrategyType = 0, int kCellCountLimit = 32768>
class TurboHashTable 
    : public WrapHash<Hash>, 
      public WrapKeyEqual<KeyEqual> {
public:
    static constexpr bool is_map = !std::is_void<T>::value;
    static constexpr bool is_set = !is_map;
    static constexpr bool is_key_flat = 
                                        std::is_same<Key, std::string>::value == false &&                                        
                                        CellMetaType == 2;

    // Internal transformation for std::string
    using key_type    = Key;
    using mapped_type = T;
    using size_type   = size_t;
    using hasher      = Hash;
    using key_equal   = typename std::conditional<  std::is_same<Key, std::string>::value == false  /* is numeric */,
                                                    KeyEqual, std::equal_to<util::Slice> >::type;
    using Self        = TurboHashTable<key_type, mapped_type, hasher, key_equal, CellMetaType, ProbeStrategyType, kCellCountLimit>;

// private:

    /* #region: for value_type */

    /** ValueType
     *  @note: for key-value record type
     *  @format:
    */
    enum ValueType: unsigned char {
        kTypeDeletion = 0x2A,   // 0b 0_0101010
        kTypeValue    = 0xAA,   // 0b 1_0101010
        kTypeMask     = 0x7F    // 0b 0_1111111
    };    

    /**
     *  @note: obtain the Record Lenght
    */
    template<bool IsT1Numeric, typename T1>
    struct Record1Size {};

    /**
     *  @note: calcuate the Record Lenght
    */
    template<bool IsT1Numeric, typename T1>
    struct Record1Format {};

    /**
     *  @note: encode T1 to memory
     *  ! allocate memory space in advance
    */
    template<bool IsT1Numeric, typename T1>
    struct EncodeToRecord1 {};

    /** DecodeInRecord1
     *  @note: decode T1
    */
    template<bool IsT1Numeric, typename T1>
    struct DecodeInRecord1 {};

    /** Case 1: T1 is numeric
     *  * record memory layout:
     *  | type |   T1  |
     *  |  1B  |
     *         |
     *     addr start here
    */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    template<typename T1>
    struct Record1Size<true, T1> {
        static inline size_t Size(char* addr) {
            return 1 + sizeof(T1);
        }
    };
    #pragma GCC diagnostic pop

    template<typename T1>
    struct Record1Format<true, T1> {
        static inline constexpr size_t Length(const T1& t1) {
            return 1 + sizeof(t1);
        }
    };
    template<typename T1>
    struct EncodeToRecord1<true, T1> {
        static inline void Encode(const T1& t1, char* addr) {
            *reinterpret_cast<T1*>(addr) = t1;
        }
    };
    template<typename T1>
    struct DecodeInRecord1<true, T1> {
        static inline T1 Decode(char* addr) {
            return *reinterpret_cast<T1*>(addr);
        }
    };

    /** Case 2: T1 is std::string
     *  * record memory layout:
     *  | type |  len1  | buffer1
     *  |  1B  | size_t | 
     *         |
     *     addr start here
    */
    template<typename T1>
    struct Record1Size<false, T1> {
        static inline size_t Size(char* addr) {
            size_t len1 = *reinterpret_cast<size_t*>(addr);
            return 1 + sizeof(size_t) + len1;
        }
    };
    template<typename T1>
    struct Record1Format<false, T1> {
        static inline size_t Length(const T1& t1) {
            return 1 + sizeof(size_t) + t1.size();
        }
    };
    template<typename T1>
    struct EncodeToRecord1<false, T1> {
        static inline void Encode(const T1& t1, char* addr) {
            *reinterpret_cast<size_t*>(addr) = t1.size();
            memcpy(addr + sizeof(size_t), t1.data(), t1.size());
        }
    };
    // When T1 is std::string, 'Decode' return util::Slice, which is the reference of real value.
    template<typename T1>
    struct DecodeInRecord1<false, T1> {
        static inline util::Slice Decode(char* addr) {
            size_t len1 = *reinterpret_cast<size_t*>(addr);
            return util::Slice(addr + sizeof(size_t), len1);
        }
    };

    /** Record1
     *  @note record with only one type, used in set
    */
    template<typename T1>
    struct Record1 {
        ValueType type;
        using T1_type = typename std::conditional<  std::is_same<T1, std::string>::value == false  /* is numeric */,
                                                    T1, util::Slice>::type;
        static inline size_t FormatLength(const T1& t1) {
            return Record1Format<
                            std::is_same<T1, std::string>::value == false /* is numeric */,
                            T1>::Length(t1);
        }

        inline T1_type first() {
            return DecodeInRecord1< 
                            std::is_same<T1, std::string>::value == false /* is numeric */,
                            T1>::Decode(reinterpret_cast<char*>(this) + 1);
        }

        inline void Encode(const T1& t1) {
            EncodeToRecord1<std::is_same<T1, std::string>::value == false /* is numeric */, 
                            T1>::Encode(t1, reinterpret_cast<char*>(this) + 1);
        }

        inline size_t Size() {
            return Record1Size<
                            std::is_same<T1, std::string>::value == false /* is numeric */,
                            T1>::Size(reinterpret_cast<char*>(this) + 1);
        }
    };  // end of class Record1


    /**
     *  @note: obtain the Record Lenght
    */
    template<bool IsT1Numeric, bool IsT2Numeric, typename T1, typename T2>
    struct Record2Size {};

    /**
     *  @note: calculate the Record Lenght
    */
    template<bool IsT1Numeric, bool IsT2Numeric, typename T1, typename T2>
    struct Record2Format {};

    /** EncodeToRecord2
     *  @note: encode T1 and T2 to addr.
     *  ! The memory space should be allocated in advance
    */
    template<bool IsT1Numeric, bool IsT2Numeric, typename T1, typename T2>
    struct EncodeToRecord2 {};

    /** DecodeInRecord2
     *  @note: decode T1 and T2
    */
    template<bool IsT1Numeric, bool IsT2Numeric, bool IsFirst, typename T1, typename T2>
    struct DecodeInRecord2 {};

    /** Case 1: T1 and T2 are numeric
     *  * record memory layout:
     *  | type |   T1  |   T2  |
     *  |  1B  |
     *         |
     *     addr start here
    */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    template<typename T1, typename T2>
    struct Record2Size<true, true, T1, T2> {
        static inline size_t Size(char* addr) {
            return 1 + sizeof(T1) + sizeof(T2);
        }
    };
    #pragma GCC diagnostic pop
    template<typename T1, typename T2>
    struct Record2Format<true, true, T1, T2> {
        static inline constexpr size_t Length(const T1& t1, const T2& t2) {
            return 1 + sizeof(t1) + sizeof(t2);
        }
    };
    template<typename T1, typename T2> 
    struct EncodeToRecord2<true, true, T1, T2> {
        static inline void Encode(const T1& t1, const T2& t2, char* addr) {
            *reinterpret_cast<T1*>(addr) = t1;
            *reinterpret_cast<T2*>(addr + sizeof(T1)) = t2;
        }
    };
    template<typename T1, typename T2>
    struct DecodeInRecord2<true, true, true /* IsFirst */, T1, T2> {
        static inline T1 Decode(char* addr) {
            return *reinterpret_cast<T1*>(addr);
        }
    };
    template<typename T1, typename T2>
    struct DecodeInRecord2<true, true, false /* IsFirst*/, T1, T2> {
        static inline T2 Decode(char* addr) {
            return *reinterpret_cast<T2*>(addr + sizeof(T1));
        }
    };

    /** Case 2: T1 is numeric, T2 is std::string
     *  * record memory layout:
     *  | type |   T1  |   len2  |   buffer2
     *  |  1B  |       |  size_t |
     *         |                 |
     *     addr start here     offset
    */
    template<typename T1, typename T2>
    struct Record2Size<true, false, T1, T2> {
        static inline size_t Size(char* addr) {
            size_t len2 = *reinterpret_cast<size_t*>(addr + sizeof(T1));
            return 1 + sizeof(T1) + sizeof(size_t) + len2;
        }
    };
    template<typename T1, typename T2>
    struct Record2Format<true, false, T1, T2> {
        static inline size_t Length(const T1& t1, const T2& t2) {
            return 1 + sizeof(t1) + sizeof(size_t) + t2.size();
        }
    };
    template<typename T1, typename T2> 
    struct EncodeToRecord2<true, false, T1, T2> {
        static inline void Encode(const T1& t1, const T2& t2, char* addr) {
            static constexpr size_t offset = sizeof(T1) + sizeof(size_t);
            memcpy(addr, &t1, sizeof(T1));
            *reinterpret_cast<size_t*>(addr + sizeof(T1)) = t2.size();
            if (t2.size() != 0) memcpy(addr + offset, t2.data(), t2.size());
        }
    };
    template<typename T1, typename T2>
    struct DecodeInRecord2<true, false, true /* IsFirst*/, T1, T2> {
        static inline T1 Decode(char* addr) {
            return *reinterpret_cast<T1*>(addr);
        }
    };
    template<typename T1, typename T2>
    struct DecodeInRecord2<true, false, false /* IsFirst*/, T1, T2> {
        static inline util::Slice Decode(char* addr) {
            size_t len2 = *reinterpret_cast<size_t*>(addr + sizeof(T1));
            return util::Slice(addr + sizeof(T1) + sizeof(size_t), len2);
        }
    };

    /** Case 3: T1 is std::string, T2 is numeric
     *  * record memory layout:
     *  | type |  len1  |   T2  |   buffer1
     *  |  1B  | size_t |       |
     *         |
     *     addr start here
    */
    template<typename T1, typename T2>
    struct Record2Size<false, true, T1, T2> {
        static inline size_t Size(char* addr) {
            size_t len1 = *reinterpret_cast<size_t*>(addr);
            return 1 + sizeof(size_t) + sizeof(T2) + len1;
        }
    };
    template<typename T1, typename T2>
    struct Record2Format<false, true, T1, T2> {
        static inline size_t Length(const T1& t1, const T2& t2) {
            return 1 + sizeof(size_t) + sizeof(t2) + t1.size();
        }
    };
    template<typename T1, typename T2> 
    struct EncodeToRecord2<false, true, T1, T2> {
        static inline void Encode(const T1& t1, const T2& t2, char* addr) {
            static constexpr size_t offset = sizeof(size_t) + sizeof(T2);
            *reinterpret_cast<size_t*>(addr) = t1.size();
            memcpy(addr + sizeof(size_t), &t2, sizeof(T2));
            memcpy(addr + offset, t1.data(), t1.size());
        }
    };
    template<typename T1, typename T2>
    struct DecodeInRecord2<false, true, true /* IsFirst*/, T1, T2> {
        static inline util::Slice Decode(char* addr) {
            size_t len1 = *reinterpret_cast<size_t*>(addr);
            return util::Slice(addr + sizeof(size_t) + sizeof(T2), len1);
            
        }
    };
    template<typename T1, typename T2>
    struct DecodeInRecord2<false, true, false /* IsFirst*/, T1, T2> {
        static inline T2 Decode(char* addr) {
            return *reinterpret_cast<T2*>(addr + sizeof(size_t));
        }
    };

    /** Case 4: T1 is std::string, T2 is std::string
     *  * record memory layout:
     *  | type |  len1  |  len2  | buffer1 | bufer2 |
     *  |  1B  | size_t | size_t |         |
     *         |                 |
     *     addr start here     offset
    */
    template<typename T1, typename T2>
    struct Record2Size<false, false, T1, T2> {
        static inline size_t Size(char* addr) {
            size_t len1 = *reinterpret_cast<size_t*>(addr);
            size_t len2 = *reinterpret_cast<size_t*>(addr + sizeof(size_t));
            return 1 + sizeof(size_t) + sizeof(size_t) + len1 + len2;
        }
    };
    template<typename T1, typename T2>
    struct Record2Format<false, false, T1, T2> {
        static inline size_t Length(const T1& t1, const T2& t2) {
            return 1 + sizeof(size_t) + sizeof(size_t) + t1.size() + t2.size();
        }
    };
    template<typename T1, typename T2> 
    struct EncodeToRecord2<false, false, T1, T2> {
        static inline void Encode(const T1& t1, const T2& t2, char* addr) {
            static constexpr size_t offset = sizeof(size_t) + sizeof(size_t);
            *reinterpret_cast<size_t*>(addr) = t1.size();
            *reinterpret_cast<size_t*>(addr + sizeof(size_t)) = t2.size();
            memcpy(addr + offset, t1.data(), t1.size());
            if (t2.size() != 0) memcpy(addr + offset + t1.size(), t2.data(), t2.size());
        }
    };
    template<typename T1, typename T2>
    struct DecodeInRecord2<false, false, true /* IsFirst*/, T1, T2> {
        static inline util::Slice Decode(char* addr) {
            size_t len1 = *reinterpret_cast<size_t*>(addr);
            return util::Slice(addr + sizeof(size_t) + sizeof(size_t), len1);
        }
    };
    template<typename T1, typename T2>
    struct DecodeInRecord2<false, false, false /* IsFirst*/, T1, T2> {
        static inline util::Slice Decode(char* addr) {
            size_t len1 = *reinterpret_cast<size_t*>(addr);
            size_t len2 = *reinterpret_cast<size_t*>(addr + sizeof(size_t));
            return util::Slice(addr + sizeof(size_t) + sizeof(size_t) + len1, len2);
        }
    };

    /** Record2
     *  @note record with two type, used in map
    */
    template<typename T1, typename T2>
    struct Record2 {
        ValueType type;
        using T1_type = typename std::conditional<  std::is_same<T1, std::string>::value == false  /* is numeric */,
                                                    T1, util::Slice>::type;
        using T2_type = typename std::conditional<  std::is_same<T2, std::string>::value == false  /* is numeric */,
                                                    T2, util::Slice>::type;

        static inline size_t FormatLength(const T1& t1, const T2& t2) {
            return Record2Format<
                    std::is_same<T1, std::string>::value == false  /* is numeric */, 
                    std::is_same<T2, std::string>::value == false  /* is numeric */,
                    T1, T2>::Length(t1, t2);
        }           

        inline T1_type first() {
            return DecodeInRecord2<
                    std::is_same<T1, std::string>::value == false  /* is numeric */, 
                    std::is_same<T2, std::string>::value == false  /* is numeric */, 
                    true, T1, T2>::Decode(reinterpret_cast<char*>(this) + 1);
        }

        inline T2_type second() {
            return DecodeInRecord2<
                    std::is_same<T1, std::string>::value == false  /* is numeric */, 
                    std::is_same<T2, std::string>::value == false  /* is numeric */, 
                    false, T1, T2>::Decode(reinterpret_cast<char*>(this) + 1);
            
        }

        inline void Encode(const T1& t1, const T2& t2) {
            EncodeToRecord2<
                    std::is_same<T1, std::string>::value == false  /* is numeric */, 
                    std::is_same<T2, std::string>::value == false  /* is numeric */,
                    T1, T2>::Encode(t1, t2, reinterpret_cast<char*>(this) + 1);
        }

        // return the record size
        inline size_t Size(void) {
            return Record2Size<
                    std::is_same<T1, std::string>::value == false  /* is numeric */, 
                    std::is_same<T2, std::string>::value == false  /* is numeric */,
                    T1, T2>::Size(reinterpret_cast<char*>(this) + 1);
        }
    }; // end of class Record2
    
    /* #endregion: for value_type */

    using value_type = typename std::conditional<is_set, Record1<key_type>, Record2<key_type, mapped_type> >::type;

    /* #region: for CellMeta */

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
     *      8 bit tag for the slot
     * 
     *  |- slot:
     *      0  -  5 byte: the pointer used to point to DIMM where store the actual kv value
     *      6  -  7 byte: two hash byte for this slot
    */ 
    class CellMeta128 {
    public:
        static constexpr uint16_t BitMapMask    = 0xFFFC;
        static constexpr int CellSizeLeftShift  = 7;
        static constexpr int SlotSizeLeftShift  = 3;
        using H2Tag = uint8_t;
        using H1Tag = uint16_t;




        /** PartialHash
         *  @note: a 64-bit hash is used to locate the cell location, and provide hash-tag.
         *  @format:
         *  | MSB    - - - - - - - - - - - - - - - - - - LSB |
         *  |     32 bit     |   16 bit  |  8 bit  |  8 bit  |
         *  |    bucket_hash |     H1    |         |   H2    |
        */
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-parameter"
        struct PartialHash {
            PartialHash(const Key& key, uint64_t hash) :
                bucket_hash_( hash >> 32 ),
                H1_( ( hash >> 16 ) & 0xFFFF ),
                H2_( hash & 0xFF )
                { };        
            uint32_t  bucket_hash_; // used to locate in the bucket directory
            H1Tag     H1_; // H1: 2 byte tag
            H2Tag     H2_; // H2: 1 byte hash tag in CellMeta for parallel comparison using SIMD cmd
        }; // end of class PartialHash
        #pragma GCC diagnostic pop

        /** HashSlot
         *  @node: highest 2 byte is used as hash-tag to reduce unnecessary touch key-value
        */
        struct HashSlot{
            uint64_t entry:48;      // pointer to key-value record
            uint64_t H1:16;         // hash-tag for the key
        };

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

        inline util::BitSet ValidBitSet() {
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

        inline static constexpr uint8_t StartSlotPos() {
            return 2;
        }

        inline static constexpr uint32_t CellSize() {
            // cell size (include meta) in byte
            return 128;
        }

        inline static constexpr uint32_t SlotMaxRange() {
            // used for rehashing. Though we have 16 slots, one is reserved for update and deletion.
            return 15;
        }

        inline static constexpr uint32_t SlotCount() {
            // slot count
            return 14;
        }

        inline static std::string Name() {
            return "CellMeta128";
        }

        inline static constexpr size_t size() {
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

        std::string ToString() {
            return BitMapToString();
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
     *      8 bit tag for the slot
     * 
     *  |- slot:
     *      0  -  5 byte: the pointer used to point to DIMM where store the actual kv value
     *      6  -  7 byte: two hash byte for this slot (totally 3 byte is used as the hash)
    */ 
    class CellMeta256 {
    public:
        static constexpr uint32_t BitMapMask = 0x0FFFFFFF0;
        static constexpr int CellSizeLeftShift = 8;
        static constexpr int SlotSizeLeftShift = 3;
        using H2Tag = uint8_t;
        using H1Tag = uint16_t;

        /** PartialHash
         *  @note: a 64-bit hash is used to locate the cell location, and provide hash-tag.
         *  @format:
         *  | MSB    - - - - - - - - - - - - - - - - - - LSB |
         *  |     32 bit     |   16 bit  |  8 bit  |  8 bit  |
         *  |    bucket_hash |     H1    |         |   H2    |
        */
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-parameter"
        struct PartialHash {
            PartialHash(const Key& key, uint64_t hash) :
                bucket_hash_( hash >> 32 ),
                H1_( ( hash >> 16 ) & 0xFFFF ),
                H2_( hash & 0xFF )
                { };
            uint32_t  bucket_hash_; // used to locate in the bucket directory
            H1Tag     H1_; // H1: 2 byte tag
            H2Tag     H2_; // H2: 1 byte hash tag in CellMeta for parallel comparison using SIMD cmd
        }; // end of class PartialHash
        #pragma GCC diagnostic pop

        /** HashSlot
         *  @node: highest 2 byte is used as hash-tag to reduce unnecessary touch key-value
        */
        struct HashSlot{
            uint64_t entry:48;      // pointer to key-value record
            uint64_t H1:16;         // hash-tag for the key
        };

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

        inline util::BitSet ValidBitSet() {
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

        inline static constexpr uint8_t StartSlotPos() {
            return 4;
        }

        inline static constexpr uint32_t CellSize() {
            // cell size (include meta) in byte
            return 256;
        }

        inline static constexpr uint32_t SlotMaxRange() {
            // used for rehashing. Though we have 32 slots, one is reserved for update and deletion.
            return 31;
        }
        
        inline static constexpr uint32_t SlotCount() {
            // slot count
            return 28;
        }

        inline static std::string Name() {
            return "CellMeta256";
        }

        inline static constexpr size_t size() {
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
        std::string ToString() {
            return BitMapToString();
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

        __m256i     meta_;          // 32 byte integer vector
        uint32_t    bitmap_;        // 1: occupied, 0: empty or deleted
    }; // end of class CellMeta256

    /** CellMeta256V2
     *  @note: Hash cell whose size is 256 byte. There are 14 slots in the cell.
     *  @format:
     *  | ----------------------- meta ------------------------| ----- slots ----- |
     *  | 4 byte bitmap | 28 byte: two byte hash for each slot | 16 byte * 14 slot |
     * 
     *  |- bitmap: 
     *            0 bit: used as a bitlock
     *            1 bit: not in use
     *      2  - 15 bit: indicate which slot is empty, 0: empty or deleted, 1: occupied
     *      16 - 31 bit: not in use
     * 
     *  |- two byte hash:
     *      16 bit tag for the slot
     * 
     *  |- slot:
     *      0  -  7 byte: 8-byte. used to store real key for numeric key
     *      8  - 15 byte: the pointer to record
    */ 
    class CellMeta256V2 {
    public:
        static constexpr uint16_t BitMapMask   = 0xFFFC;
        static constexpr int CellSizeLeftShift = 8;
        static constexpr int SlotSizeLeftShift = 4;
        using H2Tag = uint16_t;
        using H1Tag = typename std::conditional<is_key_flat, Key, uint64_t>::type;

        template<bool flat_key>
        struct H1Convert {};
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-parameter"
        template<>
        struct H1Convert<false> {
            inline H1Tag operator()(const Key& key, uint64_t hash) {
                return hash;
            }
        };
        template<>
        struct H1Convert<true> {
            inline H1Tag operator()(const Key& key, uint64_t hash) {
                return key;
            }
        };
        #pragma GCC diagnostic pop

        
        /** PartialHash
         *  @note: a 64-bit hash is used to locate the cell location, and provide hash-tag.
         *  @format:
         *  | MSB    - - - - - - - - - - - - - - - - - - LSB |
         *  |     32 bit     |   16 bit  |  8 bit  |  8 bit  |
         *  |    bucket_hash |    H2     |                   |
         *  |                     H1                         |
         *  ! if Key is numeric, H1 store the key itself, and we can skip key comparison in finding slot
        */
        struct PartialHash {
            PartialHash(const Key& key, uint64_t hash) :
                H1_( H1Convert<is_key_flat>{}(key, hash) ),
                H2_( (hash >> 16) & 0xFFFF ),
                bucket_hash_( hash >> 32 )
                { };
            H1Tag     H1_; // H1: 
            H2Tag     H2_; // H2: 1 byte hash tag in CellMeta for parallel comparison using SIMD cmd
            uint32_t  bucket_hash_; // used to locate in the bucket directory
        }; // end of class PartialHash

        /** HashSlot
         *  @node:
        */
        struct HashSlot{
            uint64_t entry;      // pointer to key-value record
            H1Tag    H1;         // hash value of the key
        };

        explicit CellMeta256V2(char* rep) {
            meta_   = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rep));
            bitmap_ = *(uint32_t*)(rep);             // the lowest 32bit is used as bitmap
            bitmap_ &= BitMapMask;                   // hidden the 0 - 1 bit in bitmap
        }

        ~CellMeta256V2() {
        }

        // return a bitset, the slot that matches the hash is set to 1
        inline util::BitSet MatchBitSet(uint16_t hash) {
            auto bitset = _mm256_set1_epi16(hash);
            uint16_t mask = _mm256_cmpeq_epi16_mask(bitset, meta_);
            return util::BitSet(mask & bitmap_);
        }

        // return a bitset, indicating the availabe slot
        inline util::BitSet EmptyBitSet() {
            return util::BitSet((~bitmap_) & BitMapMask);
        }

        inline util::BitSet ValidBitSet() {
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

        inline static constexpr uint8_t StartSlotPos() {
            return 2;
        }

        inline static constexpr uint32_t CellSize() {
            // cell size (include meta) in byte
            return 256;
        }

        inline static constexpr uint32_t SlotMaxRange() {
            // used for rehashing. Though we have 16 slots, one is reserved for update and deletion.
            return 15;
        }
        
        inline static constexpr uint32_t SlotCount() {
            // slot count
            return 14;
        }

        inline static std::string Name() {
            return "CellMeta256V2";
        }

        inline static constexpr size_t size() {
            // the meta size in byte in current cell
            return 32;
        }

        inline uint16_t bitmap() {
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

        std::string ToString() {
            return BitMapToString();
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

        __m256i     meta_;          // 32 byte integer vector
        uint16_t    bitmap_;        // 1: occupied, 0: empty or deleted
    }; // end of class CellMeta256V2

    /* #endregion: for CellMeta */

    /* #region: for CellAllocator */

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
            is_hugepage_(false),
            block_id_(block_id) {
            // Allocate memory space for this MemBlock
            size_t space = size_ * CellMeta::CellSize();
            if ((space & 0xfff) != 0) {
                // space is not several times of 4KB
                TURBO_ERROR("MemBlock size is not 4KB aligned. Space size: " << space);
                exit(1);
            }
            
            #ifdef TURBO_ENABLE_HUGEPAGE
            start_addr_ = (char*) mmap(TURBO_HUGE_ADDR, space, TURBO_HUGE_PROTECTION, TURBO_HUGE_FLAGS, -1, 0);
            if (start_addr_ == MAP_FAILED) {
                TURBO_WARNING("MemBlock mmap hugepage fail. space: " << space);
                is_hugepage_ = false;
                start_addr_ = (char* ) aligned_alloc(CellMeta::CellSize(), space);
                if (start_addr_ == nullptr) {
                    TURBO_ERROR("MemBlock malloc space fail. Space size: " << space);
                    exit(1);
                }
            }
            #else
            is_hugepage_ = false;
            start_addr_ = (char* ) aligned_alloc(CellMeta::CellSize(), space);
            if (start_addr_ == nullptr) {
                TURBO_ERROR("MemBlock malloc space fail. Space size: " << space);
                exit(1);
            }
            #endif
            cur_addr_ = start_addr_;
        }

        ~MemBlock() {
            /* munmap() size_ of MAP_HUGETLB memory must be hugepage aligned */
            size_t space = size_ * CellMeta::CellSize();
            if (is_hugepage_) {
                if (munmap(start_addr_, space)) {
                    TURBO_ERROR("munmap hugepage fail. munmap size: " << space);
                    exit(1);
                }
            } else {
                free(start_addr_);
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

        // Clear all the content to '0'
        inline void Reset(void) {
            size_t space = size_ * CellMeta::CellSize();
            memset(start_addr_, 0, space);
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
        CellAllocator(int initial_blocks = 6):
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
        inline std::pair<int /* block id */, char* /* addr */> AllocateNoSafe(size_t count) {
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
            MemBlock<CellMeta>* res = free_mem_block_list_.front();
            free_mem_block_list_.pop_front();
            // res->Reset();
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

    /* #endregion: for CellAllocator */

    /* #region: for ProbeStrategy */

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

    /* #endregion: for ProbeStrategy */

    static_assert(kCellCountLimit <= 32768, "kCellCountLimit needs to be <= 32768");
    
    using CellMeta      =    typename std::conditional<CellMetaType <= 0, CellMeta128,
                             typename std::conditional<CellMetaType <= 1, CellMeta256,
                                                                          CellMeta256V2>::type>::type;
    using ProbeStrategy = typename std::conditional<ProbeStrategyType <= 0, ProbeWithinBucket,
                                                                            ProbeWithinCell>::type;
    using WHash     = WrapHash<Hash>;
    using WKeyEqual = WrapKeyEqual<KeyEqual>;
    using HashSlot  = typename CellMeta::HashSlot;
    using H2Tag     = typename CellMeta::H2Tag;
    using H1Tag     = typename CellMeta::H1Tag;
    using PartialHash = typename CellMeta::PartialHash;

    static_assert(sizeof(HashSlot) == 8 || sizeof(HashSlot) == 16, "HashSlot size error.");
    /** SlotInfo
     *  @note: use to store the target slot location info
    */
    class SlotInfo {
    public:
        uint32_t bucket;        // bucket index
        uint16_t cell;          // cell index
        uint8_t  slot;          // slot index  
        uint8_t  old_slot;      // if equal_key, this save old slot position      
        H1Tag    H1;            // hash-tag in HashSlot
        H2Tag    H2;            // hash-tag in CellMeta
        bool equal_key;         // If we find a equal key in this slot        
        SlotInfo(uint32_t b, uint32_t a, int s, H1Tag h1, H2Tag h2, bool euqal, int os = 0):
            bucket(b),
            cell(a),
            slot(s),
            old_slot(os),                 
            H1(h1),
            H2(h2),
            equal_key(euqal) {}
        SlotInfo():
            bucket(0),
            cell(0),
            slot(0),            
            old_slot(0),                  
            H1(0),
            H2(0),
            equal_key(false) {}
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wformat"
        std::string ToString() {
            char buffer[128];
            sprintf(buffer, "b: %4u, c: %4u, s: %2u, H2: 0x%04x, H1: 0x%lx",
                bucket,
                cell,
                slot,
                H2,
                H1);
            return buffer;
        }
        #pragma GCC diagnostic pop
    }; // end of class SlotInfo

    /** RecordAllocator
     *  @note: allocate memory space for new record
    */
    class RecordAllocator {
        public:
            inline char* Allocate(size_t size) {
                return reinterpret_cast<char*>(malloc(size));
            }

            inline void Release(char* addr) {
                free(addr);
            }
    };

    /** RecordPtr
     *  @note: store the data pointer 
    */
    class RecordPtr { 
    public:
        explicit RecordPtr(uint64_t u64_addr_off):
            data_ptr_(reinterpret_cast<value_type*>(u64_addr_off)) {}

        explicit RecordPtr(char* addr):
            data_ptr_(reinterpret_cast<value_type*>(addr)) {}

        const value_type& operator*() const noexcept {
            return *data_ptr_;
        }

        value_type const* operator->() const noexcept {
            return data_ptr_;
        }

        value_type* operator->() noexcept {
            return data_ptr_;
        }

        value_type* data_ptr_;
    };

    /** BucketMeta
     *  @note: a 8-byte 
    */
    class BucketMeta {
    public:
        explicit BucketMeta(char* addr, uint16_t cell_count) {
            data_ = (((uint64_t) addr) << 16) | (__builtin_ctz(cell_count) << 12);
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
            return CellCount() - 1;
        }

        inline uint16_t CellCount() {
            return  ( 1 << ((data_ >> 12) & 0xF) );
        }

        inline void Reset(char* addr, uint16_t cell_count) {
            data_ = (data_ & 0x0000000000000FFF) | (((uint64_t) addr) << 16) | (__builtin_ctz(cell_count) << 12);
        }

        inline void Lock(void) {
            lock_.lock();
        }

        inline void Unlock(void) {
            lock_.unlock();
        }

        inline bool IsLocked(void) {
            return lock_.is_locked();
        }

        // LSB
        // | 8 bit lock | 4 bit reserved | 4 bit cell mask | 48 bit address |
        union {
            util::AtomicSpinLock lock_;
            uint64_t data_;
        };
    };

    /** Usage: iterator every slot in the bucket, return the pointer in the slot
     *  BucketIterator<CellMeta> iter(bucket_addr, cell_count_);
     *  while (iter.valid()) {
     *      ...
     *      ++iter;
     *  }
    */        
    class BucketIterator {
    public:
    typedef std::pair<SlotInfo, HashSlot> InfoPair;
        BucketIterator(uint32_t bi, char* bucket_addr, size_t cell_count, size_t cell_i = 0):
            bi_(bi),
            cell_count_(cell_count),
            cell_i_(cell_i),
            bitmap_(0),
            bucket_addr_(bucket_addr) {

            assert(bucket_addr != 0);
            CellMeta meta(bucket_addr);
            bitmap_ = meta.ValidBitSet();
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

        inline InfoPair operator*() const {
            // return the cell index, slot index and its slot content
            uint8_t slot_index  = *bitmap_;
            char* cell_addr     = bucket_addr_ + cell_i_ * CellMeta::CellSize();
            HashSlot* slot      = locateSlot(cell_addr, slot_index);
            H2Tag H2            = *locateH2Tag(cell_addr, slot_index);
            return  { { bi_ /* ignore bucket index */, 
                        cell_i_ /* cell index */, 
                        *bitmap_ /* slot index*/, 
                        static_cast<H1Tag>(slot->H1), 
                        H2, 
                        false,
                        0}, 
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

    // only provide const Iter
    // TODO:
    class Iter {
        private:
            using NodePtr = RecordPtr;
        
        public:
            using difference_type = std::ptrdiff_t;
            using value_type = typename Self::value_type;
            using reference  = typename std::conditional<true, value_type const&, value_type&>::type;
            using pointer    = typename std::conditional<true, value_type const*, value_type*>::type;
            using iterator_category = std::forward_iterator_tag;

            Iter(Iter const& other) noexcept :
                bi_(other.bi_),
                bucket_iter_(other.bucket_iter_) {}

        private:
            uint32_t        bi_;
            BucketIterator  bucket_iter_;
    }; // end of class Iter

public:
    static constexpr int kSizeVecCount = 1 << 4;
    explicit TurboHashTable(uint32_t bucket_count = 128 << 10, uint32_t cell_count = 32):
        bucket_count_(bucket_count),
        bucket_mask_(bucket_count - 1),
        capacity_( bucket_count * cell_count * (CellMeta::SlotCount() - 1) ),
        size_(0) {
        if (!isPowerOfTwo(bucket_count) ||
            !isPowerOfTwo(cell_count)) {
            printf("the hash table size setting is wrong. bucket: %u, cell: %u\n", bucket_count, cell_count);
            exit(1);
        }

        size_t bucket_meta_space = bucket_count * sizeof(BucketMeta);             
        BucketMeta* buckets_addr = nullptr;

        #ifdef TURBO_ENABLE_HUGEPAGE
        size_t bucket_meta_space_huge = (bucket_meta_space + TURBO_HUGEPAGE_SIZE - 1) / TURBO_HUGEPAGE_SIZE * TURBO_HUGEPAGE_SIZE;
        buckets_addr = (BucketMeta*) mmap(TURBO_HUGE_ADDR, bucket_meta_space_huge, TURBO_HUGE_PROTECTION, TURBO_HUGE_FLAGS, -1, 0);
        if (buckets_addr == MAP_FAILED) {
            buckets_addr = (BucketMeta* ) aligned_alloc(sizeof(BucketMeta), bucket_meta_space);            
            if (buckets_addr == nullptr) {
                fprintf(stderr, "malloc %lu space fail.\n", bucket_meta_space);
                exit(1);
            }
            TURBO_INFO(" Allocated: " << bucket_meta_space << " for BucketMeta.");
            memset((void*)buckets_addr, 0, bucket_meta_space);
        }
        else {
            TURBO_INFO(" Allocated: " << bucket_meta_space_huge << " hugepage for BucketMeta.");
            memset((void*)buckets_addr, 0, bucket_meta_space_huge);
        }
        #else
        buckets_addr = (BucketMeta* ) aligned_alloc(sizeof(BucketMeta), bucket_meta_space);
        if (buckets_addr == nullptr) {
            fprintf(stderr, "malloc %lu space fail.\n", bucket_meta_space);
            exit(1);
        }
        TURBO_INFO(" Allocated: " << bucket_meta_space);
        memset(buckets_addr, 0, bucket_meta_space);
        #endif
        
        buckets_ = buckets_addr;
        buckets_mem_block_ids_ = new int[bucket_count];
        for (size_t i = 0; i < bucket_count; ++i) {
            auto res = cell_allocator_.AllocateNoSafe(cell_count);
            memset(res.second, 0, cell_count * kCellSize);
            buckets_[i].Reset(res.second, cell_count);
            buckets_mem_block_ids_[i] = res.first;
        }

    }

    void DebugInfo()  {
        printf("%s\n", PrintCellAllocator().c_str());
    }

    std::string PrintCellAllocator() {
        return cell_allocator_.ToString();
    }

    /** MinorReHashAll
     *  @note: not thread safe. This global rehashing will double the hash table capacity.
     *  ! 
    */
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
                TURBO_ERROR("Out of capacity");
                exit(1);
            }
            std::pair<int /* block id */, char* /* addr */> res = 
                cell_allocator_.AllocateNoSafe(new_cell_count);
            if (res.second == nullptr) {
                printf("Cell allocation error\n");
                TURBO_ERROR("Cell allocation error.");
                exit(1);
            }
            old_mem_block_ids[i] = buckets_mem_block_ids_[i];
            buckets_mem_block_ids_[i] = res.first;
            new_mem_block_addr[i] = res.second;
            capacity_.fetch_add(bucket_meta.CellCount() * (CellMeta::SlotCount() - 1));
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
        double rehash_duration = util::NowMicros() - rehash_start;
        printf("Real rehash speed: %f Mops/s. entries: %lu, duration: %.2f s.\n", (double)rehash_count / rehash_duration, rehash_count.load(), rehash_duration/1000000.0);

        // release the old mem block space
        for (size_t i = 0; i < bucket_count_; ++i) {
            cell_allocator_.ReleaseNoSafe(old_mem_block_ids[i]);
        }

        free(old_mem_block_ids);
        free(new_mem_block_addr);
    }

    // return the cell index and slot index
    inline std::pair<uint16_t, uint8_t> findNextSlotInRehash(uint8_t* slot_vec, H1Tag h1, uint16_t cell_count_mask) {
        uint16_t ai = H1ToHash(h1) & cell_count_mask;
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

        // Step 1. Create new bucket and initialize its meta
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
        // Reset all cell's meta data
        for (size_t i = 0; i < new_cell_count; ++i) {
            char* des_cell_addr = new_bucket_addr + (i << kCellSizeLeftShift);
            memset(des_cell_addr, 0, CellMeta::size());
        }

        // iterator old bucket and insert slots info to new bucket
        // old: |11111111|22222222|33333333|44444444|   
        //       ========>
        // new: |1111    |22222   |333     |4444    |1111    |222     |33333   |4444    |

        // Step 2. Move the meta in old bucket to new bucket
        //      a) Record next avaliable slot position of each cell within new bucket for rehash
        uint8_t* slot_vec = (uint8_t*)malloc(new_cell_count);
        memset(slot_vec, CellMeta::StartSlotPos(), new_cell_count);   
        BucketIterator iter(bi, bucket_meta.Address(), bucket_meta.CellCount()); 
        //      b) Iterate every slot in this bucket
        while (iter.valid()) { 
            count++;
            // Step 1. obtain old slot info and slot content
            std::pair<SlotInfo, HashSlot> res = *iter;

            // Step 2. update bitmap, H2, H1 and slot pointer in new bucket
            //      a) find valid slot in new bucket
            std::pair<uint16_t /* cell index */, uint8_t /* slot index */> valid_slot = 
            findNextSlotInRehash(slot_vec, res.first.H1, new_cell_count_mask);
            //      b) obtain des cell addr
            char* des_cell_addr = new_bucket_addr + (valid_slot.first << kCellSizeLeftShift);            
            if (valid_slot.second >= CellMeta::SlotMaxRange()) {                
                printf("rehash fail: %s\n", res.first.ToString().c_str());
                TURBO_ERROR("Rehash fail: " << res.first.ToString());
                printf("%s\n", PrintBucketMeta(res.first.bucket).c_str());
                exit(1);
            }
            //      c) move the slot meta to new bucket
            moveSlot(des_cell_addr, valid_slot.second /* des_slot_i */, res.first, res.second);

            // Step 3. to next old slot
            ++iter;
        }        
        //      c) set remaining slots' slot pointer to 0 (including the backup slot)
        for (uint32_t ci = 0; ci < new_cell_count; ++ci) {
            char* des_cell_addr = new_bucket_addr + (ci << kCellSizeLeftShift);
            for (uint8_t si = slot_vec[ci]; si <= CellMeta::SlotMaxRange(); si++) {
                HashSlot* des_slot  = locateSlot(des_cell_addr, si);
                des_slot->entry = 0;
                des_slot->H1    = 0;
            }   
        }

        // Step 3. Reset bucket meta in buckets_
        buckets_[bi].Reset(new_bucket_addr, new_cell_count);
        
        free(slot_vec);
        return count;
    }

    ~TurboHashTable() {

    }

    template<typename HashKey>
    inline size_t KeyToHash(HashKey& key) {
        using Mix =
            typename std::conditional<std::is_same<::turbo::hash<Key>, hasher>::value,
                                      ::turbo::identity_hash<size_t>,
                                      ::turbo::hash<size_t>>::type;
        return Mix{}(WHash::operator()(key));
    }

    template<typename K>
    struct IdenticalReturn {
        constexpr K operator()(K const& key) const noexcept {
            return key;
        }
    };

    // For CellMeta256V2, H1 may be used to store real key,
    // we need to calculate the real hash of h1 accordingly.
    // If key is flat (store the real key), we need to hash h1.    
    inline size_t H1ToHash(H1Tag h1) {
        using ToHash = typename std::conditional<is_key_flat,
                                        ::turbo::hash<size_t>,
                                        IdenticalReturn<H1Tag> >::type;
        return ToHash{}(h1);
    }

    /** Put
     *  @note: insert or update a key-value record, return false if fails.
    */  
    bool Put(const Key& key, const T& value)  {
        // calculate hash value of the key
        size_t hash_value   = KeyToHash(key);
        
        // allocate space to store record
        // size_t buf_len      = value_type::FormatLength(key, value);
        // void*  buffer       = record_allocator_.Allocate(buf_len);
        // value_type* record  = static_cast<value_type*>(buffer);
        // record->type        = kTypeValue;
        // record->Encode(key, value);

        // update index, thread safe
        return insertSlot(kTypeValue, key, value, hash_value);
    }
    
    // Return the entry if key exists
    bool Get(const Key& key, T* value)  {
        // calculate hash value of the key
        size_t hash_value = KeyToHash(key);

        auto res = findSlot(key, hash_value);
        if (res.second) {
            // find a key in hash table
            RecordPtr record(res.first.entry);
            if (record->type == kTypeValue) {
                *value = record->second();                
                return true;
            }
            else if (record->type == kTypeDeletion) {
                // this key has been deleted
                return false;
            }
        }
        return false;
    }

    value_type* Probe(const Key& key)  {
        // calculate hash value of the key
        size_t hash_value = KeyToHash(key);

        auto res = probeFirstSlot(key, hash_value);
        if (res.second) {
            // probe a key having same H2 and H1 tag
            RecordPtr record_ptr(res.first.entry);
            return record_ptr.data_ptr_;
        }
        return nullptr;
    }

    value_type* Find(const Key& key)  {
        // calculate hash value of the key
        size_t hash_value = KeyToHash(key);

        auto res = findSlot(key, hash_value);
        if (res.second) {
            RecordPtr record_ptr(res.first.entry);
            return record_ptr.data_ptr_;
        }
        return nullptr;
    }

    void Delete(const Key& key) {
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
            BucketIterator iter(i, bucket_meta.Address(), bucket_meta.info.cell_count);
            if (iter.valid()) {
                printf("%s\n", PrintBucketMeta(i).c_str());
            }
        }
    }

    void IterateBucket(uint32_t i) {
        auto& bucket_meta = locateBucket(i);
        BucketIterator iter(i, bucket_meta.Address(), bucket_meta.CellCount());
        while (iter.valid()) {
            auto res = (*iter);
            SlotInfo& info = res.first;
            info.bucket = i;
            HashSlot& slot = res.second;
            RecordPtr record(slot.entry);
            std::cout << info.ToString() << ", addr: " << slot.entry << ". key: " << record->first() << ", value: " << record->second() << std::endl;
            ++iter;
        }
    }

    void IterateAll() {
        size_t count = 0;
        for (size_t i = 0; i < bucket_count_; ++i) {
            auto& bucket_meta = locateBucket(i);
            BucketIterator iter(i, bucket_meta.Address(), bucket_meta.CellCount());
            while (iter.valid()) {
                auto res = (*iter);
                SlotInfo& info = res.first;
                HashSlot& slot = res.second;
                RecordPtr record(slot.entry);
                std::cout << info.ToString() << ", addr: " << slot.entry << ". key: " << record->first() << ", value: " << record->second() << std::endl;
                ++iter;
                count++;
            }
        }
        printf("iterato %lu entries. total size: %lu\n", count, Size());
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
            sprintf(buffer, "\t%4u - 0x%12lx: %s. Cell valid slot count: %d. ", i++, (uint64_t)cell_addr, meta.BitMapToString().c_str(), count);            
            res += buffer;
            auto valid_bitset = meta.ValidBitSet();
            for (int i : valid_bitset) {
                std::ostringstream ss;
                HashSlot* slot = locateSlot(cell_addr, i);
                ss << "s" << i << "H1: " << slot->H1 << ", ";
                res += ss.str();
            }
            res += "\n";
            probe.next();
            count_sum += count;
        }
        sprintf(buffer, "\tBucket %u: valid slot count: %d. Load factor: %f\n", bucket_i, count_sum, (double)count_sum / ((CellMeta::SlotCount() - 1) * meta.CellCount()));
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
  
    static inline HashSlot* locateSlot(char* cell_addr, int slot_i) {
        return reinterpret_cast<HashSlot*>(cell_addr + (slot_i << CellMeta::SlotSizeLeftShift));
    }

    static inline H2Tag* locateH2Tag(char* cell_addr, int slot_i) {
        return reinterpret_cast<H2Tag*>(cell_addr) + slot_i;
    }

    // used in rehash function, move slot to new cell_addr
    inline void moveSlot(char* des_cell_addr, uint8_t des_slot_i,const SlotInfo& old_info, const HashSlot& old_slot) {
        // move slot content, including H1 and pointer
        HashSlot* des_slot  = locateSlot(des_cell_addr, des_slot_i);
        des_slot->entry     = old_slot.entry;
        des_slot->H1        = old_info.H1;
        
        // locate H2 and set H2
        H2Tag* h2_tag_ptr   = locateH2Tag(des_cell_addr, des_slot_i);
        *h2_tag_ptr         = old_info.H2;

        // obtain and set bitmap
        decltype(CellMeta::bitmap_)* bitmap = (decltype(CellMeta::bitmap_) *)des_cell_addr;
        *bitmap = (*bitmap) | (1 << des_slot_i);
    }

    /** insertToSlotAndRecycle
     *  @note: Reuse or recycle the space of target slot's old entry.
     *         Set bitmap, H2, H1, pointer.
    */
    inline void insertToSlotAndRecycle(ValueType type, const Key& key, const T& value, char* cell_addr, const SlotInfo& info) {
        // locate the target slot
        HashSlot* slot  = locateSlot(cell_addr, info.slot);

        // Check if the old entry points to some out-dated data, reuse the space or free it.
        value_type* record = nullptr;
        if (slot->entry != 0) {
            // We need to recycle the space pointed by the old slot's entry            
            RecordPtr old_record_ptr(slot->entry);
            size_t old_record_size = old_record_ptr->Size();
            size_t new_record_size = value_type::FormatLength(key, value);

            if (old_record_size >= new_record_size) {
                // reuse old space of old record
                record  = old_record_ptr.data_ptr_;
                record->type        = type;
                record->Encode(key, value);
            }
            else {
                // old space is not enough, we free old space
                TURBO_INFO("Free old slot. Key: " << old_record_ptr->first() << ", Value: " << old_record_ptr->second());
                record_allocator_.Release((char*)old_record_ptr.data_ptr_);

                // Then allocate new space                
                void*  buffer   = record_allocator_.Allocate(new_record_size);
                record          = static_cast<value_type*>(buffer);
                record->type    = type;
                record->Encode(key, value);
            }
        } 
        else 
        {
            // the old entry points to empty space, we allocate space to store new record
            size_t buf_len  = value_type::FormatLength(key, value);
            void*  buffer   = record_allocator_.Allocate(buf_len);
            record          = static_cast<value_type*>(buffer);
            record->type    = type;
            record->Encode(key, value);
        }
        slot->entry     = reinterpret_cast<uint64_t>(record); /* transform to 8-byte UL */
        slot->H1        = info.H1;
       
        // set H2
        H2Tag* h2_tag_ptr   = locateH2Tag(cell_addr, info.slot);
        *h2_tag_ptr         = info.H2;

        // add a fence here. 
        // Make sure the bitmap is updated after H2
        // https://www.modernescpp.com/index.php/fences-as-memory-barriers
        // https://preshing.com/20130922/acquire-and-release-fences/
        std::atomic_thread_fence(std::memory_order_release);

        // obtain bitmap and set bitmap
        decltype(CellMeta::bitmap_)* bitmap = (decltype(CellMeta::bitmap_) *)cell_addr;
        if ( true == info.equal_key) {
            // Update: set the new slot and toggle the old slot
            auto new_bitmap = ( (*bitmap) | (1 << info.slot) ) ^ ( 1 << info.old_slot );
            TURBO_COMPILER_FENCE();
            *bitmap = new_bitmap;
        }
        else {
            // Insertion: set the new slot
            *bitmap = (*bitmap) | (1 << info.slot);
        }
    }

    inline bool insertSlot(ValueType type, const Key& key, const T& value, size_t hash_value) {
        // Obtain the partial hash
        PartialHash partial_hash(key, hash_value);

        // Check if the bucket is locked for rehashing. Wait entil is unlocked.
        BucketMeta& bucket_meta = locateBucket(bucketIndex(partial_hash.bucket_hash_));
        while (bucket_meta.IsLocked()) {
            TURBO_CPU_RELAX();
        }

        bool retry_find = false;
        do { // concurrent insertion may find same position for insertion, retry insertion if neccessary

            std::pair<SlotInfo, bool /* find a slot or not */> res = findSlotForInsert(key, partial_hash);

            // find a valid slot in target cell
            if (res.second) { 
                char* cell_addr = locateCell({res.first.bucket, res.first.cell});

                util::SpinLockScope<0> lock_scope(cell_addr); // Lock current cell

                CellMeta meta(cell_addr);   // obtain the meta part

                if (TURBO_LIKELY( !meta.Occupy(res.first.slot) )) { 
                    // If the new slot from 'findSlotForInsert' is not occupied, insert directly

                    insertToSlotAndRecycle(type, key, value, cell_addr, res.first); // update slot content (including pointer and H1), H2 and bitmap

                    // TODO: use thread_local variable to improve write performance
                    // if (!res.first.equal_key) {
                    //     size_.fetch_add(1, std::memory_order_relaxed); // size + 1
                    // }

                    return true;
                } else if (res.first.equal_key) {
                    // If this is an update request and the backup slot is occupied,
                    // it means the backup slot has changed in current cell. So we 
                    // update the slot location.
                    util::BitSet empty_bitset = meta.EmptyBitSet(); 
                    if (TURBO_UNLIKELY(!empty_bitset)) {
                        TURBO_ERROR(" Cannot update.");
                        printf("Cannot update.\n");
                        exit(1);
                    }

                    res.first.slot = *empty_bitset;
                    insertToSlotAndRecycle(type, key, value, cell_addr, res.first);
                    return true;
                } else { 
                    // current new slot has been occupied by another concurrent thread.

                    // Before retry 'findSlotForInsert', find if there is any empty slot for insertion
                    util::BitSet empty_bitset = meta.EmptyBitSet();
                    if (empty_bitset.validCount() > 1) {
                        res.first.slot = *empty_bitset;
                        insertToSlotAndRecycle(type, key, value, cell_addr, res.first);
                        return true;
                    }

                    // Current cell has no empty slot for insertion, we retry 'findSlotForInsert'
                    // This unlikely happens with all previous effort.
                    #ifdef TURBO_ENABLE_LOGGING
                    TURBO_INFO("retry find slot in Bucket " << res.first.bucket <<
                               ". Cell " << res.first.cell <<
                               ". Slot " << res.first.slot <<
                               ". Key: " << key );
                    #endif
                    retry_find = true;
                }
            }
            else { // cannot find a valid slot for insertion, rehash current bucket then retry
                #ifdef TURBO_ENABLE_LOGGING
                TURBO_INFO("=========== Need Rehash Bucket " << res.first.bucket << " ==========");   
                TURBO_INFO(PrintBucketMeta(res.first.bucket));           
                #endif

                break;
            }
        } while (retry_find);
        
        return false;
    }

    
    inline bool slotKeyEqual(const Key& key, RecordPtr record_ptr) {
        if (is_key_flat) {
            return true;
        }
        return WKeyEqual::operator()( key, record_ptr->first() );
    }

    /** findSlotForInsert
     *  @note:  Find a valid slot for insertion.
     *  @out:   first: the slot info that should insert the key
     *          second:  whether we can find a valid (empty or belong to the same key) slot for insertion
     *  ! We cannot insert if the second is false.
    */
    inline std::pair<SlotInfo, bool> findSlotForInsert(const Key& key, PartialHash& partial_hash) {        
        uint32_t bucket_i = bucketIndex(partial_hash.bucket_hash_);
        auto& bucket_meta = locateBucket(bucket_i);
        ProbeStrategy probe(H1ToHash(partial_hash.H1_), bucket_meta.CellCountMask(), bucket_i);

        int probe_count = 0; // limit probe times
        while (probe && (probe_count++ < ProbeStrategy::MAX_PROBE_LEN)) {
            // Go to target cell
            auto offset = probe.offset();
            char* cell_addr = locateCell(offset);
            CellMeta meta(cell_addr);

            for (int i : meta.MatchBitSet(partial_hash.H2_)) {  // if there is any H2 match in this cell (SIMD)
                                                                // i is the slot index in current cell
                // locate the slot reference
                const HashSlot& slot = *locateSlot(cell_addr, i);

                if (TURBO_LIKELY(slot.H1 == partial_hash.H1_)) // compare if the H1 partial hash is equal
                {  
                    // Obtain record pointer
                    RecordPtr record(slot.entry);

                    if (TURBO_LIKELY(slotKeyEqual(key, record)))
                    {  
                        // This is an update request
                        // TURBO_DEBUG( "Update key" << key << "\n" << 
                        //               meta.ToString());
                        util::BitSet empty_bitset = meta.EmptyBitSet(); 
                        return {{   offset.first,           /* bucket */
                                    offset.second,          /* cell */
                                    *empty_bitset,          /* new slot */
                                    partial_hash.H1_,       /* H1 */ 
                                    partial_hash.H2_,       /* H2 */
                                    true,                   /* equal_key */
                                    i                       /* old slot */
                                    },                  
                                true};
                    }
                    else {                                  // two key is not equal, go to next slot
                        #ifdef LTHASH_DEBUG_OUT
                        std::cout << "H1 conflict. Slot (" << offset.first 
                                    << " - " << offset.second 
                                    << " - " << i
                                    << ") bitmap: " << meta.BitMapToString() 
                                    << ". Insert key: " << key 
                                    <<  ". Hash: 0x" << std::hex << hash_value << std::dec
                                    << " , Slot key: " << record->first() << std::endl;
                        #endif
                    }
                }
            }
            
            // If there is more than one empty slot, return one.
            util::BitSet empty_bitset = meta.EmptyBitSet(); 
            if (empty_bitset.validCount() > 1) {                    
                    // return an empty slot for new insertion            
                    return {{   offset.first,           /* bucket */
                                offset.second,          /* cell */
                                *empty_bitset,          /* pick a slot, empty_bitset.pickOne() */ 
                                partial_hash.H1_,       /* H1 */
                                partial_hash.H2_,       /* H2 */
                                false                   /* equal_key */
                                }, 
                            true};                
            }
            
            // probe the next cell in the same bucket
            probe.next(); 
        }

        #ifdef LTHASH_DEBUG_OUT
        TURBO_INFO("Fail to find one empty slot");
        TURBO_INFO(PrintBucketMeta(bucket_i));
        #endif

        // only when all the probes fail and there is no empty slot
        // exists in this bucket. 
        return {{
                    bucket_i,
                    0,
                    0, 
                    partial_hash.H1_, 
                    partial_hash.H2_, 
                    false
                    },
                false};
    }
    
    inline std::pair<HashSlot, bool> findSlot(const Key& key, size_t hash_value) {
        PartialHash partial_hash(key, hash_value);
        uint32_t bucket_i = bucketIndex(partial_hash.bucket_hash_);
        auto& bucket_meta = locateBucket(bucket_i);
        ProbeStrategy probe(H1ToHash(partial_hash.H1_),  bucket_meta.CellCountMask(), bucket_i);

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
                    RecordPtr record(slot.entry);                    
                    if (TURBO_LIKELY(slotKeyEqual(key, record)))
                    {
                        return {slot, true};
                    }
                    else {
                        // TURBO_INFO("H1 conflict. Slot (" << offset.first 
                        //             << " - " << offset.second 
                        //             << " - " << i
                        //             << ") bitmap: " << meta.BitMapToString() 
                        //             << ". Insert key: " << key 
                        //             <<  ". Hash: 0x" << std::hex << hash_value << std::dec
                        //             << " , Slot key: " << record->first()
                        //             );
                    }
                    
                }
            }

            // If this cell still has more than one empty slot, then it means the key does't exist.
            util::BitSet empty_bitset = meta.EmptyBitSet();
            if (empty_bitset.validCount() > 1) {
                HashSlot empty_slot;
                return {empty_slot, false};
            }
            
            probe.next();
        }
        
        // after all the probe, no key exist
        HashSlot empty_slot;
        return {empty_slot, false};
    }

    inline std::pair<HashSlot, bool> probeFirstSlot(const Key& key, size_t hash_value) {
        PartialHash partial_hash(key, hash_value);
        uint32_t bucket_i = bucketIndex(partial_hash.bucket_hash_);
        auto& bucket_meta = locateBucket(bucket_i);
        ProbeStrategy probe(H1ToHash(partial_hash.H1_),  bucket_meta.CellCountMask(), bucket_i);

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
                    return {slot, true};
                }
            }

            // If this cell still has more than one empty slot, then it means the key does't exist.
            util::BitSet empty_bitset = meta.EmptyBitSet();
            if (empty_bitset.validCount() > 1) {
                HashSlot empty_slot;
                return {empty_slot, false};
            }
            
            probe.next();
        }
        
        // after all the probe, no key exist
        HashSlot empty_slot;
        return {empty_slot, false};
    }

    inline bool isPowerOfTwo(uint32_t n) {
        return (n != 0 && __builtin_popcount(n) == 1);
    }

private:
    CellAllocator<CellMeta, kCellCountLimit> cell_allocator_;
    RecordAllocator                          record_allocator_;
    BucketMeta*   buckets_;
    int*          buckets_mem_block_ids_;
    const size_t  bucket_count_ = 0;
    const size_t  bucket_mask_  = 0;
    std::atomic<size_t> capacity_;
    std::atomic<size_t> size_;

    static constexpr int       kCellSize           = CellMeta::CellSize();
    static constexpr int       kCellSizeLeftShift  = CellMeta::CellSizeLeftShift;
};

}; // end of namespace turbo::detail

//  *  @note: CellMetaType:
//  *              0 - CellMeta128
//  *              1 - CellMeta256
//  *              2 - CellMeta256V2
//  *         ProbeStrategyType:
//  *              0 - ProbeWithinBucket
//  *              1 - ProbeWithinCell
// When using std::string for Key, the KeyEqual uses std::equal_to<util::Slice>
template <typename Key, typename T, typename Hash = hash<Key>,
          typename KeyEqual = std::equal_to<Key> >
using unordered_map = detail::TurboHashTable<
                                            Key, T, Hash, 
                                            typename std::conditional< std::is_same<Key, std::string>::value == false /* is numeric */, 
                                                                                        KeyEqual, 
                                                                                        std::equal_to<util::Slice> >::type,
                                            std::is_same<Key, std::string>::value == false ? 2 : 0 /* CellMetaType */,                                                                                         
                                            0 /* ProbeStrategyType */, 32768>;
}; // end of namespace turbo

#endif
