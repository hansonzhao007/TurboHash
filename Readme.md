# TurboHash

A high performance concurrent hash table for pesistent memory.

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

# compile the all the code (TurboHash, CCEH, Clevel, CLHT, Dash)
bash buildall.sh

# configure performance mode
sudo bash config_cpu_performance_mode.sh
```

# Run benchmarks

Go to folder `testbench`, and run the script start with `testbench_*.sh`

For example:

```
cd testbench

sudo bash testbench_scalability.sh

```

You may modify the number of threads in the scripts to match the CPU cores in your server.


