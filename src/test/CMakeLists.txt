# ----- Dram hash test ----
db_test(spinlock_test)
db_test(hash_test)
db_exe(example)
db_test(turbo_hash_test)

db_exe(hash_bench)
db_exe(hash_bench_pmdk)
db_exe(hash_bench_30)
db_exe(turbo_hash_test)
db_exe(motivation)
db_exe(mem_bench)

# Pmem hash test
db_test(ralloc_test)
db_test(hello_libpmem)
db_test(pmem_rehash_test)
db_test(pmem_hash_test)
db_test(pmem_hash_recover_test)
