/* =========================================================================================
* 
*	Class:		Memory Manager
*	Purpose:	Custom Pool Memory Allocator
*	Author:		Sreyash (Srey) Raychaudhuri
*	Date:		12/22/2024
* 
* ==========================================================================================
*/

#pragma once

#include <cstdint>
#include <stdio.h>
#include <unordered_map>

#define NUMBITSPERBYTE 8

class MemoryManager
{
public:

	MemoryManager(unsigned int numBlocksPerPool = 10) 
		: mNumBlocksPerPool(numBlocksPerPool)
	{
		// Pools where size of each data type = 2 pow exp;
		for (int exponent = 3; exponent <= 5; exponent++)
		{
			InitializePool(pow(2, exponent), numBlocksPerPool);
		}
	}

	~MemoryManager()
	{
		// Releasing pools
		for (auto iter = mPool.begin(); iter != mPool.end(); ++iter)
		{
			delete[] reinterpret_cast<char*>((*iter).second);
		}

		mPool.clear();
	}

	// Preallocate a block of memory
	void InitializePool(size_t size, uint16_t numBytes);

	// Allocate a block of memory and return starting address of allocated block
	template<typename T>
	T* Allocate();
	
	// Free memory pointed to by ptr variable var
	template<typename T>
	void Free(T** pointer);

private:
	unsigned int mNumBlocksPerPool;
	std::unordered_map<size_t, void*> mPool;
};


void MemoryManager::InitializePool(size_t size, uint16_t numBlocks)
{
	// Pre-allocate memory on the heap. 
	// Add an extra block in the end - will store address value of first available free block in the pool
	// Extra (numBlocks/8) bytes for more metadata. ith bit indicates allocation status for ith block in this pool
	// Bit = 1 means block is allocated, free otherwise
	
	// Layout: [Key = Size of each elem] ---> Memory: [[Actual Storage][Ptr to first free block][Bitfield to determine allocated blocks]]

	mPool[size] = (void*)(new char[(size * numBlocks) + sizeof(void*) + (numBlocks / NUMBITSPERBYTE) + 1]);

	// Store address of each next available free block in the free block itself
	// This works only if sizeof(element) >= sizeof(void*)
	// Here sizeof(void*) = 64 bits (8 bytes); So will work with pools where size of each element >= 8 bytes

	uintptr_t* firstFreeBlockAddress = reinterpret_cast<uintptr_t*>(mPool[size]);
	uintptr_t* currentFreeBlockAddress = firstFreeBlockAddress;

	uint8_t stride = (size / sizeof(void*));

	// Store addresses of next available free block from each free block within the free blocks themselves
	for (int iteration = 1; iteration < numBlocks; ++iteration)
	{
		*currentFreeBlockAddress = reinterpret_cast<uintptr_t>((currentFreeBlockAddress + stride));
		currentFreeBlockAddress += stride;
	}

	// Next from last block will be nullptr.
	*currentFreeBlockAddress = NULL;

	// The last element will store the address of the first free block in this pool
	currentFreeBlockAddress += stride;
	*currentFreeBlockAddress = reinterpret_cast<uintptr_t>(firstFreeBlockAddress);
}


template<typename T>
T* MemoryManager::Allocate()
{
	size_t dataTypeSize = sizeof(T);
	if (!mPool[dataTypeSize]) // If value is not 0, pool for elements of size sizeof(T) exists
	{
		InitializePool(dataTypeSize, mNumBlocksPerPool);
	}

	// First grab the address of the first free available block where we can store our value. 
    // This address is stored after the last block in the pool
	uintptr_t* firstElementPtr = reinterpret_cast<uintptr_t*>(mPool[dataTypeSize]);
	uintptr_t* lastElementPtr = reinterpret_cast<uintptr_t*>(firstElementPtr + (dataTypeSize / sizeof(void*)) * mNumBlocksPerPool);
	
	// First check if pool is full
	if (*lastElementPtr == NULL)
	{
#ifdef _DEBUG
		printf("[FAILURE] Pool exhausted!\n\n");
#endif // _DEBUG
		return nullptr;
	}

	uintptr_t firstFreeBlockAddressValue = *lastElementPtr;

	// Need to mark this block as allocated by setting the correct bit in the allocation-tracking bitfield.
	unsigned int indexBlockAllocated = (firstFreeBlockAddressValue - reinterpret_cast<uintptr_t>(firstElementPtr)) / dataTypeSize;
	unsigned char* desiredByte = reinterpret_cast<unsigned char*>(reinterpret_cast<uintptr_t>(lastElementPtr) + sizeof(void*) + (indexBlockAllocated / NUMBITSPERBYTE));
	*(desiredByte) |= (1 << (NUMBITSPERBYTE - (indexBlockAllocated % NUMBITSPERBYTE) - 1));


#ifdef _DEBUG
	printf("\n[SUCCESS] Index of allocated block\t= %d\nAddress of desired byte\t= %p\nByte after setting status\t= %x\nAllocating block at\t= %p\n", 
			indexBlockAllocated, 
			desiredByte,
			*(reinterpret_cast<unsigned char*>(desiredByte)),
			(void*)firstFreeBlockAddressValue
		);
#endif // _DEBUG

	uintptr_t temp = firstFreeBlockAddressValue;

	// The value in this free block is the next available free block - which will now become the first free block
	firstFreeBlockAddressValue = *(reinterpret_cast<uintptr_t*>(firstFreeBlockAddressValue));

	// Now store the actual value. Using temp since firstFreeBlockAddressValue got updated
	T* newValueAddress = reinterpret_cast<T*>(temp);

	*lastElementPtr = firstFreeBlockAddressValue;

#ifdef _DEBUG
	printf("Next block from first free block =\t%p\n\n", (void*)firstFreeBlockAddressValue);
#endif // _DEBUG

	return newValueAddress;
}


template<typename T>
void MemoryManager::Free(T** ppBlock)
{
#ifdef _DEBUG
	printf("Attempting to free memory at address = %p\n", *ppBlock);
#endif // _DEBUG

	if (*ppBlock == nullptr)
	{
#ifdef _DEBUG
		printf("[FAILURE]Invalid pointer passed\n", *ppBlock);
#endif // _DEBUG
		return;
	}

	size_t dataTypeSize = sizeof(T);

	uintptr_t* firstElementPtr = reinterpret_cast<uintptr_t*>(mPool[dataTypeSize]);
	uintptr_t* lastElementPtr = reinterpret_cast<uintptr_t*>(firstElementPtr + (dataTypeSize / sizeof(uintptr_t)) * mNumBlocksPerPool);
	
	unsigned int indexBlockAllocated = (reinterpret_cast<uintptr_t>(*ppBlock) - reinterpret_cast<uintptr_t>(firstElementPtr)) / dataTypeSize;
	unsigned char* desiredByte = reinterpret_cast<unsigned char*>(reinterpret_cast<uintptr_t>(lastElementPtr) + sizeof(void*) + (indexBlockAllocated / NUMBITSPERBYTE));
	unsigned int shiftValue = NUMBITSPERBYTE - (indexBlockAllocated % NUMBITSPERBYTE) - 1;
	
	// If bit at the ith index of the bitfield 
	// which denotes allocation status of ith block. If already 0, we're trying to do a double free
	if ((*desiredByte & (1 << shiftValue)) == 0) 
	{
#ifdef _DEBUG
		printf("[FAILURE] Attempting a double free!\n");
#endif // _DEBUG

		return;
	}

	*desiredByte ^= (1 << shiftValue); // Bit was 1; XOR with 1 to make it 0 (status set to free)

	// Location pointed to by pointer to be freed will now hold the address value of the next free block which is the previous first free available block
	*(reinterpret_cast<uintptr_t*>(*ppBlock)) = *lastElementPtr;
	
	// First free block address = Address of pointer freed
	*lastElementPtr = reinterpret_cast<uintptr_t>(*ppBlock);

#ifdef _DEBUG
	printf("[SUCCESS] Index of allocated block = \t%d\nFirst free block addr =\t%p\nAddress of next free block =\t%x\n\n",
		indexBlockAllocated,
		(void*)(*lastElementPtr), 
		(void*)(*(reinterpret_cast<uintptr_t*>(*ppBlock))));
#endif // _DEBUG

	// Invalidate pointer
	*ppBlock = nullptr;
}