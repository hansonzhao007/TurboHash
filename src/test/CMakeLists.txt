db_test(trace_test)
db_test(bitset_test)
db_test(spinlock_test)
db_test(hash_test)
db_test(ralloc_test)
db_test(hello_libpmem)

# run as sudo
# db_test(pmem_util_test)

# db_exe(string_convert)
db_exe(hash_bench)
db_exe(hash_bench_old)
db_exe(turbo_hash_test)
# db_exe(mem_bench)
db_exe(motivation)