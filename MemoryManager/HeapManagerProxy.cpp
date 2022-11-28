#include "HeapManagerProxy.h"

#include <assert.h>

namespace HeapManagerProxy
{
	const size_t _MEM_INFO_SIZE_ = sizeof(mem_info_t);
	const size_t _USER_DATA_OFFSET_ = _MEM_INFO_SIZE_ + __DEADZONE__;

	void DebugFlood(char* startPtr, size_t size, unsigned char value)
	{
		char* memEnd = startPtr + size;
		for (char* floodPtr = reinterpret_cast <char*>(startPtr); floodPtr < memEnd; floodPtr++)
		{
			*floodPtr = value;
		}
	}

	int DebugCheck(char* startPtr, size_t size)
	{
		char* memEnd = startPtr + size;
		for (char* checkPtr = reinterpret_cast<char*>(startPtr); checkPtr < memEnd; checkPtr++)
		{
			switch (*checkPtr)
			{
			case __UNUSED_MEM_MARKER__:
				break;
			case __FREED_MEM_MARKER__:
				break;
			case __ALLOC_MEM_MARKER__:
				return -1;
			case __PRE_GAURD_MEM_MARKER__:
				return -2;
			case __POST_GAURD_MEM_MARKER__:
				return -3;
			case __PADDING_MEM_MARKER__:
				return -4;
			}
		}
		return 0;
	}

	HeapManager* HeapManager::CreateHeapManager(void* pHeapMemory, size_t sizeHeap, const unsigned int numDescriptors)
	{
		//printf("Create Heap Manager %p %lld %lld\n", pHeapMemory, sizeHeap, sizeof(HeapManager));
		(void)numDescriptors;
		size_t heapManagerSize = sizeof(HeapManager);

#if _DEBUG
		DebugFlood(reinterpret_cast<char*>(pHeapMemory), sizeHeap, __UNUSED_MEM_MARKER__);
#endif

		HeapManager* manager = reinterpret_cast<HeapManager*>(pHeapMemory);
		manager->totalHeapSize = sizeHeap - heapManagerSize;
		manager->currentAllocated = 0;
		mem_info_t* firstFreeBlock = reinterpret_cast<mem_info_t*>(reinterpret_cast<char*>(pHeapMemory) + heapManagerSize);
		firstFreeBlock->blockSize = sizeHeap - heapManagerSize;
		firstFreeBlock->next = nullptr;
		firstFreeBlock->prev = nullptr;
		manager->freeBlocks = firstFreeBlock;

		manager->allocatedBlocks = nullptr;

		return manager;
	}


	void* HeapManager::Alloc(size_t size, const unsigned int alignment)
	{
		(void)alignment;
		// [ MemoryBlock ][ DEADZONE ][ AllocatedMemory ][ DEADZONE ]
		// ^freeBlocks
		// ^allocatedBlock                          
		if (!size) return nullptr;
		if (!freeBlocks) return nullptr;

		mem_info_t* allocationBlock = freeBlocks;
		mem_info_t* prev = nullptr;
		size_t totalSize = _MEM_INFO_SIZE_ + __DEADZONE__ + size + __DEADZONE__;
		//size_t sizeToCheck = totalSize + _MEM_INFO_SIZE_; // to ensure the mem_info_t space is not overwritten partially

		// Find first block that can fit the sizeToCheck. 
		while (allocationBlock && (allocationBlock->blockSize) < totalSize)
		{
			prev = allocationBlock;
			allocationBlock = allocationBlock->next;
		}
		if (!allocationBlock) return nullptr;
		mem_info_t* newBlock = reinterpret_cast<mem_info_t*>(reinterpret_cast<char*>(allocationBlock->blockEndPtr()) - totalSize);

#if _DEBUG
		int check = DebugCheck(reinterpret_cast<char*>(newBlock), totalSize);
		assert(check == 0);
#endif
		// check to avoid partial writing on mem_info_t data
		if (reinterpret_cast <char*>(allocationBlock) + _MEM_INFO_SIZE_ >= reinterpret_cast<char*>(newBlock))
		{
			// extend the allocated block to prevent partial overwrites
			newBlock = allocationBlock;
#if _DEBUG
			DebugFlood(reinterpret_cast<char*>(newBlock) + totalSize, allocationBlock - newBlock, __PADDING_MEM_MARKER__);
#endif
			totalSize = allocationBlock->blockSize;

			// Remove the block from free list because whole block will be used
			if (allocationBlock->prev) allocationBlock->prev->next = allocationBlock->next;
			if (allocationBlock->next) allocationBlock->next->prev = allocationBlock->prev;
			if (allocationBlock == freeBlocks)
			{
				freeBlocks = freeBlocks->next;
			}
		}
		else
		{
			// shrink the block for partial use
			allocationBlock->blockSize -= totalSize;
		}


#if _DEBUG
		DebugFlood(reinterpret_cast<char*>(newBlock), totalSize, __ALLOC_MEM_MARKER__);
#endif
		newBlock->blockSize = totalSize;
		newBlock->next = nullptr;
		newBlock->prev = nullptr;

		// Add to allocated blocks list
		if (this->allocatedBlocks)
		{
			newBlock->next = this->allocatedBlocks;
			this->allocatedBlocks->prev = newBlock;
		}
		this->allocatedBlocks = newBlock;
#if _DEBUG
		DebugFlood(reinterpret_cast<char*>(newBlock) + _MEM_INFO_SIZE_, __DEADZONE__, __PRE_GAURD_MEM_MARKER__);
		DebugFlood(reinterpret_cast<char*>(newBlock->blockEndPtr()) - __DEADZONE__, __DEADZONE__, __POST_GAURD_MEM_MARKER__);
#endif

		this->currentAllocated += totalSize;

		char* userMemPointer = reinterpret_cast<char*>(newBlock) + _USER_DATA_OFFSET_;
		return (void*)userMemPointer;
	}

	bool HeapManager::Free(void* ptr)
	{
		// [ MemoryBlock ][ DEADZONE ][ AllocatedMemory ][ DEADZONE ]
		// ^freeBlocks
		// ^AllocatedBlock

		mem_info_t* recoveredMemBlock = reinterpret_cast<mem_info_t*>((reinterpret_cast<char*>(ptr) - _USER_DATA_OFFSET_));

		assert(this->IsAllocated(recoveredMemBlock));

		// Remove block from allocated list
		if (recoveredMemBlock->prev) recoveredMemBlock->prev->next = recoveredMemBlock->next;
		if (recoveredMemBlock->next) recoveredMemBlock->next->prev = recoveredMemBlock->prev;
		if (this->allocatedBlocks == recoveredMemBlock) this->allocatedBlocks = recoveredMemBlock->next;

		size_t recoveredSize = recoveredMemBlock->blockSize;
		assert(recoveredSize <= this->totalHeapSize);

#if _DEBUG
		DebugFlood(reinterpret_cast<char*>(recoveredMemBlock), recoveredMemBlock->blockSize, __FREED_MEM_MARKER__);
#endif

		recoveredMemBlock->blockSize = recoveredSize;
		recoveredMemBlock->next = nullptr;
		recoveredMemBlock->prev = nullptr;

		if (!this->freeBlocks)
		{
			// if no free blocks, set this as free block
			this->freeBlocks = recoveredMemBlock;
		}
		else
		{
			// find where to insert block into list
			mem_info_t* currFreeBlock = this->freeBlocks;
			bool reachedEnd = false;
			while (recoveredMemBlock > currFreeBlock)
			{
				if (currFreeBlock->next)
				{
					currFreeBlock = currFreeBlock->next;
				}
				else
				{
					reachedEnd = true;
					break;
				}
			}

			assert(currFreeBlock);
			assert(currFreeBlock != recoveredMemBlock); // wtf why is the new free block also in the free list already

			if (!reachedEnd)
			{
				// insert freed block into free block list, behind the current block
				recoveredMemBlock->next = currFreeBlock;
				recoveredMemBlock->prev = currFreeBlock->prev;

				if (currFreeBlock->prev) currFreeBlock->prev->next = recoveredMemBlock;
				currFreeBlock->prev = recoveredMemBlock;

				if (currFreeBlock == this->freeBlocks) this->freeBlocks = recoveredMemBlock;

			}
			else
			{
				currFreeBlock->next = recoveredMemBlock;
				recoveredMemBlock->prev = currFreeBlock;
			}
		}

		this->currentAllocated -= recoveredSize;

		return true;
	}
	void HeapManager::Collect()
	{
		if (!this->freeBlocks) return;

		mem_info_t* currBlock = this->freeBlocks;

		while (currBlock)
		{
			mem_info_t* temp = currBlock->next;
			if (currBlock->prev && currBlock->prev->blockEndPtr() == currBlock)
			{
				mem_info_t* prevBlock = currBlock->prev;

				prevBlock->blockSize += currBlock->blockSize;
				prevBlock->next = currBlock->next;
				if (currBlock->next) currBlock->next->prev = prevBlock;
#if _DEBUG
				DebugFlood(reinterpret_cast<char*>(currBlock), _MEM_INFO_SIZE_, __FREED_MEM_MARKER__);
#endif
			}

			currBlock = temp;
		}

	}

	int HeapManager::IsAllocated(mem_info_t* memBlock)
	{
		mem_info_t* currAllocatedBlock = this->allocatedBlocks;

		// Check that this was pointer was actuall allocated by us
		while (currAllocatedBlock && currAllocatedBlock != memBlock)
		{
			currAllocatedBlock = currAllocatedBlock->next;
		}
		assert(currAllocatedBlock);
		return !!currAllocatedBlock;
	}

	void HeapManager::DestroyHeapManager(HeapManager* manager)
	{
		(void)manager;
		//ToDo Cleanup memory structures
	}


	HeapManager* CreateHeapManager(void* pHeapMemory, size_t sizeHeap, const unsigned int numDescriptors)
	{
		return HeapManager::CreateHeapManager(pHeapMemory, sizeHeap, numDescriptors);
	}

	void* alloc(HeapManager* manager, size_t size, const unsigned int alignment)
	{
		return manager->Alloc(size, alignment);
	}

	void Collect(HeapManager* manager)
	{
		manager->Collect();
	}

	int Contains(HeapManager*, void*)
	{
		return true;
	}

	int IsAllocated(HeapManager* manager, void* ptr)
	{
		mem_info_t* memBlock = reinterpret_cast<mem_info_t*>(reinterpret_cast<char*>(ptr) - _USER_DATA_OFFSET_);
		return manager->IsAllocated(memBlock);
	}

	bool free(HeapManager* pHeapMemory, void* ptr)
	{
		return pHeapMemory->Free(ptr);
	}

	void ShowFreeBlocks(HeapManager*)
	{}

	void ShowOutstandingAllocations(HeapManager*)
	{}

	void Destroy(HeapManager*)
	{

	}
}
