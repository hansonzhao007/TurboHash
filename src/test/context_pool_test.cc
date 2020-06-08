#include "util/object_pool.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

const long NUMBER_OF_ITEMS = 200;
const long NUMBER_OF_ITERATIONS = 100000;
const long NUMBER_OF_OPERATIONS = NUMBER_OF_ITEMS * NUMBER_OF_ITERATIONS; 
using namespace util;
struct iovec
{
	void *iov_base;	/* Pointer to data.  */
	size_t iov_len;	/* Length of data.  */
};

struct RWContext {
	bool is_read;
	uint32_t io_id;
	struct iovec* iov;

	void PrintInfo() {
		printf("[ID: %2u - %s]: %.100s \n", io_id, is_read ? "Read" : "Write", (char*)iov->iov_base);
	}
};

int main(int argc, char* argv[])
{
	// I know. Allocating a char is stupid, but the purpose is to show
	// how the pool is fast to allocate and deallocate small objects.

	ObjectPool<RWContext> pool(1000);
	RWContext* variables[NUMBER_OF_ITEMS];

	std::cout << "This application only tests the performance of the pool versus normal" << std::endl;
	std::cout << "new and delete calls. To get the real results, compile it in release mode" << std::endl;
	std::cout << "and run it outside Visual Studio" << std::endl << std::endl;

	std::cout << "Starting to test the pool..." << std::endl;
	int start = clock();
	for(int j = 0;j < NUMBER_OF_ITERATIONS; j++)
	{
		for(int i = 0; i < NUMBER_OF_ITEMS; i++)
			variables[i] = pool.Allocate();

		for(int i = 0; i < NUMBER_OF_ITEMS; i++)
			pool.Release(variables[i]);
	}
	int end = clock();

    double seconds = (end - start) / double(CLOCKS_PER_SEC);
	std::cout << "Time spent using the pool: " <<  seconds << " s" << " Speed: " << NUMBER_OF_OPERATIONS * 2.0 / seconds / 1000000.0  << " Mops/s" << std::endl << std::endl;

	std::cout << "Starting to test the normal new and delete calls..." << std::endl;
	start = clock();
	for(int j=0;j<NUMBER_OF_ITERATIONS; j++)
	{
		for(int i=0; i<NUMBER_OF_ITEMS; i++)
			variables[i] = new RWContext();

		for(int i=0; i<NUMBER_OF_ITEMS; i++)
			delete variables[i];
	}
	end = clock();
    seconds = (end - start) / double(CLOCKS_PER_SEC);

	std::cout << "Time spent using normal new/delete calls: " <<  seconds << " s" << " Speed: " << NUMBER_OF_OPERATIONS * 2.0 / seconds / 1000000.0  << " Mops/s" << std::endl << std::endl;

	// std::cout << "Program finished. Press any key to close." << std::endl;
	// std::cin.ignore();

	return 0;
}

