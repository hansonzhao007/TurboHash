#ifndef TURBO_PMDK_PPTR
#define TURBO_PMDK_PPTR

#include <libpmemobj.h>
#include <unistd.h>

#include <iostream>
#include <string>

namespace turbo_pmem {

static inline void* baseAddr[6]{nullptr};
static void* GetBaseAddr (uint16_t pool_id) { return baseAddr[pool_id]; }
static bool RegistPool (uint16_t pool_id, const std::string& path, size_t size, void** root,
                        size_t root_size) {
    PMEMobjpool* pop;
    const char* layout = "turbo_pmdk";
    if (access (path.c_str (), F_OK) != 0) {
        pop = pmemobj_create (path.c_str (), layout, size, 0666);
        if (pop == nullptr) {
            std::cerr << "Register error! "
                      << "path : " << path << std::endl;
            return false;
        }
        printf ("create pool_id: %d, %s\n", pool_id, path.c_str ());
        baseAddr[pool_id] = reinterpret_cast<void*> (pop);
    } else {
        pop = pmemobj_open (path.c_str (), layout);
        if (pop == nullptr) {
            std::cerr << "Register error! "
                      << "path : " << path << std::endl;
            return false;
        }
        printf ("open pool_id: %d, %s\n", pool_id, path.c_str ());
        baseAddr[pool_id] = reinterpret_cast<void*> (pop);
    }
    PMEMoid g_root = pmemobj_root (pop, root_size);
    *root = pmemobj_direct (g_root);
    return true;
}

static bool UnRegistPool (uint16_t pool_id) {
    PMEMobjpool* pop = reinterpret_cast<PMEMobjpool*> (baseAddr[pool_id]);
    if (pop) {
        pmemobj_close (pop);
    }
    return true;
}

template <typename T>
class pptr {
public:
    union {
        struct {
            uint64_t offset : 48;
            uint64_t pool_id : 16;
        };
        uint64_t _raw;
    };

public:
    pptr () : _raw (0) {}
    pptr (uint64_t v) : _raw (v) {}
    pptr (uint16_t _pool_id, uint64_t _off) : pool_id (_pool_id), offset (_off) {}
    pptr (void* ptr) : _raw (reinterpret_cast<uint64_t> (ptr)) {}

    pptr (T* addr = nullptr) noexcept {  // default constructor
        PMEMoid oid = pmemobj_oid (addr);
        this->pool_id = 0;  // turbo hash use pool 0 by default
        this->offset = oid.off;
    };

    template <class F>
    inline operator F* () const {  // cast to transient pointer
        void* base_addr = GetBaseAddr (pool_id);
        return reinterpret_cast<F*> ((char*)base_addr + this->offset);
    }

    template <class F>
    inline pptr& operator= (const F* v) {  // assignment
        PMEMoid oid = pmemobj_oid (v);
        this->pool_id = 0;  // turbo always set pool_id as 0
        this->offset = oid.off;
        return *this;
    }

    // explicit conversion
    inline operator uint64_t () const { return _raw; }

    T* operator-> () {
        void* base_addr = GetBaseAddr (pool_id);
        return reinterpret_cast<T*> ((char*)base_addr + this->offset);
    }

    T* getVaddr () {
        if (this->offset == 0) return nullptr;
        return reinterpret_cast<T*> (_raw);
    }
};

};  // namespace turbo_pmem

#endif