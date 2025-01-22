
#include "MemoryManager.h"
#include <cassert>
#include <time.h>
#include "stdlib.h"

#define PRINT_DATA(count, value)  printf("Count = %u; Value = %lf", count, value)

class Dummy
{
	uint64_t mCount;
	double mValue;

public:
	Dummy()
		: mCount(0), mValue(0.0) {}

	Dummy(uint64_t count, double value)
		: mCount(count), mValue(value) {}

	inline uint64_t GetCount() { return mCount; }
	inline double GetValue() { return mValue; }
};

int main()
{
	srand(time(NULL));

	int poolSize = 100;
	MemoryManager* memoryManager = new MemoryManager(poolSize);

	Dummy* ptr[1000];

	// TEST 1: Allocate over the limit
	for (int index = 0; index < poolSize + 2; index++)
	{
		ptr[index] = memoryManager->Allocate<Dummy>();
	}

	// Free (1/5)th of blocks randomly
	// Test 2: Check if memory at correct addresses are getting freed.
	for (int iteration = 0; iteration < (poolSize/5); iteration++)
	{
		int index = poolSize * (rand() / static_cast<float>(RAND_MAX));
		
#ifdef _DEBUG
		printf("Freeing index %d\n", index);
#endif
		
		memoryManager->Free(&ptr[index]);
	}

	// TEST 3: Reallocate blocks. Reallocation should happen in the correct order and there should be one overflow
	for (int iteration = 0; iteration < 21; iteration++)
	{
		ptr[poolSize + iteration] = memoryManager->Allocate<Dummy>();
	}

	// TEST 4: Try double deletes

	int randIndices[5];
	for (int iteration = 0; iteration < 5; iteration++)
	{
		randIndices[iteration] = poolSize * (rand() / static_cast<float>(RAND_MAX));
#ifdef _DEBUG
		printf("\n%d\t%d\n", iteration, randIndices[iteration]);
#endif // _DEBUG
	}
	
	Dummy* d0 = ptr[randIndices[0]];
	Dummy* d1 = ptr[randIndices[1]];
	Dummy* d2 = ptr[randIndices[2]];
	memoryManager->Free(&d0);
	memoryManager->Free(&d0); // Invalid pointer passed
	memoryManager->Free(&ptr[randIndices[1]]);
	memoryManager->Free(&ptr[randIndices[0]]); // Should fail. d0 already marked this free 
	d0 = memoryManager->Allocate<Dummy>();
	memoryManager->Free(&ptr[randIndices[1]]); // Pointer itself Invalidated by previous free
	memoryManager->Free(&d1); // Should be successful

#ifndef _DEBUG
	poolSize = 1000;
	delete memoryManager;
	memoryManager = new MemoryManager(poolSize);
	
	clock_t startTime = clock();

	for (int index = 0; index < poolSize; index++)
	{
		ptr[index] = memoryManager->Allocate<Dummy>();
	}

	for (int index = 0; index < poolSize; index++)
	{
		memoryManager->Free(&ptr[index]);
	}

	clock_t endTime = clock();

	printf("\nTime taken with custom pool memory allocator = %lf", static_cast<double>(endTime - startTime) / CLOCKS_PER_SEC);

	startTime = clock();
	
	for (int index = 0; index < poolSize; index++)
	{
		ptr[index] = new Dummy();
	}

	for (int index = 0; index < poolSize; index++)
	{
		delete ptr[index];
	}
	
	endTime = clock();

	printf("\nTime taken without = %lf", static_cast<double>(endTime - startTime) / CLOCKS_PER_SEC);

#endif // !_DEBUG
}