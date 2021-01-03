# ----- Dram hash test ----
db_test(trace_test)
db_test(bitset_test)
db_test(spinlock_test)
db_test(hash_test)
db_test(loadfactor_probe_dis)

db_exe(hash_bench)
db_exe(hash_bench_old)
db_exe(turbo_hash_test)
db_exe(motivation)

# Pmem hash test
db_test(ralloc_test)
db_test(hello_libpmem)
db_test(pmem_hash_test)
db_test(pmem_hash_recover_test)

db_exe(mem_bench)