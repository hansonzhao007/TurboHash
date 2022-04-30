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
            std::cerr << "Register error!"
                      << "pooId: " << pool_id << " path : " << path << std::endl;
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
private:
    union {
        struct {
            uint64_t offset : 48;
            uint64_t pool_id : 16;
        };
        uint64_t _raw;
    };

public:
    pptr () : _raw (0) {}
    pptr (uint16_t _pool_id, uint64_t _off) : pool_id (_pool_id), offset (_off) {}
    pptr (void* ptr) : _raw (reinterpret_cast<uint64_t> (ptr)) {}

    pptr (T* addr = nullptr) noexcept {  // default constructor
        PMEMoid oid = pmemobj_oid (addr);
        this->pool_id = 0;  // turbo hash use pool 0 by default
        this->offset = oid.off;
    };

    pptr (const pptr<T>& p) noexcept {  // copy constructor
        this->_raw = p->_raw;
    }

    template <class F>
    inline operator F* () const {  // cast to transient pointer
        void* base_addr = GetBaseAddr (pool_id);
        return reinterpret_cast<F*> (base_addr + this->offset);
    }

    T* operator-> () {
        void* base_addr = GetBaseAddr (pool_id);
        return reinterpret_cast<T*> (base_addr + this->offset);
    }

    T* getVaddr () {
        if (this->offset == 0) return nullptr;
        return reinterpret_cast<T*> (_raw);
    }
};

class TurboAllocator {
private:
public:
    static bool alloc (uint16_t pool_id, size_t size, void** ptr, PMEMoid* oid) {
        PMEMobjpool* pop = (PMEMobjpool*)GetBaseAddr (pool_id);
        int ret = pmemobj_alloc (pop, oid, size, 0, NULL, NULL);
        if (ret) {
            // alloc erro
            return false;
        }
        *ptr = reinterpret_cast<void*> (((unsigned long)pool_id) << 48 | oid->off);
        return true;
    }

    static void free (void* addr) {
        PMEMoid ptr = pmemobj_oid (addr);
        pmemobj_free (&ptr);
    }
};

};  // namespace turbo_pmem

#endif