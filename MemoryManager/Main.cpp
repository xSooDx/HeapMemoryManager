//#include "HeapManagerProxy.h"

#include "HeapManagerProxy.h"
#include <crtdbg.h>

#include <stdlib.h>
#include <stdio.h>

int main()
{
	using namespace	HeapManagerProxy;

	HeapManager_UnitTest();
	//SimplerUnitTest();

	/*void* A = malloc(sizeof(char) * 20);
	
	printf("%p\n", A);
	printf("%p\n", (int*)A +1);
	printf("%p\n", (char*)A + 1);
	printf("%p\n", (long long int *)A + 1);*/

	_CrtDumpMemoryLeaks();

}