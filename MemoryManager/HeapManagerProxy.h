#pragma once
#include <stdio.h>

namespace HeapManagerProxy {
#define __UNUSED_MEM_MARKER__		0xFF	//255
#define __FREED_MEM_MARKER__		0xFE	//254
#define __ALLOC_MEM_MARKER__		0xFD	//253
#define __PRE_GAURD_MEM_MARKER__	0xFC	//252
#define __POST_GAURD_MEM_MARKER__	0xFB	//251
#define __PADDING_MEM_MARKER__		0xFA	//250

#if _DEBUG
#define __DEADZONE__ 4
#else
#define __DEADZONE__ 0
#endif

	struct mem_info_t {
		//void* blockStart;
		size_t blockSize;
		mem_info_t* next = nullptr;
		mem_info_t* prev = nullptr;

		void* blockEndPtr() {
			return (char*)&blockSize + blockSize;
		}
	};

	class HeapManager {
	public:
		void* Alloc(size_t size, const unsigned int alignment = 0);
		bool Free(void*);
		void Collect();
		int IsAllocated(mem_info_t* memBlock);
		

		static HeapManager* CreateHeapManager(void* pHeapMemory, size_t sizeHeap, const unsigned int numDescriptors);
		static void DestroyHeapManager(HeapManager* manager);

		void printDebug()
		{
			printMemInfo();
		}

		void printMemInfo()
		{
			printf("Mem used: %lld, Mem Remaining: %lld\n", currentAllocated, totalHeapSize - currentAllocated);
		}

	private:
		static HeapManager* _heapManager;

		size_t totalHeapSize;
		size_t currentAllocated;

		mem_info_t* freeBlocks = nullptr;
		mem_info_t* allocatedBlocks = nullptr;

	};

	int DebugCheck(char* startPtr, size_t size);

	HeapManager* CreateHeapManager(void* pHeapMemory, size_t sizeHeap, const unsigned int numDescriptors);

	void* alloc(HeapManager* manager, size_t size, const unsigned int alignment = 0);

	void Collect(HeapManager*);

	int Contains(HeapManager*, void*);

	int IsAllocated(HeapManager*, void*);

	bool free(HeapManager*, void*);

	void ShowFreeBlocks(HeapManager*);

	void ShowOutstandingAllocations(HeapManager*);

	void Destroy(HeapManager*);

}


bool HeapManager_UnitTest();

bool SimplerUnitTest();