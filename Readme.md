# TurboHash

A high performance concurrent hash table for pesistent memory. It uses Ralloc persistent memory allocator.

# Directory

`CCEH-PMDK`: source code of the implementation of CCEH.

`Clevel-Hashing`: source code of clevel hashing, level hashing, p-clht and cceh version2.

`Dash`: source of dash: scalable hashing on persistent memory

`data`: all the orginal data, and scripts to plot the figures in the papar. Run `analysis_plot.sh` to re-produce the figures.

`lib`: external utils

`src/turbo/turbo_hash_pmem_pmdk.h:`: source code of TurboHash persistent verison.

`src/turbo/turbo_hash.h:`: source code of TurboHash dram verison.

`testbench`: all the benchmark scripts used in the papar.

# Configuration

mount your Pmem device to `/mnt/pmem` directory with DAX mode. For example, `sudo mount -o dax /dev/pmem0 /mnt/pmem`

install gflags to run test benchmark

```
# packages
sudo apt install libgflags-dev g++-10 gcc-10
sudo apt reinstall g++

# compile the code
bash buildall.sh
```


