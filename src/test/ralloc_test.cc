#include <atomic>
#include <functional>
#include <stdio.h>
#include <stdlib.h>

// ralloc
#include "ralloc.hpp"
#include "pptr.hpp"
#include "AllocatorMacro.hpp"

#include "gflags/gflags.h"
using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

DEFINE_string(op, "write", "write, read, delete");


struct Node {
    int32_t key;
    int32_t val;
    atomic_pptr<Node> next;
};

int main(int argc, char *argv[])
{
    ParseCommandLineFlags(&argc, &argv, true);

    printf("Size of Node with pptr pointer: %d\n", sizeof(Node));

    bool res = pm_init();

    if (FLAGS_op == "write") {
        if (res) {
            printf("Dirty restart, prepare to recover\n");
            pm_recover();

        } else {
            printf("Clean restart\n");
        }
        void* buf = pm_malloc(sizeof(Node));
        Node* head = static_cast<Node*>(buf);
        head->key = 666;
        head->val = 6969;

        pm_set_root(buf, 0);

        // add 10 nodes
        Node* cur = head;
        for (int i = 0; i < 10; i++) {
            Node* node = (Node*)pm_malloc(sizeof(Node));
            node->key = 100 + i;
            node->val = 1000 + i;
            node->next = nullptr;
            printf("Write new Node 0x%lx. key: %d, val: %d\n", node, node->key, node->val);
            cur->next.store(node);
            cur = node;
        }
    } else if (FLAGS_op == "read") {
        res = pm_init();
        if (res) {
            printf("Dirty restart, prepare to recover\n");
            pm_recover();

        } else {
            printf("Clean restart\n");
        }

        Node* head = pm_get_root<Node>(0);

        Node* cur = head;
        for (int i = 0; i < 11; i++) {
            printf("Read node 0x%lx. key: %d, val: %d\n", cur, cur->key, cur->val);
            cur = cur->next.load();
        }
    }
    
    pm_close();
    return 0;
}
