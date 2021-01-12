# TurboHash

A high performance concurrent hash table for pesistent memory. It uses Ralloc persistent memory allocator.

## Configuration

install gflags to run test benchmark

```
# compile the benchmarks
bash build all
```

`CCEH-PMDK`: source code of the implementation of CCEH.

`Clevel-Hashing`: source code of clevel hashing, level hashing, p-clht and cceh version2.

`data`: all the orginal data, and scripts to plot the figures in the papar.

`lib`: external utils

`src/turbo/turbo_hash_pmem.h:`: source code of TurboHash persistent verison.

`src/turbo/turbo_hash.h:`: source code of TurboHash dram verison.

`testbench`: all the benchmark scripts used in the papar.

