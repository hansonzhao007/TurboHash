#!/usr/bin/env bash

# build TurboHash
mkdir release
cd release
cmake ..
make -j32
cd ..

# build CCEH from the author
cd CCEH-PMDK
make -j32
cd ..

# build Clevel, level, clht and cceh variant. All support 30-byte key-value insertion
cd Clevel-Hashing
mkdir release
cd release
cmake .. -DUSE_TBB=1 -DCMAKE_BUILD_TYPE=Release
make -j32

