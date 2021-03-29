#!/usr/bin/env bash

# build TurboHash
echo "-------- build Turbo ---------"
pwd
mkdir release
cd release
cmake ..
make -j32
cd ..

# build CCEH from the author
echo "-------- build CCEH ---------"
pwd
cd CCEH-PMDK
pwd
make -j32
cd ..

# build Clevel, level, clht and cceh variant. All support 30-byte key-value insertion
echo "-------- build Clevel ---------"
cd Clevel-Hashing
pwd
mkdir release
cd release
cmake .. -DUSE_TBB=1 -DCMAKE_BUILD_TYPE=Release
make -j32
cd ..
cd ..

echo "-------- build Dash ---------"
cd Dash
pwd
mkdir release
cd release
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_PMEM=ON .. 
make -j32