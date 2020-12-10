#include <stdio.h>
#include <stdlib.h>
#include <libpmemobj.h>
#include "util/pmm_util.h"

int main()
{
    util::PCMMetric tmp("1");
    char* buffer = (char*)malloc(1024*1024*1024);
    memset(buffer, 1, 1024*1024*1024);
    PMEMobjpool *pop = pmemobj_create("/mnt/pmem0/testobj1", "", 100*1024*1024, 0666);
    if (pop == NULL)
        perror("Fail\n");
    else
        printf("Successed\n");
    return 0;
}