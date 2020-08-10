#pragma once
#include <sys/mman.h>
#include <cassert>
#include <deque>
#include <unordered_map>
#include "cell_meta.h"

namespace turbo
{
// setting huge page size 2MB
#define HUGEPAGE_SIZE (2UL*1024*1024)

// #define LENGTH (256UL*1024*1024)
#define PROTECTION (PROT_READ | PROT_WRITE)

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000 /* arch specific */
#endif

/* Only ia64 requires this */
#ifdef __ia64__
#define ADDR (void *)(0x8000000000000000UL)
#define FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_FIXED)
#else
#define ADDR (void *)(0x0UL)
#define FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)
#endif

template <class CellMeta = CellMeta128>
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
        if (space & 0xfff != 0) {
            // space is not several times of 4KB
            printf("MemBlock size is not 4KB aligned. %lu", space);
            exit(1);
        }
        // printf("Add %.2f MB MemBlock\n", space/1024.0/1024.0 );
        start_addr_ = (char*) mmap(ADDR, space, PROTECTION, FLAGS, -1, 0);
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

    inline char* Allocate(size_t count) {
        // Allocate 'count' cells 
        if (count > remaining_) {
            // There isn't enough space for allocation
            return nullptr;
        }

        // allocate count space.
        ++ref_;
        remaining_ -= count; 
        char* return_addr = cur_addr_;
        cur_addr_ += (count * CellMeta::CellSize());
        
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
        else
            return false;
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

private:
    char*   start_addr_;    // start address of MemBlock
    char*   cur_addr_;      // current position for space allocation
    int     ref_;           // reference of this MemBlock
    size_t  size_;          // number of cells
    size_t  remaining_;     // remaining cells for allocation
    bool    is_hugepage_;   // This MemBlock uses hugepage or not
    int     block_id_;          
};

template <class CellMeta = CellMeta128, size_t kBucketCellCount = 65536>
class MemAllocator {
public:
    MemAllocator(int initial_blocks = 1):
        cur_mem_block_(nullptr),
        next_id_(0) {
        AddMemBlock(initial_blocks);
        cur_mem_block_ = GetMemBlock();
    }

    // Allocate memory space for a Bucket with 'count' cells
    // Return:
    // int: the MemBlock ID
    // char*: address in MemBlock
    inline std::pair<int, char*> Allocate(int count) {
        if (cur_mem_block_->Remaining() < count) {
            // if remaining space is not enough, allocate a new MemBlock
            cur_mem_block_ = GetMemBlock();
        }
        char* addr = cur_mem_block_->Allocate(count);
        if (addr == nullptr) {
            fprintf(stderr, "MemAllocator::Allocate addr is nullptr\n");
            exit(1);
        }
        return {cur_mem_block_->ID(), addr};
    }

    inline void Release(int id) {
        auto iter = mem_block_map_.find(id);
        if (iter != mem_block_map_.end()) {
            bool should_recycle = iter->second->Release();
            if (should_recycle) {
                RecycleMemBlock(iter->second);
            }
        }
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
    int next_id_;
};
} // namespace turbo

