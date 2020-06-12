db_test(hello_libpmem)
db_test(trace_test)
db_test(bitset_test)
db_test(spinlock_test)
db_test(context_pool_test)

# run as sudo
db_test(pmem_util_test)

db_exe(hash_bench)
db_exe(hash_multithread)
db_exe(mem_bench)