#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libpmem.h>
#include <libvmmalloc.h>
#include <libvmem.h>

#include <algorithm>
#include <vector>
#include <set>

#include "util/env.h"

/* using 1GB of pmem for this example */
#define PMEM_LEN ((1ULL << 30))

#define LIST_LENGTH ((10))

#define VALUE_COUNT ((7))
// #define VALUE_COUNT ((15))
// #define VALUE_COUNT ((31))
// #define VALUE_COUNT ((63))
// #define VALUE_COUNT ((127))


// Node size depends on VALUE_COUNT
// 7:   64   Byte
// 15:  128  Byte
// 31:  256  Byte
// 63:  512  Byte
// 127: 1024 Byte
struct Node {
    uint64_t id;
    char* values[VALUE_COUNT-1];
    uint64_t next_offset;
};

#define NODE_SIZE (sizeof(Node))

void CreataLinkedList();
void ReadLinkedList();

int main(int argc, char *argv[]) {
    printf("Node size: %lu\n", NODE_SIZE);
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <-c/-r> <filename>\n", argv[0]);
        exit(0);
    }
    char *path = argv[2];

    if (strcmp (argv[1], "-c") == 0) {
        CreataLinkedList();
    }
    else {
        ReadLinkedList();
    }

    return 0;
}


void CreataLinkedList() {
    char *pmemaddr;
    size_t mapped_len;
    int is_pmem;

    /* create a pmem file and memory map it */
    if ((pmemaddr = (char *)pmem_map_file("/mnt/pmem0/linklist.txt", PMEM_LEN, PMEM_FILE_CREATE, 0777, &mapped_len, &is_pmem)) == NULL) {
        perror("pmem_map_file");
        exit(1);
    }

    // the initial NODE_SIZE of the beginning of pmemaddr 
    // is use to store the first node address of the linked list
    std::set<uint64_t> candidates;
    while (candidates.size() != LIST_LENGTH) {
        uint64_t off = (random() % (PMEM_LEN - 2 * NODE_SIZE)) / NODE_SIZE * NODE_SIZE + NODE_SIZE;
        candidates.insert(off);
    }

    
    // record the offset that should store a Node
    std::vector<uint64_t> offsets(candidates.begin(), candidates.end());
    std::random_shuffle(offsets.begin(), offsets.end());

    // for(auto off : offsets) {
    //     printf("off: %lu\n", off);
    // }

    Node* cur = nullptr;
    Node* head = nullptr;
    for (uint64_t i = 0; i < offsets.size(); i++) {
        if (i != 0) {
            cur->next_offset = offsets[i];
            cur = (Node*) (pmemaddr + offsets[i]);
            cur->id = i;
            cur->next_offset = 0;
        }
        else {
            // the first node
            head = (Node*)pmemaddr;
            head->next_offset = offsets[i];
            cur = (Node*)(pmemaddr + offsets[i]);
            cur->id = i;
            cur->next_offset = 0;
            printf("head addr: %lx\n", (unsigned long)pmemaddr);
        }
        // printf("write addr: %lx\n", pmemaddr + offsets[i]);
    }
    pmem_persist(pmemaddr, PMEM_LEN);
}

void ReadLinkedList() {
    char *pmemaddr;
    size_t mapped_len;
    int is_pmem;
    /* open the pmem file to read back the data */
    if ((pmemaddr = (char *)pmem_map_file("/mnt/pmem0/linklist.txt", PMEM_LEN, PMEM_FILE_CREATE,
        0777, &mapped_len, &is_pmem)) == NULL) {
        perror("pmem_map_file");
        exit(1);
    }

    Node* head = (Node*)(pmemaddr);
    printf("head addr: %lx\n", (unsigned long)head);
    uint64_t count = 0;
    Node* node = (Node*)(pmemaddr + head->next_offset);
    while (node->next_offset != 0) {
        // printf("read addr: %lx\n", node);
        count++;
        node = (Node*)(pmemaddr + node->next_offset);
    }
    printf("Read Node count: %lu\n", count);
}