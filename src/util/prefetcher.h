#pragma once

namespace util {

// enum FetchLocality {
//     Locality_No_Temporal = 0,   // means it need not be left in the cache after the access
//     Locality_1 = 1,
//     Locality_2 = 2,
//     Locality_3 = 3              // means the data has a high degree of temporal locality and should be left in all levels of cache possible
// };

// https://yunmingzhang.wordpress.com/2019/02/12/software-prefetching-in-c-c/
inline void PrefetchForWrite(const void* addr) {
    __builtin_prefetch(addr, 1, 0);
}

inline void PrefetchForRead(const void* addr) {
    __builtin_prefetch(addr, 0, 1);
}

}