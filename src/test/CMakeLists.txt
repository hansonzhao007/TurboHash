db_test(trace_test)
db_test(bitset_test)
db_test(spinlock_test)
db_test(hash_test)


# run as sudo
db_test(pmem_util_test)

db_exe(string_convert)
db_exe(hash_bench)
db_exe(mem_bench)
db_exe(motivation)