.PHONY: all clean

CXX := g++ -O3 -std=c++17
CFLAGS := -I./ -lrt

all: ycsb

ycsb: src/ycsb_bench.cpp CCEH
	$(CXX) $(CFLAGS) -o ycsb_bench src/ycsb_bench.cpp src/CCEH_CoW.o -lpmemobj -lpmem -lpthread -lgflags -ltbb -DMULTITHREAD

CCEH: src/CCEH.h src/CCEH.cpp
	$(CXX) $(CFLAGS) -c -o src/CCEH.o src/CCEH.cpp -DINPLACE -lpmemobj -lpmem
	$(CXX) $(CFLAGS) -c -o src/CCEH_CoW.o src/CCEH.cpp -lpmemobj -lpmem

clean:
	rm -rf src/*.o bin/* 
