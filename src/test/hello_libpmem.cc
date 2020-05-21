#include <stdio.h>
#include <stdlib.h>
#include <libpmemobj.h>

int main()
{
    PMEMobjpool *pop = pmemobj_create("/mnt/pmem0/testobj1", "", 100*1024*1024, 0666);
    if (pop == NULL)
        perror("Fail\n");
    else
        printf("Successed\n");
    return 0;
}